#include "ui/tiles.h"

#include <QColor>
#include <QFontMetrics>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QRect>
#include <QStringList>

#include <utility>

#include "core/config.h"
#include "books/cover.h"
#include "core/paths.h"
#include "ui/ui.h"
#include "core/util.h"

namespace {

const int kCaptionPx = 22;
const int kLongPressMs = 700;
const int kPad = 16;

QString nt_slug(const QString &label) {
    QString out;
    bool dash = false;
    for (int i = 0; i < label.size(); i++) {
        QChar c = label.at(i).toLower();
        if ((c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))) {
            out += c;
            dash = false;
        } else if (!dash && !out.isEmpty()) {
            out += QLatin1Char('-');
            dash = true;
        }
    }
    while (out.endsWith(QLatin1Char('-'))) out.chop(1);
    return out;
}

// Cached in memory: the home page is rebuilt during a library rescan while
// /mnt/onboard is unmounted, and the icons must not vanish then.
QHash<QString, QPixmap> &nt_icon_cache() {
    static QHash<QString, QPixmap> cache;
    return cache;
}

// Icon for a label: config override, icons/<slug>.png, else null (letter tile).
QPixmap nt_icon_for(const QString &label, const QString &explicitPath) {
    const QString cacheKey = label + QLatin1Char('\n') + explicitPath;
    if (nt_icon_cache().contains(cacheKey)) return nt_icon_cache().value(cacheKey);
    QString path = explicitPath;
    if (path.isEmpty()) {
        const QString prefix = QStringLiteral("icon");
        for (const nt_entry_t *e = nt_config_entries(); e; e = e->next) {
            if (QString::fromUtf8(e->key) != prefix) continue;
            QString v = QString::fromUtf8(e->val);
            int i = v.indexOf(QLatin1Char(':'));
            if (i > 0 && v.left(i).trimmed() == label) {
                path = v.mid(i + 1).trimmed();
                break;
            }
        }
    }
    if (path.isEmpty()) path = QStringLiteral(NT_ICON_DIR "/") + nt_slug(label) + QStringLiteral(".png");
    QPixmap pix(path);
    if (pix.isNull())
        NT_DBG("no icon for '%s' at %s", qPrintable(label), qPrintable(path));
    else
        nt_icon_cache().insert(cacheKey, pix);
    return pix;
}

QString nt_initials(const QString &label) {
    QString initials = label.left(1).toUpper();
    const int sp = label.indexOf(QLatin1Char(' '));
    if (sp > 0 && sp + 1 < label.size()) initials += label.at(sp + 1).toUpper();
    return initials;
}

void nt_draw_icon(QPainter &p, const QPixmap &icon, const QRect &box, bool inverted) {
    QPixmap scaled = icon.scaled(box.width(), box.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (inverted) {
        QImage img = scaled.toImage();
        img.invertPixels();
        scaled = QPixmap::fromImage(img);
    }
    p.drawPixmap(box.x() + (box.width() - scaled.width()) / 2, box.y() + (box.height() - scaled.height()) / 2, scaled);
}

// Tiles without an icon show the label's initials in a rounded frame.
void nt_draw_initials(QPainter &p, const QWidget *w, const QString &label, const QRect &box, const QColor &fg) {
    p.setPen(QPen(fg, 6));
    p.drawRoundedRect(box.adjusted(3, 3, -3, -3), 24, 24);
    p.setFont(nt_font(w, box.width() / 2, true));
    p.drawText(box, Qt::AlignCenter, nt_initials(label));
}

}   // namespace

// --- NtTile

NtTile::NtTile(QWidget *parent, NtTileHost *host, const QString &label, int px)
    : NtPressable(parent, kLongPressMs), m_host(host), m_label(label) {
    setObjectName(QStringLiteral("ntTile"));
    setFixedSize(px, px + 6 + kCaptionLines * QFontMetrics(nt_font(this, kCaptionPx)).lineSpacing() + 4);
}

void NtTile::setMarked(bool marked) {
    if (m_marked != marked) {
        m_marked = marked;
        update();
    }
}

void NtTile::setIcon(const QPixmap &icon) {
    m_icon = icon;
    update();
}

QStringList NtTile::caption(const QFontMetrics &fm, int textWidth) const {
    return nt_wrap(m_label, fm, textWidth, kCaptionLines);
}

void NtTile::tapped() {
    m_host->tileTapped(this);
}

