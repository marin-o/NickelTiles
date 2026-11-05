// Finding Kobo's rows by name under mainContainer follows NickelHome by Nico
// Verbruggen (MIT); see THIRD-PARTY.md.

#include "ui/home_rows.h"

#include <QObject>
#include <QStackedWidget>
#include <QString>
#include <QVariant>

QWidget *nt_find_direct_child(QWidget *parent, const char *name) {
    const QString qname = QString::fromLatin1(name);
    const QObjectList &children = parent->children();
    for (QObject *child : children) {
        QWidget *cw = qobject_cast<QWidget *>(child);
        if (cw && cw->objectName() == qname) return cw;
    }
    return nullptr;
}

QWidget *nt_find_main_container(QWidget *homeView) {
    const QList<QWidget *> containers = homeView->findChildren<QWidget *>(QStringLiteral("mainContainer"));
    for (QWidget *c : containers)
        if (nt_find_direct_child(c, "row1")) return c;
    return nullptr;
}

void nt_hide_row(QWidget *row) {
    if (row->isHidden()) return;
    row->setProperty("ntHidden", true);
    row->setVisible(false);
}

void nt_show_row(QWidget *row) {
    if (!row->property("ntHidden").toBool()) return;
    row->setProperty("ntHidden", QVariant());
    row->setVisible(true);
}

QList<QWidget *> nt_keep_rows(QWidget *container, int keep) {
    static const char *const names[] = {"row1", "row2", "row3"};
    QList<QWidget *> kept;
    for (int i = 0; i < 3; i++) {
        QWidget *r = nt_find_direct_child(container, names[i]);
        if (!r) continue;
        if (i >= keep) {
            nt_hide_row(r);
            continue;
        }
        nt_show_row(r);
        if (!r->isHidden()) kept << r;
    }
    return kept;
}

int nt_row_height(const QWidget *row) {
    return qMax(row->sizeHint().height(), row->minimumSizeHint().height());
}

QWidget *nt_top_row_right(QWidget *container) {
    QWidget *row = nt_find_direct_child(container, "row1");
    return row ? nt_find_direct_child(row, "row1col2") : nullptr;
}

bool nt_half_is_empty(QWidget *half) {
    const QStackedWidget *pages = qobject_cast<QStackedWidget *>(half);
    return pages && pages->currentWidget() && pages->currentWidget()->isHidden();
}
