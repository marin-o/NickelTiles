#!/bin/bash
# Verifies that every libnickel symbol the mod hooks or calls exists in the
# firmware's libnickel.so, and that the packaged KoboRoot.tgz contains only the
# expected paths with a 755 root. Run from WSL after tools/build.sh.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
BASE=${NICKELTC_BASE:?set NICKELTC_BASE to your NickelTC directory (the one holding tc/)}
NM=$BASE/tc/arm-nickel-linux-gnueabihf/bin/arm-nickel-linux-gnueabihf-nm
LIBNICKEL=${LIBNICKEL:-$BASE/libnickel.so.1.0.0}
SO=$HERE/src/libnickeltiles.so
rc=0

echo "== hooked libnickel symbols (from source)"
grep -ohE '"_Z[A-Za-z0-9_]+"' "$HERE"/src/*.cc | tr -d '"' | sort -u > /tmp/nt-hooked.txt
$NM -D --defined-only "$LIBNICKEL" | awk '{print $3}' | sort -u > /tmp/nt-libnickel.txt
while read -r sym; do
  if grep -qx "$sym" /tmp/nt-libnickel.txt; then echo "ok      $sym"; else echo "MISSING $sym"; rc=1; fi
done < /tmp/nt-hooked.txt

echo "== undefined symbols in the mod that are not Qt/libc/NickelHook"
$NM -D --undefined-only "$SO" | awk '{print $2}' | grep -vE '^(_ZN?[0-9]*Q|_ZNK?[0-9]*Q|_Z[A-Za-z]*Q|__|_ITM|_Unwind|dl|pthread|std|_ZSt|_ZdlPv|_Znwj|_ZTV|_ZTI|_ZN9__gnu_cxx|qt_|_ZNSt|_ZN10QArrayData|nh_|[a-z])' || true

echo "== package contents"
tar tzvf "$HERE/KoboRoot.tgz" | tee /tmp/nt-pkg.txt
# NickelHook packages name their files explicitly, so there is no "./" root
# entry (nothing can re-mode "/"). If one ever appears it must be 755 root.
if grep -E ' \./$' /tmp/nt-pkg.txt | grep -vq '^drwxr-xr-x root/root'; then echo "BAD root dir mode/owner"; rc=1; fi
if grep -vq ' root/root ' /tmp/nt-pkg.txt; then echo "BAD owner on some entry"; rc=1; fi
if grep -vE '\./(usr/(local/(Kobo/(imageformats/(libnickeltiles\.so)?)?)?)?|mnt/(onboard/(\.adds/(nickel-tiles/(doc|default|icons/.*)?)?)?)?)?$' /tmp/nt-pkg.txt | grep -q .; then
  echo "UNEXPECTED paths:"; grep -vE '\./(usr/(local/(Kobo/(imageformats/(libnickeltiles\.so)?)?)?)?|mnt/(onboard/(\.adds/(nickel-tiles/(doc|default|icons/.*)?)?)?)?)?$' /tmp/nt-pkg.txt; rc=1
fi
sha256sum "$HERE/KoboRoot.tgz" "$SO"
exit $rc
