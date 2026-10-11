// The raw config file (config.c) and the settings built on it (settings_model.cc).
// Each test writes its own config under /tmp/nt-tests/config and reloads it,
// because config.c caches what it read.

#include <QtTest>

#include <QFile>
#include <QStringList>

#include "config.h"
#include "fixtures.h"
#include "settings_model.h"

namespace {

QString configPath() {
    return QStringLiteral(NT_CONFIG_DIR "/config");
}

void setConfig(const QByteArray &text) {
    QVERIFY(writeFile(configPath(), text));
    nt_config_reload();
}

// An unreadable file reads as empty, so the comparing test fails rather than this helper.
QString readConfig() {
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll());
}

void compareSettings(const NtSettings &got, const NtSettings &want) {
    QCOMPARE(got.enabled, want.enabled);
    QCOMPARE(got.actions, want.actions);
    QCOMPARE(got.hideRows, want.hideRows);
    QCOMPARE(got.recent, want.recent);
    QVERIFY(got.recentSource == want.recentSource);
    QVERIFY(got.recentInfo == want.recentInfo);
    QCOMPARE(got.recentCover, want.recentCover);
    QCOMPARE(got.sleepCover, want.sleepCover);
    QCOMPARE(got.columns, want.columns);
    QCOMPARE(got.tilePx, want.tilePx);
    QCOMPARE(got.hidden, want.hidden);
    QCOMPARE(got.order, want.order);
}

}  // namespace

class TestConfig : public QObject {
    Q_OBJECT

private slots:
    void init() { removeFixtures(); }
    void cleanup() { removeFixtures(); }

    void defaultsWhenEmpty();
    void tilePx_data();
    void tilePx();
    void recentInfoFlags();
    void recentCoverFlag();
    void hideLines();
    void orderSplitsOnBar();
    void saveReplacesChangedKeysInPlace();
    void saveAppendsMissingKey();
    void saveEmptiedOrderRemovesLine();
    void saveRoundTrip();
};

void TestConfig::defaultsWhenEmpty() {
    setConfig(QByteArray());
    compareSettings(nt_settings(), NtSettings());
}

void TestConfig::tilePx_data() {
    QTest::addColumn<QByteArray>("config");
    QTest::addColumn<int>("px");
    QTest::newRow("missing") << QByteArray("nt_columns:4\n") << 0;
    QTest::newRow("auto") << QByteArray("nt_tile_px:auto\n") << 0;
    QTest::newRow("invalid") << QByteArray("nt_tile_px:big\n") << 0;
    QTest::newRow("negative") << QByteArray("nt_tile_px:-40\n") << 0;
    QTest::newRow("number") << QByteArray("nt_tile_px:180\n") << 180;
}

void TestConfig::tilePx() {
    QFETCH(QByteArray, config);
    QFETCH(int, px);
    setConfig(config);
    QCOMPARE(nt_settings().tilePx, px);
}

void TestConfig::recentInfoFlags() {
    setConfig("nt_recent_info:none\n");
    NtRecentInfo none = nt_settings().recentInfo;
    QVERIFY(!none.title && !none.author && !none.progress);

    setConfig("nt_recent_info:title,progress\n");
    NtRecentInfo some = nt_settings().recentInfo;
    QVERIFY(some.title);
    QVERIFY(!some.author);
    QVERIFY(some.progress);
}

void TestConfig::recentCoverFlag() {
    setConfig("nt_recent_cover:0\n");
    QVERIFY(!nt_settings().recentCover);
    setConfig("nt_sleep_cover:0\n");
    QVERIFY(!nt_settings().sleepCover);
}

void TestConfig::hideLines() {
    setConfig("hide:kpm - Update all\nhide:kpm - Status\n");
    QStringList want;
    want << QStringLiteral("kpm - Update all") << QStringLiteral("kpm - Status");
    QCOMPARE(nt_settings().hidden, want);
}

void TestConfig::orderSplitsOnBar() {
    // Blank entries and surrounding spaces are dropped.
    setConfig("order:Continue reading| KOReader |Plato|| Cobalt\n");
    QStringList want;
    want << QStringLiteral("Continue reading") << QStringLiteral("KOReader")
         << QStringLiteral("Plato") << QStringLiteral("Cobalt");
    QCOMPARE(nt_settings().order, want);
}

void TestConfig::saveReplacesChangedKeysInPlace() {
    setConfig("# my comment\nnt_columns:5\nnt_log:0\nnt_recent_cover:1\n");
    const NtSettings before = nt_settings();
    NtSettings after = before;
    after.columns = 4;
    after.recentCover = false;
    QVERIFY(nt_settings_save(before, after));
    // Only the two changed lines differ; the comment and nt_log stay as they were.
    QCOMPARE(readConfig(), QStringLiteral("# my comment\nnt_columns:4\nnt_log:0\nnt_recent_cover:0\n"));
}

void TestConfig::saveAppendsMissingKey() {
    // config.c appends a key that is not in the file yet.
    setConfig("# my comment\nnt_columns:5\n");
    const NtSettings before = nt_settings();
    NtSettings after = before;
    after.recentCover = false;
    QVERIFY(nt_settings_save(before, after));
    const QString text = readConfig();
    QVERIFY(text.startsWith(QStringLiteral("# my comment\nnt_columns:5\n")));
    QVERIFY(text.contains(QStringLiteral("\nnt_recent_cover:0\n")));
}

void TestConfig::saveEmptiedOrderRemovesLine() {
    setConfig("# keep\norder:a|b\nnt_columns:5\n");
    const NtSettings before = nt_settings();
    NtSettings after = before;
    after.order.clear();
    QVERIFY(nt_settings_save(before, after));
    QCOMPARE(readConfig(), QStringLiteral("# keep\nnt_columns:5\n"));
}

void TestConfig::saveRoundTrip() {
    setConfig(QByteArray());
    const NtSettings before = nt_settings();

    NtSettings want;
    want.enabled = false;
    want.actions = false;
    want.hideRows = false;
    want.recent = false;
    want.recentSource = NtRecentSource::Plato;
    want.recentInfo.author = false;
    want.recentCover = false;
    want.sleepCover = false;
    want.columns = 3;
    want.tilePx = 180;
    want.hidden << QStringLiteral("kpm - Status") << QStringLiteral("Other");
    want.order << QStringLiteral("Plato") << QStringLiteral("KOReader");

    QVERIFY(nt_settings_save(before, want));
    nt_config_reload();
    compareSettings(nt_settings(), want);
}

QTEST_MAIN(TestConfig)
#include "test_config.moc"
