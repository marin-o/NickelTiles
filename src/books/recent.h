#ifndef NT_RECENT_H
#define NT_RECENT_H

// The most recently read book across KOReader (history.lua and the book's
// .sdr sidecar), Plato (.metadata.json and .reading-states) and Nickel
// (KoboReader.sqlite, read through the sqlite3 already loaded in Nickel).

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

// The newest book from `source` (or from all three apps for Any).
NtRecentBook nt_recent_book(NtRecentSource source);

// Opens the book in the app that last read it: Nickel's reader (KOReader if
// that fails), KOReader with the file, or Plato (which cannot be told which
// book to open, so it starts on its own last book).
void nt_open_recent_book(const NtRecentBook &book);

const char *nt_reader_app_name(NtReaderApp app);

#endif
