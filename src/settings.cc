#include "settings.h"

#include <QBoxLayout>
#include <QByteArray>
#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QTouchEvent>

#include <string.h>

#include "config.h"
#include "nmentries.h"
#include "util.h"

// Everything here is drawn for e-ink: black on white, no grey, a pressed
// control is inverted until the finger lifts. Nickel delivers touches as
// QTouchEvent only (no synthesized mouse), as for the tiles.

namespace {

const int kTilesPerPage = 8;    // two columns of four

QFont nt_font(const QWidget *w, int px, bool bold = false) {
    QFont f = w->font();
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}

// A tappable control. Kind Button: rounded box with centred text. Kind Check:
// a check box followed by the text, left aligned (a row in the tile list).
// Kind Toggle: a fixed-width pill reading On (filled) or Off (outlined).
class NtControl : public QWidget {
public:
    enum Kind { Button, Check, Toggle };

    NtControl(QWidget *parent, Kind kind, const QString &text, std::function<void()> onTap)
        : QWidget(parent), m_kind(kind), m_text(text), m_onTap(onTap), m_pressed(false), m_checked(false), m_enabled(true) {
        setAttribute(Qt::WA_AcceptTouchEvents, true);
        if (kind == Toggle) setFixedSize(150, 72);
        else if (kind == Check) setFixedHeight(76);
        else setFixedHeight(72);
    }
    void setText(const QString &text) { m_text = text; update(); }
    void setChecked(bool checked) { m_checked = checked; update(); }
    void setControlEnabled(bool enabled) { m_enabled = enabled; update(); }

protected:
    bool event(QEvent *e) override {
        switch (e->type()) {
        case QEvent::TouchBegin:
        case QEvent::MouseButtonPress:
            e->accept();
            if (m_enabled) { m_pressed = true; repaint(); }
            return true;
        case QEvent::TouchUpdate:
            e->accept();
            return true;
        case QEvent::TouchEnd:
        case QEvent::MouseButtonRelease: {
            e->accept();
            bool inside = true;
            if (e->type() == QEvent::TouchEnd) {
                const QList<QTouchEvent::TouchPoint> pts = static_cast<QTouchEvent*>(e)->touchPoints();
                inside = pts.isEmpty();
                for (const QTouchEvent::TouchPoint &p : pts)
                    if (rect().contains(p.pos().toPoint())) inside = true;
            } else {
                inside = rect().contains(static_cast<QMouseEvent*>(e)->pos());
            }
            const bool was = m_pressed;
            m_pressed = false;
            repaint();
            if (was && inside && m_enabled && m_onTap) m_onTap();
            return true;
        }
        case QEvent::TouchCancel:
            e->accept();
            m_pressed = false;
            repaint();
            return true;
        default:
            return QWidget::event(e);
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), Qt::white);
        const QRect box = rect().adjusted(3, 3, -3, -3);
        if (m_kind == Check) {
            const int side = 44;
            QRect cb(6, (height() - side) / 2, side, side);
            QRect row = rect().adjusted(0, 4, 0, -4);
            if (m_pressed) p.fillRect(row, Qt::black);
            const QColor fg = m_pressed ? Qt::white : Qt::black;
            p.setPen(QPen(fg, 4));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(cb, 6, 6);
            if (m_checked) {
                p.setPen(QPen(fg, 7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                QPolygon tick;
                tick << QPoint(cb.left() + 10, cb.center().y() + 1)
                     << QPoint(cb.left() + 19, cb.bottom() - 10)
                     << QPoint(cb.right() - 8, cb.top() + 10);
                p.drawPolyline(tick);
            }
            p.setPen(fg);
            p.setFont(nt_font(this, 28));
            p.drawText(QRect(cb.right() + 18, 0, width() - cb.right() - 24, height()),
                       Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(m_text, Qt::ElideRight, width() - cb.right() - 24));
            return;
        }
        bool filled = m_pressed;
        QString text = m_text;
        if (m_kind == Toggle) {
            filled = m_checked != m_pressed;
            text = m_checked ? QStringLiteral("On") : QStringLiteral("Off");
        }
        if (!m_enabled) {
            p.setPen(QPen(Qt::black, 2, Qt::DotLine));
            p.setBrush(Qt::NoBrush);
        } else {
            p.setPen(QPen(Qt::black, 4));
            p.setBrush(filled ? QBrush(Qt::black) : QBrush(Qt::NoBrush));
        }
        p.drawRoundedRect(box, 16, 16);
        p.setPen(filled && m_enabled ? Qt::white : Qt::black);
        p.setFont(nt_font(this, 30, true));
        p.drawText(box, Qt::AlignCenter, text);
    }

private:
    Kind m_kind;
    QString m_text;
    std::function<void()> m_onTap;
    bool m_pressed, m_checked, m_enabled;
};

struct Source { const char *value; const char *name; };
const Source kSources[] = {
    {"auto", "Any app"}, {"koreader", "KOReader"}, {"plato", "Plato"}, {"nickel", "Kobo reader"},
};
const int kSourceCount = sizeof(kSources) / sizeof(kSources[0]);

class NtSettingsView : public QWidget {
public:
    NtSettingsView(QWidget *view, const QStringList &labels, std::function<void()> onSaved)
        : QWidget(view), m_view(view), m_labels(labels), m_onSaved(onSaved), m_page(0) {
        setObjectName(QStringLiteral("ntSettings"));
        setAttribute(Qt::WA_AcceptTouchEvents, true);
        load();
        build();
        refresh();
        // Leaving the home screen (Nickel's own navigation) closes the page
        // without saving, so it can never be left behind.
        view->installEventFilter(this);
    }

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
        case QEvent::TouchBegin: case QEvent::TouchUpdate: case QEvent::TouchEnd: case QEvent::TouchCancel:
        case QEvent::MouseButtonPress: case QEvent::MouseButtonRelease:
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
    struct State {
        int columns, tilePx, source;
        bool recent, sleepCover, hideRows;
        QSet<QString> hidden;
    };