void NtTile::longPressed() {
    m_host->tileLongPressed(this);
}

void NtTile::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const bool pressed = isPressed();
    const QColor fg = pressed ? Qt::white : Qt::black;
    const QColor bg = pressed ? Qt::black : Qt::white;
    p.fillRect(rect(), bg);
    if (m_marked) {
        p.setPen(QPen(fg, 6));
        p.drawRoundedRect(rect().adjusted(3, 3, -3, -3), 14, 14);
    }
    const int iconSide = width() - 2 * kPad;
    const QRect iconRect(kPad, kPad, iconSide, iconSide);
    if (!m_icon.isNull())
        nt_draw_icon(p, m_icon, iconRect, pressed);
    else
        nt_draw_initials(p, this, m_label, iconRect, fg);

    const QFont f = nt_font(this, kCaptionPx);
    const QFontMetrics fm(f);
    p.setFont(f);
    p.setPen(fg);
    const int textWidth = width() - 8;
    int y = kPad + iconSide + 6;
    for (const QString &line : caption(fm, textWidth)) {
        p.drawText(QRect(4, y, textWidth, fm.lineSpacing()), Qt::AlignHCenter | Qt::AlignTop, line);
        y += fm.lineSpacing();
    }
}

// --- entry, command, settings tiles

NtEntryTile::NtEntryTile(QWidget *parent, NtTileHost *host, const NtEntry &entry, int px)
    : NtTile(parent, host, entry.label, px), m_entry(entry) {
    setIcon(nt_icon_for(entry.label, QString()));
}

void NtEntryTile::activate() {
    nt_run_entry(m_entry);
}

NtCommandTile::NtCommandTile(QWidget *parent, NtTileHost *host, const QString &label, const QString &iconPath,
                             const QString &command, int px)
    : NtTile(parent, host, label, px), m_command(command) {
    setIcon(nt_icon_for(label, iconPath));
}

void NtCommandTile::activate() {
    nt_run_command(label(), m_command);
}

const char *const NtSettingsTile::kLabel = "Tiles settings";

NtSettingsTile::NtSettingsTile(QWidget *parent, NtTileHost *host, int px, std::function<void()> open)
    : NtTile(parent, host, QString::fromLatin1(kLabel), px), m_open(std::move(open)) {
    setIcon(nt_icon_for(label(), QString()));
}

void NtSettingsTile::activate() {
    m_open();
}

// --- continue reading

const char *const NtRecentTile::kLabel = "Continue reading";

NtRecentTile::NtRecentTile(QWidget *parent, NtTileHost *host, int px, const NtSettings &settings)
    : NtTile(parent, host, QString::fromLatin1(kLabel), px), m_recentSource(settings.recentSource),
      m_recentInfo(settings.recentInfo), m_recentCover(settings.recentCover) {
    setIcon(nt_icon_for(label(), QString()));
}

void NtRecentTile::refresh() {
    m_book = nt_recent_book(m_recentSource);
    if (!m_book.valid) {
        setVisible(false);
        NT_DBG("recent: no book");
        return;
    }
    QPixmap cover;
    if (m_recentCover) cover = nt_book_cover(m_book.path, QStringLiteral(NT_COVER_CACHE_DIR), 400);
    setIcon(cover.isNull() ? nt_icon_for(label(), QString()) : cover);
    setVisible(true);
    update();
    NT_LOG("recent: %s '%s' by '%s' %d%% -> %s", nt_reader_app_name(m_book.app), qPrintable(m_book.title),
           qPrintable(m_book.author), m_book.percent, qPrintable(m_book.path));
}

// Title, author and progress on their own lines, as chosen by nt_recent_info.
// The title gets whatever lines the author and progress leave.
QStringList NtRecentTile::caption(const QFontMetrics &fm, int textWidth) const {
    QStringList lines;
    QStringList tail;
    if (m_recentInfo.author && !m_book.author.isEmpty())
        tail << fm.elidedText(m_book.author, Qt::ElideRight, textWidth);
    if (m_recentInfo.progress && m_book.percent >= 0) tail << QString::number(m_book.percent) + QStringLiteral("%");
    if (m_recentInfo.title) lines << nt_wrap(m_book.title, fm, textWidth, kCaptionLines - tail.size());
    lines << tail;
    return lines;
}

void NtRecentTile::activate() {
    nt_open_recent_book(m_book);
}
