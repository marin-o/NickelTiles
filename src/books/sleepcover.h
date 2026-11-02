#ifndef NT_SLEEPCOVER_H
#define NT_SLEEPCOVER_H

// With "Show book covers full screen" on, Nickel's sleep screen shows a random
// image from .kobo/screensaver when that folder has any, else the cover of the
// last book read in Nickel. NickelTiles keeps one image there,
// nt-recent.<ext>, so the sleep screen follows the Continue reading book in
// every app. If the owner keeps their own pictures in that folder, NickelTiles
// stays out of it.

#include <functional>

#include "core/settings_model.h"
#include "kobo/nickel.h"

// Called as the sleep screen is built: writes the current book's cover (only
// when the book changed), or removes the image when the feature is off or
// there is no EPUB cover.
void nt_sleep_cover_refresh(const NtSettings &settings);

// With no book read in Nickel, Nickel gives the sleep screen an empty book and
// shows "Sleeping", never reaching the image above. Calls `use` with the book
// the sleep screen should get: `kobosBook`, or when that is empty, the Continue
// reading book, whichever app read it. `kobosBook` is also the answer when the
// feature is off or that book is not in Nickel's library.
void nt_sleep_cover_choose_book(const NtSettings &settings, const NtVolume *kobosBook,
                                const std::function<void(const NtVolume *book)> &use);

// Removes NickelTiles' image from .kobo/screensaver; called when the mod is
// uninstalled, so the sleep screen does not keep showing the last book.
void nt_sleep_cover_remove();

#endif
