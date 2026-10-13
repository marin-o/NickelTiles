// EPUB cover lookup: the formats books use, and the <rootfiles> bug.

#include <QtTest>

#include <QImage>
#include <QMap>

#include "epub.h"
#include "fixtures.h"

namespace {

const char kContainer[] = "<?xml version=\"1.0\"?>\n"
                          "<container version=\"1.0\">\n"
                          "<rootfiles>\n"
                          "<rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
                          "</rootfiles>\n"
                          "</container>\n";

// An OPF with the given manifest items and metadata elements.
QByteArray opf(const char *manifest, const char *metadata = "") {
    return QByteArray("<?xml version=\"1.0\"?><package version=\"3.0\"><metadata>") + metadata +
           "</metadata><manifest>" + manifest + "</manifest><spine/></package>";
}

QString bookPath(const QString &name) {
    return QStringLiteral("/tmp/nt-tests/books/") + name;
}

}   // namespace

class TestEpub : public QObject {
    Q_OBJECT

private slots:
    void init() { removeFixtures(); }
    void cleanup() { removeFixtures(); }

    void coverFromMetaWithRootfilesWrapper();
    void coverFromCoverImageProperty();
    void coverFromFileNameWhenOpfNamesNone();
    void notAZipGivesNothing();
};

// Regression: the <rootfiles> wrapper starts with the text "<rootfile", so the
// container parse must match the element by its whole name. Otherwise the OPF
// is never found and the cover falls back to a guess.
void TestEpub::coverFromMetaWithRootfilesWrapper() {
    const QByteArray cover = jpegBytes(8, 8);
    QMap<QString, QByteArray> files;
    files[QStringLiteral("META-INF/container.xml")] = QByteArray(kContainer);
    files[QStringLiteral("OEBPS/content.opf")] =
        opf("<item id=\"cover-image\" href=\"images/image_8.gif\" media-type=\"image/gif\"/>"
            "<item id=\"big\" href=\"images/big.jpg\" media-type=\"image/jpeg\"/>",
            "<meta name=\"cover\" content=\"cover-image\"/>");
    // The cover is the small one: picking the largest image would give big.jpg.
    files[QStringLiteral("OEBPS/images/image_8.gif")] = cover;
    files[QStringLiteral("OEBPS/images/big.jpg")] = jpegBytes(64, 64);
    const QString path = bookPath(QStringLiteral("meta.epub"));
    QVERIFY(writeEpub(path, files));

    QString member;
    const QByteArray bytes = nt_epub_cover_bytes(path, &member);
    QCOMPARE(member, QStringLiteral("OEBPS/images/image_8.gif"));
    QCOMPARE(bytes, cover);
    QVERIFY(!QImage::fromData(bytes).isNull());
}

void TestEpub::coverFromCoverImageProperty() {
    const QByteArray front = jpegBytes(16, 16);
    QMap<QString, QByteArray> files;
    files[QStringLiteral("META-INF/container.xml")] = QByteArray(kContainer);
    // cover_back.jpg has "cover" in its name too; the properties attribute must win.
    files[QStringLiteral("OEBPS/content.opf")] =
        opf("<item id=\"back\" href=\"images/cover_back.jpg\" media-type=\"image/jpeg\"/>"
            "<item id=\"front\" href=\"images/front.jpg\" properties=\"cover-image\" media-type=\"image/jpeg\"/>");
    files[QStringLiteral("OEBPS/images/cover_back.jpg")] = jpegBytes(8, 8);
    files[QStringLiteral("OEBPS/images/front.jpg")] = front;
    const QString path = bookPath(QStringLiteral("props.epub"));
    QVERIFY(writeEpub(path, files));

    QString member;
    const QByteArray bytes = nt_epub_cover_bytes(path, &member);
    QCOMPARE(member, QStringLiteral("OEBPS/images/front.jpg"));
    QCOMPARE(bytes, front);
}

void TestEpub::coverFromFileNameWhenOpfNamesNone() {
    const QByteArray cover = jpegBytes(8, 8);
    QMap<QString, QByteArray> files;
    files[QStringLiteral("META-INF/container.xml")] = QByteArray(kContainer);
    files[QStringLiteral("OEBPS/content.opf")] =
        opf("<item id=\"title\" href=\"images/title.jpg\" media-type=\"image/jpeg\"/>"
            "<item id=\"c\" href=\"images/cover.jpg\" media-type=\"image/jpeg\"/>");
    // title.jpg is the larger file, so only the name "cover" can pick cover.jpg.
    files[QStringLiteral("OEBPS/images/title.jpg")] = jpegBytes(32, 32);
    files[QStringLiteral("OEBPS/images/cover.jpg")] = cover;
    const QString path = bookPath(QStringLiteral("names.epub"));
    QVERIFY(writeEpub(path, files));

    QString member;
    const QByteArray bytes = nt_epub_cover_bytes(path, &member);
    QCOMPARE(member, QStringLiteral("OEBPS/images/cover.jpg"));
    QCOMPARE(bytes, cover);
}

void TestEpub::notAZipGivesNothing() {
    const QString path = bookPath(QStringLiteral("broken.epub"));
    QVERIFY(writeFile(path, QByteArray("plain text, not a zip")));

    QString member = QStringLiteral("not cleared");
    QVERIFY(nt_epub_cover_bytes(path, &member).isEmpty());
    QVERIFY(member.isEmpty());
}

QTEST_MAIN(TestEpub)
#include "test_epub.moc"
