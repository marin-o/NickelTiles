#include "cbz.h"

#include <QFile>
#include <QList>

#include "zip.h"

bool nt_is_cbz(const QString &path) {
    return path.endsWith(QStringLiteral(".cbz"), Qt::CaseInsensitive);
}

namespace {

bool isImage(const QString &name) {
    const QString lower = name.toLower();
    return lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg")) ||
           lower.endsWith(QStringLiteral(".png")) || lower.endsWith(QStringLiteral(".gif"));
}

// "cover" in the file name (not a folder name).
bool namedCover(const QString &name) {
    return name.section(QLatin1Char('/'), -1).contains(QStringLiteral("cover"), Qt::CaseInsensitive);
}

// A named cover beats page order.
bool betterCover(const NtZipEntry &candidate, const NtZipEntry *best) {
    if (!best) return true;
    if (namedCover(candidate.name) != namedCover(best->name)) return namedCover(candidate.name);
    return candidate.name.compare(best->name, Qt::CaseInsensitive) < 0;
}

}   // namespace

QByteArray nt_cbz_cover_bytes(const QString &path, QString *name) {
    name->clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();
    const QList<NtZipEntry> entries = nt_zip_entries(f);
    const NtZipEntry *best = nullptr;
    for (const NtZipEntry &en : entries)
        if (isImage(en.name) && betterCover(en, best)) best = &en;
    if (!best) return QByteArray();
    *name = best->name;
    return nt_zip_extract(f, *best);
}
