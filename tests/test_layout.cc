// Tile size and grid fit arithmetic (layout.h). Pure functions: no fixtures.

#include <QtTest>

#include "layout.h"

class TestLayout : public QObject {
    Q_OBJECT

private slots:
    void automaticFillsWidth();
    void fixedSizeIgnoresWidth();
    void automaticIsClamped();
    void fitsAtEdges();
};

void TestLayout::automaticFillsWidth() {
    // 1072 - 2 * 24 margins - 4 * 16 gaps = 960 px for five tiles.
    QCOMPARE(nt_tile_size(5, 0, 1072), 192);
}

void TestLayout::fixedSizeIgnoresWidth() {
    QCOMPARE(nt_tile_size(5, 150, 100), 150);
}

void TestLayout::automaticIsClamped() {
    QCOMPARE(nt_tile_size(1, 0, 2000), kNtTileMaxPx);
    QCOMPARE(nt_tile_size(5, 0, 200), kNtTileMinPx);
}

void TestLayout::fitsAtEdges() {
    // Five minimum-size tiles need 5 * 120 + 4 * 16 + 2 * 24 = 712 px.
    QVERIFY(nt_tiles_fit(5, 0, 712));
    QVERIFY(!nt_tiles_fit(5, 0, 711));

    // Five 192 px tiles need exactly 1072 px.
    QVERIFY(nt_tiles_fit(5, 192, 1072));
    QVERIFY(!nt_tiles_fit(5, 192, 1071));

    // One 300 px tile needs 300 + 2 * 24 = 348 px.
    QVERIFY(nt_tiles_fit(1, 300, 348));
    QVERIFY(!nt_tiles_fit(1, 300, 347));
}

QTEST_MAIN(TestLayout)
#include "test_layout.moc"
