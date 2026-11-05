#include "books/recent.h"

#include <QSet>

#include <algorithm>

#include "books/recent_apps.h"
#include "core/util.h"

QList<NtRecentBook> nt_recent_books(NtRecentSource source, int count) {
    const bool any = source == NtRecentSource::Any;
    QList<NtRecentBook> all;
    if (any || source == NtRecentSource::Koreader) all << nt_recent_koreader_books(count);
    if (any || source == NtRecentSource::Plato) all << nt_recent_plato_books(count);
    if (any || source == NtRecentSource::Nickel) all << nt_recent_nickel_books(count);
    // Stable, so among equal times the earlier app above wins.
    std::stable_sort(all.begin(), all.end(),
                     [](const NtRecentBook &a, const NtRecentBook &b) { return a.lastRead > b.lastRead; });

    QList<NtRecentBook> books;
    QSet<QString> paths;
    for (const NtRecentBook &book : all) {
        NT_DBG("recent candidate %s: time=%lld '%s' %d%%", nt_reader_app_name(book.app),
               static_cast<long long>(book.lastRead), qPrintable(book.title), book.percent);
        if (books.size() < count && !paths.contains(book.path)) {
            paths.insert(book.path);
            books << book;
        }
    }
    return books;
}

NtRecentBook nt_recent_book(NtRecentSource source) {
    const QList<NtRecentBook> books = nt_recent_books(source, 1);
    return books.isEmpty() ? NtRecentBook() : books.first();
}

const char *nt_reader_app_name(NtReaderApp app) {
    if (app == NtReaderApp::Koreader) return "koreader";
    if (app == NtReaderApp::Plato) return "plato";
    return "nickel";
}
