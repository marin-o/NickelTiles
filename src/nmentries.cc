#include "nmentries.h"
#include "util.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStringList>
#include <QTextStream>

#include <stdio.h>
#include <unistd.h>

// libnickel symbols, resolved by NickelHook (all optional; see nickeltiles.cc).
typedef void MainWindowController;
typedef void N3PowerWorkflowManager;
typedef void PlugWorkflowManager;
typedef void WirelessWorkflowManager;
extern MainWindowController *(*MainWindowController_sharedInstance)();
extern void (*MainWindowController_toast)(MainWindowController*, QString const&, QString const&, int);
extern N3PowerWorkflowManager *(*N3PowerWorkflowManager_sharedInstance)();
extern void (*N3PowerWorkflowManager_powerOff)(N3PowerWorkflowManager*, bool);
extern void (*N3PowerWorkflowManager_reboot)(N3PowerWorkflowManager*);
extern void (*N3PowerWorkflowManager_requestSleep)(N3PowerWorkflowManager*);
extern PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)();
extern void (*PlugWorkflowManager_sync)(PlugWorkflowManager*);
extern void (*PlugWorkflowManager_onCancelAndConnect)(PlugWorkflowManager*);
extern void (*PlugWorkflowManager_unplugged)(PlugWorkflowManager*);
extern WirelessWorkflowManager *(*WirelessWorkflowManager_sharedInstance)();
extern void (*WirelessWorkflowManager_connectWireless)(WirelessWorkflowManager*, bool, bool);
extern void (*WirelessWorkflowManager_connectWirelessSilently)(WirelessWorkflowManager*);
extern bool (*WirelessWorkflowManager_isAirplaneMode)(WirelessWorkflowManager*);
extern void (*WirelessWorkflowManager_setAirplaneMode)(WirelessWorkflowManager*, bool);
extern void (*MainNavController_home)(void*);

void nt_toast(const QString &text, int ms) {
    if (MainWindowController_sharedInstance && MainWindowController_toast) {
        MainWindowController *mwc = MainWindowController_sharedInstance();
        if (mwc) {
            MainWindowController_toast(mwc, text, QStringLiteral(""), ms);
            return;
        }
    }
    NT_LOG("toast (no Nickel toast available): %s", qPrintable(text));
}

// NickelMenu syntax: fields separated by ':' with surrounding whitespace
// ignored; the last field keeps any further ':'.
static QStringList nt_split_fields(const QString &line, int maxFields) {
    QStringList out;
    QString rest = line;
    while (out.size() < maxFields - 1) {
        int i = rest.indexOf(QLatin1Char(':'));
        if (i < 0) break;
        out << rest.left(i).trimmed();
        rest = rest.mid(i + 1);
    }
    out << rest.trimmed();
    return out;
}

QList<NtEntry> nt_read_nickelmenu_entries(const QString &dirPath) {
    QList<NtEntry> entries;
    QDir dir(dirPath);
    if (!dir.exists()) {
        NT_LOG("NickelMenu dir %s not found", qPrintable(dirPath));
        return entries;
    }
    const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : files) {
        if (name == QLatin1String("doc") || name == QLatin1String("uninstall") || name.startsWith(QLatin1Char('.')))
            continue;
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream in(&f);
        in.setCodec("UTF-8");
        NtEntry *current = NULL;
        while (!in.atEnd()) {
            QString line = in.readLine();
            int hash = line.indexOf(QLatin1Char('#'));
            if (hash >= 0) line = line.left(hash);
            line = line.trimmed();
            if (line.isEmpty()) continue;
            // leading ':' is allowed before the first field in NickelMenu files
            if (line.startsWith(QLatin1Char(':'))) line = line.mid(1).trimmed();
            QStringList f5 = nt_split_fields(line, 5);
            const QString kind = f5.value(0);
            if (kind == QLatin1String("menu_item")) {
                if (f5.size() < 5) { current = NULL; continue; }
                if (f5.at(1) != QLatin1String("main")) { current = NULL; continue; }
                NtEntry e;
                e.label = f5.at(2);
                e.source = name;
                NtAction a;
                a.kind = f5.at(3);
                a.arg = f5.at(4);
                a.chain = 0;
                e.actions << a;
                entries << e;
                current = &entries.last();
            } else if (kind.startsWith(QLatin1String("chain_"))) {
                if (!current) continue;
                QStringList f3 = nt_split_fields(line, 3);
                if (f3.size() < 3) continue;
                NtAction a;
                a.kind = f3.at(1);
                a.arg = f3.at(2);
                a.chain = kind == QLatin1String("chain_success") ? 1
                        : kind == QLatin1String("chain_failure") ? 2 : 3;
                current->actions << a;
            } else {
                current = NULL; // generator or unknown line
            }
        }
    }
    NT_LOG("NickelMenu: %d main-menu entries from %s", entries.size(), qPrintable(dirPath));
    return entries;
}

