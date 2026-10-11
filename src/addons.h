#ifndef NT_ADDONS_H
#define NT_ADDONS_H

// Repairing NickelHook mods left switched off by their failsafe. While a mod
// starts, NickelHook renames its library to <lib>.so.failsafe and renames it
// back once Nickel has survived the mod's failsafe delay (3 to 20 s). If
// Nickel dies inside that window, for example when an app restarts it, the
// file stays renamed and the mod no longer loads.

#include <QString>
#include <QStringList>

// Call once at plugin start; the repair waits for every failsafe window.
void nt_addons_mark_start();

// True once Nickel has run long enough that no failsafe is still pending, so
// a renamed library really is switched off rather than starting up.
bool nt_addons_settled();

// Libraries left switched off (".so.failsafe" without the ".so"), as file
// names such as "libnm.so".
QStringList nt_switched_off_addons();

// A readable name for a library ("libnm.so" -> "NickelMenu").
QString nt_addon_name(const QString &library);

// Renames the switched-off libraries back; they load at the next start.
// Returns the ones restored.
QStringList nt_restore_addons();

#endif
