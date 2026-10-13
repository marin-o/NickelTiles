#ifndef NT_SETTINGS_PAGE_H
#define NT_SETTINGS_PAGE_H

// The on-device settings page, a full-size page over the home view with
// Cancel and Save at the bottom. It starts below Nickel's status bar, which is
// drawn over the top of the home view. Leaving the home screen closes it
// without saving.

#include <functional>

#include <QStringList>
#include <QWidget>

// `labels` are the tiles that can be shown or hidden. Save writes the changed
// settings and then calls `onSaved`.
void nt_open_settings(QWidget *homeView, const QStringList &labels, std::function<void()> onSaved);

#endif
