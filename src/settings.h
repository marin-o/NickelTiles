#ifndef NT_SETTINGS_H
#define NT_SETTINGS_H

#include <functional>

#include <QStringList>
#include <QWidget>

// On-device settings screen for the grid: a full-size page over the home view
// with Save and Cancel. Save writes the changed keys to the config and then
// calls onSaved (which rebuilds the grid). `labels` are the tiles that can be
// shown or hidden (NickelMenu entries and tile: lines).
void nt_open_settings(QWidget *view, const QStringList &labels, std::function<void()> onSaved);

#endif
