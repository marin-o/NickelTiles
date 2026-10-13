#include "mobi.h"

#include <QFile>
#include <QList>

#include "util.h"

bool nt_is_mobi(const QString &path) {
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".mobi")) || lower.endsWith(QStringLiteral(".azw")) ||
           lower.endsWith(QStringLiteral(".azw3")) || lower.endsWith(QStringLiteral(".prc"));
}

namespace {

// Offsets from the MobileRead wiki's MOBI and PDB pages.
const int kPdbRecordCount = 76;     // uint16, then 8 bytes per record: offset, attributes, id
const int kMobiMagic = 16;          // "MOBI", from the start of record 0
const int kMobiHeaderLength = 20;   // uint32, counted from kMobiMagic
const int kMobiFirstImage = 108;    // uint32, record index of the first image
const int kMobiExthFlags = 128;     // uint32, bit 0x40: EXTH follows the header
const quint32 kExthCoverOffset = 201;
const quint32 kNoIndex = 0xffffffff;

quint32 be32(const QByteArray &b, int at) {
    if (at < 0 || at + 4 > b.size()) return kNoIndex;
    const uchar *p = reinterpret_cast<const uchar *>(b.constData()) + at;
    return (static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) | (static_cast<quint32>(p[2]) << 8) |
           static_cast<quint32>(p[3]);
}

// The EXTH cover offset (relative to the first image), or kNoIndex.
quint32 exthCoverOffset(const QByteArray &record0) {
    if (!(be32(record0, kMobiExthFlags) & 0x40)) return kNoIndex;
    const int exth = kMobiMagic + static_cast<int>(be32(record0, kMobiHeaderLength));
    if (record0.mid(exth, 4) != "EXTH") return kNoIndex;
    const quint32 count = be32(record0, exth + 8);
    int at = exth + 12;
    for (quint32 i = 0; i < count && at + 8 <= record0.size(); i++) {
        const quint32 type = be32(record0, at);
        const quint32 length = be32(record0, at + 4);
        if (length < 8) break;
        if (type == kExthCoverOffset) return be32(record0, at + 8);
        at += static_cast<int>(length);
    }
    return kNoIndex;
}

QString imageExtension(const QByteArray &data) {
    if (data.startsWith("\xff\xd8\xff")) return QStringLiteral("jpg");
    if (data.startsWith("\x89PNG")) return QStringLiteral("png");
    if (data.startsWith("GIF8")) return QStringLiteral("gif");
    return QString();
}

// The record offsets from the database header, plus the file size as the end
// of the last record.
QList<qint64> recordOffsets(QFile &f) {
    QList<qint64> offsets;
    if (!f.seek(kPdbRecordCount)) return offsets;
    const QByteArray countBytes = f.read(2);
    if (countBytes.size() != 2) return offsets;
    const int count = (static_cast<uchar>(countBytes[0]) << 8) | static_cast<uchar>(countBytes[1]);
    const QByteArray list = f.read(static_cast<qint64>(count) * 8);
    if (list.size() != count * 8) return offsets;
    for (int i = 0; i < count; i++) offsets.append(be32(list, i * 8));
    offsets.append(f.size());
    return offsets;
}

QByteArray readRecord(QFile &f, const QList<qint64> &offsets, quint32 index) {
    if (index >= static_cast<quint32>(offsets.size() - 1)) return QByteArray();
    const qint64 start = offsets[static_cast<int>(index)];
    const qint64 end = offsets[static_cast<int>(index) + 1];
    if (end <= start || !f.seek(start)) return QByteArray();
    return f.read(end - start);
}

}   // namespace

QByteArray nt_mobi_cover_bytes(const QString &path, QString *name) {
    name->clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();
    const QList<qint64> offsets = recordOffsets(f);
    const QByteArray record0 = readRecord(f, offsets, 0);
    if (record0.mid(kMobiMagic, 4) != "MOBI") {
        NT_DBG("mobi: no MOBI header in %s", qPrintable(path));
        return QByteArray();
    }
    const quint32 firstImage = be32(record0, kMobiFirstImage);
    if (firstImage == kNoIndex) return QByteArray();
    const quint32 coverOffset = exthCoverOffset(record0);
    QByteArray data = readRecord(f, offsets, firstImage + (coverOffset == kNoIndex ? 0 : coverOffset));
    const QString ext = imageExtension(data);
    if (ext.isEmpty()) return QByteArray();
    *name = QStringLiteral("cover.") + ext;
    return data;
}
