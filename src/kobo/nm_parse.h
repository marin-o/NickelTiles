#ifndef NT_NM_PARSE_H
#define NT_NM_PARSE_H

// Reads NickelMenu config files (menu_item lines in the main menu and their
// chain_* lines); generators and other menus are skipped.

#include <QList>

#include "kobo/actions.h"

// Parses every file in `dir` except doc, uninstall and dot files.
QList<NtEntry> nt_read_nickelmenu_entries(const QString &dir);

// The entries from NT_NICKELMENU_DIR. Keeps the last non-empty read, because
// Nickel rebuilds the home page during a library scan while /mnt/onboard is
// unmounted and the folder reads as empty.
QList<NtEntry> nt_nickelmenu_entries();

#endif
