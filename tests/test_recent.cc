// The most recently read book (recent.cc) from KOReader and Plato, on a fixture
// tree under /tmp/nt-tests/onboard. The Nickel source is not tested here: it
// reads KoboReader.sqlite through the sqlite3 that Nickel loads, which this host
// does not have.

#include <QtTest>

#include <QDate>
#include <QDateTime>

#include "fixtures.h"
#include "paths.h"
#include "recent.h"
#include "stubs.h"

namespace {

const qint64 kKoreaderTime = 1700000000;   // 2023-11-14, older than the Plato fixture

QString bookPath(const QString &name) {
    return QStringLiteral(NT_ONBOARD "/books/") + name;
}

// KOReader's history.lua: the newest entry comes first.
void writeKoreaderHistory(const QString &file, qint64 time) {
    const QByteArray text = "return {\n [1] = {\n  [\"file\"] = \"" + file.toUtf8()
        + "\",\n  [\"time\"] = " + QByteArray::number(time) + ",\n },\n}\n";
    QVERIFY(writeFile(QStringLiteral(NT_KOREADER_HISTORY), text));
}

// Plato's index maps the book's fingerprint to its file and metadata; the state
// file named after the fingerprint holds when it was last opened.
void writePlatoBook() {
    QVERIFY(writeFile(QStringLiteral(NT_ONBOARD "/books/b.epub"), QByteArray("book")));
    QVERIFY(writeFile(QStringLiteral(NT_PLATO_METADATA),
        QByteArray("{\"abc123\": {\"file\": {\"path\": \"books/b.epub\"}, "
                   "\"title\": \"Plato Book\", \"author\": \"Ann Author\"}}")));
    QVERIFY(writeFile(QStringLiteral(NT_PLATO_STATES) + QStringLiteral("/abc123.json"),
        QByteArray("{\"opened\": \"2026-10-01 12:00:00\", \"currentPage\": 30, \"pagesCount\": 120}")));
}

// Plato writes local time without a zone, so the expectation is local too.
qint64 platoOpenedTime() {
    return QDateTime(QDate(2026, 10, 1), QTime(12, 0, 0)).toSecsSinceEpoch();
}

}  // namespace

class TestRecent : public QObject {
    Q_OBJECT

private slots:
    void init() {
        removeFixtures();
        nt_stub_toasts.clear();
        nt_stub_commands.clear();
        nt_stub_content_ids.clear();
    }
    void cleanup() { removeFixtures(); }

    void koreaderBookWithSidecar();
    void koreaderTitleFromFileName();
    void koreaderBookMissingIsInvalid();
    void platoBook();
    void anyPicksNewestApp();
    void anySourceOnlyReadsThatApp();
    void openKoreaderBookRunsLauncher();
    void openNothingToasts();
};

void TestRecent::koreaderBookWithSidecar() {
    const QString file = bookPath(QStringLiteral("a.epub"));
    QVERIFY(writeFile(file, QByteArray("book")));
    QVERIFY(writeFile(bookPath(QStringLiteral("a.sdr/metadata.epub.lua")),
        QByteArray("return {\n [\"doc_props\"] = {\n  [\"title\"] = \"Test Book\",\n"
                   "  [\"authors\"] = \"Jane Doe\",\n },\n [\"percent_finished\"] = 0.42,\n}\n")));
    writeKoreaderHistory(file, kKoreaderTime);

    const NtRecentBook book = nt_recent_book(NtRecentSource::Koreader);
    QVERIFY(book.valid);
    QVERIFY(book.app == NtReaderApp::Koreader);
    QCOMPARE(book.path, file);
    QCOMPARE(book.title, QStringLiteral("Test Book"));
    QCOMPARE(book.author, QStringLiteral("Jane Doe"));
    QCOMPARE(book.percent, 42);
    QCOMPARE(book.lastRead, kKoreaderTime);
}

void TestRecent::koreaderTitleFromFileName() {
    // No sidecar: the title is the file name without its extension.
    const QString file = bookPath(QStringLiteral("Plain Name.epub"));
    QVERIFY(writeFile(file, QByteArray("book")));
    writeKoreaderHistory(file, kKoreaderTime);

    const NtRecentBook book = nt_recent_book(NtRecentSource::Koreader);
    QVERIFY(book.valid);
    QCOMPARE(book.title, QStringLiteral("Plain Name"));
    QVERIFY(book.author.isEmpty());
    QCOMPARE(book.percent, -1);
}

void TestRecent::koreaderBookMissingIsInvalid() {
    // The history names a book that is no longer on the reader.
    writeKoreaderHistory(bookPath(QStringLiteral("gone.epub")), kKoreaderTime);
    QVERIFY(!nt_recent_book(NtRecentSource::Koreader).valid);
}

void TestRecent::platoBook() {
    writePlatoBook();
    const NtRecentBook book = nt_recent_book(NtRecentSource::Plato);
    QVERIFY(book.valid);
    QVERIFY(book.app == NtReaderApp::Plato);
    QCOMPARE(book.path, bookPath(QStringLiteral("b.epub")));
    QCOMPARE(book.title, QStringLiteral("Plato Book"));
    QCOMPARE(book.author, QStringLiteral("Ann Author"));
    QCOMPARE(book.percent, 25);
    QCOMPARE(book.lastRead, platoOpenedTime());
}

void TestRecent::anyPicksNewestApp() {
    // KOReader's time is 2023 and Plato's is 2026, so Plato is the most recent.
    const QString file = bookPath(QStringLiteral("a.epub"));
    QVERIFY(writeFile(file, QByteArray("book")));
    writeKoreaderHistory(file, kKoreaderTime);
    writePlatoBook();

    const NtRecentBook book = nt_recent_book(NtRecentSource::Any);
    QVERIFY(book.valid);
    QVERIFY(book.app == NtReaderApp::Plato);
    QCOMPARE(book.title, QStringLiteral("Plato Book"));
}

void TestRecent::anySourceOnlyReadsThatApp() {
    const QString file = bookPath(QStringLiteral("a.epub"));
    QVERIFY(writeFile(file, QByteArray("book")));
    writeKoreaderHistory(file, kKoreaderTime);
    writePlatoBook();

    const NtRecentBook book = nt_recent_book(NtRecentSource::Koreader);
    QVERIFY(book.valid);
    QVERIFY(book.app == NtReaderApp::Koreader);
}

void TestRecent::openKoreaderBookRunsLauncher() {
    const QString file = bookPath(QStringLiteral("a.epub"));
    QVERIFY(writeFile(file, QByteArray("book")));
    writeKoreaderHistory(file, kKoreaderTime);

    nt_open_recent_book(nt_recent_book(NtRecentSource::Koreader));
    QCOMPARE(nt_stub_commands.size(), 1);
    QCOMPARE(nt_stub_commands.at(0), QStringLiteral("exec " NT_KOREADER_LAUNCHER " '") + file + QStringLiteral("'"));
    QVERIFY(nt_stub_toasts.isEmpty());
}

void TestRecent::openNothingToasts() {
    nt_open_recent_book(nt_recent_book(NtRecentSource::Koreader));
    QCOMPARE(nt_stub_toasts, QStringList(QStringLiteral("No recent book")));
    QVERIFY(nt_stub_commands.isEmpty());
}

QTEST_MAIN(TestRecent)
#include "test_recent.moc"
