#ifndef NT_EPUB_H
#define NT_EPUB_H

// EPUB cover images: the OPF cover-image item, else <meta name="cover">, else
// an image whose name mentions "cover", else the largest image.

#include <QByteArray>
#include <QString>

// True for .epub and .kepub files.
bool nt_is_epub(const QString &path);

// The cover as stored in the book (undecoded, full size); `member` receives
// its file name inside the book. Empty if the book has no usable image.
QByteArray nt_epub_cover_bytes(const QString &epubPath, QString *member);

#endif
