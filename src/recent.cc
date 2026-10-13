#include "recent.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

#include <dlfcn.h>

#include "actions.h"
#include "nickel.h"
#include "paths.h"
#include "util.h"

namespace {

const int kSqliteOpenReadOnly = 1;
const int kSqliteRow = 100;

// Nickel's last-read query: the newest file-backed book with a read time.
const char kLastReadSql[] = "select ContentID, Title, Attribution, DateLastRead, ___PercentRead from content "
                            "where ContentType='6' and ContentID like 'file://%' and DateLastRead is not null "
                            "order by DateLastRead desc limit 1";

QString readText(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    return QString::fromUtf8(f.readAll());
}

// The value of ["key"] = "value" in a Lua table, with escapes resolved and
// whitespace collapsed. Only the first occurrence is read.
QString luaString(const QString &text, const char *key) {
    const QString needle = QStringLiteral("[\"") + QString::fromLatin1(key) + QStringLiteral("\"] = \"");
    int i = text.indexOf(needle);
    if (i < 0) return QString();
    i += needle.size();
    QString out;
    while (i < text.size()) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\\') && i + 1 < text.size()) {
            const QChar n = text.at(i + 1);
            if (n == QLatin1Char('n') || n == QLatin1Char('\n'))
                out += QLatin1Char(' ');
            else
                out += n;
            i += 2;
            continue;
        }
        if (c == QLatin1Char('"')) break;
        out += c;
        i++;
    }
    return out.simplified();
}

// The bare value after ["key"] = , up to the next comma, read from at most
// `width` characters.
QString luaBareValue(const QString &text, const char *key, int width) {
    const QString needle = QStringLiteral("[\"") + QString::fromLatin1(key) + QStringLiteral("\"] = ");
    const int i = text.indexOf(needle);
    if (i < 0) return QString();
    return text.mid(i + needle.size(), width).section(QLatin1Char(','), 0, 0).trimmed();
}

// KOReader's <book>.sdr/metadata.<ext>.lua: title and authors from doc_props,
// percent_finished as a fraction. The title falls back to the file name.
void readKoreaderSidecar(const QString &file, NtRecentBook *book) {
    const int dot = file.lastIndexOf(QLatin1Char('.'));
    const int slash = file.lastIndexOf(QLatin1Char('/'));
    const bool hasExt = dot > slash;
    const QString base = hasExt ? file.left(dot) : file;
    const QString ext = hasExt ? file.mid(dot + 1) : QString();
    const QString meta = readText(base + QStringLiteral(".sdr/metadata.") + ext + QStringLiteral(".lua"));

    const int props = meta.indexOf(QStringLiteral("[\"doc_props\"]"));
    const QString propsText = props >= 0 ? meta.mid(props, 4000) : QString();
    book->title = luaString(propsText, "title");
    book->author = luaString(propsText, "authors");

    bool ok = false;
    const double fraction = luaBareValue(meta, "percent_finished", 24).toDouble(&ok);
    if (ok) book->percent = qRound(fraction * 100.0);

    if (book->title.isEmpty()) book->title = file.mid(slash + 1, (hasExt ? dot : file.size()) - slash - 1);
}

// KOReader's most recent book that still exists, from history.lua, whose
// entries ([1] = { ... }, newest first) each hold a file and a time.
NtRecentBook koreaderBook() {
    NtRecentBook book;
    book.app = NtReaderApp::Koreader;
    const QString history = readText(QStringLiteral(NT_KOREADER_HISTORY));
    const QStringList entries = history.split(QRegularExpression(QStringLiteral("\\[\\d+\\] = \\{")));
    for (const QString &entry : entries) {
        const QString file = luaString(entry, "file");
        if (file.isEmpty() || !QFile::exists(file)) continue;
        book.path = file;
        book.lastRead = luaBareValue(entry, "time", 20).toLongLong();
        readKoreaderSidecar(file, &book);
        book.valid = true;
        break;
    }
    return book;
}

// Parses a Plato index or state file; an empty object if it cannot be read.
QJsonObject readJsonObject(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QJsonObject();
    return QJsonDocument::fromJson(f.readAll()).object();
}

