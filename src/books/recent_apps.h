#ifndef NT_RECENT_APPS_H
#define NT_RECENT_APPS_H

// Each app's own record of its most recent book. Their formats belong to those
// apps and change for their own reasons, so each lives in its own file. Each
// returns an invalid book when the app has no usable record. Use nt_recent_book
// (books/recent.h) unless you need one specific app.

#include "books/recent.h"

// KOReader: history.lua and the book's .sdr sidecar.
NtRecentBook nt_recent_koreader_book();

// Plato: .metadata.json and .reading-states.
NtRecentBook nt_recent_plato_book();

// Nickel: KoboReader.sqlite, read through the sqlite3 already loaded in Nickel.
NtRecentBook nt_recent_nickel_book();

#endif
