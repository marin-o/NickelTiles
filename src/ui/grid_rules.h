#ifndef NT_GRID_RULES_H
#define NT_GRID_RULES_H

// The grid's decisions, as plain functions so they can be tested without widgets.

#include <QList>
#include <QStringList>

#include "core/settings_model.h"

// The order to show tiles in, as indexes into `labels`: tiles named in
// `savedOrder` first, in that order, then the rest as they were. Saved names with
// no tile are skipped, since a NickelMenu entry that was removed may come back.
QList<int> nt_tile_order(const QStringList &labels, const QStringList &savedOrder);

// A saved order with `recentLabels` (the extra recent book tiles, in slot order)
// added right after the tile before them, for those it doesn't name yet. So
// raising the number of books keeps them together instead of adding them last.
QStringList nt_order_with_recents(const QStringList &savedOrder, const QStringList &recentLabels);

// The room the grid has, in px.
struct NtPageGeometry {
    int available;        // on pages of tiles only
    int firstAvailable;   // on the first page, less when Kobo's kept rows share it
    int tileHeight;
    int rowSpacing;
    int margins;     // top and bottom together
    int navHeight;   // the arrows and the gap above them
};

// How `tiles` are spread over pages. Pages need room for their arrows once there
// is more than one. The first page may hold no tiles when Kobo's rows fill it;
// the other pages hold at least one row, so every tile can be reached.
struct NtPagePlan {
    int firstRows;
    int rows;   // on every page after the first
    int pages;
};
NtPagePlan nt_plan_pages(const NtPageGeometry &geometry, int tiles, int columns);

// The index of the first tile on `page`.
int nt_page_first_tile(const NtPagePlan &plan, int page, int columns);

// How many of Kobo's three home rows stay, from the top. With nothing but the
// settings tile to show (`hasAppTiles` false), a first install would look
// broken without them, so all stay.
int nt_kobo_rows_to_keep(NtKoboRows rows, bool hasAppTiles);

// Whether any of `labels` is not hidden.
bool nt_any_tile_shown(const QStringList &labels, const QStringList &hidden);

#endif
