# NickelTiles

A grid of app tiles on the Kobo home screen. Built on
[NickelHook](https://github.com/pgaskin/NickelHook), like NickelMenu and
NickelHome.

- **Your NickelMenu entries as tiles.** Every main-menu entry in
  `.adds/nm/` becomes a tile automatically (KOReader, Plato, USB connection,
  reboot, KPM, ...). Add or remove an entry and the grid follows.
- **Continue reading.** One tile always holds your most recent book, whichever
  app you read it in: KOReader, Plato or Kobo's own reader. It shows the cover,
  title and progress, and opens the book in the app that last read it.
- **Sleep screen follows that book.** With Kobo's *Show book covers full
  screen* setting on, the sleep screen shows the same book's cover, not just
  the last book opened in Kobo's reader.
- **Settings on the reader.** The *Tiles settings* tile opens a page for
  tiles per row, tile size, which tiles are shown, and the options above.
  Changes apply at once.
- **Arrange.** Long-press a tile, then tap another one to swap them.
- **E-ink friendly.** Black on white; a tile darkens while pressed.

## Status

Early. Developed and tested on firmware **4.45.23697** only. Other recent
firmware (4.23.15505 and later, which share the home screen layout) may work
but is untested. Reports welcome.

NickelHook's failsafe applies: if Nickel crashes within 3 seconds of the mod
loading, the mod disables itself (its library is renamed to
`libnickeltiles.so.failsafe`) and Nickel starts normally. Installing the
package again re-enables it.

## Install

**With KPM** (the Kobo package manager): add this repository
(`kpm add https://github.com/marin-o/NickelTiles`) and install `nickeltiles`.

**By hand**: download `KoboRoot.tgz` from the latest release, copy it into the
`.kobo` folder on the reader over USB, eject and unplug. The reader installs it
and restarts.

NickelMenu is optional; without it the grid has the built-in tiles and any
`tile:` lines from the config.

## Use

| Action | What happens |
| --- | --- |
| Tap a tile | Runs it (the tile darkens while pressed) |
| Long-press a tile, tap another | Swaps the two; the order is saved |
| *Tiles settings* tile | Settings page; Save applies at once, Cancel discards |

Everything is also in a text file, `.adds/nickel-tiles/config` (edits there
need a reboot). The installed `.adds/nickel-tiles/doc` lists every key.
Custom icons: `.adds/nickel-tiles/icons/<label>.png`, where the label is lower
case with spaces as `-` (e.g. `kpm-status.png`), 200x200 recommended.

### Sleep screen

Kobo only uses a custom sleep image when **Settings > Energy saving and
privacy > Show book covers full screen** is on. NickelTiles keeps the image as
`.kobo/screensaver/nt-recent.<ext>` and refreshes it as the reader goes to
sleep. Turn it off on the settings page (or `nt_sleep_cover:0`); images you put
in that folder yourself are left alone.

## Uninstall

Through KPM, or create an empty file named `uninstall` in
`.adds/nickel-tiles/` and reboot (deleting the folder works too). The mod
removes its library on the next start.

## Build

Needs the [NickelTC](https://github.com/pgaskin/NickelTC) toolchain (GCC 4.9,
Qt 5.2.1).

```sh
git clone --recursive https://github.com/marin-o/NickelTiles
cd NickelTiles
NICKELTC_BASE=/path/to/nickeltc tools/build.sh      # -> KoboRoot.tgz
NICKELTC_BASE=/path/to/nickeltc LIBNICKEL=/path/to/libnickel.so.1.0.0 tools/check-syms.sh
```

`tools/build.sh` patches the NickelHook submodule to accept weakly bound
symbols (the home page hook, `Ui_HomePageView::setupUi`, is one).
`tools/check-syms.sh` checks every hooked symbol against a firmware's
`libnickel.so` and the package contents. `docs/homepage-map.md` describes the
home screen widget tree this relies on.

## Notes

- *Rescan books* actions are refused on purpose: on the development device a
  library rescan deleted sideloaded books.
- Plato books open Plato itself, not the book (Plato has no open-at-launch
  option).

## License

MIT. See [LICENSE](LICENSE).
