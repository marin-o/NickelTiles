#include "nm_parse.h"

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>

#include "paths.h"
#include "util.h"

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

static NtAction::Chain nt_chain_for(const QString &kind) {
    if (kind == QLatin1String("chain_success")) return NtAction::OnSuccess;
    if (kind == QLatin1String("chain_failure")) return NtAction::OnFailure;
    return NtAction::Always;
}

static bool nt_skip_file(const QString &name) {
    return name == QLatin1String("doc") || name == QLatin1String("uninstall") || name.startsWith(QLatin1Char('.'));
}

// Handles one line. `current` is the entry that chain_* lines attach to; any
// line that is not a main-menu item or a chain ends it.
static void nt_parse_line(QString line, QList<NtEntry> *entries, NtEntry **current) {
    int hash = line.indexOf(QLatin1Char('#'));
    if (hash >= 0) line = line.left(hash);
    line = line.trimmed();
    if (line.isEmpty()) return;
    // leading ':' is allowed before the first field in NickelMenu files
    if (line.startsWith(QLatin1Char(':'))) line = line.mid(1).trimmed();

    const QStringList f5 = nt_split_fields(line, 5);
    const QString kind = f5.value(0);
    if (kind == QLatin1String("menu_item")) {
        *current = nullptr;
        if (f5.size() < 5 || f5.at(1) != QLatin1String("main")) return;
        NtEntry e;
        e.label = f5.at(2);
        NtAction a;
        a.kind = f5.at(3);
        a.arg = f5.at(4);
        e.actions << a;
        entries->append(e);
        *current = &entries->last();
    } else if (kind.startsWith(QLatin1String("chain_"))) {
        if (!*current) return;
        const QStringList f3 = nt_split_fields(line, 3);
        if (f3.size() < 3) return;
        NtAction a;
        a.kind = f3.at(1);
        a.arg = f3.at(2);
        a.chain = nt_chain_for(kind);
        (*current)->actions << a;
    } else {
        *current = nullptr;
    }
}

static void nt_read_file(const QString &path, QList<NtEntry> *entries) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    in.setCodec("UTF-8");
    NtEntry *current = nullptr;
    while (!in.atEnd()) nt_parse_line(in.readLine(), entries, &current);
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
        if (!nt_skip_file(name)) nt_read_file(dir.filePath(name), &entries);
    }
    NT_LOG("NickelMenu: %d main-menu entries from %s", entries.size(), qPrintable(dirPath));
    return entries;
}

QList<NtEntry> nt_nickelmenu_entries() {
    static QList<NtEntry> cachedEntries;
    QList<NtEntry> entries = nt_read_nickelmenu_entries(QStringLiteral(NT_NICKELMENU_DIR));
    if (entries.isEmpty() && !cachedEntries.isEmpty()) {
        NT_LOG("NickelMenu dir unreadable now; using the %d cached entries", cachedEntries.size());
        entries = cachedEntries;
    } else if (!entries.isEmpty()) {
        cachedEntries = entries;
    }
    return entries;
}
