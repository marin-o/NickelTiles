# 004: Never write to Kobo's data, and never trigger a rescan

Status: Accepted

## Context

Several features would be easier with writes: marking a book as read in
`KoboReader.sqlite`, or changing `Kobo eReader.conf`. But these files hold
people's libraries and settings, and their format is Kobo's and can change. On
the development reader, a library rescan deleted sideloaded books.

## Decision

NickelTiles writes only inside `.adds/nickel-tiles/` and its own `nt-recent.*` in
`.kobo/screensaver/`. It reads Kobo's database and uses Kobo's own functions for
anything else, and it refuses to run NickelMenu's rescan action from a tile.

## Consequences

- Some features need a workaround, such as decision 002.
- Rescan stays available from NickelMenu's own menu, where it's the user's choice.

## Revisit when

Not expected to change.
