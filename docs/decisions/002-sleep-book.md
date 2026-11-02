# 002: Give the sleep screen the Continue reading book

Status: Accepted

## Context

With *Show book covers full screen* on, `N3PowerWorkflowManager::show` passes
Kobo's latest book to `PowerViewController::setVolume`. `loadView` then builds
the cover screen and calls `updateCover` only if that book is valid. With no
book, it shows plain "Sleeping". So someone who reads only in KOReader or Plato
got no cover, and the image NickelTiles writes in `updateCover` never showed.

## Decision

Hook `setVolume` as well. When the book is empty, look the Continue reading book
up in Kobo's library and pass that instead. Kobo's own setting still decides
whether any of this runs.

## Alternatives

- Mark a book as being read in Kobo's database. Rejected: see decision 004.
- Replace the sleep screen with our own widget. Rejected: much more to maintain,
  and it would ignore Kobo's sleep settings.

## Consequences

The book must be in Kobo's library. If it isn't, Kobo still shows "Sleeping".

## Revisit when

A firmware changes how `show` or `loadView` choose the screen.
