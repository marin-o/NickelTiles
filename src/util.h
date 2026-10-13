#ifndef NT_UTIL_H
#define NT_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "config.h"

#ifndef NH_VERSION
#define NH_VERSION "dev"
#endif

// Verbose logging for this boot: nt_log:1 in the config, or a config problem was seen.
extern bool nt_log_verbose;

// Trims ASCII whitespace in place (so not on a string literal).
char *strtrim(char *s);

// Writes to syslog (nh_log, readable with `logread`) and to
// NT_CONFIG_DIR/nickel-tiles.log on the user partition, so the log can be read
// over USB without a shell. The file is rotated once per boot past NT_LOG_MAX_BYTES.
void nt_log_line(const char *file, int line, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#define NT_LOG(fmt, ...) nt_log_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NT_DBG(fmt, ...)                                                                                               \
    do {                                                                                                               \
        if (nt_log_verbose || nt_config_problem_seen()) NT_LOG(fmt, ##__VA_ARGS__);                                    \
    } while (0)

// Logs the firmware field of .kobo/version (serial deliberately dropped).
void nt_log_firmware(void);

#ifdef __cplusplus
}
#endif
#endif
