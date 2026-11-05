#include "ui/settings_tile_list.h"

#include <QGridLayout>

#include "ui/settings_rules.h"

namespace {

const int kTilesPerPage = 12;   // two columns of six; the list has a tab of its own

}   // namespace

NtTileList::NtTileList(QWidget *parent, const QStringList &labels, const QStringList &hidden)
    : m_labels(labels), m_hidden(QSet<QString>::fromList(hidden)), m_page(0) {
    m_title = ntLabel(parent, QStringLiteral("Tiles to show"), 32, true);
    m_prev = ntButton(parent, QStringLiteral("‹"), 90, [this]() {
        m_page--;
        refresh();
    });
    m_pageLabel = ntLabel(parent, QString(), 26);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setFixedWidth(120);
    m_next = ntButton(parent, QStringLiteral("›"), 90, [this]() {
        m_page++;
        refresh();
    });
    for (int i = 0; i < kTilesPerPage; i++)
        m_checks << new NtControl(parent, NtControl::Check, QString(), [this, i]() { toggleTile(i); });
}

void NtTileList::addTo(QVBoxLayout *v) {
    QHBoxLayout *listHead = new QHBoxLayout();
    listHead->addWidget(m_title, 1);
    listHead->addWidget(m_prev);
    listHead->addWidget(m_pageLabel);
    listHead->addWidget(m_next);
    v->addLayout(listHead);

    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(4);
    for (int i = 0; i < m_checks.size(); i++)
        grid->addWidget(m_checks.at(i), i % (kTilesPerPage / 2), i / (kTilesPerPage / 2));
    v->addLayout(grid);
    v->addStretch(1);
}

void NtTileList::refresh() {
    const int pages = m_labels.isEmpty() ? 1 : (m_labels.size() + kTilesPerPage - 1) / kTilesPerPage;
    if (m_page >= pages) m_page = pages - 1;
    if (m_page < 0) m_page = 0;
    m_prev->setVisible(pages > 1);
    m_next->setVisible(pages > 1);
    m_pageLabel->setVisible(pages > 1);
    m_prev->setPressEnabled(m_page > 0);
    m_next->setPressEnabled(m_page + 1 < pages);
    m_pageLabel->setText(QString::number(m_page + 1) + QStringLiteral(" / ") + QString::number(pages));
    for (int i = 0; i < m_checks.size(); i++) {
        const int index = m_page * kTilesPerPage + i;
        NtControl *c = m_checks.at(i);
        if (index < m_labels.size()) {
            c->setText(m_labels.at(index));
            c->setChecked(!m_hidden.contains(m_labels.at(index)));
            c->setVisible(true);
        } else {
            c->setVisible(false);
        }
    }
}

QStringList NtTileList::hiddenAfter(const QStringList &initialHidden) const {
    return nt_hidden_after(initialHidden, m_labels, m_hidden);
}

// Flips the tile in slot `slot` of the current page.
void NtTileList::toggleTile(int slot) {
    const int index = m_page * kTilesPerPage + slot;
    if (index >= m_labels.size()) return;
    const QString &l = m_labels.at(index);
    if (m_hidden.contains(l))
        m_hidden.remove(l);
    else
        m_hidden.insert(l);
    refresh();
}
