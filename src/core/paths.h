#ifndef NT_PATHS_H
#define NT_PATHS_H

// Fixed locations on the reader. NT_CONFIG_DIR (the mod's own folder) comes
// from the Makefile. Macros, so they work with QStringLiteral and in C.

#ifndef NT_ONBOARD   // tests point it at a fixture tree
#define NT_ONBOARD "/mnt/onboard"
#endif
#define NT_NICKELMENU_DIR NT_ONBOARD "/.adds/nm"
#define NT_KOREADER_HISTORY NT_ONBOARD "/.adds/koreader/history.lua"
#define NT_KOREADER_LAUNCHER NT_ONBOARD "/.adds/koreader/koreader.sh"
#define NT_PLATO_LAUNCHER NT_ONBOARD "/.adds/plato/plato.sh"
#define NT_PLATO_METADATA NT_ONBOARD "/.metadata.json"
#define NT_PLATO_STATES NT_ONBOARD "/.reading-states"
#define NT_KOBO_DB NT_ONBOARD "/.kobo/KoboReader.sqlite"
#define NT_SCREENSAVER_DIR NT_ONBOARD "/.kobo/screensaver"
#define NT_PLUGIN_DIR "/usr/local/Kobo/imageformats"
#define NT_NICKEL_HW_STATUS "/tmp/nickel-hardware-status"

#define NT_ICON_DIR NT_CONFIG_DIR "/icons"
#define NT_COVER_CACHE_DIR NT_CONFIG_DIR "/covers"
#define NT_SLEEP_COVER_STAMP NT_CONFIG_DIR "/sleep-cover.src"

#endif
