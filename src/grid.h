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

// Tile sizes the settings page offers.
const int kNtTileMinPx = 120;
const int kNtTileMaxPx = 300;

// Width the grid lays out in: the screen's.
int nt_grid_width();

// The tile size used for `columns` across `width`: `tilePx`, or for 0
// (automatic) the largest size that fills the width, within the limits.
int nt_tile_size(int columns, int tilePx, int width);

// Whether `columns` tiles of `tilePx` (0 = automatic) fit across `width`.
bool nt_tiles_fit(int columns, int tilePx, int width);

// Every tile label the settings page can show or hide (NickelMenu entries and
// tile: lines), hidden ones included.
QStringList nt_all_tile_labels();

#endif
