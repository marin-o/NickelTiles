#ifndef NT_SETTINGS_TILE_LIST_H
#define NT_SETTINGS_TILE_LIST_H

#include <QBoxLayout>
#include <QLabel>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "ui/controls.h"

// The paged "Tiles to show" list: one page of check rows at a time. A checked
// row is a tile that is shown; the set of hidden labels is edited in place.
class NtTileList {
public:
    NtTileList(QWidget *parent, const QStringList &labels, const QStringList &hidden);

    void addTo(QVBoxLayout *v);
    void refresh();

    // The hidden list to save; see nt_hidden_after.
    QStringList hiddenAfter(const QStringList &initialHidden) const;

private:
    void toggleTile(int slot);

    QStringList m_labels;
    QSet<QString> m_hidden;
    int m_page;
    QLabel *m_title, *m_pageLabel;
    NtControl *m_prev, *m_next;
    QList<NtControl *> m_checks;
};

#endif
