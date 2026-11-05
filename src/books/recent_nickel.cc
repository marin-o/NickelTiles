#include "books/recent_apps.h"

#include <QDateTime>
#include <QFile>

#include <dlfcn.h>

#include "core/paths.h"
#include "core/util.h"

namespace {

const int kSqliteOpenReadOnly = 1;
const int kSqliteRow = 100;

// Nickel's last-read query: file-backed books with a read time, newest first.
const char kLastReadSql[] = "select ContentID, Title, Attribution, DateLastRead, ___PercentRead from content "
                            "where ContentType='6' and ContentID like 'file://%' and DateLastRead is not null "
                            "order by DateLastRead desc";

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

}   // namespace

// From KoboReader.sqlite, read-only and WAL-safe.
QList<NtRecentBook> nt_recent_nickel_books(int count) {
    QList<NtRecentBook> books;
    Sqlite sql;
    if (!loadSqlite(&sql)) {
        NT_DBG("recent (nickel): sqlite3 symbols unavailable");
        return books;
    }
    void *db = nullptr;
    const int rc = sql.open(QStringLiteral(NT_KOBO_DB).toUtf8().constData(), &db, kSqliteOpenReadOnly, nullptr);
    if (rc != 0 || !db) {
        if (db) sql.close(db);
        return books;
    }
    void *stmt = nullptr;
    if (sql.prepare(db, kLastReadSql, -1, &stmt, nullptr) == 0 && stmt) {
        while (books.size() < count && sql.step(stmt) == kSqliteRow) {
            NtRecentBook book;
            book.app = NtReaderApp::Nickel;
            readNickelRow(sql, stmt, &book);
            if (book.valid) books << book;
        }
        sql.finalize(stmt);
    }
    sql.close(db);
    return books;
}
