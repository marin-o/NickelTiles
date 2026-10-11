#include "actions.h"

#include <QProcess>
#include <QString>
#include <QStringList>

#include <stdio.h>

#include "nickel.h"
#include "paths.h"
#include "util.h"

// Splits "head:tail" at the first ':', both sides trimmed. Without a ':' the
// whole string is the head and the tail is empty.
static void nt_split_first(const QString &s, QString *head, QString *tail) {
    const int i = s.indexOf(QLatin1Char(':'));
    if (i < 0) {
        *head = s.trimmed();
        tail->clear();
        return;
    }
    *head = s.left(i).trimmed();
    *tail = s.mid(i + 1).trimmed();
}

// Strips a leading "quiet:" option like NickelMenu does.
static QString nt_strip_quiet(const QString &arg, bool *quiet) {
    QString head, tail;
    nt_split_first(arg, &head, &tail);
    *quiet = head == QLatin1String("quiet");
    return *quiet ? tail : arg;
}

// Starts `command` detached in /. The outcome is logged under `label`.
static bool nt_spawn(const QString &label, const QString &command, qint64 *pid) {
    const bool ok = QProcess::startDetached(QStringLiteral("/bin/sh"),
        QStringList() << QStringLiteral("-c") << command, QStringLiteral("/"), pid);
    NT_LOG("%s %s pid=%lld: %s", qPrintable(label), ok ? "ok" : "FAILED", (long long)*pid, qPrintable(command));
    return ok;
}

bool nt_run_command(const QString &label, const QString &command) {
    qint64 pid = 0;
    if (nt_spawn(label, command, &pid)) return true;
    nt_toast(label + QStringLiteral(": could not start process"), 3000);
    return false;
}

QString nt_shell_quote(const QString &s) {
    QString out = s;
    out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'") + out + QStringLiteral("'");
}

static bool run_cmd_spawn(const NtAction &a, QString *err) {
    bool quiet = false;
    const QString cmd = nt_strip_quiet(a.arg, &quiet);
    qint64 pid = 0;
    if (!nt_spawn(QStringLiteral("cmd_spawn"), cmd, &pid)) {
        *err = QStringLiteral("could not start process");
        return false;
    }
    if (!quiet) nt_toast(QStringLiteral("Started (PID %1)").arg(pid));
    return true;
}

// Runs `cmd` through /bin/sh with stderr merged into stdout. On timeout, a
// non-zero exit or a crash, `*err` gets the output (or a generic message).
static bool nt_capture(const QString &cmd, int timeout, QString *out, QString *err) {
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
    *out = QString::fromUtf8(proc.readAll()).trimmed();
    NT_LOG("cmd_output rc=%d: %s", proc.exitCode(), qPrintable(cmd));
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        *err = out->isEmpty() ? QStringLiteral("command failed") : out->left(300);
        return false;
    }
    return true;
}

static bool run_cmd_output(const NtAction &a, QString *err) {
    // arg is "timeout:[quiet:]cmd"
    QString timeoutText, rest;
    nt_split_first(a.arg, &timeoutText, &rest);
    const int timeout = timeoutText.toInt();
    if (timeout <= 0 || timeout >= 10000) {
        *err = QStringLiteral("invalid timeout");
        return false;
    }
    bool quiet = false;
    const QString cmd = nt_strip_quiet(rest, &quiet);
    QString out;
    if (!nt_capture(cmd, timeout, &out, err)) return false;
    if (!quiet) nt_toast(out.isEmpty() ? QStringLiteral("(no output)") : out.left(400), 4000);
    return true;
}

