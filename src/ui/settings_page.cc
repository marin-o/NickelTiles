#include "ui/settings_page.h"

#include <QBoxLayout>
#include <QEvent>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPoint>
#include <QStringList>

#include <utility>

#include "core/settings_model.h"
#include "core/util.h"
#include "kobo/addons.h"
#include "kobo/nickel.h"
#include "ui/controls.h"
#include "ui/grid.h"
#include "ui/settings_rules.h"
#include "ui/settings_tile_list.h"
#include "ui/ui.h"

namespace {

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

    void stepTileSize(int step) {
        m_state.tilePx = nt_stepped_tile_size(m_state, step, nt_grid_width());
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
            m_state.recentSource = nt_next_source(m_state.recentSource);
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
                QStringLiteral("Off keeps as many of Kobo's home widgets as fit"));
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
        if (m_state.columns < kNtMinColumns) m_state.columns = kNtMinColumns;
        const NtSizeLimits limits = nt_size_limits(m_state, nt_grid_width());
        m_colsValue->setText(QString::number(m_state.columns));
        m_pxValue->setText(m_state.tilePx > 0 ? QString::number(limits.tilePx) : QStringLiteral("Auto"));
        m_pxAuto->setPressEnabled(m_state.tilePx > 0);
        m_colsDown->setPressEnabled(limits.canRemoveColumn);
        m_colsUp->setPressEnabled(limits.canAddColumn);
        m_pxDown->setPressEnabled(limits.canShrinkTile);
        m_pxUp->setPressEnabled(limits.canGrowTile);
        m_recent->setChecked(m_state.recent);
        m_source->setText(nt_source_name(m_state.recentSource));
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
