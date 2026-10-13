// FB2 cover lookup, shaped like a real book: XLink bound to "l:", an image
// in the body, and the cover stored after other pictures.

#include <QtTest>

#include <QImage>

#include "fb2.h"
#include "fixtures.h"

class TestFb2 : public QObject {
    Q_OBJECT

private slots:
    void init() { removeFixtures(); }
    void cleanup() { removeFixtures(); }

    void coverFromCoverpage();
};

void TestFb2::coverFromCoverpage() {
    const QByteArray cover = jpegBytes(12, 12);
    const QByteArray book =
        QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" "
                   "xmlns:l=\"http://www.w3.org/1999/xlink\"><description><title-info>"
                   "<book-title>Book</book-title><coverpage><image l:href=\"#cover2.jpg\"/></coverpage>"
                   "</title-info></description>"
                   "<body><image l:href=\"#map.jpg\"/><section><p>Text</p></section></body>"
                   "<binary id=\"map.jpg\" content-type=\"image/jpeg\">") +
        jpegBytes(40, 40).toBase64() + "</binary><binary id=\"cover2.jpg\" content-type=\"image/jpeg\">\n" +
        cover.toBase64() + "\n</binary></FictionBook>\n";
    const QString path = QStringLiteral("/tmp/nt-tests/books/book.fb2");
    QVERIFY(writeFile(path, book));

    QString name;
    const QByteArray bytes = nt_fb2_cover_bytes(path, &name);
    QCOMPARE(name, QStringLiteral("cover2.jpg"));
    QCOMPARE(bytes, cover);
    QVERIFY(!QImage::fromData(bytes).isNull());
}

QTEST_MAIN(TestFb2)
#include "test_fb2.moc"
