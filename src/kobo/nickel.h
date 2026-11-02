#ifndef NT_NICKEL_H
#define NT_NICKEL_H

// Typed access to the libnickel functions NickelTiles calls. NickelHook fills
// the pointers from nt_nickel_dlsym at load time; every symbol is optional, so
// each wrapper returns false (or does nothing) when its symbol is missing on
// the running firmware.

#include <QString>

#include <functional>

#include <NickelHook.h>

extern struct nh_dlsym nt_nickel_dlsym[];

// Logs which optional symbols resolved (startup diagnostics).
void nt_nickel_log_symbols();

// Shows a Nickel toast; logs the text instead when toasts are unavailable.
void nt_toast(const QString &text, int ms = 1500);

// Nickel's two-button dialog (the one "Computer detected" uses). `onAccept`
// runs if the accept button is tapped; the reject button just closes it.
// Returns false, showing nothing, when the dialog is missing on this firmware.
bool nt_confirm(const QString &title, const QString &text, const QString &acceptText, const QString &rejectText,
                const std::function<void()> &onAccept);

bool nt_power_off();
bool nt_reboot();
bool nt_sleep();

// Connects to the last known network; `silent` skips Nickel's dialog.
bool nt_wifi_connect(bool silent);

enum class NtWifiChange { Enable, Disable, Toggle };
bool nt_wifi_set(NtWifiChange change);

// Nickel's home screen (MainNavController::home).
bool nt_go_home();

// Whether a Nickel Volume refers to a real book; an empty one does not.
bool nt_volume_is_valid(const void *volume);

// Looks a book up in Nickel's library and calls `use` with its Volume, which is
// valid only during the call. Reads the library, never changes it. Returns false
// when the book is not in the library. `contentId` is as for nt_open_in_nickel.
bool nt_with_library_book(const QString &contentId, const std::function<void(const void *volume)> &use);

// Opens a library book in Nickel's reader the way tapping it in the library
// does. `contentId` is the content table's ContentID ("file:///mnt/onboard/...").
bool nt_open_in_nickel(const QString &contentId);

#endif
