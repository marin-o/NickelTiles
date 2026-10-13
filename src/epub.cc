#include "epub.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>

#include "util.h"
#include "zip.h"

bool nt_is_epub(const QString &path) {
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".epub")) || lower.endsWith(QStringLiteral(".kepub"));
}

static QString nt_attr(const QString &tag, const char *attr) {
    QRegularExpression re(QStringLiteral("\\b") + QString::fromLatin1(attr) +
                              QStringLiteral("\\s*=\\s*[\"']([^\"']*)[\"']"),
                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = re.match(tag);
    return m.hasMatch() ? m.captured(1) : QString();
}

// Joins an OPF-relative href onto the OPF's folder and collapses "..".
static QString nt_resolve(const QString &baseDir, const QString &href) {
    QString h = href;
    h.replace(QStringLiteral("%20"), QStringLiteral(" "));
    if (baseDir.isEmpty()) return h;
    QString full = baseDir + QLatin1Char('/') + h;
    while (full.contains(QStringLiteral("/../"))) {
        int i = full.indexOf(QStringLiteral("/../"));
        int j = full.lastIndexOf(QLatin1Char('/'), i - 1);
        if (j < 0) {
            full = full.mid(i + 4);
            break;
        }
        full = full.left(j) + full.mid(i + 3);
    }
    return full;
}

// The rootfile path named by META-INF/container.xml, or empty.
static QString nt_opf_path(QFile &f, const QList<NtZipEntry> &entries) {
    for (const NtZipEntry &en : entries) {
        if (en.name != QLatin1String("META-INF/container.xml")) continue;
        const QString xml = QString::fromUtf8(nt_zip_extract(f, en));
        // \b keeps <rootfiles>, which encloses the element, from matching.
        const QRegularExpression rootfile(QStringLiteral("<rootfile\\b[^>]*>"),
                                          QRegularExpression::CaseInsensitiveOption);
        return nt_attr(rootfile.match(xml).captured(0), "full-path");
    }
    return QString();
}

// The cover image href from the OPF manifest: the cover-image property, else
// the item named by <meta name="cover">, else an image whose href says "cover".
static QString nt_cover_from_opf(const QString &opf, const QString &opfDir) {
    QString coverId;
    QRegularExpression metaRe(QStringLiteral("<meta[^>]*name\\s*=\\s*[\"']cover[\"'][^>]*>"),
                              QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch mm = metaRe.match(opf);
    if (mm.hasMatch()) coverId = nt_attr(mm.captured(0), "content");

    QRegularExpression itemRe(QStringLiteral("<item\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = itemRe.globalMatch(opf);
    QString byProp, byId, byName;
    while (it.hasNext()) {
        const QString tag = it.next().captured(0);
        const QString href = nt_attr(tag, "href"), id = nt_attr(tag, "id"), props = nt_attr(tag, "properties"),
                      mt = nt_attr(tag, "media-type");
        if (!mt.startsWith(QLatin1String("image/"))) continue;
        if (props.contains(QLatin1String("cover-image")) && byProp.isEmpty()) byProp = href;
        if (!coverId.isEmpty() && id == coverId && byId.isEmpty()) byId = href;
        if (href.contains(QLatin1String("cover"), Qt::CaseInsensitive) && byName.isEmpty()) byName = href;
    }
    const QString chosen = !byProp.isEmpty() ? byProp : (!byId.isEmpty() ? byId : byName);
    return chosen.isEmpty() ? QString() : nt_resolve(opfDir, chosen);
}

static QString nt_cover_from_opf_path(QFile &f, const QList<NtZipEntry> &entries, const QString &opfPath) {
    const QString opfDir =
        opfPath.contains(QLatin1Char('/')) ? opfPath.left(opfPath.lastIndexOf(QLatin1Char('/'))) : QString();
    for (const NtZipEntry &en : entries) {
        if (en.name == opfPath) return nt_cover_from_opf(QString::fromUtf8(nt_zip_extract(f, en)), opfDir);
    }
    return QString();
}

// Fallback when the OPF names no cover: the largest JPEG or PNG whose name
// mentions "cover", else the largest JPEG or PNG.
static QString nt_largest_image(const QList<NtZipEntry> &entries) {
    const NtZipEntry *best = nullptr, *bestAny = nullptr;
    for (const NtZipEntry &en : entries) {
        const QString n = en.name.toLower();
        if (!(n.endsWith(QLatin1String(".jpg")) || n.endsWith(QLatin1String(".jpeg")) ||
              n.endsWith(QLatin1String(".png"))))
            continue;
        if (!bestAny || en.size > bestAny->size) bestAny = &en;
        if (n.contains(QLatin1String("cover")) && (!best || en.size > best->size)) best = &en;
    }
    if (best) return best->name;
    return bestAny ? bestAny->name : QString();
}

QByteArray nt_epub_cover_bytes(const QString &epubPath, QString *member) {
    member->clear();
    QFile f(epubPath);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();
    const QList<NtZipEntry> entries = nt_zip_entries(f);
    if (entries.isEmpty()) {
        NT_DBG("epub: no zip entries in %s", qPrintable(epubPath));
        return QByteArray();
    }

    QString coverMember;
    const QString opfPath = nt_opf_path(f, entries);
    if (!opfPath.isEmpty()) coverMember = nt_cover_from_opf_path(f, entries, opfPath);
    if (coverMember.isEmpty()) coverMember = nt_largest_image(entries);

    *member = coverMember;
    for (const NtZipEntry &en : entries) {
        if (en.name == coverMember) return nt_zip_extract(f, en);
    }
    return QByteArray();
}
