#ifndef NT_TEST_STUBS_H
#define NT_TEST_STUBS_H

// Records of the Nickel-facing calls the stubs received (stubs.cc), so a test
// can check what recent.cc asked for without a reader.

#include <QStringList>

extern QStringList nt_stub_toasts;       // nt_toast texts
extern QStringList nt_stub_commands;     // nt_run_command command lines
extern QStringList nt_stub_content_ids;  // nt_open_in_nickel content ids

#endif
