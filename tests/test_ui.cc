// Word wrap (ui.cc). Fonts differ between machines, so these check the shape of
// the result (line count, elision, widths within the limit) rather than the
// exact pixel breaks. Needs a QApplication for the font metrics.

#include <QtTest>

#include <QFont>
#include <QFontMetrics>

#include "ui.h"

namespace {

const QChar kEllipsis(0x2026);

QFontMetrics metrics() {
    QFont font;
    font.setPixelSize(20);
    return QFontMetrics(font);
}

void compareFitsWidth(const QStringList &lines, const QFontMetrics &fm, int width) {
    for (const QString &line : lines)
        QVERIFY2(fm.width(line) <= width, qPrintable(line));
}

}  // namespace

class TestUi : public QObject {
    Q_OBJECT

private slots:
    void shortTextIsOneLine();
    void longTextWrapsAndElidesLastLine();
    void singleLineElidesLongText();
    void overlongWordIsElided();
    void zeroLinesGivesNothing();
    void emptyTextGivesNothing();
};

void TestUi::shortTextIsOneLine() {
    const QStringList lines = nt_wrap(QStringLiteral("Short   label"), metrics(), 400, 3);
    QCOMPARE(lines, QStringList(QStringLiteral("Short label")));
}

void TestUi::longTextWrapsAndElidesLastLine() {
    const QFontMetrics fm = metrics();
    const int width = 160;
    const QString text = QStringLiteral("The quick brown fox jumps over the lazy dog while the farmer counts every sheep twice");
    const QStringList lines = nt_wrap(text, fm, width, 3);
    QCOMPARE(lines.size(), 3);
    QVERIFY(lines.first().startsWith(QStringLiteral("The")));
    // The first lines are full words; only the last one carries the ellipsis.
    QVERIFY(!lines.at(0).contains(kEllipsis));
    QVERIFY(lines.last().contains(kEllipsis));
    compareFitsWidth(lines, fm, width);
}

void TestUi::singleLineElidesLongText() {
    const QFontMetrics fm = metrics();
    const QStringList lines = nt_wrap(QStringLiteral("A long caption that cannot fit on one line"), fm, 120, 1);
    QCOMPARE(lines.size(), 1);
    QVERIFY(lines.first().endsWith(kEllipsis));
    compareFitsWidth(lines, fm, 120);
}

void TestUi::overlongWordIsElided() {
    const QFontMetrics fm = metrics();
    const QStringList lines = nt_wrap(QStringLiteral("Supercalifragilisticexpialidocious"), fm, 60, 2);
    QCOMPARE(lines.size(), 1);
    QVERIFY(lines.first().endsWith(kEllipsis));
    compareFitsWidth(lines, fm, 60);
}

void TestUi::zeroLinesGivesNothing() {
    QVERIFY(nt_wrap(QStringLiteral("Any text"), metrics(), 200, 0).isEmpty());
}

void TestUi::emptyTextGivesNothing() {
    QVERIFY(nt_wrap(QString(), metrics(), 200, 3).isEmpty());
}

QTEST_MAIN(TestUi)
#include "test_ui.moc"
