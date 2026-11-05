#include "ui/grid_rules.h"

#include <QtGlobal>

QList<int> nt_tile_order(const QStringList &labels, const QStringList &savedOrder) {
    QList<int> rest;
    for (int i = 0; i < labels.size(); i++) rest << i;
    QList<int> order;
    for (const QString &name : savedOrder) {
        const QString wanted = name.trimmed();
        for (int i = 0; i < rest.size(); i++) {
            if (labels.at(rest.at(i)) == wanted) {
                order << rest.takeAt(i);
                break;
            }
        }
    }
    return order + rest;
}

QStringList nt_order_with_recents(const QStringList &savedOrder, const QStringList &recentLabels) {
    QStringList order = savedOrder;
    if (order.isEmpty()) return order;
    for (int i = 1; i < recentLabels.size(); i++) {
        if (order.contains(recentLabels.at(i))) continue;
        const int before = order.indexOf(recentLabels.at(i - 1));
        if (before >= 0) order.insert(before + 1, recentLabels.at(i));
    }
    return order;
}

NtPagePlan nt_plan_pages(const NtPageGeometry &g, int tiles, int columns) {
    const auto rowsIn = [&g](int height) {
        return qMax(0, (height - g.margins + g.rowSpacing) / (g.tileHeight + g.rowSpacing));
    };
    const int needed = (tiles + columns - 1) / columns;
    NtPagePlan plan;
    if (rowsIn(g.firstAvailable) >= needed) {
        plan.firstRows = plan.rows = needed;
        plan.pages = 1;
        return plan;
    }
    plan.rows = qMax(1, rowsIn(g.available - g.navHeight));
    plan.firstRows = g.firstAvailable < g.available ? rowsIn(g.firstAvailable - g.navHeight) : plan.rows;
    const int perPage = plan.rows * columns;
    plan.pages = 1 + (tiles - plan.firstRows * columns + perPage - 1) / perPage;
    return plan;
}

int nt_page_first_tile(const NtPagePlan &plan, int page, int columns) {
    return page == 0 ? 0 : (plan.firstRows + (page - 1) * plan.rows) * columns;
}

int nt_kobo_rows_to_keep(NtKoboRows rows, bool hasAppTiles) {
    if (!hasAppTiles) return 3;
    switch (rows) {
    case NtKoboRows::Top:
        return 1;
    case NtKoboRows::All:
        return 3;
    case NtKoboRows::Hidden:
        break;
    }
    return 0;
}

bool nt_any_tile_shown(const QStringList &labels, const QStringList &hidden) {
    for (const QString &label : labels)
        if (!hidden.contains(label)) return true;
    return false;
}
