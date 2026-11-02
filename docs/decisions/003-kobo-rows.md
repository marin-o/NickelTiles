# 003: When Kobo's home rows stay visible

Status: Accepted

## Context

The grid replaces Kobo's rows by default. Two situations went wrong:

- A first install with no NickelMenu entries and no recent book showed one
  settings tile on an empty page, which looks broken.
- With *Hide Kobo's home rows* off, the rows and the grid could need more height
  than the page has. Tiles have a fixed height, so the layout squeezed the grid
  rows and the tiles overlapped.

## Decision

- With nothing to show but the settings tile, Kobo's rows stay visible even with
  the setting on.
- With rows kept, tiles win: Kobo's rows are hidden, lowest first, until
  everything fits. The check runs as the grid is shown and again shortly after,
  because rows have no size until Nickel fills them.

## Alternatives

Shrinking tiles to fit, or scrolling. Hiding rows was the simplest change that
keeps every tile at the size the owner chose.

## Consequences

The setting means "keep as many rows as fit". With two rows of tiles on a 6-inch
screen, that can be only the top row.

## Revisit when

People want a guaranteed Kobo row more than full-size tiles.
