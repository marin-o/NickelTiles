#ifndef NT_CRASH_GUARD_H
#define NT_CRASH_GUARD_H

// Turns NickelTiles off when Nickel keeps crashing soon after it starts
// (decision 005). NickelHook's own failsafe only covers the first seconds, so
// a crash on the home page a little later would restart the reader again and
// again. Crashes within kNtSettleMs of a start are counted in NT_CRASH_COUNT;
// a start that lasts longer clears the count. Ordinary exits, such as an app
// launch closing Nickel, are not crashes and don't count.

const int kNtSettleMs = 30000;

// Call at plugin start. While NickelTiles is on and the last few starts all
// crashed early, turns it off (nt_enabled:0) and clears the count.
void nt_crash_guard_start();

// Whether nt_crash_guard_start turned NickelTiles off at this start.
bool nt_crash_guard_tripped();

// Called from the crash handler, so it only uses async-signal-safe calls.
void nt_crash_guard_crashed();

// Nickel has run long enough.
void nt_crash_guard_settled();

#endif
