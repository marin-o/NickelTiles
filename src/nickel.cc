#include "nickel.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include <cstdlib>
#include <cstring>

#include <NickelHook.h>

#include "util.h"

// libnickel symbols, resolved by NickelHook. Each is optional: a missing
// symbol makes the wrappers below report failure instead of calling it.
typedef void MainWindowController;
typedef void N3PowerWorkflowManager;
typedef void WirelessWorkflowManager;
typedef void Device;
typedef void Volume;
typedef void ReadBookActionProxy;

static MainWindowController *(*MainWindowController_sharedInstance)() = nullptr;
static void (*MainWindowController_toast)(MainWindowController *, QString const &, QString const &, int) = nullptr;
static N3PowerWorkflowManager *(*N3PowerWorkflowManager_sharedInstance)() = nullptr;
static void (*N3PowerWorkflowManager_powerOff)(N3PowerWorkflowManager *, bool) = nullptr;
static void (*N3PowerWorkflowManager_reboot)(N3PowerWorkflowManager *) = nullptr;
static void (*N3PowerWorkflowManager_requestSleep)(N3PowerWorkflowManager *) = nullptr;
static WirelessWorkflowManager *(*WirelessWorkflowManager_sharedInstance)() = nullptr;
static void (*WirelessWorkflowManager_connectWireless)(WirelessWorkflowManager *, bool, bool) = nullptr;
static void (*WirelessWorkflowManager_connectWirelessSilently)(WirelessWorkflowManager *) = nullptr;
static bool (*WirelessWorkflowManager_isAirplaneMode)(WirelessWorkflowManager *) = nullptr;
static void (*WirelessWorkflowManager_setAirplaneMode)(WirelessWorkflowManager *, bool) = nullptr;
static void (*MainNavController_home)(void *) = nullptr;
// Used by nt_open_in_nickel. VolumeManager::getByIdBytes and the ReadBookActionProxy
// calls are what Nickel's own library tap runs.
static Device *(*Device_getCurrentDevice)() = nullptr;
static void (*Device_getDbName)(QString *out, Device const *) = nullptr;
static void (*VolumeManager_getByIdBytes)(Volume *out, QByteArray const &, QString const &) = nullptr;
static bool (*Volume_isValid)(Volume const *) = nullptr;
static void (*Volume_dtor)(Volume *) = nullptr;
static void (*ReadBookActionProxy_ctor)(ReadBookActionProxy *, QObject *, Volume const *) = nullptr;
static void (*ReadBookActionProxy_onSelected)(ReadBookActionProxy *) = nullptr;

