include NickelHook/NickelHook.mk

override PKGCONF  += Qt5Widgets
override LIBRARY  := src/libnickeltiles.so
override SOURCES  += src/config.c src/nmentries.cc src/epubcover.cc src/settings.cc src/nickeltiles.cc
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
