#include "ui/settings_rules.h"

#include <QtGlobal>

#include "ui/layout.h"

namespace {

const int kTileStepPx = 20;

// Display names, in the order the source button cycles through them.
struct SourceName {
    NtRecentSource source;
    const char *name;
};
const SourceName kSources[] = {
    {NtRecentSource::Any, "Any app"},
    {NtRecentSource::Koreader, "KOReader"},
    {NtRecentSource::Plato, "Plato"},
    {NtRecentSource::Nickel, "Kobo reader"},
};
const int kSourceCount = sizeof(kSources) / sizeof(kSources[0]);

int sourceIndex(NtRecentSource source) {
    for (int i = 0; i < kSourceCount; i++)
        if (kSources[i].source == source) return i;
    return 0;
}

}   // namespace

NtSizeLimits nt_size_limits(const NtSettings &settings, int width) {
    NtSizeLimits limits;
    limits.tilePx = nt_tile_size(settings.columns, settings.tilePx, width);
    limits.canRemoveColumn = settings.columns > kNtMinColumns;
    limits.canAddColumn = nt_tiles_fit(settings.columns + 1, settings.tilePx, width);
    limits.canShrinkTile = limits.tilePx > kNtTileMinPx;
    limits.canGrowTile =
        limits.tilePx < kNtTileMaxPx && nt_tiles_fit(settings.columns, limits.tilePx + kTileStepPx, width);
    return limits;
}

int nt_stepped_tile_size(const NtSettings &settings, int step, int width) {
    const int current = nt_tile_size(settings.columns, settings.tilePx, width);
    return qBound(kNtTileMinPx, (current + step) / kTileStepPx * kTileStepPx, kNtTileMaxPx);
}

NtRecentSource nt_next_source(NtRecentSource source) {
    return kSources[(sourceIndex(source) + 1) % kSourceCount].source;
}

QString nt_source_name(NtRecentSource source) {
    return QString::fromUtf8(kSources[sourceIndex(source)].name);
}

QStringList nt_hidden_after(const QStringList &initialHidden, const QStringList &labels, const QSet<QString> &hidden) {
    if (hidden == QSet<QString>::fromList(initialHidden)) return initialHidden;
    QStringList out;
    for (const QString &l : initialHidden)
        if (!labels.contains(l)) out << l;
    for (const QString &l : labels)
        if (hidden.contains(l)) out << l;
    return out;
}