struct nh_dlsym nt_nickel_dlsym[] = {
    {.name = "_ZN20MainWindowController14sharedInstanceEv",
     .out = nh_symoutptr(MainWindowController_sharedInstance),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN20MainWindowController5toastERK7QStringS2_i",
     .out = nh_symoutptr(MainWindowController_toast),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN22N3PowerWorkflowManager14sharedInstanceEv",
     .out = nh_symoutptr(N3PowerWorkflowManager_sharedInstance),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN22N3PowerWorkflowManager8powerOffEb",
     .out = nh_symoutptr(N3PowerWorkflowManager_powerOff),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN22N3PowerWorkflowManager6rebootEv",
     .out = nh_symoutptr(N3PowerWorkflowManager_reboot),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN22N3PowerWorkflowManager12requestSleepEv",
     .out = nh_symoutptr(N3PowerWorkflowManager_requestSleep),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN23WirelessWorkflowManager14sharedInstanceEv",
     .out = nh_symoutptr(WirelessWorkflowManager_sharedInstance),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN23WirelessWorkflowManager15connectWirelessEbb",
     .out = nh_symoutptr(WirelessWorkflowManager_connectWireless),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN23WirelessWorkflowManager23connectWirelessSilentlyEv",
     .out = nh_symoutptr(WirelessWorkflowManager_connectWirelessSilently),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN23WirelessWorkflowManager14isAirplaneModeEv",
     .out = nh_symoutptr(WirelessWorkflowManager_isAirplaneMode),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN23WirelessWorkflowManager15setAirplaneModeEb",
     .out = nh_symoutptr(WirelessWorkflowManager_setAirplaneMode),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN17MainNavController4homeEv",
     .out = nh_symoutptr(MainNavController_home),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN6Device16getCurrentDeviceEv",
     .out = nh_symoutptr(Device_getCurrentDevice),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZNK6Device9getDbNameEv", .out = nh_symoutptr(Device_getDbName), .desc = nullptr, .optional = true},
    {.name = "_ZN13VolumeManager12getByIdBytesERK10QByteArrayRK7QString",
     .out = nh_symoutptr(VolumeManager_getByIdBytes),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZNK6Volume7isValidEv", .out = nh_symoutptr(Volume_isValid), .desc = nullptr, .optional = true},
    {.name = "_ZN6VolumeD1Ev", .out = nh_symoutptr(Volume_dtor), .desc = nullptr, .optional = true},
    {.name = "_ZN19ReadBookActionProxyC1EP7QObjectRK6Volume",
     .out = nh_symoutptr(ReadBookActionProxy_ctor),
     .desc = nullptr,
     .optional = true},
    {.name = "_ZN19ReadBookActionProxy10onSelectedEv",
     .out = nh_symoutptr(ReadBookActionProxy_onSelected),
     .desc = nullptr,
     .optional = true},
    {nullptr},
};

void nt_nickel_log_symbols() {
    NT_LOG("nickel symbols: toast=%p reboot=%p connectWireless=%p getByIdBytes=%p proxyCtor=%p",
           reinterpret_cast<void *>(MainWindowController_toast),
           reinterpret_cast<void *>(N3PowerWorkflowManager_reboot),
           reinterpret_cast<void *>(WirelessWorkflowManager_connectWireless),
           reinterpret_cast<void *>(VolumeManager_getByIdBytes), reinterpret_cast<void *>(ReadBookActionProxy_ctor));
}

static MainWindowController *mainWindow() {
    return MainWindowController_sharedInstance ? MainWindowController_sharedInstance() : nullptr;
}

static N3PowerWorkflowManager *powerManager() {
    return N3PowerWorkflowManager_sharedInstance ? N3PowerWorkflowManager_sharedInstance() : nullptr;
}

static WirelessWorkflowManager *wirelessManager() {
    return WirelessWorkflowManager_sharedInstance ? WirelessWorkflowManager_sharedInstance() : nullptr;
}

void nt_toast(const QString &text, int ms) {
    if (MainWindowController_toast) {
        MainWindowController *mwc = mainWindow();
        if (mwc) {
            MainWindowController_toast(mwc, text, QStringLiteral(""), ms);
            return;
        }
    }
    NT_LOG("toast (no Nickel toast available): %s", qPrintable(text));
}

bool nt_power_off() {
    N3PowerWorkflowManager *pwm = powerManager();
    if (!pwm || !N3PowerWorkflowManager_powerOff) return false;
    N3PowerWorkflowManager_powerOff(pwm, false);
    return true;
}

bool nt_reboot() {
    N3PowerWorkflowManager *pwm = powerManager();
    if (!pwm || !N3PowerWorkflowManager_reboot) return false;
    N3PowerWorkflowManager_reboot(pwm);
    return true;
}

bool nt_sleep() {
    N3PowerWorkflowManager *pwm = powerManager();
    if (!pwm || !N3PowerWorkflowManager_requestSleep) return false;
    N3PowerWorkflowManager_requestSleep(pwm);
    return true;
}

bool nt_wifi_connect(bool silent) {
    WirelessWorkflowManager *wfm = wirelessManager();
    if (!wfm) return false;
    if (silent) {
        if (!WirelessWorkflowManager_connectWirelessSilently) return false;
        WirelessWorkflowManager_connectWirelessSilently(wfm);
    } else {
        if (!WirelessWorkflowManager_connectWireless) return false;
        WirelessWorkflowManager_connectWireless(wfm, true, false);
    }
    return true;
}

bool nt_wifi_set(NtWifiChange change) {
    WirelessWorkflowManager *wfm = wirelessManager();
    if (!wfm || !WirelessWorkflowManager_isAirplaneMode || !WirelessWorkflowManager_setAirplaneMode) return false;
    const bool airplane = WirelessWorkflowManager_isAirplaneMode(wfm);
    switch (change) {
    case NtWifiChange::Enable:
        if (airplane) WirelessWorkflowManager_setAirplaneMode(wfm, false);
        break;
    case NtWifiChange::Disable:
        if (!airplane) WirelessWorkflowManager_setAirplaneMode(wfm, true);
        break;
    case NtWifiChange::Toggle:
        WirelessWorkflowManager_setAirplaneMode(wfm, !airplane);
        break;
    }
    return true;
}

bool nt_go_home() {
    if (!MainNavController_home) return false;
    MainNavController_home(nullptr);
    return true;
}

static bool openSymbolsAvailable() {
    if (Device_getCurrentDevice && Device_getDbName && VolumeManager_getByIdBytes && Volume_isValid && Volume_dtor &&
        ReadBookActionProxy_ctor && ReadBookActionProxy_onSelected)
        return true;
    NT_LOG("open in nickel: symbols missing (dev=%p db=%p byId=%p valid=%p dtor=%p proxy=%p sel=%p)",
           reinterpret_cast<void *>(Device_getCurrentDevice), reinterpret_cast<void *>(Device_getDbName),
           reinterpret_cast<void *>(VolumeManager_getByIdBytes), reinterpret_cast<void *>(Volume_isValid),
           reinterpret_cast<void *>(Volume_dtor), reinterpret_cast<void *>(ReadBookActionProxy_ctor),
           reinterpret_cast<void *>(ReadBookActionProxy_onSelected));
    return false;
}

// Selects the book in `volume` the way a library tap does. The proxy is parented
// to the application so the home page's destruction while the reader opens
// cannot delete it mid-flight.
static void selectBook(Volume *volume) {
    // ReadBookActionProxy is 52 bytes on 4.45.23697 (ActionProxyMixin::readBookProxy);
    // allocate generously.
    void *proxy = std::calloc(1, 256);
    ReadBookActionProxy_ctor(proxy, QCoreApplication::instance(), volume);
    ReadBookActionProxy_onSelected(proxy);
}

bool nt_open_in_nickel(const QString &contentId) {
    if (contentId.isEmpty() || !openSymbolsAvailable()) return false;
    Device *dev = Device_getCurrentDevice();
    if (!dev) {
        NT_LOG("open in nickel: no current device");
        return false;
    }
    QString dbName;
    Device_getDbName(&dbName, dev);
    // Volume is small (vtable + shared data pointer); 64 bytes leaves plenty of room.
    unsigned char volume[64] __attribute__((aligned(8)));
    std::memset(volume, 0, sizeof(volume));
    const QByteArray idBytes = contentId.toUtf8();
    VolumeManager_getByIdBytes(volume, idBytes, dbName);
    const bool valid = Volume_isValid(volume);
    NT_LOG("open in nickel: db='%s' id='%s' valid=%d", qPrintable(dbName), idBytes.constData(), valid ? 1 : 0);
    if (!valid) {
        Volume_dtor(volume);
        return false;
    }
    selectBook(volume);
    Volume_dtor(volume);
    NT_LOG("open in nickel: proxy selected");
    return true;
}
