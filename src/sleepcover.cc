#include "sleepcover.h"

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStringList>

#include <unistd.h>

#include "epub.h"
#include "paths.h"
#include "recent.h"
#include "util.h"

namespace {

QStringList ownCovers() {
    return QDir(QStringLiteral(NT_SCREENSAVER_DIR)).entryList(QStringList(QStringLiteral("nt-recent.*")), QDir::Files);
}

void clearSleepCover() {
    QDir dir(QStringLiteral(NT_SCREENSAVER_DIR));
    const QStringList ours = ownCovers();
    for (const QString &name : ours)
        dir.remove(name);
    QFile::remove(QStringLiteral(NT_SLEEP_COVER_STAMP));
}

// The book's path with its size and modification time: a changed book is
// extracted again, an unchanged one is not.
QString stampKey(const NtRecentBook &book) {
    const QFileInfo info(book.path);
    return book.path + QLatin1Char('|') + QString::number(info.size()) + QLatin1Char('|')
        + QString::number(info.lastModified().toTime_t());
}

bool stampMatches(const QString &key) {
    QFile stamp(QStringLiteral(NT_SLEEP_COVER_STAMP));
    return stamp.open(QIODevice::ReadOnly) && QString::fromUtf8(stamp.readAll()) == key;
}

void writeStamp(const QString &key) {
    QFile stamp(QStringLiteral(NT_SLEEP_COVER_STAMP));
    if (stamp.open(QIODevice::WriteOnly | QIODevice::Truncate))
        stamp.write(key.toUtf8());
}

// Nickel's sleep screen lists the folder by extension, so only these are kept as they are.
bool isListableExt(const QString &ext) {
    return ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") || ext == QLatin1String("png");
}

QByteArray reencodeAsPng(const QByteArray &data) {
    QByteArray png;
    const QImage image = QImage::fromData(data);
    if (image.isNull()) return png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}

bool writeSynced(const QString &path, const QByteArray &data) {
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
        out.close();
        return false;
    }
    out.flush();
    fsync(out.handle());
    out.close();
    return true;
}

// Written under a name Nickel does not list, then renamed into place.
bool writeCover(const QByteArray &data, const QString &ext) {
    QDir().mkpath(QStringLiteral(NT_SCREENSAVER_DIR));
    const QDir dir(QStringLiteral(NT_SCREENSAVER_DIR));
    const QString tmp = dir.filePath(QStringLiteral(".nt-recent.tmp"));
    if (!writeSynced(tmp, data)) {
        NT_LOG("sleep cover: could not write %s", qPrintable(tmp));
        QFile::remove(tmp);
        return false;
    }
    if (!QFile::rename(tmp, dir.filePath(QStringLiteral("nt-recent.") + ext))) {
        NT_LOG("sleep cover: rename failed");
        QFile::remove(tmp);
        return false;
    }
    return true;
}

void updateSleepCover(NtRecentSource source) {
    const NtRecentBook book = nt_recent_book(source);
    if (!book.valid || !nt_is_epub(book.path)) {
        NT_LOG("sleep cover: no recent EPUB; leaving Nickel's own cover");
        clearSleepCover();
        return;
    }
    const QString key = stampKey(book);
    if (stampMatches(key) && !ownCovers().isEmpty()) {
        NT_DBG("sleep cover: unchanged (%s)", qPrintable(book.title));
        return;
    }
    QString member;
    QByteArray data = nt_epub_cover_bytes(book.path, &member);
    QString ext = member.section(QLatin1Char('.'), -1).toLower();
    if (!isListableExt(ext)) {
        data = reencodeAsPng(data);
        ext = QStringLiteral("png");
    }
    clearSleepCover();
    if (data.isEmpty()) {
        NT_LOG("sleep cover: no cover in %s; leaving Nickel's own cover", qPrintable(book.path));
        return;
    }
    if (!writeCover(data, ext)) return;
    writeStamp(key);
    NT_LOG("sleep cover: %s '%s' (%d bytes, .%s)", nt_reader_app_name(book.app), qPrintable(book.title),
        data.size(), qPrintable(ext));
}

} // namespace

void nt_sleep_cover_refresh(const NtSettings &settings) {
    if (!settings.enabled || !settings.sleepCover)
        clearSleepCover();
    else
        updateSleepCover(settings.recentSource);
}
