# Host tests for NickelTiles. They are built natively with the distribution's
# Qt 5 and g++, not with the reader's NickelTC toolchain, and run on GitHub
# (.github/workflows/tests.yml). `make -C tests check` builds and runs them all;
# any failing test fails the target.

CC ?= gcc
CXX ?= g++
# Qt's moc lives outside PATH on Debian and Ubuntu; the .pc file says where.
MOC ?= $(or $(shell pkg-config --variable=moc_location Qt5Core 2>/dev/null),$(wildcard /usr/lib/qt5/bin/moc),moc)

PKG_QT := Qt5Core Qt5Gui Qt5Widgets Qt5Test
QT_CFLAGS := $(shell pkg-config --cflags $(PKG_QT))
QT_LIBS := $(shell pkg-config --libs $(PKG_QT))

# The paths the modules compile against, pointed into /tmp/nt-tests (fixtures.cc).
TEST_DEFS := -DNT_CONFIG_DIR='"/tmp/nt-tests/config"' \
	-DNT_CONFIG_DIR_DISP='"/tmp/nt-tests/config"' \
	-DNT_ONBOARD='"/tmp/nt-tests/onboard"' \
	-DNH_VERSION='"test"'

OBJ := build
TESTS := test_layout test_config test_nm_parse test_epub test_ui test_recent

CFLAGS_ALL := -fPIC -Wall -Wextra -I../src -Istubs -I$(OBJ) $(TEST_DEFS)
CXXFLAGS_ALL := $(CFLAGS_ALL) -std=gnu++11 $(QT_CFLAGS) -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB
LDLIBS := $(QT_LIBS) -ldl

# The module objects each test links: only what it needs. recent.cc calls into
# Nickel, so its test links stubs/ instead of nickel.cc and actions.cc.
test_layout_OBJS := layout.o
test_config_OBJS := config.o settings_model.o fixtures.o
test_nm_parse_OBJS := nm_parse.o fixtures.o
test_epub_OBJS := epub.o zip.o config.o fixtures.o
test_ui_OBJS := ui.o config.o
test_recent_OBJS := recent.o config.o stubs.o fixtures.o

.SECONDEXPANSION:
.PHONY: check clean

check: $(TESTS:%=$(OBJ)/%)
	@status=0; for t in $(TESTS); do echo "== $$t"; QT_QPA_PLATFORM=offscreen $(OBJ)/$$t || status=1; done; exit $$status

$(OBJ)/test_%: $(OBJ)/test_%.o $$(addprefix $(OBJ)/,$$(test_$$*_OBJS))
	$(CXX) -o $@ $^ $(LDLIBS)

# Each test is a QObject with Q_OBJECT; moc output is included at the end of the .cc.
$(OBJ)/test_%.moc: test_%.cc | $(OBJ)
	$(MOC) $< -o $@

$(OBJ)/test_%.o: test_%.cc $(OBJ)/test_%.moc
	$(CXX) $(CXXFLAGS_ALL) -c $< -o $@

$(OBJ)/fixtures.o: fixtures.cc fixtures.h | $(OBJ)
	$(CXX) $(CXXFLAGS_ALL) -c $< -o $@

$(OBJ)/stubs.o: stubs/stubs.cc | $(OBJ)
	$(CXX) $(CXXFLAGS_ALL) -c $< -o $@

$(OBJ)/%.o: ../src/%.cc | $(OBJ)
	$(CXX) $(CXXFLAGS_ALL) -c $< -o $@

$(OBJ)/%.o: ../src/%.c | $(OBJ)
	$(CC) $(CFLAGS_ALL) -c $< -o $@

$(OBJ):
	mkdir -p $@

clean:
	rm -rf $(OBJ)
