#include "books/cover.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>

#include "books/cbz.h"
#include "books/epub.h"
#include "books/fb2.h"
#include "books/mobi.h"
#include "core/util.h"

namespace {

// Every format with a cover reader: add a format here.
struct NtCoverFormat {
    bool (*matches)(const QString &path);
    QByteArray (*coverBytes)(const QString &path, QString *name);
};

const NtCoverFormat kFormats[] = {
    {nt_is_epub, nt_epub_cover_bytes},
    {nt_is_fb2, nt_fb2_cover_bytes},
    {nt_is_mobi, nt_mobi_cover_bytes},
    {nt_is_cbz, nt_cbz_cover_bytes},
};

const NtCoverFormat *formatOf(const QString &path) {
    for (const NtCoverFormat &format : kFormats)
        if (format.matches(path)) return &format;
    return nullptr;
}

}   // namespace

QByteArray nt_book_cover_bytes(const QString &path, QString *name) {
    name->clear();
    const NtCoverFormat *format = formatOf(path);
    return format ? format->coverBytes(path, name) : QByteArray();
}

// Cache file for this book's cover, keyed by path, size and modification time.
static QString nt_cache_file(const QString &path, const QFileInfo &fi, const QString &cacheDir) {
    const QByteArray key = QCryptographicHash::hash((path + QLatin1Char('|') + QString::number(fi.size()) +
                                                     QLatin1Char('|') + QString::number(fi.lastModified().toTime_t()))
                                                        .toUtf8(),
                                                    QCryptographicHash::Md5)
                               .toHex();
    return cacheDir + QLatin1Char('/') + QString::fromLatin1(key) + QStringLiteral(".png");
}

// Scales to fit `maxSide` and reduces to greyscale: plenty for e-ink, and it
// keeps the cached file small.
static QImage nt_fit_greyscale(QImage img, int maxSide) {
    if (img.width() > maxSide || img.height() > maxSide)
        img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img.convertToFormat(QImage::Format_Indexed8, Qt::ThresholdDither);
}

// A ".none" marker remembers a book without a usable cover so it is not parsed
// on every refresh. It holds the version that wrote it: a newer NickelTiles may
// find a cover the older one missed, so a marker from another version is stale.
static bool nt_marker_current(const QString &marker) {
    QFile f(marker);
    return f.open(QIODevice::ReadOnly) && f.readAll() == QByteArray(NH_VERSION);
}

static void nt_write_marker(const QString &marker) {
    QFile f(marker);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(NH_VERSION);
}

QPixmap nt_book_cover(const QString &path, const QString &cacheDir, int maxSide) {
    if (!formatOf(path)) return QPixmap();
    QFileInfo fi(path);
    if (!fi.exists()) return QPixmap();
    QDir().mkpath(cacheDir);
    const QString cacheFile = nt_cache_file(path, fi, cacheDir);
    const QString noneMarker = cacheFile + QStringLiteral(".none");
    QPixmap cached(cacheFile);
    if (!cached.isNull()) return cached;
    if (nt_marker_current(noneMarker)) return QPixmap();

    QString coverName;
    const QByteArray imageData = nt_book_cover_bytes(path, &coverName);
    QImage img = QImage::fromData(imageData);
    NT_DBG("cover: %s -> '%s' (%d bytes) image %dx%d", qPrintable(path), qPrintable(coverName), imageData.size(),
           img.width(), img.height());
    if (img.isNull()) {
        nt_write_marker(noneMarker);
        return QPixmap();
    }
    img = nt_fit_greyscale(img, maxSide);
    img.save(cacheFile, "PNG");
    return QPixmap::fromImage(img);
}
