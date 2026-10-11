#ifndef NT_TEST_NICKELHOOK_H
#define NT_TEST_NICKELHOOK_H

// TEST STUB for NickelHook/NickelHook.h. The real header describes a mod to
// NickelHook's loader, which does not run on the host. The modules under test
// only use nh_log, struct nh_dlsym and nh_symoutptr, so that is all this has.

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define nh_symoutptr(sym) (void**)(&sym)

struct nh_dlsym {
    const char *name;
    void **out;
    const char *desc;
    bool optional;
};

// Logs to stderr, so a failing test shows the log lines next to QTest's output.
static inline void nh_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

#endif
