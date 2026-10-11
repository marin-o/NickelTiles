#include <QWidget>

#include "addons.h"
#include "config.h"
#include "grid.h"
#include "nickel.h"
#include "settings_model.h"
#include "sleepcover.h"
#include "util.h"

typedef QWidget HomePageView;
typedef void Ui_HomePageView;
typedef void PowerViewController;
static void (*Ui_HomePageView_setupUi)(Ui_HomePageView *_this, HomePageView *view);
static void (*PowerViewController_updateCover)(PowerViewController *_this);

static int nt_init() {
    nt_addons_mark_start();
    nt_config_get("");   // parse the config now, so a problem shows in the startup log
    NT_LOG("startup: NickelTiles " NH_VERSION);
    nt_log_firmware();
    const NtSettings s = nt_settings();
    NT_LOG("startup: enabled=%d actions=%d hide_rows=%d recent=%d columns=%d tile_px=%d sleep_cover=%d verbose=%d hook=%p sleep_hook=%p",
        s.enabled ? 1 : 0, s.actions ? 1 : 0, s.hideRows ? 1 : 0, s.recent ? 1 : 0, s.columns, s.tilePx,
        s.sleepCover ? 1 : 0, nt_log_verbose ? 1 : 0,
        (void *)Ui_HomePageView_setupUi, (void *)PowerViewController_updateCover);
    nt_nickel_log_symbols();
    return 0;
}

static struct nh_info NickelTiles = (struct nh_info){
    .name            = "NickelTiles",
    .desc            = "App grid on the Kobo home screen.",
    .uninstall_flag  = NT_CONFIG_DIR "/uninstall",
    .uninstall_xflag = NT_CONFIG_DIR,
    // The home page is built within a second of plugin load, so 3 s (same as
    // NickelMenu/NickelHome) covers it. Longer is harmful: launching an app
    // from a tile kills Nickel, and if that happens while the failsafe is
    // still armed the mod stays disabled.
    .failsafe_delay  = 3,
};

static struct nh_hook NickelTilesHook[] = {
    {
        // NickelHook does not chain two mods on one symbol, so this hooks
        // setupUi rather than the HomePageView constructor NickelHome hooks.
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

NickelHook(
    .init  = &nt_init,
    .info  = &NickelTiles,
    .hook  = NickelTilesHook,
    .dlsym = nt_nickel_dlsym,
)

// Called from the home page's setupUi, once the widget tree exists.
extern "C" __attribute__((visibility("default"))) void _nt_setupui_hook(Ui_HomePageView *_this, HomePageView *view) {
    NT_LOG("Ui_HomePageView::setupUi(%p, %p)", (void *)_this, (void *)view);
    Ui_HomePageView_setupUi(_this, view);

    if (view && nt_settings().enabled)
        nt_install_grid(view);
    else
        NT_DBG("nt_enabled=0 or no view; leaving the home screen untouched");
}

extern "C" __attribute__((visibility("default"))) void _nt_updatecover_hook(PowerViewController *_this) {
    nt_sleep_cover_refresh(nt_settings());
    PowerViewController_updateCover(_this);
}
