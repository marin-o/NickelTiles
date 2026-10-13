# Contributing to NickelTiles

Thanks for helping. Bug reports from devices and firmware we have not tried
are as valuable as code: so far NickelTiles has run on one reader.

## Branches

| Branch | Holds | Changes arrive by |
| --- | --- | --- |
| `dev` (default) | The next release | Pull requests from your branch |
| `main` | Released code; every release is a `vX.Y.Z` tag here | Maintainers only, by fast-forwarding to a tested `dev` |

Open pull requests against **`dev`**: it accepts changes only through pull
requests whose build, tests and lint pass, and they are squash-merged, so one pull
request becomes one commit. `main` can only be updated by maintainers, only to
commits whose build, tests and lint passed, and its history is never rewritten.

Releases: a maintainer fast-forwards `main` to `dev` once it has been tested on
a reader, and tags it (`vX.Y.Z`); GitHub Actions then builds `KoboRoot.tgz`
and publishes the release, which KPM picks up.

## Building

With Docker:

```sh
git clone --recursive https://github.com/marin-o/NickelTiles
cd NickelTiles
docker run --rm -v "$PWD:/src" -w /src ghcr.io/pgaskin/nickeltc:1.0 \
  sh -c 'sh tools/patch-nickelhook.sh && make all koboroot'
```

Or with [NickelTC](https://github.com/pgaskin/NickelTC) unpacked locally:
`NICKELTC_BASE=/path/to/nickeltc tools/build.sh`.

The build uses `-Werror` with a strict warning set for `src/` (old-style
casts, missing declarations, format strings, and more; see the `Makefile`); a
pull request must build without warnings.

## Lint

Every push and pull request also runs the Lint workflow (`tools/lint.sh`):

- **clang-format** checks the layout against `.clang-format`. Run
  `clang-format -i` on the files you changed instead of formatting by hand.
- **clang-tidy** checks the code against `.clang-tidy`: likely bugs, needless
  copies, outdated or unclear C++, and functions that grew too complex
  (cognitive complexity above 20 means the function should be split).

On Debian or Ubuntu, with the same tool versions as the workflow:

```sh
sudo apt install qtbase5-dev
pip install clang-tidy==22.1.8 clang-format==23.1.3
QT_INCLUDE=/usr/include/x86_64-linux-gnu/qt5 sh tools/lint.sh
```

Fix the finding rather than silencing it. A `NOLINT` needs a reason in the
same comment and is accepted only where the check is wrong for this code (for
example Nickel's calling conventions). Changes to the rule files go in a pull
request of their own.

## Automated tests

`tests/` holds host tests (Qt Test) for the logic that does not need a
reader: settings and config files, NickelMenu parsing, book covers, recent
books, word wrap and grid sizes. They run on every push and pull request
(the Tests workflow), and locally on a Linux machine with Qt 5 development
packages:

```sh
sudo apt install qtbase5-dev qtbase5-dev-tools pkg-config g++ make python3
make -C tests check
```

Tests are kept few on purpose. Add one when it guards a bug that actually
happened (a bug fix comes with the test that would have caught it) or a file
format NickelTiles does not control, such as EPUBs, KOReader or Plato state,
NickelMenu files, or the config file it edits in place. Trivial code,
defaults and every edge case of a helper do not need tests.

## Testing on a reader

1. Copy `KoboRoot.tgz` into `.kobo` on the reader, eject, unplug. It installs
   and restarts.
2. Set `nt_log:1` in `.adds/nickel-tiles/config` for a detailed log
   (`.adds/nickel-tiles/nickel-tiles.log`) and screenshots of the home screen
   and settings page in the same folder.
3. If you call a new libnickel function, check it exists in your firmware:
   `LIBNICKEL=/path/to/libnickel.so.1.0.0 tools/check-syms.sh` (the library is
   in the firmware's `KoboRoot.tgz`).
4. Wait about 30 seconds after each restart before launching apps or
   restarting again; NickelHook switches mods off if Nickel dies earlier.

Say in the pull request which device and firmware you tested on.

## Code

Read [docs/architecture.md](docs/architecture.md) first. In short:

- One module per job, each documented in its header. Prefer extending a
  module over adding code to `plugin.cc` or `grid.cc`.
- Toolchain limits: GCC 4.9, C++11, Qt 5.2.1. No exceptions, no moc.
- Settings are read through `nt_settings()` and saved through
  `nt_settings_save()`; new settings get their key and default in
  `settings_model.cc`, a line in `res/default`, and a mention in `res/doc`.
- Paths come from `paths.h`. Log with `NT_LOG` (always) or `NT_DBG`
  (verbose only).
- Every libnickel symbol is optional (`nickel.cc`), and a missing one only
  disables the feature that needs it. Nickel must keep working on firmware
  where a symbol is absent.
- Comments explain why, not what. No dead or commented-out code.
- No duplicated code: if two places need the same thing, it becomes one
  function in the right module. Pass a struct (often `NtSettings`) rather than
  adding a third or fourth loose parameter.
- Keep functions small; if one needs comments to separate its parts, split it.

## Safety rules

These protect people's books and readers:

- Never trigger a Nickel library rescan; on the development reader one
  deleted sideloaded books.
- Write only inside `.adds/nickel-tiles/` and `.kobo/screensaver/` (and only
  the file NickelTiles owns there). Never touch Kobo's databases or
  `Kobo eReader.conf`.
- Anything that runs during Nickel's startup must be fast and must not crash:
  a crash in the first seconds disables every NickelHook mod on the reader.

## Commits and pull requests

- One topic per pull request.
- Commit summary in the imperative, at most 72 characters ("Fit tiles to the
  screen width"); the body says why.
- Update `README.md`, `res/doc` and `res/default` when behaviour or settings
  change.

## Reporting bugs

Use the bug report form. Include the reader model, the firmware version
(Settings > Device information) and, if you can, the log from a run with
`nt_log:1`.

## License

By contributing you agree that your contribution is licensed under the
project's MIT license.
