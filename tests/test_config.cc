// Config file and settings (config.c, settings_model.cc): tile size, recent-book info, in-place saves.

#include <QtTest>

#include <QFile>

#include "core/config.h"
#include "fixtures.h"
#include "core/settings_model.h"

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

}   // namespace

class TestConfig : public QObject {
    Q_OBJECT

private slots:
    void init() { removeFixtures(); }
    void cleanup() { removeFixtures(); }

    void tilePx_data();
    void tilePx();
    void recentInfoFlags();
    void saveReplacesChangedKeysInPlace();
    void saveEmptiedOrderRemovesLine();
    void saveRoundTrip();
};

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
