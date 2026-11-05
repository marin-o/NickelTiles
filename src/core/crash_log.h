#ifndef NT_CRASH_LOG_H
#define NT_CRASH_LOG_H

// Writes a line to nickel-tiles.log when Nickel crashes, whatever nt_log says:
// the signal, the code it happened in (NickelTiles, Nickel or another mod) and
// a short backtrace. Then the crash goes on to whatever handled it before.

// Call once at plugin start.
void nt_crash_log_install();

#endif
