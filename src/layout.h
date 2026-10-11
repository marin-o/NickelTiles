#ifndef NT_LAYOUT_H
#define NT_LAYOUT_H

// Grid geometry: how big tiles are and whether a grid fits a width. Pure
// arithmetic, shared by the grid and the settings page.

// Tile sizes the settings page offers.
const int kNtTileMinPx = 120;
const int kNtTileMaxPx = 300;

// Space around and between tiles in the grid.
const int kNtGridMargin = 24;
const int kNtGridSpacing = 16;

// The tile size used for `columns` across `width`: `tilePx`, or for 0
// (automatic) the largest size that fills the width, within the limits.
int nt_tile_size(int columns, int tilePx, int width);

// Whether `columns` tiles of `tilePx` (0 = automatic) fit across `width`.
bool nt_tiles_fit(int columns, int tilePx, int width);

#endif
