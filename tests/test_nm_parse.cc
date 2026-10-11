// NickelMenu files (nm_parse.cc): which lines become menu entries and how chain_* lines attach.

#include <QtTest>

#include "fixtures.h"
#include "nm_parse.h"

namespace {

QString nmDir() {
    return QStringLiteral("/tmp/nt-tests/nm");
}

void writeMenu(const QString &name, const QByteArray &text) {
    QVERIFY(writeFile(nmDir() + QLatin1Char('/') + name, text));
}

QList<NtEntry> readMenus() {
    return nt_read_nickelmenu_entries(nmDir());
}

const char kMenus[] =
    "# NickelMenu\n"
    "menu_item:main:KOReader:cmd_spawn:/mnt/onboard/.adds/koreader/koreader.sh  # launch\n"
    ": menu_item : main : Update : cmd_spawn : /bin/update.sh\n"
    "chain_success:cmd_output:20:/bin/echo done\n"
    "chain_failure:dbg_toast:failed\n"
    "chain_always:dbg_toast:always\n"
    "menu_item:main:Terminal:cmd_spawn:echo a:b\n";

}  // namespace

class TestNmParse : public QObject {
    Q_OBJECT

private slots:
    void init() { removeFixtures(); }
    void cleanup() { removeFixtures(); }

    void mainMenuItemsBecomeEntries();
    void chainsAttachToTheirItem();
    void otherMenusAndLinesAreSkipped();
};

void TestNmParse::mainMenuItemsBecomeEntries() {
    writeMenu(QStringLiteral("menus"), QByteArray(kMenus));
    const QList<NtEntry> entries = readMenus();
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries.at(0).label, QStringLiteral("KOReader"));
    QCOMPARE(entries.at(0).actions.at(0).kind, QStringLiteral("cmd_spawn"));
    QCOMPARE(entries.at(0).actions.at(0).arg, QStringLiteral("/mnt/onboard/.adds/koreader/koreader.sh"));
    // Colons after the first arg belong to the arg.
    QCOMPARE(entries.at(2).label, QStringLiteral("Terminal"));
    QCOMPARE(entries.at(2).actions.at(0).arg, QStringLiteral("echo a:b"));
}

void TestNmParse::chainsAttachToTheirItem() {
    writeMenu(QStringLiteral("menus"), QByteArray(kMenus));
    const QList<NtEntry> entries = readMenus();
    QCOMPARE(entries.size(), 3);
    const QList<NtAction> &actions = entries.at(1).actions;
    QCOMPARE(entries.at(1).label, QStringLiteral("Update"));
    QCOMPARE(actions.size(), 4);
    QVERIFY(actions.at(0).chain == NtAction::First);
    QVERIFY(actions.at(1).chain == NtAction::OnSuccess);
    QCOMPARE(actions.at(1).kind, QStringLiteral("cmd_output"));
    QCOMPARE(actions.at(1).arg, QStringLiteral("20:/bin/echo done"));
    QVERIFY(actions.at(2).chain == NtAction::OnFailure);
    QVERIFY(actions.at(3).chain == NtAction::Always);
}

void TestNmParse::otherMenusAndLinesAreSkipped() {
    // A chapter-menu item and a generator end the current entry, so the chain
    // lines after them attach to nothing.
    writeMenu(QStringLiteral("menus"),
        QByteArray("menu_item:chapter:Other:cmd_spawn:x\n"
                   "chain_success:dbg_toast:orphan\n"
                   "menu_item:main:Gen:cmd_spawn:a\n"
                   "generator:something\n"
                   "chain_success:dbg_toast:lost\n"));
    const QList<NtEntry> entries = readMenus();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).label, QStringLiteral("Gen"));
    QCOMPARE(entries.at(0).actions.size(), 1);
}

QTEST_MAIN(TestNmParse)
#include "test_nm_parse.moc"