    void load() {
        State &s = m_initial;
        s.columns = nt_config_int("nt_columns", 5);
        s.tilePx = nt_config_int("nt_tile_px", 180);
        s.recent = nt_config_bool("nt_recent", true);
        s.sleepCover = nt_config_bool("nt_sleep_cover", true);
        s.hideRows = nt_config_bool("nt_hide_rows", true);
        const char *src = nt_config_get("nt_recent_source");
        s.source = 0;
        for (int i = 0; i < kSourceCount; i++)
            if (src && !strcmp(src, kSources[i].value)) s.source = i;
        for (const nt_entry_t *e = nt_config_entries(); e; e = e->next)
            if (!strcmp(e->key, "hide")) s.hidden.insert(QString::fromUtf8(e->val).trimmed());
        m_state = s;
    }

    // Would `columns` tiles of `px` fit across the home view?
    bool fits(int columns, int px) const {
        const int width = m_view->width() > 0 ? m_view->width() : 1072;
        return columns * px + (columns - 1) * 16 + 48 <= width;
    }

    QLabel *label(const QString &text, int px, bool bold = false) {
        QLabel *l = new QLabel(text, this);
        l->setFont(nt_font(this, px, bold));
        l->setWordWrap(true);
        return l;
    }

    // One settings row: name (and an optional note under it) on the left,
    // controls on the right.
    QHBoxLayout *row(QVBoxLayout *into, const QString &name, const QString &note = QString()) {
        QHBoxLayout *h = new QHBoxLayout();
        h->setSpacing(14);
        QVBoxLayout *text = new QVBoxLayout();
        text->setSpacing(2);
        text->addWidget(label(name, 32));
        if (!note.isEmpty()) text->addWidget(label(note, 22));
        h->addLayout(text, 1);
        into->addLayout(h);
        return h;
    }

    NtControl *button(const QString &text, int width, std::function<void()> onTap) {
        NtControl *b = new NtControl(this, NtControl::Button, text, onTap);
        b->setFixedWidth(width);
        return b;
    }

    QLabel *value(QHBoxLayout *h) {
        QLabel *v = label(QString(), 32, true);
        v->setAlignment(Qt::AlignCenter);
        v->setFixedWidth(110);
        h->addWidget(v);
        return v;
    }

    // Nickel draws its status bar over the top of the home view; Kobo's own
    // home content (mainContainer) starts below it. Fallback: 110 px.
    int topInset() const {
        for (QWidget *c : m_view->findChildren<QWidget*>(QStringLiteral("mainContainer"))) {
            const int y = c->mapTo(m_view, QPoint(0, 0)).y();
            if (y > 0 && y < m_view->height() / 3) return y;
        }
        return 110;
    }

