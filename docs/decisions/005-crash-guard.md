# 005: Turn off after repeated early crashes

Status: Accepted

## Context

NickelHook switches a mod off when Nickel dies within its failsafe delay, which
for NickelTiles is 3 seconds. A crash later, for example while Home is being
filled, is not caught: the reader would restart into the same crash again and
again, and someone without a computer at hand could be stuck.

## Decision

- A crash signal (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT) within 30 seconds of
  a start is counted in `.adds/nickel-tiles/early-crashes`. A start that lasts 30
  seconds clears the count.
- After three such crashes in a row, the next start sets `nt_enabled:0` and shows
  a dialog with a button to turn the tiles back on.
- Only signals count. Nickel also closes on purpose, when KOReader or Plato
  start, and those exits must not switch the tiles off.
- Every crash is written to `nickel-tiles.log` with where it happened, whatever
  `nt_log` says, so a report shows whether NickelTiles was the cause.

## Alternatives

- Counting every start that ends within 30 seconds: switched the tiles off for
  people who open KOReader right after starting.
- A longer window: a crash a minute in leaves time to use the NickelMenu switch
  (decision 006).

## Consequences

A crash loop that is not NickelTiles' fault also turns it off. The dialog says
it may not be the cause, and one tap turns it back on.

## Revisit when

A crash loop shows up that the 30-second window misses.
