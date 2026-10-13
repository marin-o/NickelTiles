#include "ui/ui.h"

#include <QTimer>

#include "core/util.h"

QFont nt_font(const QWidget *w, int px, bool bold) {
    QFont f = w->font();
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}

QStringList nt_wrap(const QString &text, const QFontMetrics &fm, int width, int maxLines) {
    QStringList lines;
    if (maxLines <= 0) return lines;
    const QStringList words = text.simplified().split(QLatin1Char(' '), QString::SkipEmptyParts);
    int i = 0;
    while (i < words.size()) {
        if (lines.size() == maxLines - 1) {
            lines << fm.elidedText(QStringList(words.mid(i)).join(QStringLiteral(" ")), Qt::ElideRight, width);
            return lines;
        }
        QString line = words.at(i++);
        while (i < words.size() && fm.width(line + QLatin1Char(' ') + words.at(i)) <= width)
            line += QLatin1Char(' ') + words.at(i++);
        lines << fm.elidedText(line, Qt::ElideRight, width);   // a single over-long word
    }
    return lines;
}

void nt_debug_screenshot(QWidget *w, const QString &name, int delayMs) {
    if (!nt_log_verbose) return;
    QTimer::singleShot(delayMs, w, [w, name]() {
        w->window()->grab().save(QStringLiteral(NT_CONFIG_DIR "/") + name + QStringLiteral(".png"), "PNG");
    });
}
