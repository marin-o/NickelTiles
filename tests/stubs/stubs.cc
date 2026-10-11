// Stand-ins for the Nickel-facing functions recent.cc links against
// (nickel.h, actions.h), so it builds off the reader. No test calls them.

#include "actions.h"
#include "nickel.h"

void nt_toast(const QString &, int) {}

bool nt_open_in_nickel(const QString &) {
    return false;
}

bool nt_run_command(const QString &, const QString &) {
    return true;
}

QString nt_shell_quote(const QString &s) {
    return s;
}
