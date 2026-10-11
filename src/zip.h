#ifndef NT_ZIP_H
#define NT_ZIP_H

// Minimal zip reader (stored and deflated members) using the zlib Nickel has
// already loaded, opened with dlopen so the mod never links against it.

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QString>

struct NtZipEntry {
    QString name;
    int method;          // 0 stored, 8 deflated
    quint32 compressedSize;
    quint32 size;
    quint32 localHeaderOffset;
};

// The central directory of an open zip file; empty if it is not a zip.
QList<NtZipEntry> nt_zip_entries(QFile &zip);

// The member's contents; empty on any error.
QByteArray nt_zip_extract(QFile &zip, const NtZipEntry &entry);

#endif
