#include "epubcover.h"
#include "util.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QStringList>

#include <dlfcn.h>
#include <string.h>

// --- zlib via dlopen (raw deflate)

struct z_stream_s {
    const unsigned char *next_in; unsigned avail_in; unsigned long total_in;
    unsigned char *next_out; unsigned avail_out; unsigned long total_out;
    const char *msg; void *state;
    void *zalloc; void *zfree; void *opaque;
    int data_type; unsigned long adler; unsigned long reserved;
};
typedef int (*inflateInit2_t)(z_stream_s*, int, const char*, int);
typedef int (*inflate_t)(z_stream_s*, int);
typedef int (*inflateEnd_t)(z_stream_s*);

static bool nt_zlib(inflateInit2_t *init, inflate_t *inf, inflateEnd_t *end) {
    static void *h = NULL;
    static inflateInit2_t s_init = NULL; static inflate_t s_inf = NULL; static inflateEnd_t s_end = NULL;
    if (!h) {
        h = dlopen("libz.so.1", RTLD_LAZY | RTLD_NOLOAD);
        if (!h) h = dlopen("libz.so.1", RTLD_LAZY);
        if (!h) { NT_LOG("epubcover: no libz: %s", dlerror()); return false; }
        s_init = (inflateInit2_t)dlsym(h, "inflateInit2_");
        s_inf = (inflate_t)dlsym(h, "inflate");
        s_end = (inflateEnd_t)dlsym(h, "inflateEnd");
    }
    if (!s_init || !s_inf || !s_end) return false;
    *init = s_init; *inf = s_inf; *end = s_end;
    return true;
}

static QByteArray nt_inflate_raw(const QByteArray &in, unsigned long outSize) {
    inflateInit2_t init; inflate_t inf; inflateEnd_t end;
    if (!nt_zlib(&init, &inf, &end)) return QByteArray();
    QByteArray out;
    out.resize(int(outSize));
    z_stream_s z; memset(&z, 0, sizeof(z));
    z.next_in = (const unsigned char*)in.constData(); z.avail_in = unsigned(in.size());
    z.next_out = (unsigned char*)out.data(); z.avail_out = unsigned(out.size());
    if (init(&z, -15, "1.2.11", int(sizeof(z))) != 0) return QByteArray();   // Z_OK == 0
    int rc = inf(&z, 4);   // Z_FINISH
    end(&z);
    if (rc != 1 && rc != 0) { NT_DBG("epubcover: inflate rc=%d", rc); return QByteArray(); }   // Z_STREAM_END == 1
    out.resize(int(z.total_out));
    return out;
}

