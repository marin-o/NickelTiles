#ifndef NT_NMENTRIES_H
#define NT_NMENTRIES_H

// NickelMenu config mirroring: parses /mnt/onboard/.adds/nm/* for `menu_item`
// lines in the main menu and runs their actions the way NickelMenu does, for
// the subset of actions this reader uses (cmd_spawn, cmd_output, power,
// nickel_misc, nickel_wifi, dbg_toast, chain_*). Anything else is logged and
// shown as unsupported.

#include <QList>
#include <QString>

struct NtAction {
    QString kind;      // cmd_spawn, cmd_output, power, nickel_misc, nickel_wifi, dbg_toast, ...
    QString arg;
    int chain;         // 0: first action, 1: chain_success, 2: chain_failure, 3: chain_always
};

struct NtEntry {
    QString label;
    QList<NtAction> actions;
    QString source;    // file it came from
};

// Reads every file in the NickelMenu config dir (except doc/uninstall/hidden).
QList<NtEntry> nt_read_nickelmenu_entries(const QString &dir);

// Runs the entry's action chain on the Qt main thread. Returns true if the
// last executed action succeeded. Toasts errors through Nickel when possible.
bool nt_run_entry(const NtEntry &entry);

// Shows a Nickel toast if MainWindowController symbols resolved, else logs.
void nt_toast(const QString &text, int ms = 1500);

#endif
