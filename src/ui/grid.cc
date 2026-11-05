#include "ui/grid.h"

#include <QDateTime>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGuiApplication>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QScreen>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QString>
#include <QStringList>
#include <QTimerEvent>
#include <QVBoxLayout>
#include <QWidget>

#include "books/recent.h"
#include "core/config.h"
#include "ui/controls.h"
#include "ui/grid_rules.h"
#include "ui/home_rows.h"
#include "kobo/nickel.h"
#include "kobo/nm_parse.h"
#include "core/settings_model.h"
#include "ui/settings_page.h"
#include "ui/tiles.h"
#include "ui/ui.h"
#include "core/util.h"

namespace {

const int kRowSpacing = 20;
const int kNavHeight = 72;
// Below this the container has not been laid out yet.
const int kContainerLaidOut = 300;
// Nickel's status bar and tab bar, for estimating the room before layout.
const int kHomeChromeEstimate = 230;

// Whether a tile for `label` should be built: not already built, not hidden.
// Records the label as built.
bool nt_claim_label(const QString &label, const QStringList &hidden, QStringList *seen) {
    if (seen->contains(label) || hidden.contains(label)) return false;
    *seen << label;
    return true;
}

// Whether the grid will hold anything beyond the settings tile. With only that
// tile, a first install would look broken if Kobo's rows were hidden too.
bool nt_has_app_tiles(const NtSettings &settings) {
    return nt_any_tile_shown(nt_all_tile_labels(), settings.hidden) ||
           (settings.recent && nt_recent_book(settings.recentSource).valid);
}

// The stretch nt_install_grid adds below the rows when they are hidden, kept
// per layout so that a rebuild can remove it.
QHash<QLayout *, QSpacerItem *> &nt_stretches() {
    static QHash<QLayout *, QSpacerItem *> stretches;
    return stretches;
}

void nt_log_tree(QWidget *w, int depth) {
    if (depth > 6) return;
    const QObjectList &children = w->children();
    for (QObject *child : children) {
        QWidget *cw = qobject_cast<QWidget *>(child);
        if (!cw) continue;
        NT_LOG("tree: %*s%s '%s' %dx%d visible=%d layout=%s", depth * 2, "", cw->metaObject()->className(),
               qPrintable(cw->objectName()), cw->width(), cw->height(), cw->isVisible() ? 1 : 0,
               cw->layout() ? cw->layout()->metaObject()->className() : "-");
        nt_log_tree(cw, depth + 1);
    }
}

// Removes the grid of an earlier build and its stretch. Returns true if there
// was one (the rebuild after the settings page saves).
bool nt_remove_previous_grid(QWidget *container, QVBoxLayout *mainLayout) {
    // Also inside Kobo's top row, where the settings tile may sit on its own.
    const QList<QWidget *> oldGrids = container->findChildren<QWidget *>(QStringLiteral("ntGrid"));
    if (QWidget *half = nt_top_row_right(container)) nt_show_row(half);
    for (QWidget *old : oldGrids) {
        if (QLayout *l = old->parentWidget()->layout()) l->removeWidget(old);
        old->hide();
        old->deleteLater();
    }
    if (QSpacerItem *sp = nt_stretches().take(mainLayout)) {
        mainLayout->removeItem(sp);
        delete sp;
    }
    return !oldGrids.isEmpty();
}

void nt_show_settings(QWidget *homeView) {
    nt_open_settings(homeView, nt_all_tile_labels(), [homeView]() { nt_install_grid(homeView); });
}

class NtGrid : public QWidget, public NtTileHost {
public:
    NtGrid(QWidget *homeView, QWidget *parent, const NtSettings &settings);
    void build();
    // Kobo's rows that share the first page and step aside on the others.
    void shareFirstPage(const QList<QWidget *> &rows);
    // For the settings tile alone: lets it move into the empty half of Kobo's
    // top row, out of `mainLayout`, where it would cover the row's text.
    void mayUseTopRow(QVBoxLayout *mainLayout) { m_mainLayout = mainLayout; }

