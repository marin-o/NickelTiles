#ifndef NT_EPUBCOVER_H
#define NT_EPUBCOVER_H

#include <QByteArray>
#include <QPixmap>
#include <QString>

// Minimal zip reader on top of the zlib already loaded in Nickel (dlopen'd,
// never linked). Returns the stored/deflated member or an empty array.
QByteArray nt_zip_read(const QString &zipPath, const QString &member, bool *found = 0);

// Finds and decodes the cover image of an EPUB (OPF cover-image property,
// <meta name="cover">, or a cover-ish file name). Cached as a PNG under
// cacheDir, keyed by path + size + mtime. Null pixmap if nothing usable.
QPixmap nt_epub_cover(const QString &epubPath, const QString &cacheDir, int maxSide);

// The same cover, undecoded and at full size, with the member's file name
// (for its extension). Empty array if nothing usable. Not cached.
QByteArray nt_epub_cover_bytes(const QString &epubPath, QString *member);

#endif
