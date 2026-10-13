#ifndef NT_TILES_H
#define NT_TILES_H

// The tiles in the grid. Each is a square icon (or the label's initials) with
// a caption of up to three lines underneath; it inverts while pressed and gets
// a thick frame while marked for arranging.

#include <functional>

#include <QPixmap>
#include <QString>
#include <QStringList>

#include "actions.h"
#include "pressable.h"
#include "recent.h"
#include "settings_model.h"

class NtTile;

// What a tile reports to the grid that holds it.
class NtTileHost {
public:
    virtual ~NtTileHost() = default;
    virtual void tileTapped(NtTile *tile) = 0;
    virtual void tileLongPressed(NtTile *tile) = 0;
};

class NtTile : public NtPressable {
public:
    static const int kCaptionLines = 3;

    NtTile(QWidget *parent, NtTileHost *host, const QString &label, int px);

    // Identity used by order: and hide: (the NickelMenu label for entry tiles).
    const QString &label() const { return m_label; }

    void setMarked(bool marked);

    // What a tap does outside arrange mode.
    virtual void activate() = 0;

    // False for tiles that only change the grid itself (settings), which keep
    // working when nt_actions is off.
    virtual bool runsAction() const { return true; }

protected:
    void setIcon(const QPixmap &icon);

    // The caption lines for `width` px; the default wraps the label.
    virtual QStringList caption(const QFontMetrics &fm, int width) const;

    void tapped() override;
    void longPressed() override;
    void paintEvent(QPaintEvent *event) override;

private:
    NtTileHost *m_host;
    QString m_label;
    QPixmap m_icon;
    bool m_marked = false;
};

// A NickelMenu main-menu entry.
class NtEntryTile : public NtTile {
public:
    NtEntryTile(QWidget *parent, NtTileHost *host, const NtEntry &entry, int px);
    void activate() override;

private:
    NtEntry m_entry;
};

// A tile: line from the config (label, icon path, shell command).
class NtCommandTile : public NtTile {
public:
    NtCommandTile(QWidget *parent, NtTileHost *host, const QString &label, const QString &iconPath,
                  const QString &command, int px);
    void activate() override;

private:
    QString m_command;
};

// Opens the settings page.
class NtSettingsTile : public NtTile {
public:
    static const char *const kLabel;

    NtSettingsTile(QWidget *parent, NtTileHost *host, int px, std::function<void()> open);
    void activate() override;
    bool runsAction() const override { return false; }

private:
    std::function<void()> m_open;
};

// Continue reading: the most recent book, with its cover or (when the recent
// cover setting is off, or the book has none) a book icon; hidden when there
// is no book. Takes the recent: settings (source, caption, cover) from
// `settings`. refresh() re-reads the apps' records.
class NtRecentTile : public NtTile {
public:
    static const char *const kLabel;

    NtRecentTile(QWidget *parent, NtTileHost *host, int px, const NtSettings &settings);
    void refresh();
    void activate() override;

protected:
    QStringList caption(const QFontMetrics &fm, int width) const override;

private:
    NtRecentSource m_recentSource;
    NtRecentInfo m_recentInfo;
    bool m_recentCover;
    NtRecentBook m_book;
};

#endif