    void tileTapped(NtTile *tile) override;
    void tileLongPressed(NtTile *tile) override;
    bool tileSwiped(int distance) override;

protected:
    // A swipe in the gaps between tiles. Touches are only taken while there are
    // pages to turn, so that with one page they reach Nickel as they always did.
    bool event(QEvent *event) override {
        if (m_pages > 1 && event->type() == QEvent::TouchBegin) {
            event->accept();
            return true;
        }
        if (event->type() == QEvent::TouchEnd && tileSwiped(nt_swipe_distance(static_cast<QTouchEvent *>(event)))) {
            event->accept();
            return true;
        }
        return QWidget::event(event);
    }

    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        if (m_mainLayout) restartTimer(&m_placeTimer, 0);
        refreshRecents();
        // Coming back home shows Kobo's rows, as Kobo's own home would.
        if (!m_koboRows.isEmpty()) m_page = 0;
        relayout();
        restartTimer(&m_refreshTimer, 2000);   // Nickel updates its database and widgets shortly after
        // One file per screen visit (a later one may show a dialog on top).
        nt_debug_screenshot(
            this, QStringLiteral("home-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")),
            4000);
    }
    void timerEvent(QTimerEvent *event) override {
        const int id = event->timerId();
        killTimer(id);
        if (id == m_placeTimer) {
            m_placeTimer = 0;
            placeLoneTile();
        } else if (id == m_checkTimer) {
            m_checkTimer = 0;
            warnIfSqueezed();
        } else if (id == m_refreshTimer) {
            m_refreshTimer = 0;
            refreshRecents();
            relayout();
            // Checked once Nickel has laid the page out around the new grid.
            restartTimer(&m_checkTimer, 1000);
        }
    }

private:
    // One timer per job: starting it again replaces the pending one, so
    // refreshes can't multiply.
    void restartTimer(int *id, int ms) {
        if (*id) killTimer(*id);
        *id = startTimer(ms);
    }
    void buildNav();
    QList<NtTile *> tilesWithContent() const;
    void measureRoom();
    void showKoboRows(bool shown);
    void refreshRecents();
    void applyOrder();
    void relayout();
    void turnPage(int step);
    void warnIfSqueezed();
    void saveOrder();
    void placeLoneTile();

    QWidget *m_homeView;
    NtSettings m_settings;   // snapshot taken when the grid was built
    QGridLayout *m_layout;
    QList<NtTile *> m_tiles;
    QList<NtRecentTile *> m_recents;
    NtTile *m_marked;
    QWidget *m_nav;
    NtControl *m_prev, *m_next;
    QLabel *m_pageLabel;
    QList<QWidget *> m_koboRows;   // sharing the page with the grid now
    QList<QWidget *> m_keptRows;   // the same, while the grid is under them
    QVBoxLayout *m_mainLayout = nullptr;
    int m_placeTimer = 0;
    int m_refreshTimer = 0;
    NtPagePlan m_plan = {0, 0, 1};
    int m_room = 0;        // on pages of tiles only
    int m_firstRoom = 0;   // on the first page, with Kobo's kept rows
    int m_page = 0;
    int m_pages = 1;
    int m_checkTimer = 0;
};

NtGrid::NtGrid(QWidget *homeView, QWidget *parent, const NtSettings &settings)
    : QWidget(parent), m_homeView(homeView), m_settings(settings), m_layout(new QGridLayout(this)), m_marked(nullptr),
      m_nav(nullptr), m_prev(nullptr), m_next(nullptr), m_pageLabel(nullptr) {
    setObjectName(QStringLiteral("ntGrid"));
    m_layout->setObjectName(QStringLiteral("ntGridLayout"));
    m_layout->setContentsMargins(kNtGridMargin, kNtGridMargin, kNtGridMargin, kNtGridMargin);
    m_layout->setHorizontalSpacing(kNtGridSpacing);
    m_layout->setVerticalSpacing(kRowSpacing);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    buildNav();
}

