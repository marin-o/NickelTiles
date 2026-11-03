#include "ui/grid_rules.h"

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

bool nt_any_tile_shown(const QStringList &labels, const QStringList &hidden) {
    for (const QString &label : labels)
        if (!hidden.contains(label)) return true;
    return false;
}
