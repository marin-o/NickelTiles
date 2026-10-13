# Architecture

NickelTiles is a NickelHook plugin loaded into Nickel, Kobo's reading app. It
hooks two libnickel functions and calls a handful of others; everything else is
plain Qt 5.2 widgets added to Nickel's home page.

## Hooks

| Hooked function | Why | Handler |
| --- | --- | --- |
| `Ui_HomePageView::setupUi` | Builds the home page; the grid is inserted right after. NickelHome hooks `HomePageView`'s constructor and NickelHook does not chain two mods on one symbol, so this one is used instead. | `plugin.cc` → `nt_install_grid` |
| `PowerViewController::updateCover` (optional) | Picks the sleep screen image; the Continue reading cover is written just before. | `plugin.cc` → `nt_sleep_cover_refresh` |

## Modules

Each `.h` in `src/` documents its module; this is the map.

| Module | Job |
| --- | --- |
| `plugin` | NickelHook registration, startup log, the two hook entry points |
| `nickel` | Typed wrappers over the optional libnickel functions (toast, power, Wi-Fi, home, open a book in Nickel's reader) |
| `util` | Logging (`NT_LOG`, `NT_DBG`) and small string helpers, shared by C and C++ (C) |
| `config` | The raw `key:value` config file: parse, cache, atomic in-place writes (C) |
| `settings_model` | All settings as one `NtSettings` value with names and defaults in one place; load and save |
| `nm_parse` | Reads NickelMenu's config files into entries |
| `actions` | Runs NickelMenu actions and plain shell commands |
| `recent` | The most recently read book across KOReader, Plato and Nickel, and opening it in the right app |
| `zip` | Reading zip members with the zlib Nickel already loaded |
| `epub`, `fb2`, `mobi`, `cbz` | Each finds the cover inside one book format |
| `cover` | A book's cover in any of those formats, scaled and cached; adding a format is one line in its table |
| `sleepcover` | Keeps the Continue reading cover in `.kobo/screensaver` |
| `addons` | Finds and restores NickelHook add-ons a crash left switched off |
| `pressable` | Touch handling shared by every tappable widget (Nickel sends only `QTouchEvent`s) |
| `ui` | Fonts, word wrap, debug screenshots |
| `layout` | Tile size and grid fit arithmetic, shared by the grid and the settings page |
| `tiles` | The tile widgets |
| `grid` | The grid on the home page: building, arranging, inserting into Nickel's layout |
| `settings_page` | The on-device settings page |

Dependencies point downwards: UI (`grid`, `tiles`, `settings_page`) uses the
domain modules (`recent`, `actions`, `settings_model`), which use the low-level
ones (`nickel`, `config`, `zip`). Nothing below the UI includes a widget header
except `ui` and `pressable`.

## Conventions

- GCC 4.9, C++11, Qt 5.2.1 (NickelTC); no exceptions, no moc.
- Settings are read through `nt_settings()`, never by key elsewhere.
- Paths come from `paths.h`.
- `NT_LOG` always logs (syslog and `.adds/nickel-tiles/nickel-tiles.log`);
  `NT_DBG` only with `nt_log:1`.
- Every libnickel symbol is optional except the home page hook; a missing one
  disables just the feature that needs it.