// Strips a leading "quiet:" option like NickelMenu does.
static QString nt_strip_quiet(const QString &arg, bool *quiet) {
    QString a = arg;
    int i = a.indexOf(QLatin1Char(':'));
    if (i >= 0 && a.left(i).trimmed() == QLatin1String("quiet")) {
        if (quiet) *quiet = true;
        return a.mid(i + 1).trimmed();
    }
    if (quiet) *quiet = false;
    return a;
}

static bool nt_run_action(const NtAction &a, QString *err) {
    const QString &kind = a.kind;
    if (kind == QLatin1String("cmd_spawn")) {
        bool quiet = false;
        QString cmd = nt_strip_quiet(a.arg, &quiet);
        qint64 pid = 0;
        bool ok = QProcess::startDetached(QStringLiteral("/bin/sh"),
            QStringList() << QStringLiteral("-c") << cmd, QStringLiteral("/"), &pid);
        NT_LOG("cmd_spawn %s pid=%lld: %s", ok ? "ok" : "FAILED", (long long)pid, qPrintable(cmd));
        if (!ok) { *err = QStringLiteral("could not start process"); return false; }
        if (!quiet) nt_toast(QStringLiteral("Started (PID %1)").arg(pid));
        return true;
    }
    if (kind == QLatin1String("cmd_output")) {
        // timeout:[quiet:]cmd
        QStringList f = nt_split_fields(a.arg, 2);
        int timeout = f.value(0).toInt();
        if (timeout <= 0 || timeout >= 10000) { *err = QStringLiteral("invalid timeout"); return false; }
        bool quiet = false;
        QString cmd = nt_strip_quiet(f.value(1), &quiet);
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.setWorkingDirectory(QStringLiteral("/"));
        proc.start(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << cmd, QIODevice::ReadOnly);
        if (!proc.waitForFinished(timeout)) {
            proc.kill();
            *err = QStringLiteral("command timed out");
            NT_LOG("cmd_output timed out: %s", qPrintable(cmd));
            return false;
        }
        QString out = QString::fromUtf8(proc.readAll()).trimmed();
        NT_LOG("cmd_output rc=%d: %s", proc.exitCode(), qPrintable(cmd));
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            *err = out.isEmpty() ? QStringLiteral("command failed") : out.left(300);
            return false;
        }
        if (!quiet) nt_toast(out.isEmpty() ? QStringLiteral("(no output)") : out.left(400), 4000);
        return true;
    }
    if (kind == QLatin1String("dbg_toast")) {
        nt_toast(a.arg);
        return true;
    }
    if (kind == QLatin1String("power")) {
        if (!N3PowerWorkflowManager_sharedInstance) { *err = QStringLiteral("power manager unavailable"); return false; }
        N3PowerWorkflowManager *pwm = N3PowerWorkflowManager_sharedInstance();
        if (!pwm) { *err = QStringLiteral("no power manager"); return false; }
        if (a.arg == QLatin1String("shutdown") && N3PowerWorkflowManager_powerOff) {
            nt_toast(QStringLiteral("Shutting down..."));
            N3PowerWorkflowManager_powerOff(pwm, false);
            return true;
        }
        if (a.arg == QLatin1String("reboot") && N3PowerWorkflowManager_reboot) {
            nt_toast(QStringLiteral("Rebooting..."));
            N3PowerWorkflowManager_reboot(pwm);
            return true;
        }
        if (a.arg == QLatin1String("sleep") && N3PowerWorkflowManager_requestSleep) {
            N3PowerWorkflowManager_requestSleep(pwm);
            return true;
        }
        *err = QStringLiteral("unsupported power action ") + a.arg;
        return false;
    }
    if (kind == QLatin1String("nickel_misc")) {
        if (a.arg == QLatin1String("force_usb_connection")) {
            FILE *nhs = fopen("/tmp/nickel-hardware-status", "w");
            if (!nhs) { *err = QStringLiteral("could not open hardware status pipe"); return false; }
            fputs("usb plug add", nhs);
            fclose(nhs);
            return true;
        }
        if (a.arg == QLatin1String("home") && MainNavController_home) {
            MainNavController_home(NULL);
            return true;
        }
        if (a.arg == QLatin1String("rescan_books_full") || a.arg == QLatin1String("rescan_books")) {
            // Disabled on purpose (2026-10-10): a full rescan on this reader
            // coincided with four sideloaded EPUBs being deleted from
            // /mnt/onboard/Books. Until Nickel's import behaviour with
            // excluded files is understood, no library rescan runs from a tile.
            *err = QStringLiteral("library rescan is disabled in NickelTiles");
            return false;
        }
        *err = QStringLiteral("unsupported nickel_misc action ") + a.arg;
        return false;
    }
    if (kind == QLatin1String("nickel_wifi")) {
        if (!WirelessWorkflowManager_sharedInstance) { *err = QStringLiteral("wireless manager unavailable"); return false; }
        WirelessWorkflowManager *wfm = WirelessWorkflowManager_sharedInstance();
        if (!wfm) { *err = QStringLiteral("no wireless manager"); return false; }
        if (a.arg == QLatin1String("autoconnect") && WirelessWorkflowManager_connectWireless) {
            WirelessWorkflowManager_connectWireless(wfm, true, false);
            return true;
        }
        if (a.arg == QLatin1String("autoconnect_silent") && WirelessWorkflowManager_connectWirelessSilently) {
            WirelessWorkflowManager_connectWirelessSilently(wfm);
            return true;
        }
        if ((a.arg == QLatin1String("enable") || a.arg == QLatin1String("disable") || a.arg == QLatin1String("toggle"))
                && WirelessWorkflowManager_isAirplaneMode && WirelessWorkflowManager_setAirplaneMode) {
            bool airplane = WirelessWorkflowManager_isAirplaneMode(wfm);
            if (a.arg == QLatin1String("enable")) { if (airplane) WirelessWorkflowManager_setAirplaneMode(wfm, false); }
            else if (a.arg == QLatin1String("disable")) { if (!airplane) WirelessWorkflowManager_setAirplaneMode(wfm, true); }
            else WirelessWorkflowManager_setAirplaneMode(wfm, !airplane);
            return true;
        }
        *err = QStringLiteral("unsupported nickel_wifi action ") + a.arg;
        return false;
    }
    *err = QStringLiteral("unsupported action ") + kind;
    return false;
}

bool nt_run_entry(const NtEntry &entry) {
    bool ok = true;
    bool ran = false;
    for (const NtAction &a : entry.actions) {
        if (a.chain == 1 && (!ran || !ok)) continue;
        if (a.chain == 2 && (!ran || ok)) continue;
        QString err;
        bool r = nt_run_action(a, &err);
        NT_LOG("entry '%s': %s %s -> %s%s%s", qPrintable(entry.label), qPrintable(a.kind),
            qPrintable(a.arg.left(80)), r ? "ok" : "failed", r ? "" : ": ", r ? "" : qPrintable(err));
        if (!r)
            nt_toast(entry.label + QStringLiteral(": ") + err, 3000);
        ok = r;
        ran = true;
    }
    return ok;
}