    void build() {
        QVBoxLayout *v = new QVBoxLayout(this);
        const int inset = topInset();
        NT_LOG("settings: top inset %d", inset);
        v->setContentsMargins(32, inset + 8, 32, 24);
        v->setSpacing(14);

        v->addWidget(label(QStringLiteral("Tiles settings"), 40, true));

        QHBoxLayout *h = row(v, QStringLiteral("Tiles per row"));
        m_colsDown = button(QStringLiteral("−"), 90, [this]() { m_state.columns--; refresh(); });
        h->addWidget(m_colsDown);
        m_colsValue = value(h);
        m_colsUp = button(QStringLiteral("+"), 90, [this]() { m_state.columns++; refresh(); });
        h->addWidget(m_colsUp);

        h = row(v, QStringLiteral("Tile size"));
        m_pxDown = button(QStringLiteral("−"), 90, [this]() { m_state.tilePx -= 20; refresh(); });
        h->addWidget(m_pxDown);
        m_pxValue = value(h);
        m_pxUp = button(QStringLiteral("+"), 90, [this]() { m_state.tilePx += 20; refresh(); });
        h->addWidget(m_pxUp);

        h = row(v, QStringLiteral("Continue reading tile"), QStringLiteral("Your most recent book, opened in the app that read it"));
        m_recent = new NtControl(this, NtControl::Toggle, QString(), [this]() { m_state.recent = !m_state.recent; refresh(); });
        h->addWidget(m_recent);

        h = row(v, QStringLiteral("Most recent book from"));
        m_source = button(QString(), 314, [this]() { m_state.source = (m_state.source + 1) % kSourceCount; refresh(); });
        h->addWidget(m_source);

        h = row(v, QStringLiteral("Sleep screen shows that book"),
                QStringLiteral("Also turn on Kobo's “Show book covers full screen”"));
        m_sleep = new NtControl(this, NtControl::Toggle, QString(), [this]() { m_state.sleepCover = !m_state.sleepCover; refresh(); });
        h->addWidget(m_sleep);

        h = row(v, QStringLiteral("Hide Kobo's home rows"), QStringLiteral("Off shows Kobo's own home widgets under the tiles"));
        m_hideRows = new NtControl(this, NtControl::Toggle, QString(), [this]() { m_state.hideRows = !m_state.hideRows; refresh(); });
        h->addWidget(m_hideRows);

        v->addSpacing(10);
        QHBoxLayout *listHead = new QHBoxLayout();
        listHead->addWidget(label(QStringLiteral("Tiles to show"), 32, true), 1);
        m_prev = button(QStringLiteral("‹"), 90, [this]() { m_page--; refresh(); });
        m_pageLabel = label(QString(), 26);
        m_pageLabel->setAlignment(Qt::AlignCenter);
        m_pageLabel->setFixedWidth(120);
        m_next = button(QStringLiteral("›"), 90, [this]() { m_page++; refresh(); });
        listHead->addWidget(m_prev);
        listHead->addWidget(m_pageLabel);
        listHead->addWidget(m_next);
        v->addLayout(listHead);

        QGridLayout *grid = new QGridLayout();
        grid->setHorizontalSpacing(24);
        grid->setVerticalSpacing(4);
        for (int i = 0; i < kTilesPerPage; i++) {
            NtControl *c = new NtControl(this, NtControl::Check, QString(), [this, i]() {
                const int index = m_page * kTilesPerPage + i;
                if (index >= m_labels.size()) return;
                const QString &l = m_labels.at(index);
                if (m_state.hidden.contains(l)) m_state.hidden.remove(l); else m_state.hidden.insert(l);
                refresh();
            });
            grid->addWidget(c, i % (kTilesPerPage / 2), i / (kTilesPerPage / 2));
            m_checks << c;
        }
        v->addLayout(grid);
        v->addStretch(1);

        QHBoxLayout *bottom = new QHBoxLayout();
        bottom->setSpacing(24);
        NtControl *cancel = new NtControl(this, NtControl::Button, QStringLiteral("Cancel"), [this]() { close(false); });
        NtControl *save = new NtControl(this, NtControl::Button, QStringLiteral("Save"), [this]() { close(true); });
        cancel->setFixedHeight(90);
        save->setFixedHeight(90);
        bottom->addWidget(cancel, 1);
        bottom->addWidget(save, 1);
        v->addLayout(bottom);
    }

