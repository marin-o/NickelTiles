#include "books/open_book.h"

#include "core/paths.h"
#include "kobo/actions.h"
#include "kobo/nickel.h"

void nt_open_recent_book(const NtRecentBook &book) {
    if (!book.valid) {
        nt_toast(QStringLiteral("No recent book"));
        return;
    }
    if (book.app == NtReaderApp::Plato) {
        nt_run_command(QStringLiteral("Continue reading"), QStringLiteral("exec " NT_PLATO_LAUNCHER));
        return;
    }
    if (book.app == NtReaderApp::Nickel && nt_open_in_nickel(book.contentId)) return;
    const QString command = QStringLiteral("exec " NT_KOREADER_LAUNCHER " ") + nt_shell_quote(book.path);
    nt_run_command(QStringLiteral("Continue reading"), command);
}
