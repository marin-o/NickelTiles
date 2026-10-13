#!/bin/sh
# Two small, idempotent patches to the NickelHook submodule. Run from the
# repository root before make (tools/build.sh and CI do).
#
# Ui_HomePageView::setupUi is a weakly bound (inline, uic-generated) symbol.
# Upstream NickelHook only patches STB_GLOBAL jump slots; a weak jump slot is
# hooked the same way, so allow it.
set -eu
if ! grep -q 'STB_WEAK' NickelHook/nh.c; then
  sed -i 's/ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL, "not a globally bound symbol/(ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL || ELFW(ST_BIND)(sym->st_info) == STB_WEAK), "not a globally or weakly bound symbol/' NickelHook/nh.c
  grep -q 'STB_WEAK' NickelHook/nh.c || { echo "NickelHook weak-symbol patch did not apply" >&2; exit 1; }
  echo "-- patched NickelHook/nh.c to accept STB_WEAK jump slots"
fi

# Object files in a build folder: NickelHook.mk puts each object next to its
# source. With BUILDDIR set (the Makefile sets build/), objects go under it
# instead; unset, NickelHook behaves as upstream.
MK=NickelHook/NickelHook.mk
if ! grep -q 'BUILDDIR' "$MK"; then
  sed -i \
    -e 's|^override OBJECTS_C    := \$(filter %.o,\$(SOURCES:%.c=%.o))|override OBJECTS_C    := $(addprefix $(BUILDDIR),$(filter %.o,$(SOURCES:%.c=%.o)))|' \
    -e 's|^override OBJECTS_CXX  := \$(filter %.o,\$(SOURCES:%.cc=%.o))|override OBJECTS_CXX  := $(addprefix $(BUILDDIR),$(filter %.o,$(SOURCES:%.cc=%.o)))|' \
    -e 's|^\$(OBJECTS_C): %.o: %.c|$(OBJECTS_C): $(BUILDDIR)%.o: %.c|' \
    -e 's|^\$(OBJECTS_CXX): %.o: %.cc|$(OBJECTS_CXX): $(BUILDDIR)%.o: %.cc|' \
    -e 's|^override nh_cmd_c     = |override nh_cmd_c     = mkdir -p $(dir $(1)) \&\& |' \
    -e 's|^override nh_cmd_cc    = |override nh_cmd_cc    = mkdir -p $(dir $(1)) \&\& |' \
    "$MK"
  [ "$(grep -c 'BUILDDIR' "$MK")" -eq 4 ] || { echo "NickelHook build-folder patch did not apply" >&2; exit 1; }
  grep -q 'nh_cmd_cc    = mkdir' "$MK" || { echo "NickelHook build-folder patch did not apply" >&2; exit 1; }
  echo "-- patched NickelHook/NickelHook.mk to build objects under BUILDDIR"
fi
