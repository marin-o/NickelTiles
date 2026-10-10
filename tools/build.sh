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
sh tools/patch-nickelhook.sh
[ $# -gt 0 ] || set -- all koboroot
make CROSS_COMPILE=arm-nickel-linux-gnueabihf- \
     MOC="$SYSROOT/usr/bin/moc" RCC="$SYSROOT/usr/bin/rcc" \
     "$@"
