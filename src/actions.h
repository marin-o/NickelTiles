#ifndef NT_ACTIONS_H
#define NT_ACTIONS_H

// NickelMenu-style actions and the entries that chain them. The supported
// kinds are the ones this mod mirrors from NickelMenu: cmd_spawn, cmd_output,
// dbg_toast, power, nickel_misc (force_usb_connection, home), nickel_wifi.
// Library rescans (nickel_misc rescan_books*) are refused: on the development
// reader a rescan deleted sideloaded books.

#include <QList>
#include <QString>

struct NtAction {
    enum Chain { First, OnSuccess, OnFailure, Always };
    QString kind;
    QString arg;
    Chain chain = First;
};

struct NtEntry {
    QString label;
    QList<NtAction> actions;
};

// Runs the entry's actions in order, honouring chain_success/failure/always.
// Failures are toasted with the entry's label. Returns the last result.
bool nt_run_entry(const NtEntry &entry);

// Starts `command` with /bin/sh -c, detached and without a toast. `label`
// names it in the log and in an error toast.
bool nt_run_command(const QString &label, const QString &command);

// Quotes `s` as one /bin/sh word.
QString nt_shell_quote(const QString &s);

#endif