// Plato's "opened" time, or -1 when it is missing or unparsable. Plato writes
// local time without a zone, so it is read as such.
qint64 platoOpenedTime(const QJsonObject &state) {
    const QString opened = state.value(QStringLiteral("opened")).toString();
    QDateTime dt = QDateTime::fromString(opened, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid()) dt = QDateTime::fromString(opened, Qt::ISODate);
    return dt.isValid() ? static_cast<qint64>(dt.toTime_t()) : -1;
}

int platoPercent(const QJsonObject &state) {
    const double current = state.value(QStringLiteral("currentPage")).toDouble(-1);
    const double total = state.value(QStringLiteral("pagesCount")).toDouble(0);
    return (current >= 0 && total > 0) ? qRound(current * 100.0 / total) : -1;
}

// Plato keeps its library index at the root of onboard: .metadata.json, one
// object per book with "file": {"path": ...}. Per-book state is in
// .reading-states/<fingerprint>.json, where <fingerprint> is the book's key in
// the index. The newest opened state of a book that still exists wins.
NtRecentBook platoBook() {
    NtRecentBook book;
    book.app = NtReaderApp::Plato;
    QFile f(QStringLiteral(NT_PLATO_METADATA));
    if (!f.open(QIODevice::ReadOnly)) return book;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (!doc.isObject()) {
        NT_DBG("recent (plato): metadata parse: %s", qPrintable(err.errorString()));
        return book;
    }
    const QJsonObject index = doc.object();
    const QDir states(QStringLiteral(NT_PLATO_STATES));
    const QStringList files = states.entryList(QStringList(QStringLiteral("*.json")), QDir::Files);
    for (const QString &name : files) {
        const QJsonObject state = readJsonObject(states.filePath(name));
        const qint64 opened = platoOpenedTime(state);
        if (opened < 0 || opened <= book.lastRead) continue;
        const QString key = name.left(name.size() - 5);
        const QJsonObject entry = index.value(key).toObject();
        if (entry.isEmpty()) {
            NT_DBG("recent (plato): state %s has no index entry", qPrintable(key));
            continue;
        }
        const QString path = QStringLiteral(NT_ONBOARD "/") +
                             entry.value(QStringLiteral("file")).toObject().value(QStringLiteral("path")).toString();
        if (!QFile::exists(path)) continue;
        book.lastRead = opened;
        book.path = path;
        book.title = entry.value(QStringLiteral("title")).toString();
        book.author = entry.value(QStringLiteral("author")).toString();
        book.percent = platoPercent(state);
        book.valid = true;
        if (book.title.isEmpty()) book.title = book.path.section(QLatin1Char('/'), -1).section(QLatin1Char('.'), 0, -2);
    }
    return book;
}

// The sqlite3 functions Nickel already has loaded; looked up by name, so
// each may be missing on another firmware.
struct Sqlite {
    int (*open)(const char *, void **, int, const char *);
    int (*prepare)(void *, const char *, int, void **, const char **);
    int (*step)(void *);
    const unsigned char *(*columnText)(void *, int);
    int (*columnInt)(void *, int);
    int (*finalize)(void *);
    int (*close)(void *);
};

bool loadSqlite(Sqlite *s) {
    s->open =
        reinterpret_cast<int (*)(const char *, void **, int, const char *)>(dlsym(RTLD_DEFAULT, "sqlite3_open_v2"));
    s->prepare = reinterpret_cast<int (*)(void *, const char *, int, void **, const char **)>(
        dlsym(RTLD_DEFAULT, "sqlite3_prepare_v2"));
    s->step = reinterpret_cast<int (*)(void *)>(dlsym(RTLD_DEFAULT, "sqlite3_step"));
    s->columnText = reinterpret_cast<const unsigned char *(*)(void *, int)>(dlsym(RTLD_DEFAULT, "sqlite3_column_text"));
    s->columnInt = reinterpret_cast<int (*)(void *, int)>(dlsym(RTLD_DEFAULT, "sqlite3_column_int"));
    s->finalize = reinterpret_cast<int (*)(void *)>(dlsym(RTLD_DEFAULT, "sqlite3_finalize"));
    s->close = reinterpret_cast<int (*)(void *)>(dlsym(RTLD_DEFAULT, "sqlite3_close"));
    return s->open && s->prepare && s->step && s->columnText && s->columnInt && s->finalize && s->close;
}

