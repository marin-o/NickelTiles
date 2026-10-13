#include "books/zip.h"

#include <QtGlobal>

#include <dlfcn.h>
#include <cstring>

#include "core/util.h"

// zlib's entry points, looked up at run time: Nickel already has libz loaded,
// so the mod borrows it and never links against it.
struct z_stream_s {
    const unsigned char *next_in;
    unsigned avail_in;
    unsigned long total_in;
    unsigned char *next_out;
    unsigned avail_out;
    unsigned long total_out;
    const char *msg;
    void *state;
    void *zalloc;
    void *zfree;
    void *opaque;
    int data_type;
    unsigned long adler;
    unsigned long reserved;
};
typedef int (*inflateInit2_t)(z_stream_s *, int, const char *, int);
typedef int (*inflate_t)(z_stream_s *, int);
typedef int (*inflateEnd_t)(z_stream_s *);

struct Zlib {
    inflateInit2_t init;
    inflate_t inflate;
    inflateEnd_t end;
};

static bool nt_load_zlib(Zlib *z) {
    static void *handle = nullptr;
    static Zlib fns = {nullptr, nullptr, nullptr};
    if (!handle) {
        handle = dlopen("libz.so.1", RTLD_LAZY | RTLD_NOLOAD);
        if (!handle) handle = dlopen("libz.so.1", RTLD_LAZY);
        if (!handle) {
            NT_LOG("zip: no libz: %s", dlerror());
            return false;
        }
        fns.init = reinterpret_cast<inflateInit2_t>(dlsym(handle, "inflateInit2_"));
        fns.inflate = reinterpret_cast<inflate_t>(dlsym(handle, "inflate"));
        fns.end = reinterpret_cast<inflateEnd_t>(dlsym(handle, "inflateEnd"));
    }
    if (!fns.init || !fns.inflate || !fns.end) return false;
    *z = fns;
    return true;
}

// Inflates a raw deflate stream (no zlib header) into `outSize` bytes.
static QByteArray nt_inflate_raw(const QByteArray &in, unsigned long outSize) {
    Zlib z;
    if (!nt_load_zlib(&z)) return QByteArray();
    QByteArray out;
    out.resize(static_cast<int>(outSize));
    z_stream_s s;
    memset(&s, 0, sizeof(s));
    s.next_in = reinterpret_cast<const unsigned char *>(in.constData());
    s.avail_in = static_cast<unsigned>(in.size());
    s.next_out = reinterpret_cast<unsigned char *>(out.data());
    s.avail_out = static_cast<unsigned>(out.size());
    if (z.init(&s, -15, "1.2.11", static_cast<int>(sizeof(s))) != 0) return QByteArray();   // Z_OK == 0
    const int rc = z.inflate(&s, 4);                                                        // Z_FINISH
    z.end(&s);
    if (rc != 1 && rc != 0) {   // Z_STREAM_END == 1
        NT_DBG("zip: inflate rc=%d", rc);
        return QByteArray();
    }
    out.resize(static_cast<int>(s.total_out));
    return out;
}

static quint16 le16(const uchar *p) {
    return static_cast<quint16>(p[0] | (p[1] << 8));
}
static quint32 le32(const uchar *p) {
    return static_cast<quint32>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// Parses the central-directory record at offset `p`; `*next` receives the
// offset of the record after it.
static bool nt_central_record(const QByteArray &cd, int p, NtZipEntry *en, int *next) {
    if (p + 46 > cd.size()) return false;
    const uchar *h = reinterpret_cast<const uchar *>(cd.constData()) + p;
    if (memcmp(h, "PK\x01\x02", 4) != 0) return false;
    en->method = le16(h + 10);
    en->compressedSize = le32(h + 20);
    en->size = le32(h + 24);
    en->localHeaderOffset = le32(h + 42);
    const quint16 nlen = le16(h + 28), xlen = le16(h + 30), clen = le16(h + 32);
    en->name = QString::fromUtf8(cd.constData() + p + 46, nlen);
    *next = p + 46 + nlen + xlen + clen;
    return true;
}

QList<NtZipEntry> nt_zip_entries(QFile &zip) {
    QList<NtZipEntry> out;
    const qint64 size = zip.size();
    // The end-of-central-directory record is at the very end, behind a comment of up to 65535 bytes.
    const qint64 tail = qMin<qint64>(size, 66000);
    zip.seek(size - tail);
    const QByteArray t = zip.read(tail);
    const int eocd = t.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (eocd < 0) return out;
    const uchar *e = reinterpret_cast<const uchar *>(t.constData()) + eocd;
    const quint16 count = le16(e + 10);
    const quint32 cdSize = le32(e + 12), cdOff = le32(e + 16);
    zip.seek(cdOff);
    const QByteArray cd = zip.read(cdSize);
    int p = 0;
    for (int i = 0; i < count; i++) {
        NtZipEntry en = NtZipEntry();
        int next = p;
        if (!nt_central_record(cd, p, &en, &next)) break;
        out << en;
        p = next;
    }
    return out;
}

QByteArray nt_zip_extract(QFile &zip, const NtZipEntry &entry) {
    zip.seek(entry.localHeaderOffset);
    const QByteArray lh = zip.read(30);
    if (lh.size() < 30 || memcmp(lh.constData(), "PK\x03\x04", 4) != 0) return QByteArray();
    const uchar *h = reinterpret_cast<const uchar *>(lh.constData());
    const quint16 nlen = le16(h + 26), xlen = le16(h + 28);
    zip.seek(entry.localHeaderOffset + 30 + nlen + xlen);
    QByteArray data = zip.read(entry.compressedSize);
    if (entry.method == 0) return data;
    if (entry.method == 8) return nt_inflate_raw(data, entry.size);
    return QByteArray();
}
