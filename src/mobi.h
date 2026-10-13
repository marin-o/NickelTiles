#ifndef NT_MOBI_H
#define NT_MOBI_H

// Kindle book covers (MOBI, AZW, AZW3, PRC). The book is a Palm database:
// record 0 holds the MOBI header, whose EXTH metadata gives the cover as an
// offset from the first image record.

#include <QByteArray>
#include <QString>

// True for .mobi, .azw, .azw3 and .prc files.
bool nt_is_mobi(const QString &path);

// The cover image record as stored (JPEG, PNG or GIF); `name` receives a
// file name whose extension matches the image type. Without a cover entry
// the first image is used. Empty if there is none.
QByteArray nt_mobi_cover_bytes(const QString &path, QString *name);

#endif
