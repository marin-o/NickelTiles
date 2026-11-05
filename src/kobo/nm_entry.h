#ifndef NT_NM_ENTRY_H
#define NT_NM_ENTRY_H

// NickelTiles' own NickelMenu entry, "NickelTiles off/on" (NT_NM_ENTRY). It
// turns the tiles off or on and restarts the reader. NickelMenu is reachable
// from every screen, so this works even when Home is unusable. Chosen after
// NickelTiles was removed, the entry deletes itself. The grid does not show it
// as a tile.

// Whether NickelMenu is installed.
bool nt_nm_entry_possible();

bool nt_nm_entry_present();

// Adds or removes the entry; nothing to do if it already is that way.
// Returns false if the file could not be written or removed.
bool nt_nm_entry_set(bool wanted);

// True once: NickelMenu is installed, the entry is missing and it was never
// offered. Remembers that it was offered.
bool nt_nm_entry_offer_due();

#endif