// The arrows and page number under the tiles, shown only when there are pages.
void NtGrid::buildNav() {
    m_nav = new QWidget(this);
    QHBoxLayout *h = new QHBoxLayout(m_nav);
    h->setContentsMargins(0, 0, 0, 0);
    h->addStretch(1);
    m_prev = ntButton(m_nav, QStringLiteral("‹"), 110, [this]() { turnPage(-1); });
    h->addWidget(m_prev);
    m_pageLabel = ntLabel(m_nav, QString(), 26);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setFixedWidth(120);
    h->addWidget(m_pageLabel);
    m_next = ntButton(m_nav, QStringLiteral("›"), 110, [this]() { turnPage(1); });
    h->addWidget(m_next);
    h->addStretch(1);
    m_nav->setFixedHeight(kNavHeight);
    m_nav->hide();
}

void NtGrid::build() {
    const int px = nt_tile_size(m_settings.columns, m_settings.tilePx, nt_grid_width());
    const QStringList &hidden = m_settings.hidden;
    QStringList seen;

    if (m_settings.recent) {
        for (int slot = 0; slot < m_settings.recentCount; slot++) {
            NtRecentTile *tile = new NtRecentTile(this, this, px, m_settings, slot);
            m_recents << tile;
            m_tiles << tile;
        }
    }
    for (const NtEntry &e : nt_nickelmenu_entries())
        if (nt_claim_label(e.label, hidden, &seen)) m_tiles << new NtEntryTile(this, this, e, px);
    for (const nt_tile_t *t = nt_config_tiles(); t; t = t->next) {
        const QString label = QString::fromUtf8(t->label);
        if (nt_claim_label(label, hidden, &seen))
            m_tiles << new NtCommandTile(this, this, label, QString::fromUtf8(t->icon), QString::fromUtf8(t->command),
                                         px);
    }
    if (!hidden.contains(QString::fromLatin1(NtSettingsTile::kLabel))) {
        QWidget *view = m_homeView;
        m_tiles << new NtSettingsTile(this, this, px, [view]() { nt_show_settings(view); });
    }

    applyOrder();
    relayout();
    NT_LOG("grid: %d tiles, %d columns, %d px", m_tiles.size(), m_settings.columns, px);
    restartTimer(&m_refreshTimer, 1500);   // first refresh of the recent tiles after Nickel fills row1
}

void NtGrid::applyOrder() {
    QStringList labels;
    for (const NtTile *tile : m_tiles) labels << tile->label();
    QStringList recentLabels;
    for (const NtRecentTile *tile : m_recents) recentLabels << tile->label();
    const QStringList order = nt_order_with_recents(m_settings.order, recentLabels);
    QList<NtTile *> sorted;
    for (const int index : nt_tile_order(labels, order)) sorted << m_tiles.at(index);
    m_tiles = sorted;
}

void NtGrid::refreshRecents() {
    if (m_recents.isEmpty()) return;
    const QList<NtRecentBook> books = nt_recent_books(m_settings.recentSource, m_recents.size());
    for (int i = 0; i < m_recents.size(); i++)
        m_recents.at(i)->setBook(i < books.size() ? books.at(i) : NtRecentBook());
}

QList<NtTile *> NtGrid::tilesWithContent() const {
    QList<NtTile *> tiles;
    for (NtTile *tile : m_tiles)
        if (tile->hasContent()) tiles << tile;
    return tiles;
}

