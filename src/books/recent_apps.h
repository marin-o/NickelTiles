#ifndef NT_RECENT_APPS_H
#define NT_RECENT_APPS_H

// Each app's own record of its recent books. Their formats belong to those apps
// and change for their own reasons, so each lives in its own file. Each returns
// up to `count` books that still exist, newest first, and nothing when the app
// has no usable record. Use nt_recent_books (books/recent.h) unless you need one
// specific app.

#include <QList>

#include "books/recent.h"

// KOReader: history.lua and the book's .sdr sidecar.
QList<NtRecentBook> nt_recent_koreader_books(int count);

// Plato: .metadata.json and .reading-states.
QList<NtRecentBook> nt_recent_plato_books(int count);

// Nickel: KoboReader.sqlite, read through the sqlite3 already loaded in Nickel.
QList<NtRecentBook> nt_recent_nickel_books(int count);

#endif
