# 006: An off/on switch in NickelMenu

Status: Accepted

## Context

If Home ever becomes unusable, turning NickelTiles off needs a computer
(`nt_enabled:0` or the uninstall file). NickelMenu is reachable from every
screen, so an entry there works even when Home doesn't.

## Decision

- On its first start, NickelTiles offers to add `.adds/nm/nickel-tiles` with one
  entry, *NickelTiles off/on*. It writes the file only if the user taps Add, and
  the offer comes once. Tiles settings can add or remove it later. This is the
  one file NickelTiles writes outside its own folders (decision 004).
- The entry's shell command flips `nt_enabled` in the config and restarts. It
  doesn't depend on NickelTiles running, so it works while NickelTiles is off.
- If NickelTiles' library is gone, the command deletes its own file instead.
  NickelTiles' uninstall hook removes it as well.
- The offer waits until no Nickel dialog is open, so it is not hidden under
  "Computer detected".
- The grid does not show this entry as a tile.

## Consequences

People without NickelMenu don't get the entry; for them the crash guard
(decision 005) and USB remain.

## Revisit when

NickelMenu's config format changes.
