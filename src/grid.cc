#include "grid.h"

#include <QDateTime>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHash>
#include <QList>
#include <QScreen>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QString>
#include <QStringList>
#include <QTimerEvent>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include "config.h"
#include "nickel.h"
#include "nm_parse.h"
#include "settings_model.h"
#include "settings_page.h"
#include "tiles.h"
#include "ui.h"
#include "util.h"

namespace {

// Whether a tile for `label` should be built: not already built, not hidden.
// Records the label as built.
bool nt_claim_label(const QString &label, const QStringList &hidden, QStringList *seen) {
    if (seen->contains(label) || hidden.contains(label)) return false;
    *seen << label;
    return true;
}

// The stretch nt_install_grid adds below the rows when they are hidden, kept
// per layout so that a rebuild can remove it.
QHash<QLayout*, QSpacerItem*> &nt_stretches() {
    static QHash<QLayout*, QSpacerItem*> stretches;
    return stretches;
}

QWidget *nt_find_direct_child(QWidget *parent, const char *name) {
    const QString qname = QString::fromLatin1(name);
    const QObjectList children = parent->children();
    for (QObject *child : children) {
        QWidget *cw = qobject_cast<QWidget*>(child);
        if (cw && cw->objectName() == qname)
            return cw;
    }
    return NULL;
}

void nt_log_tree(QWidget *w, int depth) {
    if (depth > 6) return;
    const QObjectList children = w->children();
    for (QObject *child : children) {
        QWidget *cw = qobject_cast<QWidget*>(child);
        if (!cw) continue;
        NT_LOG("tree: %*s%s '%s' %dx%d visible=%d layout=%s", depth * 2, "",
            cw->metaObject()->className(), qPrintable(cw->objectName()),
            cw->width(), cw->height(), cw->isVisible() ? 1 : 0,
            cw->layout() ? cw->layout()->metaObject()->className() : "-");
        nt_log_tree(cw, depth + 1);
    }
}

// The mainContainer that holds the three home rows.
QWidget *nt_find_main_container(QWidget *homeView) {
    const QList<QWidget*> containers = homeView->findChildren<QWidget*>(QStringLiteral("mainContainer"));
    for (QWidget *c : containers)
        if (nt_find_direct_child(c, "row1")) return c;
    return NULL;
}

// Removes the grid of an earlier build and its stretch. Returns true if there
// was one (the rebuild after the settings page saves).
bool nt_remove_previous_grid(QWidget *container, QVBoxLayout *mainLayout) {
    const QList<QWidget*> oldGrids = container->findChildren<QWidget*>(QStringLiteral("ntGrid"), Qt::FindDirectChildrenOnly);
    for (QWidget *old : oldGrids) {
        mainLayout->removeWidget(old);
        old->hide();
        old->deleteLater();
    }
    if (QSpacerItem *sp = nt_stretches().take(mainLayout)) {
        mainLayout->removeItem(sp);
        delete sp;
    }
    return !oldGrids.isEmpty();
}

// Hides Kobo's rows when asked. Rows are only shown again if this mod hid
// them, since another mod may have hidden some.
void nt_set_rows_hidden(QWidget *container, bool hideRows) {
    static const char *const rows[] = {"row1", "row2", "row3"};
    for (const char *name : rows) {
        QWidget *r = nt_find_direct_child(container, name);
        if (!r) continue;
        if (hideRows && !r->isHidden()) {
            r->setProperty("ntHidden", true);
            r->setVisible(false);
        } else if (!hideRows && r->property("ntHidden").toBool()) {
            r->setProperty("ntHidden", QVariant());
            r->setVisible(true);
        }
    }
}

void nt_show_settings(QWidget *homeView) {
    nt_open_settings(homeView, nt_all_tile_labels(), [homeView]() { nt_install_grid(homeView); });
}

class NtGrid : public QWidget, public NtTileHost {
public:
    NtGrid(QWidget *homeView, QWidget *parent, const NtSettings &settings);
    void build();

    void tileTapped(NtTile *tile) override;
    void tileLongPressed(NtTile *tile) override;

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        if (m_recent) m_recent->refresh();
        startTimer(2000);   // Nickel updates its database and widgets shortly after
        // One file per screen visit (a later one may show a dialog on top).
        nt_debug_screenshot(this, QStringLiteral("home-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")), 4000);
    }
    void timerEvent(QTimerEvent *event) override {
        killTimer(event->timerId());
        if (m_recent) m_recent->refresh();
    }

private:
    void applyOrder();
    void relayout();
    void saveOrder();

    QWidget *m_homeView;
    NtSettings m_settings;   // snapshot taken when the grid was built
    QGridLayout *m_layout;
    QList<NtTile*> m_tiles;
    NtRecentTile *m_recent;
    NtTile *m_marked;
};

NtGrid::NtGrid(QWidget *homeView, QWidget *parent, const NtSettings &settings)
    : QWidget(parent), m_homeView(homeView), m_settings(settings), m_layout(new QGridLayout(this)),
      m_recent(NULL), m_marked(NULL) {
    setObjectName(QStringLiteral("ntGrid"));
    m_layout->setObjectName(QStringLiteral("ntGridLayout"));
    m_layout->setContentsMargins(kNtGridMargin, kNtGridMargin, kNtGridMargin, kNtGridMargin);
    m_layout->setHorizontalSpacing(kNtGridSpacing);
    m_layout->setVerticalSpacing(20);
}

