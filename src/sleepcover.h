#ifndef NT_SLEEPCOVER_H
#define NT_SLEEPCOVER_H

// With "Show book covers full screen" on, Nickel's sleep screen shows a random
// image from .kobo/screensaver when that folder has any, else the cover of the
// last book read in Nickel. NickelTiles keeps one image there,
// nt-recent.<ext>, so the sleep screen follows the Continue reading book in
// every app. If the owner keeps their own pictures in that folder, NickelTiles
// stays out of it.

#include "settings_model.h"

// Called as the sleep screen is built: writes the current book's cover (only
// when the book changed), or removes the image when the feature is off or
// there is no EPUB cover.
void nt_sleep_cover_refresh(const NtSettings &settings);

#endif
