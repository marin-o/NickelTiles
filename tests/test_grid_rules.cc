// Saved tile order (grid_rules.h): it is the owner's data, and it has to survive
// NickelMenu entries coming and going.

#include <QtTest>

#include "ui/grid_rules.h"

class TestGridRules : public QObject {
    Q_OBJECT

private slots:
    void savedOrder();
    void newRecentTilesStayTogether();
    void pagesFitTheHeight();
};

void TestGridRules::savedOrder() {
    const QStringList labels = {"KOReader", "Plato", "New entry", "Reboot"};
    // "Removed" has no tile now; "New entry" isn't saved, so it stays after the saved ones.
    const QStringList saved = {"Reboot", "Removed", " KOReader ", "Plato"};
    QCOMPARE(nt_tile_order(labels, saved), QList<int>({3, 0, 1, 2}));
}

void TestGridRules::newRecentTilesStayTogether() {
    const QStringList saved = {"Plato", "Continue reading", "Reboot"};
    const QStringList recents = {"Continue reading", "Recent book 2", "Recent book 3"};
    const QStringList want = {"Plato", "Continue reading", "Recent book 2", "Recent book 3", "Reboot"};
    QCOMPARE(nt_order_with_recents(saved, recents), want);
}

// The owner's reader: 19 tiles need pages, 15 do not, on a Home about 1220 px tall.
void TestGridRules::pagesFitTheHeight() {
    const NtPageGeometry g = {1220, 1220, 307, 20, 48, 92};
    QCOMPARE(nt_plan_pages(g, 15, 5).pages, 1);
    const NtPagePlan two = nt_plan_pages(g, 19, 5);
    QCOMPARE(two.rows, 3);
    QCOMPARE(two.pages, 2);
    QCOMPARE(nt_page_first_tile(two, 1, 5), 15);
    // Kobo's rows fill the first page (the overlap bug): its tiles move on, none squeezed in.
    const NtPageGeometry shared = {1220, 50, 307, 20, 48, 92};
    const NtPagePlan moved = nt_plan_pages(shared, 19, 5);
    QCOMPARE(moved.firstRows, 0);
    QCOMPARE(moved.pages, 3);
    QCOMPARE(nt_page_first_tile(moved, 1, 5), 0);
    // Nothing fits: still one row, so tiles can be reached.
    const NtPageGeometry tiny = {100, 100, 307, 20, 48, 92};
    QCOMPARE(nt_plan_pages(tiny, 3, 5).rows, 1);
}

QTEST_MAIN(TestGridRules)
#include "test_grid_rules.moc"
