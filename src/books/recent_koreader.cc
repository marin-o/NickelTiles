#include "books/recent_apps.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>

#include "core/paths.h"

namespace {

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

}   // namespace

// KOReader's most recent book that still exists, from history.lua, whose
// entries ([1] = { ... }, newest first) each hold a file and a time.
NtRecentBook nt_recent_koreader_book() {
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
