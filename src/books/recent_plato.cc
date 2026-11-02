#include "books/recent_apps.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include "core/paths.h"
#include "core/util.h"

namespace {

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

}   // namespace

// Plato keeps its library index at the root of onboard: .metadata.json, one
// object per book with "file": {"path": ...}. Per-book state is in
// .reading-states/<fingerprint>.json, where <fingerprint> is the book's key in
// the index. The newest opened state of a book that still exists wins.
NtRecentBook nt_recent_plato_book() {
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
