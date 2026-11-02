#include "books/recent.h"

#include "books/recent_apps.h"
#include "core/util.h"

namespace {

// Keeps `candidate` if it is valid and newer than `best`; the first of equal
// times wins.
void consider(NtRecentBook *best, const NtRecentBook &candidate) {
    NT_DBG("recent candidate %s: valid=%d time=%lld '%s' %d%%", nt_reader_app_name(candidate.app),
           candidate.valid ? 1 : 0, static_cast<long long>(candidate.lastRead), qPrintable(candidate.title),
           candidate.percent);
    if (candidate.valid && (!best->valid || candidate.lastRead > best->lastRead)) *best = candidate;
}

}   // namespace

NtRecentBook nt_recent_book(NtRecentSource source) {
    const bool any = source == NtRecentSource::Any;
    NtRecentBook best;
    if (any || source == NtRecentSource::Koreader) consider(&best, nt_recent_koreader_book());
    if (any || source == NtRecentSource::Plato) consider(&best, nt_recent_plato_book());
    if (any || source == NtRecentSource::Nickel) consider(&best, nt_recent_nickel_book());
    return best;
}

const char *nt_reader_app_name(NtReaderApp app) {
    if (app == NtReaderApp::Koreader) return "koreader";
    if (app == NtReaderApp::Plato) return "plato";
    return "nickel";
}