static bool run_power(const NtAction &a, QString *err) {
    // Toast before the call: a shutdown or reboot leaves no time to show it after.
    if (a.arg == QLatin1String("shutdown")) {
        nt_toast(QStringLiteral("Shutting down..."));
        if (nt_power_off()) return true;
    } else if (a.arg == QLatin1String("reboot")) {
        nt_toast(QStringLiteral("Rebooting..."));
        if (nt_reboot()) return true;
    } else if (a.arg == QLatin1String("sleep")) {
        if (nt_sleep()) return true;
    } else {
        *err = QStringLiteral("unsupported power action ") + a.arg;
        return false;
    }
    *err = QStringLiteral("power manager unavailable");
    return false;
}

// Writes the hardware status pipe Nickel reads to start a USB connection.
static bool nt_force_usb_connection(QString *err) {
    FILE *nhs = fopen(NT_NICKEL_HW_STATUS, "w");
    if (!nhs) {
        *err = QStringLiteral("could not open hardware status pipe");
        return false;
    }
    fputs("usb plug add", nhs);
    fclose(nhs);
    return true;
}

static bool run_nickel_misc(const NtAction &a, QString *err) {
    if (a.arg == QLatin1String("force_usb_connection")) return nt_force_usb_connection(err);
    if (a.arg == QLatin1String("home") && nt_go_home()) return true;
    if (a.arg == QLatin1String("rescan_books_full") || a.arg == QLatin1String("rescan_books")) {
        // Refused on purpose; see actions.h.
        *err = QStringLiteral("library rescan is disabled in NickelTiles");
        return false;
    }
    *err = QStringLiteral("unsupported nickel_misc action ") + a.arg;
    return false;
}

static bool wifiChangeFor(const QString &arg, NtWifiChange *change) {
    if (arg == QLatin1String("enable")) *change = NtWifiChange::Enable;
    else if (arg == QLatin1String("disable")) *change = NtWifiChange::Disable;
    else if (arg == QLatin1String("toggle")) *change = NtWifiChange::Toggle;
    else return false;
    return true;
}

static bool run_nickel_wifi(const NtAction &a, QString *err) {
    NtWifiChange change = NtWifiChange::Toggle;
    if (a.arg == QLatin1String("autoconnect") || a.arg == QLatin1String("autoconnect_silent")) {
        if (nt_wifi_connect(a.arg == QLatin1String("autoconnect_silent"))) return true;
    } else if (wifiChangeFor(a.arg, &change)) {
        if (nt_wifi_set(change)) return true;
    } else {
        *err = QStringLiteral("unsupported nickel_wifi action ") + a.arg;
        return false;
    }
    *err = QStringLiteral("wireless manager unavailable");
    return false;
}

static bool nt_run_action(const NtAction &a, QString *err) {
    const QString &kind = a.kind;
    if (kind == QLatin1String("cmd_spawn")) return run_cmd_spawn(a, err);
    if (kind == QLatin1String("cmd_output")) return run_cmd_output(a, err);
    if (kind == QLatin1String("dbg_toast")) {
        nt_toast(a.arg);
        return true;
    }
    if (kind == QLatin1String("power")) return run_power(a, err);
    if (kind == QLatin1String("nickel_misc")) return run_nickel_misc(a, err);
    if (kind == QLatin1String("nickel_wifi")) return run_nickel_wifi(a, err);
    *err = QStringLiteral("unsupported action ") + kind;
    return false;
}

bool nt_run_entry(const NtEntry &entry) {
    bool ok = true;
    bool ran = false;
    for (const NtAction &a : entry.actions) {
        if (a.chain == NtAction::OnSuccess && (!ran || !ok)) continue;
        if (a.chain == NtAction::OnFailure && (!ran || ok)) continue;
        QString err;
        const bool r = nt_run_action(a, &err);
        NT_LOG("entry '%s': %s %s -> %s%s%s", qPrintable(entry.label), qPrintable(a.kind),
            qPrintable(a.arg.left(80)), r ? "ok" : "failed", r ? "" : ": ", r ? "" : qPrintable(err));
        if (!r) nt_toast(entry.label + QStringLiteral(": ") + err, 3000);
        ok = r;
        ran = true;
    }
    return ok;
}
