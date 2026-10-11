// Test stubs for the functions recent.cc calls into Nickel (nickel.h, actions.h).
// They only record what they were asked; nothing touches a reader. nt_shell_quote
// keeps the real quoting, since a test checks the exact command line.

#include "stubs.h"

#include "actions.h"
#include "nickel.h"

QStringList nt_stub_toasts;
QStringList nt_stub_commands;
QStringList nt_stub_content_ids;

void nt_toast(const QString &text, int) {
    nt_stub_toasts << text;
}

// Reports failure so the caller falls back to KOReader, as it does when Nickel
// cannot open the book.
bool nt_open_in_nickel(const QString &contentId) {
    nt_stub_content_ids << contentId;
    return false;
}

bool nt_run_command(const QString &, const QString &command) {
    nt_stub_commands << command;
    return true;
}

QString nt_shell_quote(const QString &s) {
    QString out = s;
    out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'") + out + QStringLiteral("'");
}
