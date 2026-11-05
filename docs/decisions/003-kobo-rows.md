# 003: When Kobo's home rows stay visible

Status: Accepted

## Context

The grid replaces Kobo's rows by default. Three situations went wrong:

- A first install with no NickelMenu entries and no recent book showed one
  settings tile on an empty page, which looks broken.
- With Kobo's rows kept, the rows and the grid could need more height than the
  page has. Tiles have a fixed height, so the layout squeezed the grid rows and
  the tiles overlapped.
- Hiding rows quietly to make room for tiles made an on/off "Hide Kobo's home
  rows" setting misleading: off still hid rows.

## Decision

- *Kobo's home rows* has three values: hidden, top row, all rows
  (`nt_kobo_rows`; the older `nt_hide_rows` still reads as hidden or all).
- Kept rows are never hidden to fit tiles on the home page. They share the
  first page with as many tiles as fit under them, which can be none. The other
  tiles go on further pages, where Kobo's rows make way. Coming back home shows
  the first page.
- With nothing to show but the settings tile, Kobo's rows stay visible whatever
  the setting. The tile sits in the empty right half of Kobo's top row, which a
  reader with no books leaves empty: below the row, it covered the row's text,
  since Kobo's rows already fill a page with no books. If that half isn't empty,
  the tile goes below the top row as before.

## Alternatives

- Shrinking tiles to fit: tiles keep the size the owner chose.
- Hiding rows, lowest first, until tiles fit: what the setting promised was then
  not what the home page showed.
- Scrolling the home page: poor on e-ink, and Nickel's home page doesn't scroll.

## Consequences

With all three rows kept on a 6-inch screen, the first page is Kobo's home
with only the arrows below, and the tiles start on page 2.

## Revisit when

People want smaller tiles beside Kobo's rows rather than a second page.