// Kobo's rows report less height than they draw and count on the page's spare
// room; a grid that grew into it would cut them off, so it takes only its own.
void NtGrid::shareFirstPage(const QList<QWidget *> &rows) {
    m_koboRows = m_keptRows = rows;
    if (!rows.isEmpty()) setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

// Kobo fills its top row only after setupUi, so this runs once Home shows: the
// settings tile goes in the row's right half while Kobo leaves it empty (a
// reader with no books), and back under the row once Kobo puts something there.
void NtGrid::placeLoneTile() {
    QWidget *half = nt_top_row_right(m_mainLayout->parentWidget());
    QBoxLayout *row = half ? qobject_cast<QBoxLayout *>(half->parentWidget()->layout()) : nullptr;
    if (!row) return;
    const bool empty = nt_half_is_empty(half);
    if (empty == (parentWidget() == half->parentWidget())) return;
    if (empty) {
        nt_hide_row(half);
        m_mainLayout->removeWidget(this);
        row->addWidget(this, row->stretch(row->indexOf(half)));
        m_koboRows.clear();
    } else {
        row->removeWidget(this);
        nt_show_row(half);
        m_mainLayout->insertWidget(1, this);
        m_koboRows = m_keptRows;
    }
    show();
    relayout();
    NT_LOG("settings tile %s Kobo's top row", empty ? "moved into" : "moved back under");
}

// The room for tiles, measured while Kobo's kept rows show: the first page has
// what they leave, the others what they free up when they step aside. Before
// the page is laid out the container has no real size, so this estimates.
void NtGrid::measureRoom() {
    QWidget *container = parentWidget();
    if (container->height() < kContainerLaidOut) {
        const QScreen *screen = QGuiApplication::primaryScreen();
        m_room = m_firstRoom = (screen ? screen->size().height() : 1448) - kHomeChromeEstimate;
        for (const QWidget *r : m_koboRows) m_firstRoom -= nt_row_height(r);
        return;
    }
    container->layout()->activate();
    // The home container runs on under Nickel's tab bar, so the room ends where the home view does.
    const int viewBottom = m_homeView->height() - container->mapTo(m_homeView, QPoint(0, 0)).y();
    const int bottom = qMin(container->height(), viewBottom);
    // Kobo's rows give way when the grid asks for more than is left, so where they
    // sit now can understate what they need; their heights can too (a row of
    // book covers asks for less than it draws). The grid starts below both.
    const int spacing = qMax(0, container->layout()->spacing());
    int top = y();
    int rowsNeed = 0;
    for (const QWidget *r : m_koboRows) {
        top = qMin(top, r->y());
        rowsNeed += nt_row_height(r) + spacing;
    }
    m_room = bottom - top;
    m_firstRoom = bottom - qMax(y(), top + rowsNeed);
}

void NtGrid::showKoboRows(bool shown) {
    for (QWidget *r : m_koboRows) {
        if (shown)
            nt_show_row(r);
        else
            nt_hide_row(r);
    }
}

void NtGrid::relayout() {
    showKoboRows(m_page == 0);
    if (m_page == 0) measureRoom();
    while (QLayoutItem *item = m_layout->takeAt(0)) delete item;
    for (NtTile *tile : m_tiles) tile->hide();
    for (int row = 0; row < m_layout->rowCount(); row++) {
        m_layout->setRowMinimumHeight(row, 0);
        m_layout->setRowStretch(row, 0);
    }

    const QList<NtTile *> tiles = tilesWithContent();
    const int columns = m_settings.columns;
    const int tileHeight = tiles.isEmpty() ? 1 : tiles.first()->height();
    const NtPageGeometry geometry = {m_room,      m_firstRoom,       tileHeight,
                                     kRowSpacing, 2 * kNtGridMargin, kNavHeight + kRowSpacing};
    m_plan = nt_plan_pages(geometry, tiles.size(), columns);
    m_pages = m_plan.pages;
    const int page = qBound(0, m_page, m_pages - 1);
    if (page != m_page) {
        m_page = page;
        showKoboRows(m_page == 0);
    }

    const int rows = m_page == 0 ? m_plan.firstRows : m_plan.rows;
    // A page of only arrows takes as little of Kobo's rows' room as it can.
    const int margin = rows > 0 ? kNtGridMargin : 0;
    m_layout->setContentsMargins(kNtGridMargin, margin, kNtGridMargin, margin);
    const int first = nt_page_first_tile(m_plan, m_page, columns);
    for (int i = 0; i < rows * columns && first + i < tiles.size(); i++) {
        m_layout->addWidget(tiles.at(first + i), i / columns, 1 + i % columns, Qt::AlignTop | Qt::AlignLeft);
        tiles.at(first + i)->show();
    }
    // Rows keep their height on a short last page, so the arrows stay where they are.
    if (m_pages > 1)
        for (int row = 0; row < rows; row++) m_layout->setRowMinimumHeight(row, tileHeight);
    // Tiles at their largest may not fill the width (two per row, say); empty
    // columns on both sides keep them centred.
    m_layout->setColumnStretch(0, 1);
    m_layout->setColumnStretch(columns + 1, 1);
    m_layout->setRowStretch(rows + 1, 1);

    m_nav->setVisible(m_pages > 1);
    if (m_pages > 1) {
        m_layout->addWidget(m_nav, rows, 0, 1, columns + 2);
        m_prev->setPressEnabled(m_page > 0);
        m_next->setPressEnabled(m_page + 1 < m_pages);
        m_pageLabel->setText(QString::number(m_page + 1) + QStringLiteral(" / ") + QString::number(m_pages));
    }
    // Kobo's kept rows report a minimum height of 0, so without this the page
    // layout may give them the grid's room and overlap the tiles.
    setMinimumHeight(m_layout->minimumSize().height());
}

// Tiles have a fixed size, so a grid given less height than it needs draws them
// over each other, and one that runs past the screen hides its last rows and
// arrows. Either is a bug in the room estimates above, so this logs it. A grid
// pushed down after it was planned (Kobo's covers arriving late) plans again.
void NtGrid::warnIfSqueezed() {
    const int needed = m_layout->minimumSize().height();
    if (isVisible() && height() < needed) NT_LOG("warning: grid squeezed: %d px for %d px of tiles", height(), needed);
    const int overflow = mapTo(m_homeView, QPoint(0, height())).y() - m_homeView->height();
    if (isVisible() && overflow > 0) {
        NT_LOG("warning: grid squeezed off the screen by %d px; planning again", overflow);
        relayout();
    }
    NT_DBG("grid at y %d, %d px (needs %d)", y(), height(), needed);
    for (const QWidget *r : m_koboRows)
        NT_DBG("Kobo %s at y %d, %d px (asks %d)", qPrintable(r->objectName()), r->y(), r->height(), nt_row_height(r));
}

void NtGrid::turnPage(int step) {
    const int page = qBound(0, m_page + step, m_pages - 1);
    if (page == m_page) return;
    m_page = page;
    relayout();
}

bool NtGrid::tileSwiped(int distance) {
    if (m_pages <= 1 || distance == 0) return false;
    turnPage(distance < 0 ? 1 : -1);
    return true;
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
        m_marked = nullptr;
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

}   // namespace

void nt_install_grid(QWidget *homeView) {
    if (nt_log_verbose) nt_log_tree(homeView, 0);
    QWidget *container = nt_find_main_container(homeView);
    if (!container) {
        NT_LOG("warning: mainContainer with row1 not found; not inserting the grid");
        return;
    }
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(container->layout());
    if (!mainLayout) {
        NT_LOG("warning: mainContainer has no QVBoxLayout; not inserting");
        return;
    }
    const bool replacing = nt_remove_previous_grid(container, mainLayout);
    const NtSettings settings = nt_settings();
    const bool hasAppTiles = nt_has_app_tiles(settings);
    const int keep = nt_kobo_rows_to_keep(settings.koboRows, hasAppTiles);
    const QList<QWidget *> kept = nt_keep_rows(container, keep);

    // The settings tile alone fits in the empty half of Kobo's top row, where
    // it covers none of Kobo's rows.
    NtGrid *grid = new NtGrid(homeView, container, settings);
    grid->shareFirstPage(kept);
    if (!hasAppTiles) grid->mayUseTopRow(mainLayout);
    grid->build();
    const int index = keep > 0 ? 1 : 0;
    mainLayout->insertWidget(index, grid);
    if (keep < 3) {
        QSpacerItem *sp = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        mainLayout->addItem(sp);
        nt_stretches().insert(mainLayout, sp);
    }
    if (replacing) grid->show();
    NT_LOG("inserted grid at mainLayout index %d (Kobo rows kept=%d)", index, kept.size());
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
