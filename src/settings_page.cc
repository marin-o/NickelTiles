#include "settings_page.h"

#include <QBoxLayout>
#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QList>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QRect>
#include <QSet>
#include <QStringList>

#include <utility>

#include "addons.h"
#include "grid.h"
#include "nickel.h"
#include "pressable.h"
#include "settings_model.h"
#include "ui.h"
#include "util.h"

namespace {

const int kTilesPerPage = 6;   // two columns of three

// A tappable control. Kind Button: rounded box with centred text. Kind Check:
// a check box followed by the text, left aligned (a row in the tile list).
// Kind Toggle: a fixed-width pill reading On (filled) or Off (outlined).
class NtControl : public NtPressable {
public:
    enum Kind { Button, Check, Toggle };

    NtControl(QWidget *parent, Kind kind, const QString &text, std::function<void()> onTap)
        : NtPressable(parent), m_kind(kind), m_text(text), m_onTap(std::move(onTap)), m_checked(false) {
        if (kind == Toggle)
            setFixedSize(150, 72);
        else if (kind == Check)
            setFixedHeight(76);
        else
            setFixedHeight(72);
    }
    void setText(const QString &text) {
        m_text = text;
        update();
    }
    void setChecked(bool checked) {
        m_checked = checked;
        update();
    }

protected:
    void tapped() override {
        if (m_onTap) m_onTap();
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), Qt::white);
        if (m_kind == Check)
            paintCheck(p);
        else
            paintBox(p);
    }

private:
    void paintCheck(QPainter &p) {
        const int side = 44;
        const QRect cb(6, (height() - side) / 2, side, side);
        const QRect row = rect().adjusted(0, 4, 0, -4);
        const bool pressed = isPressed();
        if (pressed) p.fillRect(row, Qt::black);
        const QColor fg = pressed ? Qt::white : Qt::black;
        p.setPen(QPen(fg, 4));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(cb, 6, 6);
        if (m_checked) {
            p.setPen(QPen(fg, 7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPolygon tick;
            tick << QPoint(cb.left() + 10, cb.center().y() + 1) << QPoint(cb.left() + 19, cb.bottom() - 10)
                 << QPoint(cb.right() - 8, cb.top() + 10);
            p.drawPolyline(tick);
        }
        p.setPen(fg);
        p.setFont(nt_font(this, 28));
        p.drawText(QRect(cb.right() + 18, 0, width() - cb.right() - 24, height()), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_text, Qt::ElideRight, width() - cb.right() - 24));
    }

    void paintBox(QPainter &p) {
        const QRect box = rect().adjusted(3, 3, -3, -3);
        const bool enabled = isPressEnabled();
        bool filled = isPressed();
        QString text = m_text;
        if (m_kind == Toggle) {
            filled = m_checked != isPressed();
            text = m_checked ? QStringLiteral("On") : QStringLiteral("Off");
        }
        // A disabled control is drawn dotted, so it is not mistaken for a filled one.
        if (!enabled) {
            p.setPen(QPen(Qt::black, 2, Qt::DotLine));
            p.setBrush(Qt::NoBrush);
        } else {
            p.setPen(QPen(Qt::black, 4));
            p.setBrush(filled ? QBrush(Qt::black) : QBrush(Qt::NoBrush));
        }
        p.drawRoundedRect(box, 16, 16);
        p.setPen(filled && enabled ? Qt::white : Qt::black);
        p.setFont(nt_font(this, 30, true));
        p.drawText(box, Qt::AlignCenter, text);
    }

    Kind m_kind;
    QString m_text;
    std::function<void()> m_onTap;
    bool m_checked;
};

NtControl *ntButton(QWidget *parent, const QString &text, int width, std::function<void()> onTap) {
    NtControl *b = new NtControl(parent, NtControl::Button, text, std::move(onTap));
    b->setFixedWidth(width);
    return b;
}

