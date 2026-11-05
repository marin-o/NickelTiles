#include "core/crash_guard.h"

#include <fcntl.h>
#include <unistd.h>

#include "core/paths.h"
#include "core/settings_model.h"
#include "core/util.h"

namespace {

const int kCrashLimit = 3;
volatile bool g_settled = false;
bool g_tripped = false;

// Plain read/write rather than stdio or Qt: nt_crash_guard_crashed runs inside
// a signal handler.
int readCount() {
    const int fd = open(NT_CRASH_COUNT, O_RDONLY);
    if (fd < 0) return 0;
    char buf[8] = {};
    const ssize_t got = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    int count = 0;
    for (ssize_t i = 0; i < got && buf[i] >= '0' && buf[i] <= '9'; i++) count = count * 10 + (buf[i] - '0');
    return count;
}

void writeCount(int count) {
    const int fd = open(NT_CRASH_COUNT, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    const char text[] = {static_cast<char>('0' + (count > 9 ? 9 : count)), '\n'};
    if (write(fd, text, sizeof(text)) == static_cast<ssize_t>(sizeof(text))) fsync(fd);
    close(fd);
}

}   // namespace

void nt_crash_guard_start() {
    const NtSettings before = nt_settings();
    if (!before.enabled) return;
    const int crashes = readCount();
    if (crashes > 0)
        NT_LOG("crash guard: Nickel crashed within %d s of starting, %d time(s) in a row", kNtSettleMs / 1000, crashes);
    if (crashes < kCrashLimit) return;
    NT_LOG("crash guard: turning NickelTiles off after %d early crashes in a row", crashes);
    writeCount(0);
    NtSettings off = before;
    off.enabled = false;
    nt_settings_save(before, off);
    g_tripped = true;
}

bool nt_crash_guard_tripped() {
    return g_tripped;
}

void nt_crash_guard_crashed() {
    if (!g_settled) writeCount(readCount() + 1);
}

void nt_crash_guard_settled() {
    if (g_settled) return;
    g_settled = true;
    writeCount(0);
}
