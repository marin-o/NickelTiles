#ifndef NT_GRID_H
#define NT_GRID_H

// The tile grid on the home page. Home page tree on firmware 4.45.23697 (built
// by Ui_HomePageView::setupUi; see docs/homepage-map.md):
//   HomePageView > verticalLayout > mainContainer > mainLayout (QVBoxLayout)
//     [0] row1: current read, [1] row2: My Books / Top picks, [2] row3: Kobo store links
// The grid goes first in mainLayout; with nt_hide_rows the three rows are hidden.

#include <QStringList>
#include <QWidget>

// Inserts the grid into a freshly built home page, or rebuilds it with the
// current settings (after the settings page saves).
void nt_install_grid(QWidget *homeView);

// Every tile label the settings page can show or hide (NickelMenu entries and
// tile: lines), hidden ones included.
QStringList nt_all_tile_labels();

#endif
