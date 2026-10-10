// NickelTiles: an app grid on the Kobo home screen, built on NickelHook.
//
// It hooks Ui_HomePageView::setupUi, which HomePageView's constructor calls
// through the PLT to build the widget tree. NickelHome hooks the constructor
// itself; NickelHook does not chain two mods on one symbol (each gets the real
// function as "original", last patch wins), so this mod must not share that
// symbol. After the tree is built it inserts an NtGrid at the top of
// mainContainer's mainLayout and (by default) hides Kobo's own three rows.
//
// Tiles: "Continue reading" (delegates to Kobo's hidden current-read widget),
// one per NickelMenu main-menu entry (actions ported in nmentries.cc), and
// extra tile: lines from the config. Press darkens the tile; long-press enters
// arrange mode (tap another tile to swap; order saved to the config).
//
// Home page tree on firmware 4.45.23697 (from Ui_HomePageView::setupUi):
//   HomePageView > verticalLayout > mainContainer (GenericContainerWidget)
//     > mainLayout (QVBoxLayout)
//        [0] row1 / row1Layout: row1col1 {findReadWidget, fourBookWidget}, row1col2 {quickTourWidget}
//        [1] row2 / row2Layout: row2col1 {myBooksWidget, topPicksWidget}, row2col2 {quickTourWidget2}
//        [2] row3 / row3Layout: browseKoboContainer, smartLinkContainer   (stretch 1)
// See docs/homepage-map.md.

#include <QBoxLayout>
#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QMetaObject>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QRegion>
#include <QSize>
#include <QShowEvent>
#include <QSpacerItem>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QTimerEvent>
#include <QTouchEvent>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <cstddef>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <NickelHook.h>

#include "config.h"
#include "epubcover.h"
#include "nmentries.h"
#include "settings.h"
#include "util.h"

typedef QWidget HomePageView;
typedef void Ui_HomePageView;
void (*Ui_HomePageView_setupUi)(Ui_HomePageView *_this, HomePageView *view);
typedef void PowerViewController;
void (*PowerViewController_updateCover)(PowerViewController *_this) = NULL;

// libnickel symbols used by the ported NickelMenu actions (all optional).
typedef void MainWindowController;
typedef void N3PowerWorkflowManager;
typedef void PlugWorkflowManager;
typedef void WirelessWorkflowManager;
MainWindowController *(*MainWindowController_sharedInstance)() = NULL;
void (*MainWindowController_toast)(MainWindowController*, QString const&, QString const&, int) = NULL;
N3PowerWorkflowManager *(*N3PowerWorkflowManager_sharedInstance)() = NULL;
void (*N3PowerWorkflowManager_powerOff)(N3PowerWorkflowManager*, bool) = NULL;
void (*N3PowerWorkflowManager_reboot)(N3PowerWorkflowManager*) = NULL;
void (*N3PowerWorkflowManager_requestSleep)(N3PowerWorkflowManager*) = NULL;
PlugWorkflowManager *(*PlugWorkflowManager_sharedInstance)() = NULL;
void (*PlugWorkflowManager_sync)(PlugWorkflowManager*) = NULL;
void (*PlugWorkflowManager_onCancelAndConnect)(PlugWorkflowManager*) = NULL;
void (*PlugWorkflowManager_unplugged)(PlugWorkflowManager*) = NULL;
WirelessWorkflowManager *(*WirelessWorkflowManager_sharedInstance)() = NULL;
void (*WirelessWorkflowManager_connectWireless)(WirelessWorkflowManager*, bool, bool) = NULL;
void (*WirelessWorkflowManager_connectWirelessSilently)(WirelessWorkflowManager*) = NULL;
bool (*WirelessWorkflowManager_isAirplaneMode)(WirelessWorkflowManager*) = NULL;
void (*WirelessWorkflowManager_setAirplaneMode)(WirelessWorkflowManager*, bool) = NULL;
void (*MainNavController_home)(void*) = NULL;
// Nickel's current-read widget tap (a Qt signal; calling the emitter is what
// a real tap does). Invoking it by name through the meta-object failed.
void (*SingleBookHomeWidget_tapped)(void*) = NULL;
// Nickel's own "open this book" path (what tapping a book in the library does):
//   Volume v = VolumeManager::getByIdBytes(contentId, Device::getDbName(Device::getCurrentDevice()));
//   ReadBookActionProxy *p = new ReadBookActionProxy(parent, v);   // 52 bytes on 4.45.23697
//   p->onSelected();                                               // -> startWorker -> openBook
typedef void Device;
typedef void Volume;
typedef void ReadBookActionProxy;
Device *(*Device_getCurrentDevice)() = NULL;
void (*Device_getDbName)(QString *out, Device const*) = NULL;                 // QString Device::getDbName() const (sret)
void (*VolumeManager_getByIdBytes)(Volume *out, QByteArray const&, QString const&) = NULL;   // static, sret
bool (*Volume_isValid)(Volume const*) = NULL;
void (*Volume_dtor)(Volume*) = NULL;
void (*ReadBookActionProxy_ctor)(ReadBookActionProxy*, QObject*, Volume const*) = NULL;   // Volume const& in the ABI
void (*ReadBookActionProxy_onSelected)(ReadBookActionProxy*) = NULL;

static int nt_init();

static struct nh_info NickelTiles = (struct nh_info){
    .name            = "NickelTiles",
    .desc            = "App grid on the Kobo home screen.",
    .uninstall_flag  = NT_CONFIG_DIR "/uninstall",
    .uninstall_xflag = NT_CONFIG_DIR,
    // The home page is built within a second of plugin load, so 3 s (same as
    // NickelMenu/NickelHome) covers it. Longer is harmful: launching an app
    // from a tile kills Nickel, and if that happens while the failsafe is
    // still armed the mod stays disabled (seen with 20 s on 2026-10-09).
    .failsafe_delay  = 3,
};

static struct nh_hook NickelTilesHook[] = {
    {
        .sym      = "_ZN15Ui_HomePageView7setupUiEP7QWidget",
        .sym_new  = "_nt_setupui_hook",
        .lib      = "libnickel.so.1.0.0",
        .out      = nh_symoutptr(Ui_HomePageView_setupUi),
        .desc     = "home page grid (4.23.15505+ layout)",
        .optional = false,
    }, //libnickel 4.45.23697 * _ZN15Ui_HomePageView7setupUiEP7QWidget
    {
        // Called through the PLT from PowerViewController::loadView. With
        // "Show book covers full screen" on, it shows a random image from
        // .kobo/screensaver before falling back to Nickel's last-read cover.
        .sym      = "_ZN19PowerViewController11updateCoverEv",
        .sym_new  = "_nt_updatecover_hook",
        .lib      = "libnickel.so.1.0.0",
        .out      = nh_symoutptr(PowerViewController_updateCover),
        .desc     = "sleep screen cover of the most recent book",
        .optional = true,
    }, //libnickel 4.45.23697 * _ZN19PowerViewController11updateCoverEv
    {0},
};

