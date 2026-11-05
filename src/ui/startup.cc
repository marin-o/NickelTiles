#include "ui/startup.h"

#include <QPointer>
#include <QString>
#include <QTimer>

#include "core/crash_guard.h"
#include "core/settings_model.h"
#include "kobo/nickel.h"
#include "kobo/nm_entry.h"
#include "ui/grid.h"

namespace {

// Nickel opens its own dialogs (such as "Computer detected") up to about 15 s
// after start, and a later one would open under ours.
const int kPromptDelayMs = 20000;
const int kPromptTries = 15;

void offerTurnOn(QWidget *homeView) {
    QPointer<QWidget> view(homeView);
    const bool asked = nt_confirm(
        QStringLiteral("NickelTiles is off"),
        QStringLiteral("Kobo crashed three times in a row soon after starting, so NickelTiles turned itself off "
                       "in case it was the cause. Its log is in .adds/nickel-tiles. Turn the tiles back on?"),
        QStringLiteral("Turn on"), QStringLiteral("Keep off"), [view]() {
            const NtSettings before = nt_settings();
            NtSettings after = before;
            after.enabled = true;
            nt_settings_save(before, after);
            if (view) nt_install_grid(view);
        });
    if (!asked) nt_toast(QStringLiteral("NickelTiles turned itself off after repeated crashes"), 4000);
}

void offerMenuEntry() {
    nt_confirm(QStringLiteral("NickelTiles off/on in NickelMenu"),
               QStringLiteral("Add an entry to NickelMenu that turns the tiles off or on from any screen? Handy if the "
                              "home screen ever gets in your way. You can change this in Tiles settings."),
               QStringLiteral("Add"), QStringLiteral("No thanks"), []() {
                   nt_toast(nt_nm_entry_set(true) ? QStringLiteral("Added to NickelMenu")
                                                  : QStringLiteral("Could not add the NickelMenu entry"),
                            2500);
               });
}

// Waits while one of Nickel's dialogs is open, so the two don't cover each
// other; gives up after a few minutes.
void promptWhenFree(const QPointer<QWidget> &view, int triesLeft) {
    QTimer::singleShot(kPromptDelayMs, [view, triesLeft]() {
        if (!view) return;
        if (nt_dialog_showing()) {
            if (triesLeft > 0) promptWhenFree(view, triesLeft - 1);
            return;
        }
        if (nt_crash_guard_tripped())
            offerTurnOn(view);
        else if (nt_settings().enabled && nt_nm_entry_offer_due())
            offerMenuEntry();
    });
}

}   // namespace

void nt_startup_home_ready(QWidget *homeView) {
    static bool done = false;
    if (done) return;
    done = true;
    QTimer::singleShot(kNtSettleMs, []() { nt_crash_guard_settled(); });
    promptWhenFree(QPointer<QWidget>(homeView), kPromptTries);
}
