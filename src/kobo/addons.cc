#include "kobo/addons.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

#include "core/paths.h"
#include "core/util.h"

namespace {

// Longest failsafe delay among the common mods (NickelMenu's 20 s) plus slack.
const qint64 kSettleSeconds = 30;
const char kFailsafeSuffix[] = ".failsafe";

qint64 &startTime() {
    static qint64 t = 0;
    return t;
}

}   // namespace

void nt_addons_mark_start() {
    startTime() = QDateTime::currentDateTime().toTime_t();
}

bool nt_addons_settled() {
    return startTime() > 0 && QDateTime::currentDateTime().toTime_t() - startTime() >= kSettleSeconds;
}

QStringList nt_switched_off_addons() {
    const QDir dir(QStringLiteral(NT_PLUGIN_DIR));
    QStringList off;
    for (const QString &name :
         dir.entryList(QStringList(QStringLiteral("*.so") + QLatin1String(kFailsafeSuffix)), QDir::Files)) {
        const QString library = name.left(name.size() - static_cast<int>(sizeof(kFailsafeSuffix) - 1));
        if (!dir.exists(library)) off << library;
    }
    return off;
}

QString nt_addon_name(const QString &library) {
    if (library == QLatin1String("libnm.so")) return QStringLiteral("NickelMenu");
    if (library == QLatin1String("libnickelhome.so")) return QStringLiteral("NickelHome");
    if (library == QLatin1String("libnkpm.so")) return QStringLiteral("KPM");
    if (library == QLatin1String("libnickeltiles.so")) return QStringLiteral("NickelTiles");
    return library;
}

QStringList nt_restore_addons() {
    const QDir dir(QStringLiteral(NT_PLUGIN_DIR));
    QStringList restored;
    for (const QString &library : nt_switched_off_addons()) {
        const bool ok = QFile::rename(dir.filePath(library + QLatin1String(kFailsafeSuffix)), dir.filePath(library));
        NT_LOG("addons: restore %s -> %s", qPrintable(library), ok ? "ok" : "FAILED");
        if (ok) restored << library;
    }
    return restored;
}
