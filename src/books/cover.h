#ifndef NT_COVER_H
#define NT_COVER_H

// A book's cover, whatever its format (EPUB, FB2, Kindle, CBZ), for the Continue reading
// tile and the sleep screen.

#include <QByteArray>
#include <QPixmap>
#include <QString>

// The cover as stored in the book (undecoded, full size); `name` receives its
// file name in the book, whose extension tells the image type. Empty for
// other formats or a book without one.
QByteArray nt_book_cover_bytes(const QString &path, QString *name);

// The cover scaled to fit `maxSide`, in greyscale. Cached as a PNG in
// `cacheDir`, keyed by path, size and modification time. Null if none.
QPixmap nt_book_cover(const QString &path, const QString &cacheDir, int maxSide);

#endif
