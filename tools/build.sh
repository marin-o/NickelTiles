#!/bin/bash
# Build NickelTiles with the NickelTC toolchain in WSL (no Docker).
# Usage (from WSL): tools/build.sh [make targets...]   default: all koboroot
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
BASE=${NICKELTC_BASE:?set NICKELTC_BASE to your NickelTC directory (the one holding tc/)}
TC=$BASE/tc/arm-nickel-linux-gnueabihf
SYSROOT=$TC/arm-nickel-linux-gnueabihf/sysroot
# Host pkg-config is absent in this WSL; a small stand-in lives in $BASE/hostbin.
export PATH="$BASE/hostbin:$TC/bin:$PATH"
cd "$HERE"
# Ui_HomePageView::setupUi is a weakly bound (inline, uic-generated) symbol.
# Upstream NickelHook only patches STB_GLOBAL jump slots; a weak jump slot is
# hooked the same way, so allow it. Idempotent in-place edit of the submodule.
if ! grep -q 'STB_WEAK' NickelHook/nh.c; then
  sed -i 's/ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL, "not a globally bound symbol/(ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL || ELFW(ST_BIND)(sym->st_info) == STB_WEAK), "not a globally or weakly bound symbol/' NickelHook/nh.c
  grep -q 'STB_WEAK' NickelHook/nh.c || { echo "NickelHook weak-symbol patch did not apply" >&2; exit 1; }
  echo "-- patched NickelHook/nh.c to accept STB_WEAK jump slots"
fi
[ $# -gt 0 ] || set -- all koboroot
make CROSS_COMPILE=arm-nickel-linux-gnueabihf- \
     MOC="$SYSROOT/usr/bin/moc" RCC="$SYSROOT/usr/bin/rcc" \
     "$@"
