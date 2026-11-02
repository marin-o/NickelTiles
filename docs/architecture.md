# Architecture

NickelTiles is a NickelHook plugin loaded into Nickel, Kobo's reading app. It
hooks three libnickel functions and calls a handful of others; everything else is
plain Qt 5.2 widgets added to Nickel's home page.

## Hooks

| Hooked function | Why | Handler |
| --- | --- | --- |
| `Ui_HomePageView::setupUi` | Builds the home page; the grid is inserted right after. NickelHome hooks `HomePageView`'s constructor and NickelHook does not chain two mods on one symbol, so this one is used instead. | `plugin.cc` → `nt_install_grid` |
| `PowerViewController::updateCover` (optional) | Picks the sleep screen image; the Continue reading cover is written just before. | `plugin.cc` → `nt_sleep_cover_refresh` |
| `PowerViewController::setVolume` (optional) | Receives Nickel's latest book as the sleep screen is shown. When it is empty (nothing read in Nickel), the Continue reading book goes in its place, so the cover screen is still chosen. | `plugin.cc` → `nt_sleep_cover_choose_book` |

## How a feature works

**Tapping a tile.** `NtPressable` turns touch events into a tap, and `NtTile::tapped`
tells the grid (`NtGrid::tileTapped`). Unless a tile is marked for arranging, the
grid calls that tile's `activate()`. For Continue reading that is
`nt_open_recent_book` (`books/recent`): KOReader and Plato are launched through
`kobo/actions`, Nickel books open through `nt_open_in_nickel` (`kobo/nickel`).

**Showing the most recent book.** `NtRecentTile::refresh` calls `nt_recent_book`,
which reads each app's records and keeps the newest. It runs when Home is shown
and again shortly after, since apps and Nickel update their records late.

**Going to sleep.** `N3PowerWorkflowManager::show` passes Kobo's latest book to
`PowerViewController::setVolume`. If that book is empty, `_nt_setvolume_hook` passes
the Continue reading book instead (decision 002). `loadView` then calls `updateCover`,
and `_nt_updatecover_hook` runs `nt_sleep_cover_refresh` first, which keeps the cover
in `.kobo/screensaver` for Kobo to display.

**Building Home.** `_nt_setupui_hook` calls `nt_install_grid`, which decides whether
Kobo's rows stay (decision 003), builds the tiles and inserts the grid. Saving in
Tiles settings runs `nt_install_grid` again.

Reasons behind the less obvious choices are in `docs/decisions/`.

## Modules

Each `.h` documents its module; this is the map. Modules are grouped in
folders by job, and includes name the folder (`#include "books/cover.h"`).

| Folder | Module | Job |
| --- | --- | --- |
| `src/` | `plugin` | NickelHook registration, startup log, the hook entry points |
| `core/` | `util` | Logging (`NT_LOG`, `NT_DBG`) and small string helpers, shared by C and C++ (C) |
| | `config` | The raw `key:value` config file: parse, cache, atomic in-place writes (C) |
| | `settings_model` | All settings as one `NtSettings` value with names and defaults in one place; load and save |
| | `paths` | Fixed locations on the reader (header only) |
| `kobo/` | `nickel` | Typed wrappers over the optional libnickel functions (toast, dialog, power, Wi-Fi, home, open a book in Nickel's reader) |
| | `nm_parse` | Reads NickelMenu's config files into entries |
| | `actions` | Runs NickelMenu actions and plain shell commands |
| | `addons` | Finds and restores NickelHook add-ons a crash left switched off |
| `books/` | `recent` | The most recently read book across KOReader, Plato and Nickel, and opening it in the right app |
| | `zip` | Reading zip members with the zlib Nickel already loaded |
| | `epub`, `fb2`, `mobi`, `cbz` | Each finds the cover inside one book format |
| | `cover` | A book's cover in any of those formats, scaled and cached; adding a format is one line in its table |
| | `sleepcover` | Keeps the Continue reading cover in `.kobo/screensaver` |
| `ui/` | `pressable` | Touch handling shared by every tappable widget (Nickel sends only `QTouchEvent`s) |
| | `ui` | Fonts, word wrap, debug screenshots |
| | `layout` | Tile size and grid fit arithmetic, shared by the grid and the settings page |
| | `tiles` | The tile widgets |
| | `grid` | The grid on the home page: building, arranging, inserting into Nickel's layout |
| | `settings_page` | The on-device settings page |

Dependencies point downwards: `ui/` uses `books/` and `kobo/`, which use
`core/`. Widgets stay in `ui/`; the exceptions are `plugin.cc`, which receives
the home page widget from the hook, and `kobo/nickel.cc`, which opens Kobo's
own dialog.

The build puts objects and the library in `build/` (the Makefile sets
`BUILDDIR`, which `tools/patch-nickelhook.sh` adds to NickelHook's makefile).

## Conventions

- GCC 4.9, C++11, Qt 5.2.1 (NickelTC); no exceptions, no moc.
- Settings are read through `nt_settings()`, never by key elsewhere.
- Paths come from `paths.h`.
- `NT_LOG` always logs (syslog and `.adds/nickel-tiles/nickel-tiles.log`);
  `NT_DBG` only with `nt_log:1`.
- Every libnickel symbol is optional except the home page hook; a missing one
  disables just the feature that needs it.
