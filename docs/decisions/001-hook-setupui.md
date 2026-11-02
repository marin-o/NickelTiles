# 001: Insert the grid from `Ui_HomePageView::setupUi`

Status: Accepted

## Context

The grid has to go in right after Nickel builds its home page. The natural hook
is `HomePageView`'s constructor, but NickelHome already hooks that symbol, and
NickelHook does not chain two mods on one symbol. Many people run both.

## Decision

Hook `Ui_HomePageView::setupUi` and insert the grid after the original returns.
It builds the same widget tree, and the constructor calls it on every new home
page.

## Consequences

- NickelTiles and NickelHome work together. Their hooks don't collide.
- `setupUi` is an inline, weakly bound symbol, and NickelHook only accepts
  globally bound ones, so `tools/patch-nickelhook.sh` patches that check.
- This hook is the only required one. Without it, NickelTiles has nothing to do.

## Revisit when

NickelHook can chain hooks, or a firmware no longer calls `setupUi` through the PLT.
