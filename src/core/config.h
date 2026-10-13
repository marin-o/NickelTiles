#ifndef NT_CONFIG_H
#define NT_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#if !(defined(NT_CONFIG_DIR) && defined(NT_CONFIG_DIR_DISP))
#error "NT_CONFIG_DIR not set (the Makefile sets it)"
#endif

// The config file, NT_CONFIG_DIR/config: one `key:value` per line, `#` starts
// a comment. If the file is missing it is seeded from NT_CONFIG_DIR/default.
// This is the raw key/value layer; the settings themselves (names, defaults,
// meaning) are in settings_model.h. Besides those keys:
//   nt_log:0|1                            verbose log and debug screenshots
//   icon:<label>:<png path>               icon override (repeatable)
//   tile:<label>:<icon path>:<command>    extra tile (repeatable)

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

// The file is parsed on first use and cached; an unreadable file behaves as
// an empty one. nt_config_get returns the first value of `key`, or NULL.
const char *nt_config_get(const char *key);
bool nt_config_bool(const char *key, bool default_value);
int nt_config_int(const char *key, int default_value);
const nt_entry_t *nt_config_entries(void);   // every key:value line in file order, except tile:
const nt_tile_t *nt_config_tiles(void);

// Sets `key` to the n values (n > 1 for repeatable keys such as hide; n == 0
// removes it). The first line that set the key is replaced in place, the
// others dropped; a new key is appended. Comments and other lines are kept.
// Atomic (temp file, fsync, rename); the cached config is updated too.
bool nt_config_set(const char *key, const char *const *vals, int n);

// Drops the cached config (entries and tiles) so the next access re-reads the
// file. Only the host tests need it, after they write a fixture config.
void nt_config_reload(void);

#ifdef __cplusplus
}
#endif
#endif
