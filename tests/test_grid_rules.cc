// Saved tile order (grid_rules.h): it is the owner's data, and it has to survive
// NickelMenu entries coming and going.

#include <QtTest>

#include "ui/grid_rules.h"

class TestGridRules : public QObject {
    Q_OBJECT

private slots:
    void savedOrder();
};

void TestGridRules::savedOrder() {
    const QStringList labels = {"KOReader", "Plato", "New entry", "Reboot"};
    // "Removed" has no tile now; "New entry" isn't saved, so it stays after the saved ones.
    const QStringList saved = {"Reboot", "Removed", " KOReader ", "Plato"};
    QCOMPARE(nt_tile_order(labels, saved), QList<int>({3, 0, 1, 2}));
}

QTEST_MAIN(TestGridRules)
#include "test_grid_rules.moc"
