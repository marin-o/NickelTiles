include NickelHook/NickelHook.mk

override PKGCONF  += Qt5Widgets
override LIBRARY  := src/libnickeltiles.so
override SOURCES  += src/util.c src/config.c src/nickel.cc src/actions.cc src/nm_parse.cc src/zip.cc src/epub.cc src/settings_model.cc src/recent.cc src/sleepcover.cc src/ui.cc src/pressable.cc src/layout.cc src/tiles.cc src/grid.cc src/settings_page.cc src/addons.cc src/plugin.cc
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -fvisibility=hidden -fvisibility-inlines-hidden
override KOBOROOT += res/doc:$(NT_CONFIG_DIR)/doc res/default:$(NT_CONFIG_DIR)/default res/icons:$(NT_CONFIG_DIR)/icons

override SKIPCONFIGURE += strip
strip:
	$(STRIP) --strip-unneeded src/libnickeltiles.so
.PHONY: strip

ifeq ($(NT_CONFIG_DIR),)
override NT_CONFIG_DIR := /mnt/onboard/.adds/nickel-tiles
endif

override CPPFLAGS += -DNT_CONFIG_DIR='"$(NT_CONFIG_DIR)"' -DNT_CONFIG_DIR_DISP='"$(patsubst /mnt/onboard/%,KOBOeReader/%,$(NT_CONFIG_DIR))"'

include NickelHook/NickelHook.mk

# Stricter warnings for our own code only (not NickelHook or Qt). -Wshadow is
# C only: GCC 4.9 also flags Qt-style parameters named parent, event or width.
NT_WARN := -Wcast-qual -Wformat=2 -Wlogical-op -Wmissing-declarations -Wredundant-decls -Wundef
# Qt and NickelHook headers count as system headers there, so they are not judged.
src/%.o: override CPPFLAGS += -isystem NickelHook $(patsubst -I%,-isystem %,$(filter -I%,$(QT5WIDGETS_CFLAGS)))
src/%.o: override CFLAGS   += $(NT_WARN) -Wshadow -Wstrict-prototypes -Wmissing-prototypes
src/%.o: override CXXFLAGS += $(NT_WARN) -Wold-style-cast -Wuseless-cast -Wnon-virtual-dtor -Woverloaded-virtual
