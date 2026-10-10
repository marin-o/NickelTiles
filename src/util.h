#ifndef NT_UTIL_H
#define NT_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <NickelHook.h>

#ifndef NH_VERSION
#define NH_VERSION "dev"
#endif

#ifndef NT_LOG_MAX_BYTES
#define NT_LOG_MAX_BYTES (256 * 1024)
#endif

// Verbose logging for this boot: nt_log:1 in the config, or a config problem was seen.
extern bool nt_log_verbose;
bool nt_config_problem_seen(void);

// strtrim trims ASCII whitespace in place (so not on a string literal).
__attribute__((unused)) static inline char *strtrim(char *s) {
    if (!s) return NULL;
    char *a = s, *b = s + strlen(s);
    for (; a < b && isspace((unsigned char)(*a)); a++);
    for (; b > a && isspace((unsigned char)(*(b-1))); b--);
    *b = '\0';
    return a;
}

// nt_log_line writes to syslog (nh_log, readable with `logread`) and to
// NT_CONFIG_DIR/nickel-tiles.log on the user partition, so the log can be read
// over USB without a shell. The file is rotated once per boot past NT_LOG_MAX_BYTES.
__attribute__((unused)) static inline void nt_log_line(const char *file, int line, const char *fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    msg[sizeof(msg) - 1] = '\0';

    nh_log("%s (%s:%d)", msg, file, line);

    mkdir(NT_CONFIG_DIR, 0755);

    static bool rotate_checked = false;
    if (!rotate_checked) {
        rotate_checked = true;
        struct stat st;
        if (stat(NT_CONFIG_DIR "/nickel-tiles.log", &st) == 0 && st.st_size > NT_LOG_MAX_BYTES)
            rename(NT_CONFIG_DIR "/nickel-tiles.log", NT_CONFIG_DIR "/nickel-tiles.log.old");
    }

    FILE *f = fopen(NT_CONFIG_DIR "/nickel-tiles.log", "a");
    if (!f)
        return;
    time_t now = time(NULL);
    struct tm tmbuf;
    struct tm *tm = localtime_r(&now, &tmbuf);
    if (tm)
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec);
    fprintf(f, "NickelTiles " NH_VERSION ": %s (%s:%d)\n", msg, file, line);
    fclose(f);
}

#define NT_LOG(fmt, ...) nt_log_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NT_DBG(fmt, ...) do { \
        if (nt_log_verbose || nt_config_problem_seen()) \
            NT_LOG(fmt, ##__VA_ARGS__); \
    } while (0)

// Logs the firmware field of .kobo/version (serial deliberately dropped).
__attribute__((unused)) static inline void nt_log_firmware(void) {
    FILE *f = fopen("/mnt/onboard/.kobo/version", "r");
    if (!f) {
        NT_LOG("startup: firmware version unavailable");
        return;
    }
    char vline[512];
    char *got = fgets(vline, sizeof(vline), f);
    fclose(f);
    if (!got)
        return;
    vline[strcspn(vline, "\r\n")] = '\0';
    const char *comma = strchr(vline, ',');
    NT_LOG("startup: firmware %s", comma ? comma + 1 : vline);
}

#ifdef __cplusplus
}
#endif
#endif
