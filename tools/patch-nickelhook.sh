#!/bin/sh
# Ui_HomePageView::setupUi is a weakly bound (inline, uic-generated) symbol.
# Upstream NickelHook only patches STB_GLOBAL jump slots; a weak jump slot is
# hooked the same way, so allow it. Idempotent in-place edit of the submodule.
# Run from the repository root before make (tools/build.sh and CI do).
set -eu
if ! grep -q 'STB_WEAK' NickelHook/nh.c; then
  sed -i 's/ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL, "not a globally bound symbol/(ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL || ELFW(ST_BIND)(sym->st_info) == STB_WEAK), "not a globally or weakly bound symbol/' NickelHook/nh.c
  grep -q 'STB_WEAK' NickelHook/nh.c || { echo "NickelHook weak-symbol patch did not apply" >&2; exit 1; }
  echo "-- patched NickelHook/nh.c to accept STB_WEAK jump slots"
fi
