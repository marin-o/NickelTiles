#include "kobo/nm_entry.h"

#include <QByteArray>
#include <QFile>

#include "core/paths.h"
#include "core/util.h"

namespace {

// NickelMenu runs the command with /bin/sh. It flips nt_enabled in the config,
// or deletes this file if NickelTiles' library is gone. The newline before
// the setting guards against a config that doesn't end in one.
const char kEntry[] = "# Added by NickelTiles: turns its home screen tiles off or on, then restarts.\n"
                      "# If NickelTiles was removed, choosing it deletes this file.\n"
                      "menu_item:main:NickelTiles off/on:cmd_output:5000:quiet:"
                      "C=" NT_CONFIG_DIR "/config; "
                      "[ -e " NT_LIBRARY " ] || { rm -f " NT_NM_ENTRY "; exit 1; }; "
                      "if grep -q '^nt_enabled:0' $C; then V=1; else V=0; fi; "
                      "sed -i '/^nt_enabled:/d' $C; printf '\\nnt_enabled:%s\\n' $V >> $C; sync\n"
                      "  chain_success:power:reboot\n"
                      "  chain_failure:dbg_toast:NickelTiles is not installed, so this menu entry removed itself.\n";

}   // namespace

bool nt_nm_entry_possible() {
    return QFile::exists(QStringLiteral(NT_NM_LIBRARY));
}

bool nt_nm_entry_present() {
    return QFile::exists(QStringLiteral(NT_NM_ENTRY));
}

bool nt_nm_entry_set(bool wanted) {
    if (wanted == nt_nm_entry_present()) return true;
    QFile f(QStringLiteral(NT_NM_ENTRY));
    if (!wanted) return f.remove();
    const QByteArray text(kEntry);
    const bool ok = f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(text) == text.size();
    NT_LOG("NickelMenu entry added=%d", ok ? 1 : 0);
    return ok;
}

bool nt_nm_entry_offer_due() {
    if (!nt_nm_entry_possible() || nt_nm_entry_present() || QFile::exists(QStringLiteral(NT_NM_ENTRY_OFFERED)))
        return false;
    QFile mark(QStringLiteral(NT_NM_ENTRY_OFFERED));
    // Not offered at all if this can't be remembered, rather than offered every start.
    return mark.open(QIODevice::WriteOnly);
}
