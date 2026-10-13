#!/bin/sh
# Checks formatting (clang-format) and code (clang-tidy, rules in .clang-tidy)
# of src/ and the formatting of tests/. Fails on any finding.
#
#   QT_INCLUDE   folder holding QtCore/, QtGui/, QtWidgets/ (required)
#   LINT_FLAGS   extra compiler flags, e.g. a target and sysroot (optional)
#   CLANG_TIDY, CLANG_FORMAT   the tools to use (default: from PATH)
#
# GitHub runs this on every push and pull request (Lint workflow). Locally, on
# Debian or Ubuntu:
#   sudo apt install qtbase5-dev && pip install clang-tidy==22.1.8 clang-format==23.1.3
#   QT_INCLUDE=/usr/include/x86_64-linux-gnu/qt5 sh tools/lint.sh
# To fix formatting: clang-format -i $(find src tests -name '*.h' -o -name '*.c' -o -name '*.cc')
set -eu
cd "$(dirname "$0")/.."
: "${QT_INCLUDE:?set QT_INCLUDE to the folder holding QtCore/, QtGui/ and QtWidgets/}"
CLANG_TIDY=${CLANG_TIDY:-clang-tidy}
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}
LINT_FLAGS=${LINT_FLAGS:-}

status=0
echo "== clang-format"
# shellcheck disable=SC2046
"$CLANG_FORMAT" --dry-run --Werror $(find src tests -name '*.h' -o -name '*.c' -o -name '*.cc') || status=1

DEFS="-DNT_CONFIG_DIR=\"/mnt/onboard/.adds/nickel-tiles\" -DNT_CONFIG_DIR_DISP=\"KOBOeReader/.adds/nickel-tiles\" -DNH_VERSION=\"lint\""
QT="-isystem $QT_INCLUDE -isystem $QT_INCLUDE/QtCore -isystem $QT_INCLUDE/QtGui -isystem $QT_INCLUDE/QtWidgets"

echo "== clang-tidy"
for f in $(find src -name '*.cc'); do
    # shellcheck disable=SC2086
    "$CLANG_TIDY" --quiet "$f" -- -x c++ -std=gnu++11 -fPIC -Isrc -INickelHook $QT $DEFS $LINT_FLAGS || status=1
done
for f in $(find src -name '*.c'); do
    # shellcheck disable=SC2086
    "$CLANG_TIDY" --quiet "$f" -- -x c -std=gnu11 -fPIC -Isrc -INickelHook $DEFS $LINT_FLAGS || status=1
done
exit $status