static struct nh_dlsym NickelTilesDlsym[] = {
    {.name = "_ZN20MainWindowController14sharedInstanceEv",            .out = nh_symoutptr(MainWindowController_sharedInstance),            .desc = NULL, .optional = true},
    {.name = "_ZN20MainWindowController5toastERK7QStringS2_i",         .out = nh_symoutptr(MainWindowController_toast),                     .desc = NULL, .optional = true},
    {.name = "_ZN22N3PowerWorkflowManager14sharedInstanceEv",          .out = nh_symoutptr(N3PowerWorkflowManager_sharedInstance),          .desc = NULL, .optional = true},
    {.name = "_ZN22N3PowerWorkflowManager8powerOffEb",                 .out = nh_symoutptr(N3PowerWorkflowManager_powerOff),                .desc = NULL, .optional = true},
    {.name = "_ZN22N3PowerWorkflowManager6rebootEv",                   .out = nh_symoutptr(N3PowerWorkflowManager_reboot),                  .desc = NULL, .optional = true},
    {.name = "_ZN22N3PowerWorkflowManager12requestSleepEv",            .out = nh_symoutptr(N3PowerWorkflowManager_requestSleep),            .desc = NULL, .optional = true},
    {.name = "_ZN19PlugWorkflowManager14sharedInstanceEv",             .out = nh_symoutptr(PlugWorkflowManager_sharedInstance),             .desc = NULL, .optional = true},
    {.name = "_ZN19PlugWorkflowManager4syncEv",                        .out = nh_symoutptr(PlugWorkflowManager_sync),                       .desc = NULL, .optional = true},
    {.name = "_ZN19PlugWorkflowManager18onCancelAndConnectEv",         .out = nh_symoutptr(PlugWorkflowManager_onCancelAndConnect),         .desc = NULL, .optional = true},
    {.name = "_ZN19PlugWorkflowManager9unpluggedEv",                   .out = nh_symoutptr(PlugWorkflowManager_unplugged),                  .desc = NULL, .optional = true},
    {.name = "_ZN23WirelessWorkflowManager14sharedInstanceEv",         .out = nh_symoutptr(WirelessWorkflowManager_sharedInstance),         .desc = NULL, .optional = true},
    {.name = "_ZN23WirelessWorkflowManager15connectWirelessEbb",       .out = nh_symoutptr(WirelessWorkflowManager_connectWireless),        .desc = NULL, .optional = true},
    {.name = "_ZN23WirelessWorkflowManager23connectWirelessSilentlyEv",.out = nh_symoutptr(WirelessWorkflowManager_connectWirelessSilently),.desc = NULL, .optional = true},
    {.name = "_ZN23WirelessWorkflowManager14isAirplaneModeEv",         .out = nh_symoutptr(WirelessWorkflowManager_isAirplaneMode),         .desc = NULL, .optional = true},
    {.name = "_ZN23WirelessWorkflowManager15setAirplaneModeEb",        .out = nh_symoutptr(WirelessWorkflowManager_setAirplaneMode),        .desc = NULL, .optional = true},
    {.name = "_ZN17MainNavController4homeEv",                          .out = nh_symoutptr(MainNavController_home),                         .desc = NULL, .optional = true},
    {.name = "_ZN20SingleBookHomeWidget6tappedEv",                     .out = nh_symoutptr(SingleBookHomeWidget_tapped),                    .desc = NULL, .optional = true},
    {.name = "_ZN6Device16getCurrentDeviceEv",                         .out = nh_symoutptr(Device_getCurrentDevice),                        .desc = NULL, .optional = true},
    {.name = "_ZNK6Device9getDbNameEv",                                .out = nh_symoutptr(Device_getDbName),                               .desc = NULL, .optional = true},
    {.name = "_ZN13VolumeManager12getByIdBytesERK10QByteArrayRK7QString", .out = nh_symoutptr(VolumeManager_getByIdBytes),                  .desc = NULL, .optional = true},
    {.name = "_ZNK6Volume7isValidEv",                                  .out = nh_symoutptr(Volume_isValid),                                 .desc = NULL, .optional = true},
    {.name = "_ZN6VolumeD1Ev",                                         .out = nh_symoutptr(Volume_dtor),                                    .desc = NULL, .optional = true},
    {.name = "_ZN19ReadBookActionProxyC1EP7QObjectRK6Volume",          .out = nh_symoutptr(ReadBookActionProxy_ctor),                       .desc = NULL, .optional = true},
    {.name = "_ZN19ReadBookActionProxy10onSelectedEv",                 .out = nh_symoutptr(ReadBookActionProxy_onSelected),                 .desc = NULL, .optional = true},
    {0},
};

NickelHook(
    .init  = &nt_init,
    .info  = &NickelTiles,
    .hook  = NickelTilesHook,
    .dlsym = NickelTilesDlsym,
)

static int nt_init() {
    nt_config_get("");
    NT_LOG("startup: NickelTiles " NH_VERSION);
    nt_log_firmware();
    NT_LOG("startup: enabled=%d draw=%d actions=%d hide_rows=%d recent=%d columns=%d tile_px=%d sleep_cover=%d verbose=%d hook=%p sleep_hook=%p toast=%p power=%p wifi=%p",
        nt_config_bool("nt_enabled", true),
        nt_config_bool("nt_draw", true),
        nt_config_bool("nt_actions", true),
        nt_config_bool("nt_hide_rows", true),
        nt_config_bool("nt_recent", true),
        nt_config_int("nt_columns", 5),
        nt_config_int("nt_tile_px", 180),
        nt_config_bool("nt_sleep_cover", true),
        nt_log_verbose,
        (void *)Ui_HomePageView_setupUi,
        (void *)PowerViewController_updateCover,
        (void *)MainWindowController_toast,
        (void *)N3PowerWorkflowManager_reboot,
        (void *)WirelessWorkflowManager_connectWireless);
    return 0;
}

// --- helpers

