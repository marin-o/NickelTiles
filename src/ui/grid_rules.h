#ifndef NT_GRID_RULES_H
#define NT_GRID_RULES_H

// The grid's decisions, as plain functions so they can be tested without widgets.

#include <QList>
#include <QStringList>

// The order to show tiles in, as indexes into `labels`: tiles named in
// `savedOrder` first, in that order, then the rest as they were. Saved names with
// no tile are skipped, since a NickelMenu entry that was removed may come back.
QList<int> nt_tile_order(const QStringList &labels, const QStringList &savedOrder);

// Whether any of `labels` is not hidden.
bool nt_any_tile_shown(const QStringList &labels, const QStringList &hidden);

#endif
