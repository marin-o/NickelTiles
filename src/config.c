#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "util.h"

static nt_entry_t *entries = NULL, *entries_tail = NULL;
static nt_tile_t *tiles = NULL, *tiles_tail = NULL;
static bool parsed = false;
static bool problem = false;
bool nt_log_verbose = false;

// nt_draw is no longer used; still accepted so older configs load without warnings.
static const char *const known_keys[] = {
    "nt_draw",
    "nt_enabled", "nt_log", "nt_actions", "nt_hide_rows", "nt_recent",
    "nt_columns", "nt_tile_px", "nt_recent_source", "nt_recent_info", "nt_sleep_cover", "hide", "icon", "order", "tile", NULL,
};

bool nt_config_problem_seen(void) {
    return problem;
}

static void append_entry(const char *key, const char *val) {
    nt_entry_t *e = calloc(1, sizeof(*e));
    if (!e || !(e->key = strdup(key)) || !(e->val = strdup(val))) {
        if (e) { free(e->key); free(e->val); free(e); }
        NT_LOG("warning: out of memory parsing config; skipping '%s'", key);
        return;
    }
    if (entries_tail) entries_tail->next = e; else entries = e;
    entries_tail = e;
}

// tile:<label>:<icon>:<command>  (the command may itself contain ':')
static void append_tile(char *val, int lineno) {
    char *cur = val;
    char *label = strsep(&cur, ":");
    char *icon = cur ? strsep(&cur, ":") : NULL;
    char *command = cur;
    label = strtrim(label);
    icon = icon ? strtrim(icon) : NULL;
    command = command ? strtrim(command) : NULL;
    if (!label || !*label || !icon || !command || !*command) {
        problem = true;
        NT_LOG("warning: %s/config: line %d: tile needs <label>:<icon>:<command>", NT_CONFIG_DIR_DISP, lineno);
        return;
    }
    nt_tile_t *t = calloc(1, sizeof(*t));
    if (!t || !(t->label = strdup(label)) || !(t->icon = strdup(icon)) || !(t->command = strdup(command))) {
        if (t) { free(t->label); free(t->icon); free(t->command); free(t); }
        NT_LOG("warning: out of memory parsing tile on line %d", lineno);
        return;
    }
    if (tiles_tail) tiles_tail->next = t; else tiles = t;
    tiles_tail = t;
}

// Seeds NT_CONFIG_DIR/config from NT_CONFIG_DIR/default atomically (temp file,
// fsync, rename) so a power cut cannot leave a truncated config.
static void write_default(void) {
    if (mkdir(NT_CONFIG_DIR, 0755) != 0 && errno != EEXIST) {
        NT_LOG("warning: could not create %s (%s)", NT_CONFIG_DIR_DISP, strerror(errno));
        return;
    }
    FILE *src = fopen(NT_CONFIG_DIR "/default", "r");
    if (!src) {
        NT_LOG("warning: no template at %s/default (%s)", NT_CONFIG_DIR_DISP, strerror(errno));
        return;
    }
    char tmp[512];
    snprintf(tmp, sizeof(tmp), NT_CONFIG_DIR "/config.tmp.%ld", (long)getpid());
    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (fd < 0) {
        NT_LOG("warning: could not write %s/config (%s)", NT_CONFIG_DIR_DISP, strerror(errno));
        fclose(src);
        return;
    }
    FILE *dst = fdopen(fd, "w");
    if (!dst) {
        close(fd); unlink(tmp); fclose(src);
        return;
    }
    bool ok = true;
    char buf[4096];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), src)) > 0)
        if (fwrite(buf, 1, got, dst) != got) { ok = false; break; }
    if (ferror(src)) ok = false;
    fclose(src);
    if (ok && fflush(dst) != 0) ok = false;
    if (ok && fsync(fileno(dst)) != 0) ok = false;
    if (fclose(dst) != 0) ok = false;
    if (!ok || rename(tmp, NT_CONFIG_DIR "/config") != 0) {
        NT_LOG("warning: could not install default config (%s)", strerror(errno));
        unlink(tmp);
        return;
    }
    int dfd = open(NT_CONFIG_DIR, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dfd >= 0) { fsync(dfd); close(dfd); }
    NT_LOG("wrote default config to %s/config", NT_CONFIG_DIR_DISP);
}

