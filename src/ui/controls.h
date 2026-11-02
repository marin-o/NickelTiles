#ifndef NT_CONTROLS_H
#define NT_CONTROLS_H

// The settings page's controls, drawn for e-ink: black on white, with a filled
// state instead of colour.

#include <functional>

#include <QLabel>
#include <QString>

#include "ui/pressable.h"

// A tappable control. Kind Button: rounded box with centred text. Kind Check:
// a check box followed by the text, left aligned (a row in the tile list).
// Kind Toggle: a fixed-width pill reading On (filled) or Off (outlined).
class NtControl : public NtPressable {
public:
    enum Kind { Button, Check, Toggle };

    NtControl(QWidget *parent, Kind kind, const QString &text, std::function<void()> onTap);
    void setText(const QString &text);
    void setChecked(bool checked);

protected:
    void tapped() override;
    void paintEvent(QPaintEvent *event) override;

private:
    void paintCheck(QPainter &p);
    void paintBox(QPainter &p);

    Kind m_kind;
    QString m_text;
    std::function<void()> m_onTap;
    bool m_checked;
};

// A Button control `width` px wide.
NtControl *ntButton(QWidget *parent, const QString &text, int width, std::function<void()> onTap);

// A word-wrapped label in the reading font.
QLabel *ntLabel(QWidget *parent, const QString &text, int px, bool bold = false);

#endif
