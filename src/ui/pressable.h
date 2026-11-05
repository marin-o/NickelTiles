#ifndef NT_PRESSABLE_H
#define NT_PRESSABLE_H

// Base class for tappable widgets. Nickel delivers touches as QTouchEvent only
// (no synthesized mouse events), so this handles touch as well as mouse: the
// widget repaints while pressed and gets tapped() when the finger lifts inside
// it, or longPressed() after a hold when long presses are enabled.

#include <QTouchEvent>
#include <QWidget>

// Horizontal movement in px if the touch that just ended was a swipe (mostly
// sideways and long enough), else 0.
int nt_swipe_distance(const QTouchEvent *touch);

class NtPressable : public QWidget {
public:
    // `longPressMs` > 0 enables long presses; a long press is not also a tap.
    explicit NtPressable(QWidget *parent, int longPressMs = 0);

    bool isPressed() const { return m_pressed; }

    // A disabled widget ignores presses (and draws itself as disabled).
    void setPressEnabled(bool enabled);
    bool isPressEnabled() const { return m_enabled; }

protected:
    virtual void tapped() = 0;
    virtual void longPressed() {}
    // A swipe that started on this widget. Returning true means it was used, so
    // it is not also a tap.
    virtual bool swiped(int /*distance*/) { return false; }

    bool event(QEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void press();
    void release(bool inside);

    int m_longPressMs;
    int m_timer = 0;
    bool m_pressed = false;
    bool m_longPressFired = false;
    bool m_enabled = true;
};

#endif
