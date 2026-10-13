#ifndef NT_UI_H
#define NT_UI_H

// Drawing helpers shared by the grid and the settings page. Everything is
// black on white for e-ink.

#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QStringList>
#include <QWidget>

QFont nt_font(const QWidget *w, int px, bool bold = false);

// Greedy word wrap into at most `maxLines` lines of `width` px; the last line
// takes the rest of the text, elided if it does not fit.
QStringList nt_wrap(const QString &text, const QFontMetrics &fm, int width, int maxLines);

// With nt_log:1, saves a picture of `w`'s window as NT_CONFIG_DIR/<name>.png
// after `delayMs` (for bug reports). Does nothing otherwise.
void nt_debug_screenshot(QWidget *w, const QString &name, int delayMs);

#endif
