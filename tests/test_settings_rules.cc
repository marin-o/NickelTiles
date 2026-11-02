// The settings page's size limits (settings_rules.h): a wrong one offers a grid
// that does not fit the screen.

#include <QtTest>

#include "ui/settings_rules.h"

class TestSettingsRules : public QObject {
    Q_OBJECT

private slots:
    void sizeLimits();
};

void TestSettingsRules::sizeLimits() {
    NtSettings settings;
    settings.columns = 3;
    settings.tilePx = 300;
    NtSizeLimits limits = nt_size_limits(settings, 1072);
    QVERIFY(limits.canRemoveColumn);
    QVERIFY(!limits.canAddColumn);   // four 300 px tiles are wider than the screen
    QVERIFY(!limits.canGrowTile);    // 300 px is the largest size
    QVERIFY(limits.canShrinkTile);

    settings.columns = kNtMinColumns;
    QVERIFY(!nt_size_limits(settings, 1072).canRemoveColumn);

    // Leaving automatic (192 px at five columns) lands on the 20 px grid.
    settings.columns = 5;
    settings.tilePx = 0;
    QCOMPARE(nt_stepped_tile_size(settings, 20, 1072), 200);
}

QTEST_MAIN(TestSettingsRules)
#include "test_settings_rules.moc"
