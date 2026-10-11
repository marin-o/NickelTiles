#ifndef NT_SETTINGS_MODEL_H
#define NT_SETTINGS_MODEL_H

// The mod's settings as one value, with every key name and default in
// settings_model.cc. Code reads NtSettings, never config keys directly.
// Icon overrides (icon:) and extra tiles (tile:) stay in the config API.

#include <QString>
#include <QStringList>

enum class NtRecentSource { Any, Koreader, Plato, Nickel };

struct NtRecentInfo {
    bool title = true;
    bool author = true;
    bool progress = true;
    bool operator==(const NtRecentInfo &o) const { return title == o.title && author == o.author && progress == o.progress; }
    bool operator!=(const NtRecentInfo &o) const { return !(*this == o); }
};

struct NtSettings {
    bool enabled = true;           // nt_enabled: master switch
    bool actions = true;           // nt_actions: run tiles when tapped
    bool hideRows = true;          // nt_hide_rows: hide Kobo's own home rows
    bool recent = true;            // nt_recent: Continue reading tile
    NtRecentSource recentSource = NtRecentSource::Any;   // nt_recent_source
    NtRecentInfo recentInfo;       // nt_recent_info: its caption
    bool recentCover = true;       // nt_recent_cover: the book's cover, else a book icon
    bool sleepCover = true;        // nt_sleep_cover
    int columns = 5;               // nt_columns
    int tilePx = 0;                // nt_tile_px: 0 = automatic (fills the width)
    QStringList hidden;            // hide: (repeatable)
    QStringList order;             // order: a|b|c
};

// Current settings, defaults filled in for missing or invalid keys.
NtSettings nt_settings();

// Writes the keys that differ between `before` and `after`. Returns false if
// any write failed.
bool nt_settings_save(const NtSettings &before, const NtSettings &after);

#endif
