include NickelHook/NickelHook.mk

override PKGCONF  += Qt5Widgets
# Objects and the library go to build/ (tools/patch-nickelhook.sh adds BUILDDIR).
override BUILDDIR := build/
override LIBRARY  := build/libnickeltiles.so
override SOURCES  += src/plugin.cc \
	src/core/util.c src/core/config.c src/core/settings_model.cc \
	src/kobo/nickel.cc src/kobo/actions.cc src/kobo/nm_parse.cc src/kobo/addons.cc \
	src/books/zip.cc src/books/epub.cc src/books/fb2.cc src/books/mobi.cc src/books/cbz.cc \
	src/books/cover.cc src/books/recent.cc src/books/sleepcover.cc \
	src/ui/ui.cc src/ui/pressable.cc src/ui/layout.cc src/ui/tiles.cc src/ui/grid.cc src/ui/settings_page.cc
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -fvisibility=hidden -fvisibility-inlines-hidden
override KOBOROOT += res/doc:$(NT_CONFIG_DIR)/doc res/default:$(NT_CONFIG_DIR)/default res/icons:$(NT_CONFIG_DIR)/icons

override SKIPCONFIGURE += strip
strip:
	$(STRIP) --strip-unneeded $(LIBRARY)
.PHONY: strip

ifeq ($(NT_CONFIG_DIR),)
override NT_CONFIG_DIR := /mnt/onboard/.adds/nickel-tiles
endif

override CPPFLAGS += -Isrc -DNT_CONFIG_DIR='"$(NT_CONFIG_DIR)"' -DNT_CONFIG_DIR_DISP='"$(patsubst /mnt/onboard/%,KOBOeReader/%,$(NT_CONFIG_DIR))"'

include NickelHook/NickelHook.mk

# Stricter warnings for our own code only (not NickelHook or Qt). -Wshadow is
# C only: GCC 4.9 also flags Qt-style parameters named parent, event or width.
NT_WARN := -Wcast-qual -Wformat=2 -Wlogical-op -Wmissing-declarations -Wredundant-decls -Wundef
# Qt and NickelHook headers count as system headers there, so they are not judged.
build/src/%.o: override CPPFLAGS += -isystem NickelHook $(patsubst -I%,-isystem %,$(filter -I%,$(QT5WIDGETS_CFLAGS)))
build/src/%.o: override CFLAGS   += $(NT_WARN) -Wshadow -Wstrict-prototypes -Wmissing-prototypes
build/src/%.o: override CXXFLAGS += $(NT_WARN) -Wold-style-cast -Wuseless-cast -Wnon-virtual-dtor -Woverloaded-virtual

# make clean removes the objects; this removes the folders they were in.
clean: nt-clean-build
nt-clean-build:
	rm -rf build
.PHONY: nt-clean-build