static void parse(void) {
    parsed = true;
    FILE *f = fopen(NT_CONFIG_DIR "/config", "r");
    if (!f && errno == ENOENT) {
        NT_LOG("no config at %s/config; writing the default one", NT_CONFIG_DIR_DISP);
        write_default();
        f = fopen(NT_CONFIG_DIR "/config", "r");
    }
    if (!f) {
        NT_LOG("could not open %s/config (%s); no tiles", NT_CONFIG_DIR_DISP, strerror(errno));
        return;
    }
    char *buf = NULL;
    size_t bufsz = 0;
    ssize_t len;
    int lineno = 0;
    while ((len = getline(&buf, &bufsz, f)) != -1) {
        lineno++;
        char *hash = strchr(buf, '#');
        if (hash) *hash = '\0';
        char *line = strtrim(buf);
        if (!*line) continue;
        char *cur = line;
        char *key = strtrim(strsep(&cur, ":"));
        if (!key || !*key || !cur) {
            problem = true;
            NT_LOG("warning: %s/config: line %d: expected key:value", NT_CONFIG_DIR_DISP, lineno);
            continue;
        }
        bool known = false;
        for (size_t i = 0; known_keys[i]; i++)
            if (!strcmp(key, known_keys[i])) { known = true; break; }
        if (!known) {
            problem = true;
            NT_LOG("warning: %s/config: line %d: unknown setting '%s'", NT_CONFIG_DIR_DISP, lineno, key);
            continue;
        }
        if (!strcmp(key, "tile"))
            append_tile(cur, lineno);
        else
            append_entry(key, strtrim(cur));
    }
    free(buf);
    fclose(f);
    nt_log_verbose = nt_config_bool("nt_log", false);
    if (nt_log_verbose || problem) {
        for (nt_entry_t *e = entries; e; e = e->next)
            NT_LOG("config: %s = %s", e->key, e->val);
        for (nt_tile_t *t = tiles; t; t = t->next)
            NT_LOG("config: tile '%s' icon '%s' command '%s'", t->label, t->icon, t->command);
    }
}

const char *nt_config_get(const char *key) {
    if (!parsed) parse();
    for (nt_entry_t *e = entries; e; e = e->next)
        if (!strcmp(e->key, key))
            return e->val;
    return NULL;
}

bool nt_config_bool(const char *key, bool default_value) {
    const char *val = nt_config_get(key);
    if (!val || !*val) return default_value;
    if (!strcmp(val, "1") || !strcasecmp(val, "true") || !strcasecmp(val, "yes") || !strcasecmp(val, "on")) return true;
    if (!strcmp(val, "0") || !strcasecmp(val, "false") || !strcasecmp(val, "no") || !strcasecmp(val, "off")) return false;
    problem = true;
    NT_LOG("warning: invalid boolean for '%s': '%s'; using %d", key, val, default_value ? 1 : 0);
    return default_value;
}

int nt_config_int(const char *key, int default_value) {
    const char *val = nt_config_get(key);
    if (!val || !*val) return default_value;
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (!end || *end) {
        problem = true;
        NT_LOG("warning: invalid integer for '%s': '%s'; using %d", key, val, default_value);
        return default_value;
    }
    return (int)v;
}

const nt_entry_t *nt_config_entries(void) {
    if (!parsed) parse();
    return entries;
}

// True if the (uncommented) line sets `key`: optional blanks, the key, optional
// blanks, then ':' (so "nt_recent" does not match "nt_recent_source").
static bool line_sets(const char *line, const char *key) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(key);
    if (strncmp(p, key, n)) return false;
    p += n;
    while (*p == ' ' || *p == '\t') p++;
    return *p == ':';
}

bool nt_config_set(const char *key, const char *const *vals, int n) {
    if (!parsed) parse();
    FILE *src = fopen(NT_CONFIG_DIR "/config", "r");
    if (!src) {
        NT_LOG("warning: cannot read config to save '%s' (%s)", key, strerror(errno));
        return false;
    }
    char tmp[512];
    snprintf(tmp, sizeof(tmp), NT_CONFIG_DIR "/config.tmp.%ld", (long)getpid());
    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (fd < 0) { fclose(src); return false; }
    FILE *dst = fdopen(fd, "w");
    if (!dst) { close(fd); unlink(tmp); fclose(src); return false; }
    char *buf = NULL;
    size_t bufsz = 0;
    bool ok = true, written = false;
    while (ok && getline(&buf, &bufsz, src) != -1) {
        if (!line_sets(buf, key)) {
            if (fputs(buf, dst) < 0) ok = false;
            continue;
        }
        // The first line setting the key becomes the new value(s); later ones go.
        if (!written)
            for (int i = 0; i < n && ok; i++)
                if (fprintf(dst, "%s:%s\n", key, vals[i]) < 0) ok = false;
        written = true;
    }
    free(buf);
    fclose(src);
    if (ok && !written && n > 0) {
        if (fputs("\n", dst) < 0) ok = false;
        for (int i = 0; i < n && ok; i++)
            if (fprintf(dst, "%s:%s\n", key, vals[i]) < 0) ok = false;
    }
    if (ok && fflush(dst) != 0) ok = false;
    if (ok && fsync(fileno(dst)) != 0) ok = false;
    if (fclose(dst) != 0) ok = false;
    if (!ok || rename(tmp, NT_CONFIG_DIR "/config") != 0) {
        NT_LOG("warning: could not save '%s' (%s)", key, strerror(errno));
        unlink(tmp);
        return false;
    }
    // Update the cached config: drop the key's entries, append the new ones.
    nt_entry_t **link = &entries;
    entries_tail = NULL;
    while (*link) {
        nt_entry_t *e = *link;
        if (!strcmp(e->key, key)) {
            *link = e->next;
            free(e->key); free(e->val); free(e);
        } else {
            entries_tail = e;
            link = &e->next;
        }
    }
    for (int i = 0; i < n; i++) append_entry(key, vals[i]);
    if (!strcmp(key, "nt_log")) nt_log_verbose = nt_config_bool("nt_log", false);
    return true;
}


const nt_tile_t *nt_config_tiles(void) {
    if (!parsed) parse();
    return tiles;
}