static QString nt_slug(const QString &label) {
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

static QWidget *nt_find_direct_child(QWidget *parent, const char *name) {
    const QString qname = QString::fromLatin1(name);
    const QObjectList children = parent->children();
    for (QObject *child : children) {
        QWidget *cw = qobject_cast<QWidget*>(child);
        if (cw && cw->objectName() == qname)
            return cw;
    }
    return NULL;
}

static void nt_log_tree(QWidget *w, int depth) {
    if (depth > 6) return;
    const QObjectList children = w->children();
    for (QObject *child : children) {
        QWidget *cw = qobject_cast<QWidget*>(child);
        if (!cw) continue;
        NT_LOG("tree: %*s%s '%s' %dx%d visible=%d layout=%s", depth * 2, "",
            cw->metaObject()->className(), qPrintable(cw->objectName()),
            cw->width(), cw->height(), cw->isVisible() ? 1 : 0,
            cw->layout() ? cw->layout()->metaObject()->className() : "-");
        nt_log_tree(cw, depth + 1);
    }
}

// Icon for a label: config override, icons/<slug>.png, else null (letter tile).
// Cached in memory: the home page is rebuilt during a library rescan while
// /mnt/onboard is unmounted, and the icons must not vanish then.
static QHash<QString, QPixmap> &nt_icon_cache() {
    static QHash<QString, QPixmap> cache;
    return cache;
}

static QPixmap nt_icon_for(const QString &label, const QString &explicitPath) {
    const QString cacheKey = label + QLatin1Char('\n') + explicitPath;
    if (nt_icon_cache().contains(cacheKey))
        return nt_icon_cache().value(cacheKey);
    QString path = explicitPath;
    if (path.isEmpty()) {
        const QString prefix = QStringLiteral("icon");
        for (const nt_entry_t *e = nt_config_entries(); e; e = e->next) {
            if (QString::fromUtf8(e->key) != prefix) continue;
            QString v = QString::fromUtf8(e->val);
            int i = v.indexOf(QLatin1Char(':'));
            if (i > 0 && v.left(i).trimmed() == label) { path = v.mid(i + 1).trimmed(); break; }
        }
    }
    if (path.isEmpty())
        path = QString::fromLatin1(NT_CONFIG_DIR "/icons/") + nt_slug(label) + QStringLiteral(".png");
    QPixmap pix(path);
    if (pix.isNull())
        NT_DBG("no icon for '%s' at %s", qPrintable(label), qPrintable(path));
    else
        nt_icon_cache().insert(cacheKey, pix);
    return pix;
}

// --- tiles

// Tile captions: 22 px text, up to three lines under the icon.
static const int kNtCaptionPx = 22;
static const int kNtCaptionLines = 3;

static QFont nt_caption_font(const QWidget *w) {
    QFont f = w->font();
    f.setPixelSize(kNtCaptionPx);
    return f;
}

// Greedy word wrap of `text` into at most `maxLines` lines of `width` px; the
// last line takes the rest of the text and is elided if it does not fit.
static QStringList nt_wrap(const QString &text, const QFontMetrics &fm, int width, int maxLines) {
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

// What the Continue reading caption shows: nt_recent_info is a comma list of
// title, author, progress ("none" for nothing). Default: all three.
static void nt_recent_info(bool *title, bool *author, bool *progress) {
    const char *v = nt_config_get("nt_recent_info");
    if (!v || !*v) { *title = *author = *progress = true; return; }
    const QString s = QString::fromUtf8(v).toLower();
    *title = s.contains(QLatin1String("title"));
    *author = s.contains(QLatin1String("author"));
    *progress = s.contains(QLatin1String("progress"));
}

class NtGrid;

class NtTile : public QWidget {
public:
    NtTile(NtGrid *grid, const QString &label, const QPixmap &icon, int px);
    const QString &label() const { return m_label; }
    void setIcon(const QPixmap &icon) { m_icon = icon; update(); }
    void setTitle(const QString &title) { m_title = title; m_hasInfo = false; update(); }
    // Book caption: title, author and progress on their own lines (as chosen by
    // nt_recent_info); percent < 0 means unknown.
    void setInfo(const QString &title, const QString &author, int percent) {
        m_title = title; m_author = author; m_percent = percent; m_hasInfo = true; update();
    }
    void setMarked(bool marked) { if (m_marked != marked) { m_marked = marked; update(); } }
    virtual void activate() = 0;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void pressed(bool down);
    void released(bool inside);

    NtGrid *m_grid;
    QString m_label;     // identity (NickelMenu label)
    QString m_title;     // drawn text (label unless overridden, e.g. book title)
    QString m_author;
    int m_percent;
    bool m_hasInfo;
    QPixmap m_icon;
    bool m_pressed;
    bool m_marked;
    bool m_longPressed;
    int m_timer;
};

class NtEntryTile : public NtTile {
public:
    NtEntryTile(NtGrid *grid, const NtEntry &entry, int px)
        : NtTile(grid, entry.label, nt_icon_for(entry.label, QString()), px), m_entry(entry) {}
    void activate() override {
        if (!nt_config_bool("nt_actions", true)) { NT_LOG("nt_actions=0; not running '%s'", qPrintable(label())); return; }
        nt_run_entry(m_entry);
    }
private:
    NtEntry m_entry;
};

class NtCommandTile : public NtTile {
public:
    NtCommandTile(NtGrid *grid, const nt_tile_t *t, int px)
        : NtTile(grid, QString::fromUtf8(t->label), nt_icon_for(QString::fromUtf8(t->label), QString::fromUtf8(t->icon)), px),
          m_command(QString::fromUtf8(t->command)) {}
    void activate() override {
        if (!nt_config_bool("nt_actions", true)) { NT_LOG("nt_actions=0; not running '%s'", qPrintable(label())); return; }
        NtEntry e;
        e.label = label();
        NtAction a;
        a.kind = QStringLiteral("cmd_spawn");
        a.arg = QStringLiteral("quiet:") + m_command;
        a.chain = 0;
        e.actions << a;
        nt_run_entry(e);
    }
private:
    QString m_command;
};

static void nt_install_grid(HomePageView *view);
static QStringList nt_all_tile_labels();

// Built-in tile that opens the settings screen. Always present (a hide: line in
// the text config still removes it); not listed on the settings screen itself.
#define NT_SETTINGS_LABEL "Tiles settings"
class NtSettingsTile : public NtTile {
public:
    NtSettingsTile(NtGrid *grid, HomePageView *view, int px)
        : NtTile(grid, QStringLiteral(NT_SETTINGS_LABEL), nt_icon_for(QStringLiteral(NT_SETTINGS_LABEL), QString()), px), m_view(view) {}
    void activate() override {
        HomePageView *view = m_view;
        nt_open_settings(view, nt_all_tile_labels(), [view]() { nt_install_grid(view); });
    }
private:
    HomePageView *m_view;
};

// Opens a book in Nickel's own reader through the same objects the library
// uses when a book is tapped. Returns false (and logs why) if any piece is
// unavailable, so the caller can fall back.
static bool nt_open_in_nickel(const QString &contentId) {
    if (contentId.isEmpty()) return false;
    if (!Device_getCurrentDevice || !Device_getDbName || !VolumeManager_getByIdBytes || !Volume_isValid
            || !Volume_dtor || !ReadBookActionProxy_ctor || !ReadBookActionProxy_onSelected) {
        NT_LOG("open in nickel: symbols missing (dev=%p db=%p byId=%p valid=%p dtor=%p proxy=%p sel=%p)",
            (void*)Device_getCurrentDevice, (void*)Device_getDbName, (void*)VolumeManager_getByIdBytes, (void*)Volume_isValid,
            (void*)Volume_dtor, (void*)ReadBookActionProxy_ctor, (void*)ReadBookActionProxy_onSelected);
        return false;
    }
    Device *dev = Device_getCurrentDevice();
    if (!dev) { NT_LOG("open in nickel: no current device"); return false; }
    QString dbName;
    Device_getDbName(&dbName, dev);
    // Volume is small (vtable + shared data pointer); 64 bytes leaves plenty of room.
    unsigned char volume[64] __attribute__((aligned(8)));
    memset(volume, 0, sizeof(volume));
    const QByteArray idBytes = contentId.toUtf8();
    VolumeManager_getByIdBytes(volume, idBytes, dbName);
    const bool valid = Volume_isValid(volume);
    NT_LOG("open in nickel: db='%s' id='%s' valid=%d", qPrintable(dbName), idBytes.constData(), valid ? 1 : 0);
    if (!valid) { Volume_dtor(volume); return false; }
    // ReadBookActionProxy is 52 bytes on 4.45.23697 (ActionProxyMixin::readBookProxy);
    // allocate generously. Parented to the application so the home page's
    // destruction while the reader opens cannot delete it mid-flight.
    void *proxy = calloc(1, 256);
    ReadBookActionProxy_ctor(proxy, QCoreApplication::instance(), volume);
    ReadBookActionProxy_onSelected(proxy);
    Volume_dtor(volume);
    NT_LOG("open in nickel: proxy selected");
    return true;
}

// Reads KOReader's most recent book: history.lua's first entry, with title and
// author from the book's sidecar (<book>.sdr/metadata.<ext>.lua). Returns false
// if there is no usable entry.
static QString nt_lua_string(const QString &text, const char *key) {
    // ["key"] = "value" (value may contain escaped quotes / line continuations)
    const QString needle = QStringLiteral("[\"") + QString::fromLatin1(key) + QStringLiteral("\"] = \"");
    int i = text.indexOf(needle);
    if (i < 0) return QString();
    i += needle.size();
    QString out;
    while (i < text.size()) {
        QChar c = text.at(i);
        if (c == QLatin1Char('\\') && i + 1 < text.size()) {
            QChar n = text.at(i + 1);
            if (n == QLatin1Char('n')) out += QLatin1Char(' ');
            else if (n == QLatin1Char('\n')) out += QLatin1Char(' ');
            else out += n;
            i += 2;
            continue;
        }
        if (c == QLatin1Char('"')) break;
        out += c;
        i++;
    }
    return out.simplified();
}

static bool nt_koreader_recent(QString *path, QString *title, QString *author, int *percent) {
    QFile h(QStringLiteral("/mnt/onboard/.adds/koreader/history.lua"));
    if (!h.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString hist = QString::fromUtf8(h.readAll());
    const QString file = nt_lua_string(hist, "file");
    if (file.isEmpty() || !QFile::exists(file)) return false;
    *path = file;
    *title = QString();
    *author = QString();
    *percent = -1;
    int dot = file.lastIndexOf(QLatin1Char('.'));
    int slash = file.lastIndexOf(QLatin1Char('/'));
    const QString base = dot > slash ? file.left(dot) : file;
    const QString ext = dot > slash ? file.mid(dot + 1) : QString();
    QFile s(base + QStringLiteral(".sdr/metadata.") + ext + QStringLiteral(".lua"));
    if (s.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString meta = QString::fromUtf8(s.readAll());
        int dp = meta.indexOf(QStringLiteral("[\"doc_props\"]"));
        const QString props = dp >= 0 ? meta.mid(dp, 4000) : QString();
        *title = nt_lua_string(props, "title");
        *author = nt_lua_string(props, "authors");
        int pf = meta.indexOf(QStringLiteral("[\"percent_finished\"] = "));
        if (pf >= 0) {
            bool ok = false;
            double v = meta.mid(pf + 23, 24).section(QLatin1Char(','), 0, 0).trimmed().toDouble(&ok);
            if (ok) *percent = int(v * 100.0 + 0.5);
        }
    }
    if (title->isEmpty()) title->operator=(file.mid(slash + 1, (dot > slash ? dot : file.size()) - slash - 1));
    return true;
}

// "Continue reading": by default KOReader's most recent book (opened in
// KOReader, where this reader's books live). With nt_recent_source:nickel it
// mirrors Kobo's hidden current-read widget instead and forwards the tap to it.
class NtRecentTile : public NtTile {
public:
    NtRecentTile(NtGrid *grid, HomePageView *view, int px)
        : NtTile(grid, QStringLiteral("Continue reading"), nt_icon_for(QStringLiteral("Continue reading"), QString()), px),
          m_view(view), m_target(NULL), m_platoLaunch(false) {}

    struct Candidate { QString app, path, title, author, id; qint64 time; int percent; bool valid; };

    static Candidate fromKoreader() {
        Candidate c; c.valid = false; c.time = 0; c.percent = -1; c.app = QStringLiteral("koreader");
        QFile h(QStringLiteral("/mnt/onboard/.adds/koreader/history.lua"));
        if (!h.open(QIODevice::ReadOnly | QIODevice::Text)) return c;
        const QString hist = QString::fromUtf8(h.readAll());
        int t = hist.indexOf(QStringLiteral("[\"time\"] = "));
        if (t >= 0) c.time = hist.mid(t + 11, 20).section(QLatin1Char(','), 0, 0).trimmed().toLongLong();
        QString path, title, author; int percent = -1;
        if (!nt_koreader_recent(&path, &title, &author, &percent)) return c;
        c.path = path; c.title = title; c.author = author; c.percent = percent; c.valid = true;
        return c;
    }

    // Plato keeps its library index at the root of onboard: .metadata.json,
    // one object per book with "file": {"path": ...} and, once opened,
    // "reader": {"opened": <ISO time>, "current_page", "pages_count", ...}.
    static Candidate fromPlato() {
        Candidate c; c.valid = false; c.time = 0; c.percent = -1; c.app = QStringLiteral("plato");
        QFile f(QStringLiteral("/mnt/onboard/.metadata.json"));
        if (!f.open(QIODevice::ReadOnly)) return c;
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (!doc.isObject()) { NT_DBG("recent (plato): metadata parse: %s", qPrintable(err.errorString())); return c; }
        const QJsonObject root = doc.object();
        // Plato (database mode) keeps the per-book state in
        // /mnt/onboard/.reading-states/<fingerprint>.json:
        //   {"opened": "2026-10-10 02:01:57", "currentPage": N, "pagesCount": M, ...}
        // where <fingerprint> is the key of the book in .metadata.json.
        QDir states(QStringLiteral("/mnt/onboard/.reading-states"));
        const QStringList files = states.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
        for (const QString &name : files) {
            QFile sf(states.filePath(name));
            if (!sf.open(QIODevice::ReadOnly)) continue;
            const QJsonObject st = QJsonDocument::fromJson(sf.readAll()).object();
            const QString opened = st.value(QStringLiteral("opened")).toString();
            QDateTime dt = QDateTime::fromString(opened, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            if (!dt.isValid()) dt = QDateTime::fromString(opened, Qt::ISODate);
            if (!dt.isValid()) continue;
            const qint64 t = dt.toTime_t();
            if (t <= c.time) continue;
            const QString key = name.left(name.size() - 5);
            const QJsonObject b = root.value(key).toObject();
            if (b.isEmpty()) { NT_DBG("recent (plato): state %s has no index entry", qPrintable(key)); continue; }
            c.time = t;
            c.path = QStringLiteral("/mnt/onboard/") + b.value(QStringLiteral("file")).toObject().value(QStringLiteral("path")).toString();
            c.title = b.value(QStringLiteral("title")).toString();
            c.author = b.value(QStringLiteral("author")).toString();
            const double cur = st.value(QStringLiteral("currentPage")).toDouble(-1), tot = st.value(QStringLiteral("pagesCount")).toDouble(0);
            c.percent = (cur >= 0 && tot > 0) ? int(cur * 100.0 / tot + 0.5) : -1;
            c.valid = QFile::exists(c.path);
            if (c.title.isEmpty()) c.title = c.path.section(QLatin1Char('/'), -1).section(QLatin1Char('.'), 0, -2);
        }
        return c;
    }

    // Nickel: KoboReader.sqlite through the sqlite3 already inside the process
    // (read-only, WAL-safe). Opened natively through the hidden current-read widget.
    static Candidate fromNickel() {
        Candidate c; c.valid = false; c.time = 0; c.percent = -1; c.app = QStringLiteral("nickel");
        typedef int (*open_t)(const char*, void**, int, const char*);
        typedef int (*prep_t)(void*, const char*, int, void**, const char**);
        typedef int (*step_t)(void*);
        typedef const unsigned char *(*coltext_t)(void*, int);
        typedef int (*colint_t)(void*, int);
        typedef int (*fin_t)(void*);
        typedef int (*close_t)(void*);
        open_t op = (open_t)dlsym(RTLD_DEFAULT, "sqlite3_open_v2");
        prep_t pr = (prep_t)dlsym(RTLD_DEFAULT, "sqlite3_prepare_v2");
        step_t st = (step_t)dlsym(RTLD_DEFAULT, "sqlite3_step");
        coltext_t ct = (coltext_t)dlsym(RTLD_DEFAULT, "sqlite3_column_text");
        colint_t ci = (colint_t)dlsym(RTLD_DEFAULT, "sqlite3_column_int");
        fin_t fn = (fin_t)dlsym(RTLD_DEFAULT, "sqlite3_finalize");
        close_t cl = (close_t)dlsym(RTLD_DEFAULT, "sqlite3_close");
        if (!op || !pr || !st || !ct || !ci || !fn || !cl) { NT_DBG("recent (nickel): sqlite3 symbols unavailable"); return c; }
        void *db = NULL;
        if (op("/mnt/onboard/.kobo/KoboReader.sqlite", &db, 1 /*SQLITE_OPEN_READONLY*/, NULL) != 0 || !db) { if (db) cl(db); return c; }
        void *stmt = NULL;
        const char *sql = "select ContentID, Title, Attribution, DateLastRead, ___PercentRead from content "
                          "where ContentType='6' and ContentID like 'file://%' and DateLastRead is not null "
                          "order by DateLastRead desc limit 1";
        if (pr(db, sql, -1, &stmt, NULL) == 0 && stmt) {
            if (st(stmt) == 100 /*SQLITE_ROW*/) {
                const QString id = QString::fromUtf8((const char*)ct(stmt, 0));
                c.title = QString::fromUtf8((const char*)(ct(stmt, 1) ? ct(stmt, 1) : (const unsigned char*)""));
                c.author = QString::fromUtf8((const char*)(ct(stmt, 2) ? ct(stmt, 2) : (const unsigned char*)""));
                const QString when = QString::fromUtf8((const char*)(ct(stmt, 3) ? ct(stmt, 3) : (const unsigned char*)""));
                c.percent = ci(stmt, 4);
                // DateLastRead is UTC with a trailing Z; Qt 5.2's ISODate parse
                // treats it as local time, which put Nickel 2 h behind KOReader.
                QString w = when;
                if (w.endsWith(QLatin1Char('Z'))) w.chop(1);
                QDateTime dt = QDateTime::fromString(w, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
                dt.setTimeSpec(Qt::UTC);
                c.time = dt.isValid() ? dt.toTime_t() : 0;
                c.path = id.startsWith(QLatin1String("file://")) ? id.mid(7) : id;
                c.id = id;
                c.valid = QFile::exists(c.path);
            }
            fn(stmt);
        }
        cl(db);
        return c;
    }

    // The newest of the three, limited to nt_recent_source when it names one.
    static Candidate best(QString *source = NULL) {
        const char *srcCfg = nt_config_get("nt_recent_source");
        const QString src = srcCfg ? QString::fromUtf8(srcCfg) : QStringLiteral("auto");
        if (source) *source = src;
        Candidate best; best.valid = false; best.time = 0;
        Candidate cands[3] = { fromKoreader(), fromPlato(), fromNickel() };
        for (int i = 0; i < 3; i++) {
            const Candidate &c = cands[i];
            NT_DBG("recent candidate %s: valid=%d time=%lld '%s' %d%%", qPrintable(c.app), c.valid ? 1 : 0, (long long)c.time, qPrintable(c.title), c.percent);
            if (!c.valid) continue;
            if (src != QLatin1String("auto") && src != c.app) continue;
            if (!best.valid || c.time > best.time) best = c;
        }
        return best;
    }

    void refresh() {
        QString src;
        const Candidate best = NtRecentTile::best(&src);
        m_koPath.clear(); m_platoLaunch = false; m_target = NULL; m_app = best.valid ? best.app : QString();
        m_nickelId = (best.valid && best.app == QLatin1String("nickel")) ? best.id : QString();
        if (!best.valid) {
            NT_DBG("recent: nothing to show (source=%s)", qPrintable(src));
            refreshNickel();
            return;
        }
        if (best.app == QLatin1String("koreader")) m_koPath = best.path;
        else if (best.app == QLatin1String("plato")) m_platoLaunch = true;
        else {
            refreshNickel();   // sets m_target when Nickel's widget has the book
            // Nickel's current-read widget is often empty on this firmware even
            // when the database has a last-read book; open the file in KOReader
            // then, which handles anything Nickel can.
            if (!m_target) { m_koPath = best.path; NT_DBG("recent: Nickel widget empty, will open in KOReader"); }
        }
        setInfo(best.title, best.author, best.percent);
        const QString lower = best.path.toLower();
        QPixmap cover;
        if (lower.endsWith(QLatin1String(".epub")) || lower.endsWith(QLatin1String(".kepub")))
            cover = nt_epub_cover(best.path, QString::fromLatin1(NT_CONFIG_DIR "/covers"), 400);
        setIcon(cover.isNull() ? nt_icon_for(QStringLiteral("Continue reading"), QString()) : cover);
        setVisible(true);
        NT_LOG("recent: %s '%s' by '%s' %d%% -> %s", qPrintable(best.app), qPrintable(best.title), qPrintable(best.author), best.percent, qPrintable(best.path));
    }

    // Finds the first SingleBookHomeWidget with a cover under row1col1.
    void refreshNickel() {
        m_target = NULL;
        QWidget *four = m_view->findChild<QWidget*>(QStringLiteral("fourBookWidget"));
        QList<QWidget*> candidates;
        if (four) {
            const QObjectList children = four->children();
            for (QObject *c : children) {
                QWidget *w = qobject_cast<QWidget*>(c);
                if (w && !strcmp(w->metaObject()->className(), "SingleBookHomeWidget"))
                    candidates << w;
            }
        }
        for (QWidget *w : candidates) {
            QPixmap cover;
            QString title, meta;
            const QList<QLabel*> labels = w->findChildren<QLabel*>();
            for (QLabel *l : labels) {
                if (l->pixmap() && !l->pixmap()->isNull() && cover.isNull()) cover = *l->pixmap();
                const QString t = l->text().trimmed();
                if (t.isEmpty()) continue;
                NT_DBG("recent label '%s' (%s) = '%s'", qPrintable(l->objectName()), l->metaObject()->className(), qPrintable(t.left(60)));
                if (t.contains(QLatin1Char('%')) || t.endsWith(QLatin1String("Read"), Qt::CaseInsensitive)) { if (meta.isEmpty()) meta = t; continue; }
                if (t.size() > title.size()) title = t;   // longest non-progress text = the title
            }
            // Book title is also exposed as a property on some firmware builds.
            const QString prop = w->property("title").toString().trimmed();
            if (!prop.isEmpty()) title = prop;
            NT_DBG("recent candidate %p cover=%dx%d title='%s' meta='%s' size=%dx%d", (void *)w,
                cover.width(), cover.height(), qPrintable(title), qPrintable(meta), w->width(), w->height());
            if (cover.isNull() && title.isEmpty() && meta.isEmpty()) continue;
            m_target = w;
            if (cover.isNull()) {
                // No plain pixmap label: the cover is painted by a custom widget.
                // Render the hidden widget itself into an image and use that.
                QSize sz = w->size();
                if (sz.width() < 50 || sz.height() < 50) sz = w->sizeHint();
                if (sz.width() >= 50 && sz.height() >= 50) {
                    QPixmap pm(sz);
                    pm.fill(Qt::white);
                    w->render(&pm, QPoint(), QRegion(), QWidget::DrawChildren);
                    cover = pm;
                    NT_DBG("recent: rendered hidden widget to %dx%d", sz.width(), sz.height());
                }
            }
            if (!cover.isNull()) setIcon(cover);
            setTitle(!title.isEmpty() ? title : (!meta.isEmpty() ? meta : QStringLiteral("Continue reading")));
            break;
        }
        setVisible(m_target != NULL);
        NT_DBG("recent tile: target=%p visible=%d", (void *)m_target, m_target ? 1 : 0);
    }

    void activate() override {
        if (!nt_config_bool("nt_actions", true)) { NT_LOG("nt_actions=0; not opening the recent book"); return; }
        // Nickel-read book: open it in Nickel's reader through the library's own path first.
        if (m_app == QLatin1String("nickel") && nt_open_in_nickel(m_nickelId)) return;
        if (!m_koPath.isEmpty()) {
            NtEntry e;
            e.label = QStringLiteral("Continue reading");
            NtAction a;
            a.kind = QStringLiteral("cmd_spawn");
            QString quoted = m_koPath;
            quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
            a.arg = QStringLiteral("quiet:exec /mnt/onboard/.adds/koreader/koreader.sh '") + quoted + QStringLiteral("'");
            a.chain = 0;
            e.actions << a;
            nt_run_entry(e);
            return;
        }
        if (m_app == QLatin1String("nickel") && !m_target) {
            // Nickel fills its current-read widget a moment after the home page
            // shows; look again now before falling back to KOReader.
            refreshNickel();
            if (m_target) m_koPath.clear();
        }
        if (m_platoLaunch) {
            if (!nt_config_bool("nt_actions", true)) return;
            NtEntry e;
            e.label = QStringLiteral("Continue reading");
            NtAction a;
            a.kind = QStringLiteral("cmd_spawn");
            a.arg = QStringLiteral("quiet:exec /mnt/onboard/.adds/plato/plato.sh");
            a.chain = 0;
            e.actions << a;
            nt_run_entry(e);
            return;
        }
        if (!m_target) { nt_toast(QStringLiteral("No current read")); return; }
        if (SingleBookHomeWidget_tapped) {
            NT_LOG("continue reading: SingleBookHomeWidget::tapped(%p)", (void *)m_target);
            SingleBookHomeWidget_tapped(m_target);
            return;
        }
        bool ok = QMetaObject::invokeMethod(m_target, "tapped", Qt::DirectConnection);
        NT_LOG("continue reading: invoke tapped by name on %p -> %d", (void *)m_target, ok ? 1 : 0);
        if (!ok) nt_toast(QStringLiteral("Could not open the current read"));
    }

private:
    HomePageView *m_view;
    QWidget *m_target;
    QString m_koPath;
    QString m_app;
    QString m_nickelId;
    bool m_platoLaunch;
};

class NtGrid : public QWidget {
public:
    NtGrid(HomePageView *view, QWidget *parent);
    void build();
    void beginArrange(NtTile *tile);
    void tapped(NtTile *tile);   // returns true if consumed by arrange mode
    bool arranging() const { return m_marked != NULL; }

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        if (m_recent) m_recent->refresh();
        startTimer(2000);   // Nickel updates its database and widgets shortly after
        // With nt_log:1, a picture of the home screen once it has settled
        // (for bug reports and the README).
        if (nt_log_verbose)
            QTimer::singleShot(4000, this, [this]() {
                // One file per screen visit (a later one may show a dialog on top).
                const QString name = QStringLiteral(NT_CONFIG_DIR "/home-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".png");
                window()->grab().save(name, "PNG");
            });
    }
    void timerEvent(QTimerEvent *event) override {
        killTimer(event->timerId());
        if (m_recent) m_recent->refresh();
    }

private:
    void relayout();
    void saveOrder();

    HomePageView *m_view;
    QGridLayout *m_layout;
    QList<NtTile*> m_tiles;
    NtRecentTile *m_recent;
    NtTile *m_marked;
    int m_columns;
};

// --- NtTile

NtTile::NtTile(NtGrid *grid, const QString &label, const QPixmap &icon, int px)
    : QWidget(grid), m_grid(grid), m_label(label), m_title(label), m_percent(-1), m_hasInfo(false), m_icon(icon),
      m_pressed(false), m_marked(false), m_longPressed(false), m_timer(0) {
    setObjectName(QStringLiteral("ntTile"));
    setFixedSize(px, px + 6 + kNtCaptionLines * QFontMetrics(nt_caption_font(this)).lineSpacing() + 4);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
}

void NtTile::pressed(bool down) {
    if (m_pressed == down) return;
    m_pressed = down;
    repaint();
}

void NtTile::released(bool inside) {
    if (m_timer) { killTimer(m_timer); m_timer = 0; }
    pressed(false);
    if (m_longPressed) { m_longPressed = false; return; }
    if (inside) m_grid->tapped(this);
}

void NtTile::timerEvent(QTimerEvent *event) {
    if (event->timerId() != m_timer) return;
    killTimer(m_timer);
    m_timer = 0;
    m_longPressed = true;
    pressed(false);
    m_grid->beginArrange(this);
}

bool NtTile::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::TouchBegin:
        event->accept();
        m_longPressed = false;
        if (m_timer) killTimer(m_timer);
        m_timer = startTimer(700);
        pressed(true);
        return true;
    case QEvent::TouchUpdate:
        event->accept();
        return true;
    case QEvent::TouchEnd: {
        QTouchEvent *touch = static_cast<QTouchEvent*>(event);
        const QList<QTouchEvent::TouchPoint> points = touch->touchPoints();
        bool inside = points.isEmpty();
        for (const QTouchEvent::TouchPoint &p : points)
            if (rect().contains(p.pos().toPoint())) inside = true;
        event->accept();
        released(inside);
        return true;
    }
    case QEvent::TouchCancel:
        event->accept();
        released(false);
        return true;
    case QEvent::MouseButtonPress:
        event->accept();
        pressed(true);
        return true;
    case QEvent::MouseButtonRelease: {
        QMouseEvent *mouse = static_cast<QMouseEvent*>(event);
        event->accept();
        released(rect().contains(mouse->pos()));
        return true;
    }
    default:
        break;
    }
    return QWidget::event(event);
}

void NtTile::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor fg = m_pressed ? Qt::white : Qt::black;
    const QColor bg = m_pressed ? Qt::black : Qt::white;
    p.fillRect(rect(), bg);
    if (m_marked) {
        p.setPen(QPen(fg, 6));
        p.drawRoundedRect(rect().adjusted(3, 3, -3, -3), 14, 14);
    }
    const int pad = 16;
    const int iconSide = width() - 2 * pad;
    QRect iconRect(pad, pad, iconSide, iconSide);
    if (!m_icon.isNull()) {
        QPixmap scaled = m_icon.scaled(iconSide, iconSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (m_pressed) {
            QImage img = scaled.toImage();
            img.invertPixels();
            scaled = QPixmap::fromImage(img);
        }
        p.drawPixmap(iconRect.x() + (iconSide - scaled.width()) / 2, iconRect.y() + (iconSide - scaled.height()) / 2, scaled);
    } else {
        p.setPen(QPen(fg, 6));
        p.drawRoundedRect(iconRect.adjusted(3, 3, -3, -3), 24, 24);
        QFont f = font();
        f.setPixelSize(iconSide / 2);
        f.setBold(true);
        p.setFont(f);
        QString initials = m_label.left(1).toUpper();
        int sp = m_label.indexOf(QLatin1Char(' '));
        if (sp > 0 && sp + 1 < m_label.size()) initials += m_label.at(sp + 1).toUpper();
        p.drawText(iconRect, Qt::AlignCenter, initials);
    }
    const QFont f = nt_caption_font(this);
    const QFontMetrics fm(f);
    p.setFont(f);
    p.setPen(fg);
    const int textWidth = width() - 8;
    QStringList lines;
    if (m_hasInfo) {
        bool showTitle, showAuthor, showProgress;
        nt_recent_info(&showTitle, &showAuthor, &showProgress);
        QStringList tail;
        if (showAuthor && !m_author.isEmpty()) tail << fm.elidedText(m_author, Qt::ElideRight, textWidth);
        if (showProgress && m_percent >= 0) tail << QString::number(m_percent) + QStringLiteral("%");
        // The title gets whatever lines the author and progress leave.
        if (showTitle) lines << nt_wrap(m_title, fm, textWidth, kNtCaptionLines - tail.size());
        lines << tail;
    } else {
        lines = nt_wrap(m_title, fm, textWidth, kNtCaptionLines);
    }
    int y = pad + iconSide + 6;
    for (const QString &line : lines) {
        p.drawText(QRect(4, y, textWidth, fm.lineSpacing()), Qt::AlignHCenter | Qt::AlignTop, line);
        y += fm.lineSpacing();
    }
}

// --- NtGrid

// NickelMenu main-menu entries. Keeps the last good read: during a rescan the
// home page is rebuilt while /mnt/onboard is unmounted and the dir reads empty.
static QList<NtEntry> nt_nickelmenu_entries() {
    static QList<NtEntry> cachedEntries;
    QList<NtEntry> entries = nt_read_nickelmenu_entries(QStringLiteral("/mnt/onboard/.adds/nm"));
    if (entries.isEmpty() && !cachedEntries.isEmpty()) {
        NT_LOG("NickelMenu dir unreadable now; using the %d cached entries", cachedEntries.size());
        entries = cachedEntries;
    } else if (!entries.isEmpty()) {
        cachedEntries = entries;
    }
    return entries;
}

// Every tile the settings screen can show or hide, hidden ones included.
static QStringList nt_all_tile_labels() {
    QStringList labels;
    for (const NtEntry &e : nt_nickelmenu_entries())
        if (!labels.contains(e.label)) labels << e.label;
    for (const nt_tile_t *t = nt_config_tiles(); t; t = t->next) {
        const QString l = QString::fromUtf8(t->label);
        if (!labels.contains(l)) labels << l;
    }
    return labels;
}

NtGrid::NtGrid(HomePageView *view, QWidget *parent)
    : QWidget(parent), m_view(view), m_layout(NULL), m_recent(NULL), m_marked(NULL),
      m_columns(nt_config_int("nt_columns", 5)) {
    setObjectName(QStringLiteral("ntGrid"));
    if (m_columns < 1) m_columns = 1;
    m_layout = new QGridLayout(this);
    m_layout->setObjectName(QStringLiteral("ntGridLayout"));
    m_layout->setContentsMargins(24, 24, 24, 24);
    m_layout->setHorizontalSpacing(16);
    m_layout->setVerticalSpacing(20);
}

void NtGrid::build() {
    const int px = nt_config_int("nt_tile_px", 180);
    QStringList hidden;
    for (const nt_entry_t *e = nt_config_entries(); e; e = e->next)
        if (!strcmp(e->key, "hide")) hidden << QString::fromUtf8(e->val).trimmed();

    if (nt_config_bool("nt_recent", true)) {
        m_recent = new NtRecentTile(this, m_view, px);
        m_tiles << m_recent;
    }
    QStringList seen;
    const QList<NtEntry> entries = nt_nickelmenu_entries();
    for (const NtEntry &e : entries) {
        if (seen.contains(e.label) || hidden.contains(e.label)) continue;
        seen << e.label;
        m_tiles << new NtEntryTile(this, e, px);
    }
    for (const nt_tile_t *t = nt_config_tiles(); t; t = t->next) {
        QString label = QString::fromUtf8(t->label);
        if (seen.contains(label) || hidden.contains(label)) continue;
        seen << label;
        m_tiles << new NtCommandTile(this, t, px);
    }
    if (!hidden.contains(QStringLiteral(NT_SETTINGS_LABEL)))
        m_tiles << new NtSettingsTile(this, m_view, px);

    // apply saved order: known labels first in that order, the rest after
    const char *orderStr = nt_config_get("order");
    if (orderStr && *orderStr) {
        const QStringList order = QString::fromUtf8(orderStr).split(QLatin1Char('|'));
        QList<NtTile*> sorted;
        for (const QString &name : order) {
            const QString n = name.trimmed();
            for (int i = 0; i < m_tiles.size(); i++)
                if (m_tiles.at(i)->label() == n) { sorted << m_tiles.takeAt(i); break; }
        }
        sorted << m_tiles;
        m_tiles = sorted;
    }
    relayout();
    NT_LOG("grid: %d tiles (%d NickelMenu entries, %d hidden), %d columns, %d px",
        m_tiles.size(), entries.size(), hidden.size(), m_columns, px);
    startTimer(1500);   // first refresh of the recent tile after Nickel fills row1
}

void NtGrid::relayout() {
    while (QLayoutItem *item = m_layout->takeAt(0)) delete item;
    int visibleIndex = 0;
    for (NtTile *t : m_tiles) {
        m_layout->addWidget(t, visibleIndex / m_columns, visibleIndex % m_columns, Qt::AlignTop | Qt::AlignLeft);
        visibleIndex++;
    }
    m_layout->setColumnStretch(m_columns, 1);
    m_layout->setRowStretch(m_layout->rowCount(), 1);
}

void NtGrid::beginArrange(NtTile *tile) {
    if (m_marked) m_marked->setMarked(false);
    m_marked = tile;
    tile->setMarked(true);
    NT_DBG("arrange: picked '%s'", qPrintable(tile->label()));
    nt_toast(QStringLiteral("Tap another tile to swap"), 1500);
}

void NtGrid::tapped(NtTile *tile) {
    if (m_marked) {
        NtTile *from = m_marked;
        m_marked->setMarked(false);
        m_marked = NULL;
        if (from != tile) {
            int a = m_tiles.indexOf(from), b = m_tiles.indexOf(tile);
            if (a >= 0 && b >= 0) {
                m_tiles.swap(a, b);
                relayout();
                saveOrder();
            }
        }
        return;
    }
    NT_LOG("tile tapped: '%s'", qPrintable(tile->label()));
    tile->activate();
}

void NtGrid::saveOrder() {
    QStringList names;
    for (NtTile *t : m_tiles) names << t->label();
    const QByteArray order = names.join(QStringLiteral("|")).toUtf8();
    bool ok = nt_config_write_order(order.constData());
    NT_LOG("order saved=%d: %s", ok ? 1 : 0, order.constData());
}

// --- hook

static void nt_install_grid(HomePageView *view) {
    const QList<QWidget*> containers = view->findChildren<QWidget*>(QStringLiteral("mainContainer"));
    QWidget *container = NULL;
    for (QWidget *c : containers)
        if (nt_find_direct_child(c, "row1")) { container = c; break; }
    if (!container) {
        NT_LOG("warning: mainContainer with row1 not found; not inserting the grid");
        return;
    }
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(container->layout());
    if (!mainLayout) {
        NT_LOG("warning: mainContainer has no QVBoxLayout; not inserting");
        return;
    }
    // Also the rebuild after the settings screen saves: drop the old grid and
    // the stretch added with it first.
    static QHash<QLayout*, QSpacerItem*> stretches;
    const QList<QWidget*> oldGrids = container->findChildren<QWidget*>(QStringLiteral("ntGrid"), Qt::FindDirectChildrenOnly);
    for (QWidget *old : oldGrids) {
        mainLayout->removeWidget(old);
        old->hide();
        old->deleteLater();
    }
    if (QSpacerItem *sp = stretches.take(mainLayout)) {
        mainLayout->removeItem(sp);
        delete sp;
    }
    const bool hideRows = nt_config_bool("nt_hide_rows", true);
    const char *rows[] = {"row1", "row2", "row3"};
    for (size_t i = 0; i < 3; i++) {
        QWidget *r = nt_find_direct_child(container, rows[i]);
        if (!r) continue;
        // Only rows this mod hid are shown again (another mod may hide some).
        if (hideRows && !r->isHidden()) { r->setProperty("ntHidden", true); r->setVisible(false); }
        else if (!hideRows && r->property("ntHidden").toBool()) { r->setProperty("ntHidden", QVariant()); r->setVisible(true); }
    }
    NtGrid *grid = new NtGrid(view, container);
    grid->build();
    mainLayout->insertWidget(hideRows ? 0 : 1, grid);
    if (hideRows) {
        QSpacerItem *sp = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        mainLayout->addItem(sp);
        stretches.insert(mainLayout, sp);
    }
    if (!oldGrids.isEmpty()) grid->show();
    NT_LOG("inserted grid at mainLayout index %d (rows hidden=%d)", hideRows ? 0 : 1, hideRows ? 1 : 0);
}

extern "C" __attribute__((visibility("default"))) void _nt_setupui_hook(Ui_HomePageView *_this, HomePageView *view) {
    NT_LOG("Ui_HomePageView::setupUi(%p, %p)", (void *)_this, (void *)view);
    Ui_HomePageView_setupUi(_this, view);

    if (!view || !nt_config_bool("nt_enabled", true)) {
        NT_DBG("nt_enabled=0 or no view; leaving the home screen untouched");
        return;
    }
    if (nt_log_verbose)
        nt_log_tree(view, 0);
    if (nt_config_bool("nt_draw", true))
        nt_install_grid(view);
    else
        NT_LOG("nt_draw=0; logging only, nothing inserted");
}

// --- sleep screen
//
// Nickel's full-screen sleep cover (PowerSettings FullscreenCoverStretch, "Show
// book covers full screen") is a random image from .kobo/screensaver when that
// folder has any, else the cover of the last book read in Nickel. This keeps
// one image there, nt-recent.<ext>, holding the cover of the "Continue reading"
// book, so the sleep screen follows KOReader and Plato too. Other images the
// owner puts in the folder are left alone (Nickel then picks among all).

#define NT_SLEEP_DIR "/mnt/onboard/.kobo/screensaver"

static void nt_sleep_cover_clear() {
    QDir dir(QStringLiteral(NT_SLEEP_DIR));
    const QStringList ours = dir.entryList(QStringList() << QStringLiteral("nt-recent.*"), QDir::Files);
    for (const QString &name : ours) dir.remove(name);
    QFile::remove(QStringLiteral(NT_CONFIG_DIR "/sleep-cover.src"));
}

static void nt_sleep_cover_update() {
    const NtRecentTile::Candidate best = NtRecentTile::best();
    const QString lower = best.path.toLower();
    if (!best.valid || !(lower.endsWith(QLatin1String(".epub")) || lower.endsWith(QLatin1String(".kepub")))) {
        NT_LOG("sleep cover: no recent EPUB; leaving Nickel's own cover");
        nt_sleep_cover_clear();
        return;
    }
    // Re-extract only when the book (or its file) changed since the last sleep.
    const QFileInfo fi(best.path);
    const QString key = best.path + QLatin1Char('|') + QString::number(fi.size()) + QLatin1Char('|') + QString::number(fi.lastModified().toTime_t());
    QDir dir(QStringLiteral(NT_SLEEP_DIR));
    QFile stamp(QStringLiteral(NT_CONFIG_DIR "/sleep-cover.src"));
    if (stamp.open(QIODevice::ReadOnly)) {
        const bool same = QString::fromUtf8(stamp.readAll()) == key;
        stamp.close();
        if (same && !dir.entryList(QStringList() << QStringLiteral("nt-recent.*"), QDir::Files).isEmpty()) {
            NT_DBG("sleep cover: unchanged (%s)", qPrintable(best.title));
            return;
        }
    }
    QString member;
    QByteArray data = nt_epub_cover_bytes(best.path, &member);
    QString ext = member.section(QLatin1Char('.'), -1).toLower();
    if (ext != QLatin1String("jpg") && ext != QLatin1String("jpeg") && ext != QLatin1String("png")) {
        // Anything else is re-encoded, since Nickel lists the folder by extension.
        const QImage img = QImage::fromData(data);
        data.clear();
        if (!img.isNull()) {
            QBuffer buf(&data);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
        }
        ext = QStringLiteral("png");
    }
    nt_sleep_cover_clear();
    if (data.isEmpty()) {
        NT_LOG("sleep cover: no cover in %s; leaving Nickel's own cover", qPrintable(best.path));
        return;
    }
    QDir().mkpath(QStringLiteral(NT_SLEEP_DIR));
    // Written under a name Nickel does not list, then renamed into place.
    const QString tmp = dir.filePath(QStringLiteral(".nt-recent.tmp"));
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
        NT_LOG("sleep cover: could not write %s", qPrintable(tmp));
        out.close();
        QFile::remove(tmp);
        return;
    }
    out.flush();
    fsync(out.handle());
    out.close();
    if (!QFile::rename(tmp, dir.filePath(QStringLiteral("nt-recent.") + ext))) {
        NT_LOG("sleep cover: rename failed");
        QFile::remove(tmp);
        return;
    }
    if (stamp.open(QIODevice::WriteOnly | QIODevice::Truncate)) { stamp.write(key.toUtf8()); stamp.close(); }
    NT_LOG("sleep cover: %s '%s' (%d bytes, .%s)", qPrintable(best.app), qPrintable(best.title), data.size(), qPrintable(ext));
}

extern "C" __attribute__((visibility("default"))) void _nt_updatecover_hook(PowerViewController *_this) {
    if (nt_config_bool("nt_enabled", true) && nt_config_bool("nt_sleep_cover", true))
        nt_sleep_cover_update();
    else
        nt_sleep_cover_clear();
    PowerViewController_updateCover(_this);
}