static quint16 le16(const uchar *p) { return quint16(p[0] | (p[1] << 8)); }
static quint32 le32(const uchar *p) { return quint32(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }

// Walks the central directory; calls back with (name, method, csize, usize, localHeaderOffset).
struct NtZipEntry { QString name; int method; quint32 csize, usize, local; };

static QList<NtZipEntry> nt_zip_entries(QFile &f) {
    QList<NtZipEntry> out;
    const qint64 size = f.size();
    const qint64 tail = qMin<qint64>(size, 66000);
    f.seek(size - tail);
    QByteArray t = f.read(tail);
    int eocd = t.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (eocd < 0) return out;
    const uchar *e = (const uchar*)t.constData() + eocd;
    quint16 count = le16(e + 10);
    quint32 cdSize = le32(e + 12), cdOff = le32(e + 16);
    f.seek(cdOff);
    QByteArray cd = f.read(cdSize);
    int p = 0;
    for (int i = 0; i < count && p + 46 <= cd.size(); i++) {
        const uchar *h = (const uchar*)cd.constData() + p;
        if (memcmp(h, "PK\x01\x02", 4) != 0) break;
        NtZipEntry en;
        en.method = le16(h + 10);
        en.csize = le32(h + 20); en.usize = le32(h + 24);
        quint16 nlen = le16(h + 28), xlen = le16(h + 30), clen = le16(h + 32);
        en.local = le32(h + 42);
        en.name = QString::fromUtf8(cd.constData() + p + 46, nlen);
        out << en;
        p += 46 + nlen + xlen + clen;
    }
    return out;
}

static QByteArray nt_zip_extract(QFile &f, const NtZipEntry &en) {
    f.seek(en.local);
    QByteArray lh = f.read(30);
    if (lh.size() < 30 || memcmp(lh.constData(), "PK\x03\x04", 4) != 0) return QByteArray();
    const uchar *h = (const uchar*)lh.constData();
    quint16 nlen = le16(h + 26), xlen = le16(h + 28);
    f.seek(en.local + 30 + nlen + xlen);
    QByteArray data = f.read(en.csize);
    if (en.method == 0) return data;
    if (en.method == 8) return nt_inflate_raw(data, en.usize);
    return QByteArray();
}

QByteArray nt_zip_read(const QString &zipPath, const QString &member, bool *found) {
    if (found) *found = false;
    QFile f(zipPath);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();
    const QList<NtZipEntry> entries = nt_zip_entries(f);
    for (const NtZipEntry &en : entries) {
        if (en.name == member) {
            if (found) *found = true;
            return nt_zip_extract(f, en);
        }
    }
    return QByteArray();
}

// --- EPUB cover

static QString nt_attr(const QString &tag, const char *attr) {
    QRegularExpression re(QStringLiteral("\\b") + QString::fromLatin1(attr) + QStringLiteral("\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = re.match(tag);
    return m.hasMatch() ? m.captured(1) : QString();
}

static QString nt_resolve(const QString &baseDir, const QString &href) {
    QString h = href;
    h.replace(QStringLiteral("%20"), QStringLiteral(" "));
    if (baseDir.isEmpty()) return h;
    QString full = baseDir + QLatin1Char('/') + h;
    while (full.contains(QStringLiteral("/../"))) {
        int i = full.indexOf(QStringLiteral("/../"));
        int j = full.lastIndexOf(QLatin1Char('/'), i - 1);
        if (j < 0) { full = full.mid(i + 4); break; }
        full = full.left(j) + full.mid(i + 3);
    }
    return full;
}

QPixmap nt_epub_cover(const QString &epubPath, const QString &cacheDir, int maxSide) {
    QFileInfo fi(epubPath);
    if (!fi.exists()) return QPixmap();
    QDir().mkpath(cacheDir);
    const QByteArray key = QCryptographicHash::hash(
        (epubPath + QLatin1Char('|') + QString::number(fi.size()) + QLatin1Char('|') + QString::number(fi.lastModified().toTime_t())).toUtf8(),
        QCryptographicHash::Md5).toHex();
    const QString cacheFile = cacheDir + QLatin1Char('/') + QString::fromLatin1(key) + QStringLiteral(".png");
    QPixmap cached(cacheFile);
    if (!cached.isNull()) return cached;
    if (QFile::exists(cacheFile + QStringLiteral(".none"))) return QPixmap();

    QString coverMember;
    const QByteArray imageData = nt_epub_cover_bytes(epubPath, &coverMember);
    QImage img = QImage::fromData(imageData);
    NT_DBG("epubcover: %s -> member '%s' (%d bytes) image %dx%d", qPrintable(epubPath), qPrintable(coverMember), imageData.size(), img.width(), img.height());
    if (img.isNull()) {
        QFile none(cacheFile + QStringLiteral(".none")); none.open(QIODevice::WriteOnly); none.close();
        return QPixmap();
    }
    if (img.width() > maxSide || img.height() > maxSide)
        img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img = img.convertToFormat(QImage::Format_Indexed8, Qt::ThresholdDither);   // e-ink: greyscale is plenty and the file stays small
    img.save(cacheFile, "PNG");
    return QPixmap::fromImage(img);
}

QByteArray nt_epub_cover_bytes(const QString &epubPath, QString *member) {
    member->clear();
    QFile f(epubPath);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();
    const QList<NtZipEntry> entries = nt_zip_entries(f);
    if (entries.isEmpty()) { NT_DBG("epubcover: no zip entries in %s", qPrintable(epubPath)); return QByteArray(); }

    // container.xml -> OPF path
    QString opfPath;
    for (const NtZipEntry &en : entries)
        if (en.name == QLatin1String("META-INF/container.xml")) {
            opfPath = nt_attr(QString::fromUtf8(nt_zip_extract(f, en)).section(QStringLiteral("<rootfile"), 1, 1).section(QLatin1Char('>'), 0, 0), "full-path");
            break;
        }
    QString coverMember;
    if (!opfPath.isEmpty()) {
        QString opfDir = opfPath.contains(QLatin1Char('/')) ? opfPath.left(opfPath.lastIndexOf(QLatin1Char('/'))) : QString();
        for (const NtZipEntry &en : entries) {
            if (en.name != opfPath) continue;
            const QString opf = QString::fromUtf8(nt_zip_extract(f, en));
            QString coverId;
            QRegularExpression metaRe(QStringLiteral("<meta[^>]*name\\s*=\\s*[\"']cover[\"'][^>]*>"), QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch mm = metaRe.match(opf);
            if (mm.hasMatch()) coverId = nt_attr(mm.captured(0), "content");
            QRegularExpression itemRe(QStringLiteral("<item\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator it = itemRe.globalMatch(opf);
            QString byProp, byId, byName;
            while (it.hasNext()) {
                const QString tag = it.next().captured(0);
                const QString href = nt_attr(tag, "href"), id = nt_attr(tag, "id"), props = nt_attr(tag, "properties"), mt = nt_attr(tag, "media-type");
                if (!mt.startsWith(QLatin1String("image/"))) continue;
                if (props.contains(QLatin1String("cover-image")) && byProp.isEmpty()) byProp = href;
                if (!coverId.isEmpty() && id == coverId && byId.isEmpty()) byId = href;
                if (href.contains(QLatin1String("cover"), Qt::CaseInsensitive) && byName.isEmpty()) byName = href;
            }
            const QString chosen = !byProp.isEmpty() ? byProp : (!byId.isEmpty() ? byId : byName);
            if (!chosen.isEmpty()) coverMember = nt_resolve(opfDir, chosen);
            break;
        }
    }
    if (coverMember.isEmpty()) {
        // fall back to the largest image whose name mentions "cover", else the largest image
        const NtZipEntry *best = NULL, *bestAny = NULL;
        for (const NtZipEntry &en : entries) {
            const QString n = en.name.toLower();
            if (!(n.endsWith(QLatin1String(".jpg")) || n.endsWith(QLatin1String(".jpeg")) || n.endsWith(QLatin1String(".png")))) continue;
            if (!bestAny || en.usize > bestAny->usize) bestAny = &en;
            if (n.contains(QLatin1String("cover")) && (!best || en.usize > best->usize)) best = &en;
        }
        if (best) coverMember = best->name; else if (bestAny) coverMember = bestAny->name;
    }
    *member = coverMember;
    for (const NtZipEntry &en : entries)
        if (en.name == coverMember) return nt_zip_extract(f, en);
    return QByteArray();
}