QLabel *ntLabel(QWidget *parent, const QString &text, int px, bool bold = false) {
    QLabel *l = new QLabel(text, parent);
    l->setFont(nt_font(parent, px, bold));
    l->setWordWrap(true);
    return l;
}

// Display names, in the order the source button cycles through them.
struct SourceName {
    NtRecentSource source;
    const char *name;
};
const SourceName kSources[] = {
    {NtRecentSource::Any, "Any app"},
    {NtRecentSource::Koreader, "KOReader"},
    {NtRecentSource::Plato, "Plato"},
    {NtRecentSource::Nickel, "Kobo reader"},
};
const int kSourceCount = sizeof(kSources) / sizeof(kSources[0]);

int sourceIndex(NtRecentSource source) {
    for (int i = 0; i < kSourceCount; i++)
        if (kSources[i].source == source) return i;
    return 0;
}

NtRecentSource nextSource(NtRecentSource source) {
    return kSources[(sourceIndex(source) + 1) % kSourceCount].source;
}

// The paged "Tiles to show" list: one page of check rows at a time. A checked
// row is a tile that is shown; the set of hidden labels is edited in place.
class NtTileList {
public:
    NtTileList(QWidget *parent, const QStringList &labels, const QStringList &hidden)
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

    void addTo(QVBoxLayout *v) {
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

    void refresh() {
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

    // The hidden list in the order it is written. Hide: lines for tiles that
    // are not listed now are kept (an entry that was removed may come back),
    // in the order they had; then the listed tiles in list order.
    QStringList hiddenAfter(const QStringList &initialHidden) const {
        if (m_hidden == QSet<QString>::fromList(initialHidden)) return initialHidden;
        QStringList out;
        for (const QString &l : initialHidden)
            if (!m_labels.contains(l)) out << l;
        for (const QString &l : m_labels)
            if (m_hidden.contains(l)) out << l;
        return out;
    }

private:
    // Flips the tile in slot `slot` of the current page.
    void toggleTile(int slot) {
        const int index = m_page * kTilesPerPage + slot;
        if (index >= m_labels.size()) return;
        const QString &l = m_labels.at(index);
        if (m_hidden.contains(l))
            m_hidden.remove(l);
        else
            m_hidden.insert(l);
        refresh();
    }

    QStringList m_labels;
    QSet<QString> m_hidden;
    int m_page;
    QLabel *m_title, *m_pageLabel;
    NtControl *m_prev, *m_next;
    QList<NtControl *> m_checks;
};

class NtSettingsView : public QWidget {
public:
    NtSettingsView(QWidget *view, const QStringList &labels, std::function<void()> onSaved)
        : QWidget(view), m_view(view), m_labels(labels), m_onSaved(std::move(onSaved)), m_tiles(nullptr) {
        setObjectName(QStringLiteral("ntSettings"));
        setAttribute(Qt::WA_AcceptTouchEvents, true);
        load();
        build();
        refresh();
        // Leaving the home screen (Nickel's own navigation) closes the page
        // without saving, so it can never be left behind.
        view->installEventFilter(this);
    }

    ~NtSettingsView() override { delete m_tiles; }

    bool eventFilter(QObject *watched, QEvent *e) override {
        if (watched == m_view && e->type() == QEvent::Hide && !isHidden()) {
            // isVisible() is already false here: when an ancestor hides, Qt clears
            // the visible state of the whole subtree before sending the Hide events.
            NT_LOG("settings: home view hidden; closing without saving");
            close(false);
        }
        return QWidget::eventFilter(watched, e);
    }

protected:
    // Swallow touches that land between controls so nothing underneath reacts.
    bool event(QEvent *e) override {
        switch (e->type()) {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            e->accept();
            return true;
        default:
            return QWidget::event(e);
        }
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
    }

private:
    void load() {
        m_initial = nt_settings();
        m_state = m_initial;
    }

    bool fits(int columns, int px) const { return nt_tiles_fit(columns, px, nt_grid_width()); }

    // The fixed size `step` px away from the current one; leaving automatic
    // starts from the size automatic gives now.
    void stepTileSize(int step) {
        const int current = nt_tile_size(m_state.columns, m_state.tilePx, nt_grid_width());
        m_state.tilePx = qBound(kNtTileMinPx, (current + step) / 20 * 20, kNtTileMaxPx);
        refresh();
    }

    // One settings row: name (and an optional note under it) on the left,
    // controls on the right. If noteOut is given, it receives the note label
    // so the note can be set later; the label is made even when the note is
    // empty.
    QHBoxLayout *row(QVBoxLayout *into, const QString &name, const QString &note = QString(),
                     QLabel **noteOut = nullptr) {
        QHBoxLayout *h = new QHBoxLayout();
        h->setSpacing(14);
        QVBoxLayout *text = new QVBoxLayout();
        text->setSpacing(2);
        text->addWidget(ntLabel(this, name, 32));
        if (!note.isEmpty() || noteOut) {
            QLabel *n = ntLabel(this, note, 22);
            text->addWidget(n);
            if (noteOut) *noteOut = n;
        }
        h->addLayout(text, 1);
        into->addLayout(h);
        return h;
    }

    NtControl *toggle(std::function<void()> onTap) {
        return new NtControl(this, NtControl::Toggle, QString(), std::move(onTap));
    }

    // A Check control in row `h` that flips *flag on each tap.
    NtControl *flagCheck(QHBoxLayout *h, const QString &text, int width, bool *flag) {
        NtControl *c = new NtControl(this, NtControl::Check, text, [this, flag]() {
            *flag = !*flag;
            refresh();
        });
        c->setFixedWidth(width);
        h->addWidget(c);
        return c;
    }

    QLabel *value(QHBoxLayout *h) {
        QLabel *v = ntLabel(this, QString(), 32, true);
        v->setAlignment(Qt::AlignCenter);
        v->setFixedWidth(110);
        h->addWidget(v);
        return v;
    }

    // Nickel draws its status bar over the top of the home view; Kobo's own
    // home content (mainContainer) starts below it. Fallback: 110 px.
    int topInset() const {
        for (QWidget *c : m_view->findChildren<QWidget *>(QStringLiteral("mainContainer"))) {
            const int y = c->mapTo(m_view, QPoint(0, 0)).y();
            if (y > 0 && y < m_view->height() / 3) return y;
        }
        return 110;
    }

    void build() {
        QVBoxLayout *v = new QVBoxLayout(this);
        const int inset = topInset();
        NT_DBG("settings: top inset %d", inset);
        v->setContentsMargins(32, inset + 8, 32, 24);
        v->setSpacing(14);

        v->addWidget(ntLabel(this, QStringLiteral("Tiles settings"), 40, true));
        buildSizeRows(v);
        buildRecentRows(v);
        buildRepairRow(v);
        buildTileList(v);

        QHBoxLayout *bottom = new QHBoxLayout();
        bottom->setSpacing(24);
        NtControl *cancel =
            new NtControl(this, NtControl::Button, QStringLiteral("Cancel"), [this]() { close(false); });
        NtControl *save = new NtControl(this, NtControl::Button, QStringLiteral("Save"), [this]() { close(true); });
        cancel->setFixedHeight(90);
        save->setFixedHeight(90);
        bottom->addWidget(cancel, 1);
        bottom->addWidget(save, 1);
        v->addLayout(bottom);
    }

    void buildSizeRows(QVBoxLayout *v) {
        QHBoxLayout *h = row(v, QStringLiteral("Tiles per row"));
        m_colsDown = ntButton(this, QStringLiteral("−"), 90, [this]() {
            m_state.columns--;
            refresh();
        });
        h->addWidget(m_colsDown);
        m_colsValue = value(h);
        m_colsUp = ntButton(this, QStringLiteral("+"), 90, [this]() {
            m_state.columns++;
            refresh();
        });
        h->addWidget(m_colsUp);

        h = row(v, QStringLiteral("Tile size"));
        m_pxAuto = ntButton(this, QStringLiteral("Auto"), 120, [this]() {
            m_state.tilePx = 0;
            refresh();
        });
        h->addWidget(m_pxAuto);
        m_pxDown = ntButton(this, QStringLiteral("−"), 90, [this]() { stepTileSize(-20); });
        h->addWidget(m_pxDown);
        m_pxValue = value(h);
        m_pxUp = ntButton(this, QStringLiteral("+"), 90, [this]() { stepTileSize(20); });
        h->addWidget(m_pxUp);
    }

    void buildRecentRows(QVBoxLayout *v) {
        QHBoxLayout *h = row(v, QStringLiteral("Continue reading tile"),
                             QStringLiteral("Your most recent book, opened in the app that read it"));
        m_recent = toggle([this]() {
            m_state.recent = !m_state.recent;
            refresh();
        });
        h->addWidget(m_recent);

        h = row(v, QStringLiteral("Most recent book from"));
        m_source = ntButton(this, QString(), 314, [this]() {
            m_state.recentSource = nextSource(m_state.recentSource);
            refresh();
        });
        h->addWidget(m_source);

        h = row(v, QStringLiteral("Tile shows"));
        m_infoCover = flagCheck(h, QStringLiteral("Cover"), 150, &m_state.recentCover);
        m_infoTitle = flagCheck(h, QStringLiteral("Title"), 140, &m_state.recentInfo.title);
        m_infoAuthor = flagCheck(h, QStringLiteral("Author"), 165, &m_state.recentInfo.author);
        m_infoProgress = flagCheck(h, QStringLiteral("Progress"), 185, &m_state.recentInfo.progress);

        h = row(v, QStringLiteral("Sleep screen shows that book"),
                QStringLiteral("Also turn on Kobo's “Show book covers full screen”"));
        m_sleep = toggle([this]() {
            m_state.sleepCover = !m_state.sleepCover;
            refresh();
        });
        h->addWidget(m_sleep);

        h = row(v, QStringLiteral("Hide Kobo's home rows"),
                QStringLiteral("Off shows Kobo's own home widgets under the tiles"));
        m_hideRows = toggle([this]() {
            m_state.hideRows = !m_state.hideRows;
            refresh();
        });
        h->addWidget(m_hideRows);
    }

    // Add-ons left switched off by a crash, with a button to turn them back on.
    void buildRepairRow(QVBoxLayout *v) {
        QHBoxLayout *h = row(v, QStringLiteral("Switched-off add-ons"), QString(), &m_repairNote);
        m_repair = ntButton(this, QStringLiteral("Repair"), 200, [this]() { repairAddons(); });
        h->addWidget(m_repair);
    }

    void repairAddons() {
        const QStringList restored = nt_restore_addons();
        refreshRepairRow();
        if (restored.isEmpty()) {
            nt_toast(QStringLiteral("Nothing to repair"), 2500);
            return;
        }
        QStringList names;
        for (const QString &library : restored) names << nt_addon_name(library);
        const QString done = names.join(QStringLiteral(", ")) + QStringLiteral(" turned back on.");
        const bool asked = nt_confirm(
            QStringLiteral("Add-ons repaired"), done + QStringLiteral(" They load the next time the reader restarts."),
            QStringLiteral("Restart now"), QStringLiteral("Later"), []() {
                if (!nt_reboot()) nt_toast(QStringLiteral("Could not restart; please restart the reader"), 4000);
            });
        if (!asked) nt_toast(done + QStringLiteral(" Restart the reader to load them."), 4000);
    }

    void refreshRepairRow() {
        const QStringList off = nt_switched_off_addons();
        QStringList names;
        for (const QString &library : off) names << nt_addon_name(library);
        if (off.isEmpty())
            m_repairNote->setText(QStringLiteral("None. A crash right after Kobo starts can switch add-ons off."));
        else if (!nt_addons_settled())
            m_repairNote->setText(names.join(QStringLiteral(", ")) +
                                  QStringLiteral(" (still starting; check again in half a minute)"));
        else
            m_repairNote->setText(names.join(QStringLiteral(", ")));
        // Before every failsafe window has passed, a renamed library may just be starting up.
        m_repair->setPressEnabled(!off.isEmpty() && nt_addons_settled());
    }

    void buildTileList(QVBoxLayout *v) {
        v->addSpacing(10);
        m_tiles = new NtTileList(this, m_labels, m_initial.hidden);
        m_tiles->addTo(v);
    }

    void refresh() {
        if (m_state.columns < 2) m_state.columns = 2;
        const int px = nt_tile_size(m_state.columns, m_state.tilePx, nt_grid_width());
        m_colsValue->setText(QString::number(m_state.columns));
        m_pxValue->setText(m_state.tilePx > 0 ? QString::number(px) : QStringLiteral("Auto"));
        m_pxAuto->setPressEnabled(m_state.tilePx > 0);
        m_colsDown->setPressEnabled(m_state.columns > 2);
        m_colsUp->setPressEnabled(fits(m_state.columns + 1, m_state.tilePx));
        m_pxDown->setPressEnabled(px > kNtTileMinPx);
        m_pxUp->setPressEnabled(px < kNtTileMaxPx && fits(m_state.columns, px + 20));
        m_recent->setChecked(m_state.recent);
        m_source->setText(QString::fromUtf8(kSources[sourceIndex(m_state.recentSource)].name));
        m_source->setPressEnabled(m_state.recent);
        m_infoCover->setChecked(m_state.recentCover);
        m_infoTitle->setChecked(m_state.recentInfo.title);
        m_infoAuthor->setChecked(m_state.recentInfo.author);
        m_infoProgress->setChecked(m_state.recentInfo.progress);
        m_sleep->setChecked(m_state.sleepCover);
        m_hideRows->setChecked(m_state.hideRows);
        refreshRepairRow();
        m_tiles->refresh();
    }

    bool save() {
        NtSettings after = m_state;
        after.hidden = m_tiles->hiddenAfter(m_initial.hidden);
        return nt_settings_save(m_initial, after);
    }

    void close(bool keep) {
        NT_LOG("settings: close keep=%d", keep ? 1 : 0);
        m_view->removeEventFilter(this);
        if (keep) {
            if (!save()) nt_toast(QStringLiteral("Could not save all settings"), 2500);
            if (m_onSaved) m_onSaved();
        }
        hide();
        deleteLater();
    }

    QWidget *m_view;
    QStringList m_labels;
    std::function<void()> m_onSaved;
    NtSettings m_initial;
    NtSettings m_state;   // edited copy; `hidden` is taken from m_tiles on save
    NtTileList *m_tiles;
    NtControl *m_colsDown, *m_colsUp, *m_pxAuto, *m_pxDown, *m_pxUp, *m_recent, *m_source, *m_sleep, *m_hideRows;
    NtControl *m_infoCover, *m_infoTitle, *m_infoAuthor, *m_infoProgress, *m_repair;
    QLabel *m_repairNote;
    QLabel *m_colsValue, *m_pxValue;
};

}   // namespace

void nt_open_settings(QWidget *view, const QStringList &labels, std::function<void()> onSaved) {
    if (!view) return;
    if (QWidget *open = view->findChild<QWidget *>(QStringLiteral("ntSettings"), Qt::FindDirectChildrenOnly)) {
        open->raise();
        return;
    }
    NtSettingsView *s = new NtSettingsView(view, labels, std::move(onSaved));
    s->setGeometry(view->rect());
    s->raise();
    s->show();
    NT_LOG("settings: opened (%d tiles listed) %dx%d", labels.size(), view->width(), view->height());
    nt_debug_screenshot(s, QStringLiteral("settings-window"), 800);
}
