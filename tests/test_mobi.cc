// Kindle (MOBI/AZW3) cover lookup on a hand-built Palm database: the cover is
// the EXTH 201 offset from the first image, not the first image itself.

#include <QtTest>

#include <QImage>
#include <QList>

#include "fixtures.h"
#include "mobi.h"

namespace {

void putBe32(QByteArray &b, int at, quint32 v) {
    b[at] = char(v >> 24);
    b[at + 1] = char(v >> 16);
    b[at + 2] = char(v >> 8);
    b[at + 3] = char(v);
}

QByteArray be32(quint32 v) {
    QByteArray b(4, '\0');
    putBe32(b, 0, v);
    return b;
}

// Record 0: PalmDOC header, a 232-byte MOBI header naming the first image
// record, then EXTH with the cover offset.
QByteArray record0(quint32 firstImage, quint32 coverOffset) {
    const int mobiLength = 232;
    QByteArray r(16 + mobiLength, '\0');
    r.replace(16, 4, "MOBI");
    putBe32(r, 20, mobiLength);
    putBe32(r, 108, firstImage);
    putBe32(r, 128, 0x40);
    r += "EXTH" + be32(12 + 12) + be32(1) + be32(201) + be32(12) + be32(coverOffset);
    return r;
}

QByteArray palmDatabase(const QList<QByteArray> &records) {
    QByteArray db(78, '\0');
    db.replace(60, 8, "BOOKMOBI");
    db[76] = char(records.size() >> 8);
    db[77] = char(records.size());
    quint32 offset = 78 + 8 * records.size() + 2;
    for (const QByteArray &r : records) {
        db += be32(offset) + QByteArray(4, '\0');
        offset += r.size();
    }
    db += QByteArray(2, '\0');
    for (const QByteArray &r : records) db += r;
    return db;
}

}   // namespace

class TestMobi : public QObject {
    Q_OBJECT

private slots:
    void init() { removeFixtures(); }
    void cleanup() { removeFixtures(); }

    void coverFromExthOffset();
};

void TestMobi::coverFromExthOffset() {
    const QByteArray cover = jpegBytes(12, 12);
    const QList<QByteArray> records = {record0(2, 1), QByteArray("text"), jpegBytes(40, 40), cover};
    const QString path = QStringLiteral("/tmp/nt-tests/books/book.azw3");
    QVERIFY(writeFile(path, palmDatabase(records)));

    QString name;
    const QByteArray bytes = nt_mobi_cover_bytes(path, &name);
    QCOMPARE(name, QStringLiteral("cover.jpg"));
    QCOMPARE(bytes, cover);
    QVERIFY(!QImage::fromData(bytes).isNull());
}

QTEST_MAIN(TestMobi)
#include "test_mobi.moc"
