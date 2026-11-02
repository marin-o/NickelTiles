#ifndef NT_OPEN_BOOK_H
#define NT_OPEN_BOOK_H

#include "books/recent.h"

// Opens the book in the app that last read it: Nickel's reader (KOReader if
// that fails), KOReader with the file, or Plato (which cannot be told which
// book to open, so it starts on its own last book).
void nt_open_recent_book(const NtRecentBook &book);

#endif
