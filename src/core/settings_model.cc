#include "core/settings_model.h"

#include <QByteArray>
#include <QStringList>
#include <QVector>

#include <cstring>

#include "core/config.h"
#include "core/util.h"

namespace {

const char kKeyEnabled[] = "nt_enabled";
const char kKeyActions[] = "nt_actions";
const char kKeyHideRows[] = "nt_hide_rows";
const char kKeyRecent[] = "nt_recent";
const char kKeyRecentSource[] = "nt_recent_source";
const char kKeyRecentInfo[] = "nt_recent_info";
const char kKeyRecentCover[] = "nt_recent_cover";
const char kKeySleepCover[] = "nt_sleep_cover";
const char kKeyColumns[] = "nt_columns";
const char kKeyTilePx[] = "nt_tile_px";
const char kKeyHide[] = "hide";
const char kKeyOrder[] = "order";

NtRecentSource parseSource(const char *value) {
    if (!value) return NtRecentSource::Any;
    if (!strcmp(value, "koreader")) return NtRecentSource::Koreader;
    if (!strcmp(value, "plato")) return NtRecentSource::Plato;
    if (!strcmp(value, "nickel")) return NtRecentSource::Nickel;
    return NtRecentSource::Any;
}

const char *sourceName(NtRecentSource source) {
    switch (source) {
    case NtRecentSource::Koreader:
        return "koreader";
    case NtRecentSource::Plato:
        return "plato";
    case NtRecentSource::Nickel:
        return "nickel";
    case NtRecentSource::Any:
        break;
    }
    return "auto";
}

// Unset or empty means all three; "none" comes out as all false.
NtRecentInfo parseInfo(const char *value) {
    NtRecentInfo info;
    if (!value || !*value) return info;
    const QString text = QString::fromUtf8(value).toLower();
    info.title = text.contains(QStringLiteral("title"));
    info.author = text.contains(QStringLiteral("author"));
    info.progress = text.contains(QStringLiteral("progress"));
    return info;
}

QStringList parseHidden() {
    QStringList hidden;
    for (const nt_entry_t *e = nt_config_entries(); e; e = e->next)
        if (!strcmp(e->key, kKeyHide)) hidden << QString::fromUtf8(e->val).trimmed();
    return hidden;
}

QStringList parseOrder(const char *value) {
    QStringList order;
    if (!value) return order;
    const QStringList parts = QString::fromUtf8(value).split(QLatin1Char('|'));
    for (const QString &part : parts) {
        const QString label = part.trimmed();
        if (!label.isEmpty()) order << label;
    }
    return order;
}

// "auto", missing or invalid -> 0 (automatic).
int parseTilePx(const char *value) {
    if (!value) return 0;
    bool ok = false;
    const int px = QString::fromUtf8(value).trimmed().toInt(&ok);
    return ok && px > 0 ? px : 0;
}

QString infoText(const NtRecentInfo &info) {
    QStringList parts;
    if (info.title) parts << QStringLiteral("title");
    if (info.author) parts << QStringLiteral("author");
    if (info.progress) parts << QStringLiteral("progress");
    return parts.isEmpty() ? QStringLiteral("none") : parts.join(QStringLiteral(","));
}

QString flag(bool value) {
    return value ? QStringLiteral("1") : QStringLiteral("0");
}

// The UTF-8 bytes live in this function, so the pointer config_set reads stays valid.
bool setValue(const char *key, const QString &value) {
    const QByteArray bytes = value.toUtf8();
    const char *vals[] = {bytes.constData()};
    return nt_config_set(key, vals, 1);
}

bool saveHidden(const QStringList &hidden) {
    QVector<QByteArray> bytes;
    for (const QString &label : hidden) bytes << label.toUtf8();
    QVector<const char *> vals;
    for (const QByteArray &b : bytes) vals << b.constData();
    return nt_config_set(kKeyHide, vals.constData(), vals.size());
}

}   // namespace

NtSettings nt_settings() {
    const NtSettings defaults;
    NtSettings s;
    s.enabled = nt_config_bool(kKeyEnabled, defaults.enabled);
    s.actions = nt_config_bool(kKeyActions, defaults.actions);
    s.hideRows = nt_config_bool(kKeyHideRows, defaults.hideRows);
    s.recent = nt_config_bool(kKeyRecent, defaults.recent);
    s.recentSource = parseSource(nt_config_get(kKeyRecentSource));
    s.recentInfo = parseInfo(nt_config_get(kKeyRecentInfo));
    s.recentCover = nt_config_bool(kKeyRecentCover, defaults.recentCover);
    s.sleepCover = nt_config_bool(kKeySleepCover, defaults.sleepCover);
    s.columns = nt_config_int(kKeyColumns, defaults.columns);
    if (s.columns < 1) s.columns = 1;
    s.tilePx = parseTilePx(nt_config_get(kKeyTilePx));
    s.hidden = parseHidden();
    s.order = parseOrder(nt_config_get(kKeyOrder));
    return s;
}

bool nt_settings_save(const NtSettings &before, const NtSettings &after) {
    bool ok = true;
    if (before.enabled != after.enabled) ok &= setValue(kKeyEnabled, flag(after.enabled));
    if (before.actions != after.actions) ok &= setValue(kKeyActions, flag(after.actions));
    if (before.hideRows != after.hideRows) ok &= setValue(kKeyHideRows, flag(after.hideRows));
    if (before.recent != after.recent) ok &= setValue(kKeyRecent, flag(after.recent));
    if (before.recentSource != after.recentSource)
        ok &= setValue(kKeyRecentSource, QString::fromLatin1(sourceName(after.recentSource)));
    if (before.recentInfo != after.recentInfo) ok &= setValue(kKeyRecentInfo, infoText(after.recentInfo));
    if (before.recentCover != after.recentCover) ok &= setValue(kKeyRecentCover, flag(after.recentCover));
    if (before.sleepCover != after.sleepCover) ok &= setValue(kKeySleepCover, flag(after.sleepCover));
    if (before.columns != after.columns) ok &= setValue(kKeyColumns, QString::number(after.columns));
    if (before.tilePx != after.tilePx)
        ok &= setValue(kKeyTilePx, after.tilePx > 0 ? QString::number(after.tilePx) : QStringLiteral("auto"));
    if (before.hidden != after.hidden) ok &= saveHidden(after.hidden);
    if (before.order != after.order)
        ok &= after.order.isEmpty() ? nt_config_set(kKeyOrder, nullptr, 0)
                                    : setValue(kKeyOrder, after.order.join(QStringLiteral("|")));

    NT_LOG("settings: saved ok=%d columns=%d px=%d recent=%d source=%s sleep=%d hide_rows=%d hidden=%d order=%d",
           ok ? 1 : 0, after.columns, after.tilePx, after.recent ? 1 : 0, sourceName(after.recentSource),
           after.sleepCover ? 1 : 0, after.hideRows ? 1 : 0, after.hidden.size(), after.order.size());
    return ok;
}
