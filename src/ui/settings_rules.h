#ifndef NT_SETTINGS_RULES_H
#define NT_SETTINGS_RULES_H

// What the settings page may do and what its controls produce, as plain
// functions over the settings, so the rules can be tested without widgets.

#include <QSet>
#include <QString>
#include <QStringList>

#include "core/settings_model.h"

const int kNtMinColumns = 2;

// What the size controls may do from the current settings on a grid `width` px wide.
struct NtSizeLimits {
    int tilePx;   // the tile size in effect; automatic resolved to its real size
    bool canRemoveColumn;
    bool canAddColumn;
    bool canShrinkTile;
    bool canGrowTile;
};
NtSizeLimits nt_size_limits(const NtSettings &settings, int width);

// The fixed size `step` px away from the current one, on a 20 px grid and within
// the limits. Leaving automatic starts from the size automatic gives now.
int nt_stepped_tile_size(const NtSettings &settings, int step, int width);

// Clamps a number of recent book tiles to what the setting allows.
int nt_recent_count_within_limits(int count);

// The source button's next value, and its display name.
NtRecentSource nt_next_source(NtRecentSource source);
QString nt_source_name(NtRecentSource source);

// The Kobo rows button's next value, and its display name.
NtKoboRows nt_next_kobo_rows(NtKoboRows rows);
QString nt_kobo_rows_name(NtKoboRows rows);

// The hidden list in the order it is written. Hide: lines for tiles that are
// not listed now are kept (an entry that was removed may come back), in the
// order they had; then the listed tiles in list order. Unchanged if the set is.
QStringList nt_hidden_after(const QStringList &initialHidden, const QStringList &labels, const QSet<QString> &hidden);

#endif