QString columnText(const Sqlite &sql, void *stmt, int column) {
    const unsigned char *text = sql.columnText(stmt, column);
    return text ? QString::fromUtf8(reinterpret_cast<const char *>(text)) : QString();
}

// DateLastRead is UTC with a trailing Z. Qt 5.2's ISODate parse treats it as
// local time, which put Nickel 2 h behind KOReader, so the zone is set here.
qint64 nickelLastReadTime(QString when) {
    if (when.endsWith(QLatin1Char('Z'))) when.chop(1);
    QDateTime dt = QDateTime::fromString(when, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    dt.setTimeSpec(Qt::UTC);
    return dt.isValid() ? static_cast<qint64>(dt.toTime_t()) : 0;
}

void readNickelRow(const Sqlite &sql, void *stmt, NtRecentBook *book) {
    const QString id = columnText(sql, stmt, 0);
    book->contentId = id;
    book->path = id.startsWith(QLatin1String("file://")) ? id.mid(7) : id;
    book->title = columnText(sql, stmt, 1);
    book->author = columnText(sql, stmt, 2);
    book->lastRead = nickelLastReadTime(columnText(sql, stmt, 3));
    book->percent = sql.columnInt(stmt, 4);
    book->valid = QFile::exists(book->path);
}

// Nickel's last-read book from KoboReader.sqlite, read-only and WAL-safe.
NtRecentBook nickelBook() {
    NtRecentBook book;
    book.app = NtReaderApp::Nickel;
    Sqlite sql;
    if (!loadSqlite(&sql)) {
        NT_DBG("recent (nickel): sqlite3 symbols unavailable");
        return book;
    }
    void *db = nullptr;
    const int rc = sql.open(QStringLiteral(NT_KOBO_DB).toUtf8().constData(), &db, kSqliteOpenReadOnly, nullptr);
    if (rc != 0 || !db) {
        if (db) sql.close(db);
        return book;
    }
    void *stmt = nullptr;
    if (sql.prepare(db, kLastReadSql, -1, &stmt, nullptr) == 0 && stmt) {
        if (sql.step(stmt) == kSqliteRow) readNickelRow(sql, stmt, &book);
        sql.finalize(stmt);
    }
    sql.close(db);
    return book;
}

// Keeps `candidate` if it is valid and newer than `best`; the first of equal
// times wins.
void consider(NtRecentBook *best, const NtRecentBook &candidate) {
    NT_DBG("recent candidate %s: valid=%d time=%lld '%s' %d%%", nt_reader_app_name(candidate.app),
           candidate.valid ? 1 : 0, static_cast<long long>(candidate.lastRead), qPrintable(candidate.title),
           candidate.percent);
    if (candidate.valid && (!best->valid || candidate.lastRead > best->lastRead)) *best = candidate;
}

}   // namespace

NtRecentBook nt_recent_book(NtRecentSource source) {
    const bool any = source == NtRecentSource::Any;
    NtRecentBook best;
    if (any || source == NtRecentSource::Koreader) consider(&best, koreaderBook());
    if (any || source == NtRecentSource::Plato) consider(&best, platoBook());
    if (any || source == NtRecentSource::Nickel) consider(&best, nickelBook());
    return best;
}

void nt_open_recent_book(const NtRecentBook &book) {
    if (!book.valid) {
        nt_toast(QStringLiteral("No recent book"));
        return;
    }
    if (book.app == NtReaderApp::Plato) {
        nt_run_command(QStringLiteral("Continue reading"), QStringLiteral("exec " NT_PLATO_LAUNCHER));
        return;
    }
    if (book.app == NtReaderApp::Nickel && nt_open_in_nickel(book.contentId)) return;
    const QString command = QStringLiteral("exec " NT_KOREADER_LAUNCHER " ") + nt_shell_quote(book.path);
    nt_run_command(QStringLiteral("Continue reading"), command);
}

const char *nt_reader_app_name(NtReaderApp app) {
    if (app == NtReaderApp::Koreader) return "koreader";
    if (app == NtReaderApp::Plato) return "plato";
    return "nickel";
}
