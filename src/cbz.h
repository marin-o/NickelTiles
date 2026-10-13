#ifndef NT_CBZ_H
#define NT_CBZ_H

// Comic book covers (CBZ): a zip of page images; the cover is the image whose
// name mentions "cover", else the first page in name order.

#include <QByteArray>
#include <QString>

// True for .cbz files.
bool nt_is_cbz(const QString &path);

// The cover image as stored; `name` receives its file name in the zip. Empty
// if the zip holds no image.
QByteArray nt_cbz_cover_bytes(const QString &path, QString *name);

#endif
