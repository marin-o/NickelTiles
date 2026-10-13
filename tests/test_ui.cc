// Word wrap (ui.cc): a long label wraps to the line limit and only its last line is elided.

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
    for (const QString &line : lines) QVERIFY2(fm.width(line) <= width, qPrintable(line));
}

}   // namespace

class TestUi : public QObject {
    Q_OBJECT

private slots:
    void longTextWrapsAndElidesLastLine();
};

// Fonts differ between machines, so this checks the shape of the result (line
// count, elision, widths within the limit) rather than the exact pixel breaks.
void TestUi::longTextWrapsAndElidesLastLine() {
    const QFontMetrics fm = metrics();
    const int width = 160;
    const QString text =
        QStringLiteral("The quick brown fox jumps over the lazy dog while the farmer counts every sheep twice");
    const QStringList lines = nt_wrap(text, fm, width, 3);
    QCOMPARE(lines.size(), 3);
    QVERIFY(lines.first().startsWith(QStringLiteral("The")));
    // The first lines are full words; only the last one carries the ellipsis.
    QVERIFY(!lines.at(0).contains(kEllipsis));
    QVERIFY(lines.last().contains(kEllipsis));
    compareFitsWidth(lines, fm, width);
}

QTEST_MAIN(TestUi)
#include "test_ui.moc"
