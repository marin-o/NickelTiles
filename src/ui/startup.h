#ifndef NT_STARTUP_H
#define NT_STARTUP_H

// What happens once Home first shows after Nickel starts: the crash guard
// learns when Nickel has settled, and the user is told, if the guard turned
// NickelTiles off, with a button to turn it back on; otherwise, once ever, they
// are offered the NickelMenu entry that turns tiles off and on.

#include <QWidget>

// Call from the home page hook; only the first call each start does anything.
void nt_startup_home_ready(QWidget *homeView);

#endif
