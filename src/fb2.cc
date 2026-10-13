#include "fb2.h"

#include <QFile>
#include <QList>
#include <QXmlStreamReader>

#include "util.h"
#include "zip.h"

bool nt_is_fb2(const QString &path) {
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".fb2")) || lower.endsWith(QStringLiteral(".fb2.zip"));
}

namespace {

// The target of an <image>, without the leading '#'. Books bind the XLink
// namespace to different prefixes (l:, xlink:), so match the local name.
QString imageTarget(const QXmlStreamReader &xml) {
    const QXmlStreamAttributes attrs = xml.attributes();
    for (const QXmlStreamAttribute &a : attrs) {
        if (a.name() == QLatin1String("href")) {
            const QString href = a.value().toString();
            return href.startsWith(QLatin1Char('#')) ? href.mid(1) : href;
        }
    }
    return QString();
}

bool isCoverBinary(const QString &id, const QString &coverId) {
    if (!coverId.isEmpty()) return id == coverId;
    return id.contains(QStringLiteral("cover"), Qt::CaseInsensitive);
}

// Walks the book once: the cover id comes from the description, which
// precedes the binaries. The body is skipped unread.
QByteArray coverFromXml(QXmlStreamReader &xml, QString *name) {
    QString coverId;
    bool inCoverpage = false;
    while (!xml.atEnd()) {
        const QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::EndElement && xml.name() == QLatin1String("coverpage")) inCoverpage = false;
        if (token != QXmlStreamReader::StartElement) continue;
        const QStringRef element = xml.name();
        if (element == QLatin1String("body")) {
            xml.skipCurrentElement();
        } else if (element == QLatin1String("coverpage")) {
            inCoverpage = true;
        } else if (inCoverpage && coverId.isEmpty() && element == QLatin1String("image")) {
            coverId = imageTarget(xml);
        } else if (element == QLatin1String("binary")) {
            const QString id = xml.attributes().value(QStringLiteral("id")).toString();
            if (!isCoverBinary(id, coverId)) continue;
            *name = id;
            return QByteArray::fromBase64(xml.readElementText().toLatin1());
        }
    }
    if (xml.hasError()) NT_DBG("fb2: %s", qPrintable(xml.errorString()));
    return QByteArray();
}

// The .fb2 inside a .fb2.zip.
QByteArray fb2FromZip(QFile &f) {
    const QList<NtZipEntry> entries = nt_zip_entries(f);
    for (const NtZipEntry &en : entries)
        if (en.name.toLower().endsWith(QStringLiteral(".fb2"))) return nt_zip_extract(f, en);
    return QByteArray();
}

}   // namespace

QByteArray nt_fb2_cover_bytes(const QString &path, QString *name) {
    name->clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();
    if (!path.toLower().endsWith(QStringLiteral(".zip"))) {
        QXmlStreamReader xml(&f);
        return coverFromXml(xml, name);
    }
    const QByteArray book = fb2FromZip(f);
    if (book.isEmpty()) {
        NT_DBG("fb2: no .fb2 inside %s", qPrintable(path));
        return QByteArray();
    }
    QXmlStreamReader xml(book);
    return coverFromXml(xml, name);
}