void NtGrid::build() {
    const int px = nt_tile_size(m_settings.columns, m_settings.tilePx, nt_grid_width());
    const QStringList &hidden = m_settings.hidden;
    QStringList seen;

    if (m_settings.recent) {
        m_recent = new NtRecentTile(this, this, px, m_settings);
        m_tiles << m_recent;
    }
    for (const NtEntry &e : nt_nickelmenu_entries())
        if (nt_claim_label(e.label, hidden, &seen))
            m_tiles << new NtEntryTile(this, this, e, px);
    for (const nt_tile_t *t = nt_config_tiles(); t; t = t->next) {
        const QString label = QString::fromUtf8(t->label);
        if (nt_claim_label(label, hidden, &seen))
            m_tiles << new NtCommandTile(this, this, label, QString::fromUtf8(t->icon), QString::fromUtf8(t->command), px);
    }
    if (!hidden.contains(QString::fromLatin1(NtSettingsTile::kLabel))) {
        QWidget *view = m_homeView;
        m_tiles << new NtSettingsTile(this, this, px, [view]() { nt_show_settings(view); });
    }

    applyOrder();
    relayout();
    NT_LOG("grid: %d tiles, %d columns, %d px", m_tiles.size(), m_settings.columns, px);
    startTimer(1500);   // first refresh of the recent tile after Nickel fills row1
}

// Known labels first in the saved order, the rest after.
void NtGrid::applyOrder() {
    if (m_settings.order.isEmpty()) return;
    QList<NtTile*> sorted;
    for (const QString &name : m_settings.order) {
        const QString n = name.trimmed();
        for (int i = 0; i < m_tiles.size(); i++) {
            if (m_tiles.at(i)->label() == n) {
                sorted << m_tiles.takeAt(i);
                break;
            }
        }
    }
    sorted << m_tiles;
    m_tiles = sorted;
}

void NtGrid::relayout() {
    while (QLayoutItem *item = m_layout->takeAt(0)) delete item;
    const int columns = m_settings.columns;
    for (int i = 0; i < m_tiles.size(); i++)
        m_layout->addWidget(m_tiles.at(i), i / columns, i % columns, Qt::AlignTop | Qt::AlignLeft);
    m_layout->setColumnStretch(columns, 1);
    m_layout->setRowStretch(m_layout->rowCount(), 1);
}

void NtGrid::tileLongPressed(NtTile *tile) {
    if (m_marked) m_marked->setMarked(false);
    m_marked = tile;
    tile->setMarked(true);
    NT_DBG("arrange: picked '%s'", qPrintable(tile->label()));
    nt_toast(QStringLiteral("Tap another tile to swap"), 1500);
}

void NtGrid::tileTapped(NtTile *tile) {
    if (m_marked) {
        NtTile *from = m_marked;
        m_marked->setMarked(false);
        m_marked = NULL;
        if (from != tile) {
            const int a = m_tiles.indexOf(from), b = m_tiles.indexOf(tile);
            if (a >= 0 && b >= 0) {
                m_tiles.swap(a, b);
                relayout();
                saveOrder();
            }
        }
        return;
    }
    if (!m_settings.actions && tile->runsAction()) {
        NT_LOG("nt_actions=0; not running '%s'", qPrintable(tile->label()));
        return;
    }
    NT_LOG("tile tapped: '%s'", qPrintable(tile->label()));
    tile->activate();
}

void NtGrid::saveOrder() {
    NtSettings updated = m_settings;
    updated.order.clear();
    for (NtTile *t : m_tiles) updated.order << t->label();
    const bool ok = nt_settings_save(m_settings, updated);
    NT_LOG("order saved=%d: %s", ok ? 1 : 0, qPrintable(updated.order.join(QStringLiteral("|"))));
    m_settings = updated;
}

}  // namespace

void nt_install_grid(QWidget *homeView) {
    if (nt_log_verbose) nt_log_tree(homeView, 0);
    QWidget *container = nt_find_main_container(homeView);
    if (!container) {
        NT_LOG("warning: mainContainer with row1 not found; not inserting the grid");
        return;
    }
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(container->layout());
    if (!mainLayout) {
        NT_LOG("warning: mainContainer has no QVBoxLayout; not inserting");
        return;
    }
    const bool replacing = nt_remove_previous_grid(container, mainLayout);
    const NtSettings settings = nt_settings();
    nt_set_rows_hidden(container, settings.hideRows);

    NtGrid *grid = new NtGrid(homeView, container, settings);
    grid->build();
    const int index = settings.hideRows ? 0 : 1;
    mainLayout->insertWidget(index, grid);
    if (settings.hideRows) {
        QSpacerItem *sp = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        mainLayout->addItem(sp);
        nt_stretches().insert(mainLayout, sp);
    }
    if (replacing) grid->show();
    NT_LOG("inserted grid at mainLayout index %d (rows hidden=%d)", index, settings.hideRows ? 1 : 0);
}

int nt_grid_width() {
    const QScreen *screen = QGuiApplication::primaryScreen();
    return screen ? screen->size().width() : 1072;
}

QStringList nt_all_tile_labels() {
    QStringList labels;
    for (const NtEntry &e : nt_nickelmenu_entries())
        if (!labels.contains(e.label)) labels << e.label;
    for (const nt_tile_t *t = nt_config_tiles(); t; t = t->next) {
        const QString l = QString::fromUtf8(t->label);
        if (!labels.contains(l)) labels << l;
    }
    return labels;
}
