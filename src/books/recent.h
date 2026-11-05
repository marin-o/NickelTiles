#ifndef NT_RECENT_H
#define NT_RECENT_H

// The most recently read book across KOReader, Plato and Nickel. How each app's
// records are read is in books/recent_apps.h; opening a book is in books/open_book.h.

#include <QList>
#include <QString>

#include "core/settings_model.h"

enum class NtReaderApp { Koreader, Plato, Nickel };

struct NtRecentBook {
    bool valid = false;
    NtReaderApp app = NtReaderApp::Koreader;
    QString path;        // the book file
    QString contentId;   // Nickel's ContentID (Nickel books only)
    QString title;
    QString author;
    int percent = -1;      // -1 if unknown
    qint64 lastRead = 0;   // Unix time
};

// Up to `count` books from `source` (or from all three apps for Any), newest
// first. A book read in two apps appears once, as the app that read it last.
QList<NtRecentBook> nt_recent_books(NtRecentSource source, int count);

// The newest of those, or an invalid book if there is none.
NtRecentBook nt_recent_book(NtRecentSource source);

const char *nt_reader_app_name(NtReaderApp app);

#endif
