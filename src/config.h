#ifndef NT_CONFIG_H
#define NT_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#if !(defined(NT_CONFIG_DIR) && defined(NT_CONFIG_DIR_DISP))
#error "NT_CONFIG_DIR not set (the Makefile sets it)"
#endif

// Config file: NT_CONFIG_DIR/config. One `key:value` per line, `#` comments.
// Keys:
//   nt_enabled:0|1   master switch (default 1)
//   nt_log:0|1       verbose log (default 0)
//   nt_draw:0|1      insert the tile row into the home screen (default 0 in the
//                    skeleton: Phase 2 turns it on after the logging-only boot)
//   nt_actions:0|1   run a tile's command on tap (default 0 until Phase 3)
//   nt_hide_rows:0|1 hide Kobo's own home rows, the grid is the home (default 1)
//   nt_recent:0|1    "Continue reading" tile for the current read (default 1)
//   nt_columns:N     grid columns (default 5)
//   nt_tile_px:N     tile width in pixels (default 180)
//   hide:<label>     do not show this NickelMenu entry as a tile
//   icon:<label>:<png path>   icon override for a label
//   order:<label>|<label>|... display order (written by arrange mode)
//   tile:<label>:<icon path>:<command>   extra tile not in NickelMenu
// If the file is missing it is seeded from NT_CONFIG_DIR/default.

typedef struct nt_tile_t {
    char *label;
    char *icon;
    char *command;
    struct nt_tile_t *next;
} nt_tile_t;

typedef struct nt_entry_t {
    char *key;
    char *val;
    struct nt_entry_t *next;
} nt_entry_t;

bool nt_config_problem_seen(void);

// Parses the config on first use and caches it. Never returns NULL for the
// accessors; an unreadable config behaves as "no tiles, defaults".
const char *nt_config_get(const char *key);
bool nt_config_bool(const char *key, bool default_value);
int nt_config_int(const char *key, int default_value);
const nt_entry_t *nt_config_entries(void);   // all key:value lines in file order (not tile: lines)
const nt_tile_t *nt_config_tiles(void);
int nt_config_tile_count(void);

// Sets `key` to the n values (n > 1 for repeatable keys such as hide; n == 0
// removes it). The first line that set the key is replaced in place, the
// others dropped; a new key is appended. Comments and other lines are kept.
// Atomic (temp file, fsync, rename); the cached config is updated too.
bool nt_config_set(const char *key, const char *const *vals, int n);

// nt_config_set("order", &order, 1): the arrange-mode order.
bool nt_config_write_order(const char *order);

#ifdef __cplusplus
}
#endif
#endif
