#ifndef NT_FB2_H
#define NT_FB2_H

// FictionBook (FB2) covers. The book is one XML file (or a zip holding it);
// <coverpage> in the description names an <image>, whose picture is a
// base64 <binary> later in the file.

#include <QByteArray>
#include <QString>

// True for .fb2 and .fb2.zip files.
bool nt_is_fb2(const QString &path);

// The cover as stored in the book (decoded from base64, full size); `name`
// receives its binary id, e.g. "cover.jpg". Without a <coverpage>, a binary
// whose id mentions "cover" is used. Empty if the book has none.
QByteArray nt_fb2_cover_bytes(const QString &path, QString *name);

#endif
