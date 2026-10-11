// Automatic tile size fills the grid width (layout.h).

#include <QtTest>

#include "layout.h"

class TestLayout : public QObject {
    Q_OBJECT

private slots:
    void automaticFillsWidth();
};

void TestLayout::automaticFillsWidth() {
    // 1072 - 2 * 24 margins - 4 * 16 gaps = 960 px for five tiles.
    QCOMPARE(nt_tile_size(5, 0, 1072), 192);
}

QTEST_MAIN(TestLayout)
#include "test_layout.moc"
