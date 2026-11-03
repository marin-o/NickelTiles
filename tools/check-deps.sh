#!/bin/sh
# Checks the dependency rule in docs/architecture.md: each source folder may only
# include the folders below it, and widgets stay in ui/. Run by tools/lint.sh.
set -eu
cd "$(dirname "$0")/.."

status=0

# Fails on any quoted include in `folder` that names one of the forbidden folders.
forbid() {
    folder=$1
    shift
    for other in "$@"; do
        if grep -rn "#include \"$other/" "src/$folder"; then
            echo "$folder/ must not include $other/ (see docs/architecture.md)" >&2
            status=1
        fi
    done
}

forbid core kobo books ui
forbid kobo books ui
forbid books ui

# The exceptions the docs name: plugin.cc receives the home page widget from the
# hook, and kobo/nickel.cc opens Kobo's own dialog.
widgets='#include <(QWidget|QLabel|QBoxLayout|QGridLayout|QVBoxLayout|QHBoxLayout|QLayout|QPushButton|QDialog|QApplication)>'
if grep -rnE "$widgets" src --exclude-dir=ui --exclude=plugin.cc --exclude=nickel.cc; then
    echo "widget headers belong in ui/ (see docs/architecture.md)" >&2
    status=1
fi

exit $status
