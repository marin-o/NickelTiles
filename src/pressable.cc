#include "pressable.h"

#include <QEvent>
#include <QList>
#include <QMouseEvent>
#include <QTimerEvent>
#include <QTouchEvent>

namespace {

// A touch end counts as a tap on this widget if any finger lifted inside it;
// with no points left (the touch was consumed) it counts as inside.
bool nt_touch_inside(const QWidget *w, const QTouchEvent *touch) {
    const QList<QTouchEvent::TouchPoint> &points = touch->touchPoints();
    if (points.isEmpty()) return true;
    for (const QTouchEvent::TouchPoint &p : points)
        if (w->rect().contains(p.pos().toPoint())) return true;
    return false;
}

}   // namespace

NtPressable::NtPressable(QWidget *parent, int longPressMs) : QWidget(parent), m_longPressMs(longPressMs) {
    setAttribute(Qt::WA_AcceptTouchEvents, true);
}

void NtPressable::setPressEnabled(bool enabled) {
    m_enabled = enabled;
    update();
}

void NtPressable::press() {
    if (m_timer) {
        killTimer(m_timer);
        m_timer = 0;
    }
    m_pressed = true;
    repaint();   // immediately: e-ink does not show a deferred update in time
    m_longPressFired = false;
    if (m_longPressMs > 0) m_timer = startTimer(m_longPressMs);
}

void NtPressable::release(bool inside) {
    if (m_timer) {
        killTimer(m_timer);
        m_timer = 0;
    }
    const bool wasPressed = m_pressed;
    m_pressed = false;
    repaint();
    // A long press that already fired is not also a tap.
    if (m_longPressFired) {
        m_longPressFired = false;
        return;
    }
    if (wasPressed && inside && m_enabled) tapped();
}

void NtPressable::timerEvent(QTimerEvent *event) {
    if (event->timerId() != m_timer) return;
    killTimer(m_timer);
    m_timer = 0;
    m_longPressFired = true;
    m_pressed = false;
    repaint();
    longPressed();
}

bool NtPressable::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::MouseButtonPress:
        event->accept();
        if (m_enabled) press();
        return true;
    case QEvent::TouchUpdate:
        event->accept();
        return true;
    case QEvent::TouchEnd:
        event->accept();
        release(nt_touch_inside(this, static_cast<QTouchEvent *>(event)));
        return true;
    case QEvent::TouchCancel:
        event->accept();
        release(false);
        return true;
    case QEvent::MouseButtonRelease:
        event->accept();
        release(rect().contains(static_cast<QMouseEvent *>(event)->pos()));
        return true;
    default:
        return QWidget::event(event);
    }
}
