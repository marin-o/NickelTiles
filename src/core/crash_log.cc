#include "core/crash_log.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <ucontext.h>
#include <unistd.h>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "core/crash_guard.h"
#include "core/paths.h"
#include "core/util.h"

namespace {

const int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
const int kMaxFrames = 16;
struct sigaction g_previous[NSIG];

void writeText(int fd, const char *text) {
    if (write(fd, text, strlen(text)) < 0) return;
}

// "<what> 0x... in /path/lib.so (symbol+0x..)", as far as dladdr knows.
void writeAddress(int fd, const char *what, const void *address) {
    char line[320];
    Dl_info info;
    if (address && dladdr(address, &info) && info.dli_fname) {
        const void *base = info.dli_saddr ? info.dli_saddr : info.dli_fbase;
        const uintptr_t offset = reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(base);
        snprintf(line, sizeof(line), "  %s %p in %s (%s+0x%lx)\n", what, address, info.dli_fname,
                 info.dli_sname ? info.dli_sname : "?", static_cast<unsigned long>(offset));
    } else {
        snprintf(line, sizeof(line), "  %s %p\n", what, address);
    }
    writeText(fd, line);
}

void writeHeader(int fd, int sig, const siginfo_t *info) {
    char stamp[32] = "";
    const time_t now = time(nullptr);
    struct tm tm;
    if (localtime_r(&now, &tm)) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S ", &tm);
    char line[200];
    snprintf(line, sizeof(line), "%sNickelTiles " NH_VERSION ": crash: Nickel got signal %d, fault address %p\n", stamp,
             sig, info ? info->si_addr : nullptr);
    writeText(fd, line);
}

void onCrash(int sig, siginfo_t *info, void *context) {
    const int fd = open(NT_CONFIG_DIR "/nickel-tiles.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd >= 0) {
        writeHeader(fd, sig, info);
#ifdef __arm__
        const mcontext_t &regs = static_cast<ucontext_t *>(context)->uc_mcontext;
        writeAddress(fd, "at", reinterpret_cast<const void *>(regs.arm_pc));
        writeAddress(fd, "called from", reinterpret_cast<const void *>(regs.arm_lr));
#else
        (void)context;
#endif
        void *frames[kMaxFrames];
        const int count = backtrace(frames, kMaxFrames);
        // Frame 0 is this handler, which would make every crash look like NickelTiles'.
        for (int i = 1; i < count; i++) writeAddress(fd, "frame", frames[i]);
        fsync(fd);   // the reader may restart right after
        close(fd);
    }
    nt_crash_guard_crashed();
    sigaction(sig, &g_previous[sig], nullptr);
    raise(sig);
}

}   // namespace

void nt_crash_log_install() {
    // backtrace loads libgcc_s on first use, which is not safe inside a signal handler.
    void *warmUp[1];
    backtrace(warmUp, 1);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = onCrash;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    for (const int sig : kSignals) sigaction(sig, &action, &g_previous[sig]);
}
