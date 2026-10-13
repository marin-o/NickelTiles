#include "ui/layout.h"

#include <QtGlobal>

namespace {

// Width of a grid of `columns` tiles that are `px` wide each.
int gridWidthFor(int columns, int px) {
    return columns * px + (columns - 1) * kNtGridSpacing + 2 * kNtGridMargin;
}

}   // namespace

int nt_tile_size(int columns, int tilePx, int width) {
    if (tilePx > 0) return tilePx;
    const int fill = (width - 2 * kNtGridMargin - (columns - 1) * kNtGridSpacing) / qMax(1, columns);
    return qBound(kNtTileMinPx, fill, kNtTileMaxPx);
}

bool nt_tiles_fit(int columns, int tilePx, int width) {
    return gridWidthFor(columns, nt_tile_size(columns, tilePx, width)) <= width;
}