    void refresh() {
        if (m_state.columns < 2) m_state.columns = 2;
        if (m_state.tilePx < 120) m_state.tilePx = 120;
        if (m_state.tilePx > 300) m_state.tilePx = 300;
        m_colsValue->setText(QString::number(m_state.columns));
        m_pxValue->setText(QString::number(m_state.tilePx));
        m_colsDown->setControlEnabled(m_state.columns > 2);
        m_colsUp->setControlEnabled(fits(m_state.columns + 1, m_state.tilePx));
        m_pxDown->setControlEnabled(m_state.tilePx > 120);
        m_pxUp->setControlEnabled(m_state.tilePx < 300 && fits(m_state.columns, m_state.tilePx + 20));
        m_recent->setChecked(m_state.recent);
        m_source->setText(QString::fromUtf8(kSources[m_state.source].name));
        m_source->setControlEnabled(m_state.recent);
        m_sleep->setChecked(m_state.sleepCover);
        m_hideRows->setChecked(m_state.hideRows);

        const int pages = m_labels.isEmpty() ? 1 : (m_labels.size() + kTilesPerPage - 1) / kTilesPerPage;
        if (m_page >= pages) m_page = pages - 1;
        if (m_page < 0) m_page = 0;
        m_prev->setVisible(pages > 1);
        m_next->setVisible(pages > 1);
        m_pageLabel->setVisible(pages > 1);
        m_prev->setControlEnabled(m_page > 0);
        m_next->setControlEnabled(m_page + 1 < pages);
        m_pageLabel->setText(QString::number(m_page + 1) + QStringLiteral(" / ") + QString::number(pages));
        for (int i = 0; i < m_checks.size(); i++) {
            const int index = m_page * kTilesPerPage + i;
            NtControl *c = m_checks.at(i);
            if (index < m_labels.size()) {
                c->setText(m_labels.at(index));
                c->setChecked(!m_state.hidden.contains(m_labels.at(index)));
                c->setVisible(true);
            } else {
                c->setVisible(false);
            }
        }
    }

    static bool setValue(const char *key, const QString &value) {
        const QByteArray v = value.toUtf8();
        const char *vals[] = { v.constData() };
        return nt_config_set(key, vals, 1);
    }

    bool save() {
        const State &a = m_initial, &b = m_state;
        bool ok = true;
        if (a.columns != b.columns) ok &= setValue("nt_columns", QString::number(b.columns));
        if (a.tilePx != b.tilePx) ok &= setValue("nt_tile_px", QString::number(b.tilePx));
        if (a.recent != b.recent) ok &= setValue("nt_recent", QString::number(b.recent ? 1 : 0));
        if (a.source != b.source) ok &= setValue("nt_recent_source", QString::fromUtf8(kSources[b.source].value));
        if (a.sleepCover != b.sleepCover) ok &= setValue("nt_sleep_cover", QString::number(b.sleepCover ? 1 : 0));
        if (a.hideRows != b.hideRows) ok &= setValue("nt_hide_rows", QString::number(b.hideRows ? 1 : 0));
        if (a.hidden != b.hidden) {
            // Keep hide: lines for tiles that are not listed now (an entry that
            // was removed and may come back), in the order they had.
            QList<QByteArray> hidden;
            for (const nt_entry_t *e = nt_config_entries(); e; e = e->next) {
                if (strcmp(e->key, "hide")) continue;
                const QString l = QString::fromUtf8(e->val).trimmed();
                if (!m_labels.contains(l)) hidden << l.toUtf8();
            }
            for (const QString &l : m_labels)
                if (b.hidden.contains(l)) hidden << l.toUtf8();
            QList<const char*> vals;
            for (const QByteArray &h : hidden) vals << h.constData();
            ok &= nt_config_set("hide", vals.isEmpty() ? NULL : &vals[0], vals.size());
        }
        NT_LOG("settings: saved ok=%d columns=%d px=%d recent=%d source=%s sleep=%d hide_rows=%d hidden=%d",
            ok ? 1 : 0, b.columns, b.tilePx, b.recent ? 1 : 0, kSources[b.source].value, b.sleepCover ? 1 : 0,
            b.hideRows ? 1 : 0, b.hidden.size());
        return ok;
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
    State m_initial, m_state;
    int m_page;
    NtControl *m_colsDown, *m_colsUp, *m_pxDown, *m_pxUp, *m_recent, *m_source, *m_sleep, *m_hideRows, *m_prev, *m_next;
    QLabel *m_colsValue, *m_pxValue, *m_pageLabel;
    QList<NtControl*> m_checks;
};

} // namespace

void nt_open_settings(QWidget *view, const QStringList &labels, std::function<void()> onSaved) {
    if (!view) return;
    if (QWidget *open = view->findChild<QWidget*>(QStringLiteral("ntSettings"), Qt::FindDirectChildrenOnly)) {
        open->raise();
        return;
    }
    NtSettingsView *s = new NtSettingsView(view, labels, onSaved);
    s->setGeometry(view->rect());
    s->raise();
    s->show();
    NT_LOG("settings: opened (%d tiles listed) %dx%d", labels.size(), view->width(), view->height());
    if (nt_log_verbose)
        QTimer::singleShot(800, s, [s, view]() {
            view->window()->grab().save(QStringLiteral(NT_CONFIG_DIR "/settings-window.png"), "PNG");
        });
}
