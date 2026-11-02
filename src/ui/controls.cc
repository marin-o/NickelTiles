#include "ui/controls.h"

#include <QBrush>
#include <QColor>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QRect>

#include <utility>

#include "ui/ui.h"

NtControl::NtControl(QWidget *parent, Kind kind, const QString &text, std::function<void()> onTap)
    : NtPressable(parent), m_kind(kind), m_text(text), m_onTap(std::move(onTap)), m_checked(false) {
    if (kind == Toggle)
        setFixedSize(150, 72);
    else if (kind == Check)
        setFixedHeight(76);
    else
        setFixedHeight(72);
}

void NtControl::setText(const QString &text) {
    m_text = text;
    update();
}

void NtControl::setChecked(bool checked) {
    m_checked = checked;
    update();
}

void NtControl::tapped() {
    if (m_onTap) m_onTap();
}

void NtControl::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::white);
    if (m_kind == Check)
        paintCheck(p);
    else
        paintBox(p);
}

void NtControl::paintCheck(QPainter &p) {
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

void NtControl::paintBox(QPainter &p) {
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

NtControl *ntButton(QWidget *parent, const QString &text, int width, std::function<void()> onTap) {
    NtControl *b = new NtControl(parent, NtControl::Button, text, std::move(onTap));
    b->setFixedWidth(width);
    return b;
}

QLabel *ntLabel(QWidget *parent, const QString &text, int px, bool bold) {
    QLabel *l = new QLabel(text, parent);
    l->setFont(nt_font(parent, px, bold));
    l->setWordWrap(true);
    return l;
}
