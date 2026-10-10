# Nickel home page map (firmware 4.45.23697, libnickel.so.1.0.0)

Recovered from the firmware's `libnickel.so.1.0.0` (dynamic symbols are not
stripped) with `objdump -d` and a small literal-resolving tracer. Addresses are
file virtual addresses in that library.

## Construction path

| Step | Symbol | Address | Size |
|---|---|---|---|
| Main window asks for the home page | `MainWindowController::createHomePage(bool)` | 0xecd6ec | 2192 |
| Controller created (`new(100)`) | `HomePageController::HomePageController(bool, QObject*)` | | |
| View created (`new(184)`) and configured | `HomePageController::loadView()` | 0xd8c408 | 1348 |
| View constructor (the hook point) | `HomePageView::HomePageView(QWidget*)` = `_ZN12HomePageViewC1EP7QWidget` | 0xd93a4c | 844 |
| Widget tree built | `Ui_HomePageView::setupUi(QWidget*)` | 0xd940ec | 3776 |
| Slots filled | `HomePageController::configureWidgets()` | 0xd8bf80 | 390 |

`loadView` creates a fresh `HomePageView` each time the home page is (re)created,
so a constructor hook runs for every instance. The constructor calls `setupUi`,
sets the properties `showIndicators`, `showStatusBottomLine`, `showTitle`,
`mainNavView = "Home"`, connects one signal, and calls `QLayout::activate` on the
three row layouts. Nothing in it depends on content, so inserting into the
layouts right after the original constructor returns is safe. NickelHome hooks
this exact symbol the same way.

## Widget tree built by `Ui_HomePageView::setupUi`

Object names in quotes, class in parentheses, `Ui` struct member offset in brackets.

```
"HomePageView" (QWidget, 184 bytes)        property styleSheetFile = "HomePageView"
└─ "verticalLayout" (QVBoxLayout) [0x00]   spacing 0, margins 0
   └─ "mainContainer" (GenericContainerWidget) [0x04]
      └─ "mainLayout" (QVBoxLayout) [0x08]  spacing 0, margins 0
         ├─ [index 0] "row1" (QWidget) [0x0c]
         │    └─ "row1Layout" (QHBoxLayout) [0x10]   stretch 1:1
         │       ├─ "row1col1" (QStackedWidget) [0x14]
         │       │    ├─ "findReadWidget" (FindReadHomeWidget) [0x18]
         │       │    └─ "fourBookWidget" (FourBookHomeWidget) [0x1c]
         │       └─ "row1col2" (QStackedWidget) [0x20]
         │            └─ "quickTourWidget" (QuickTourWidget) [0x24]   (+ dynamic tiles, see below)
         ├─ [index 1] "row2" (QWidget) [0x28]
         │    └─ "row2Layout" (QHBoxLayout) [0x2c]   stretch 1:1
         │       ├─ "row2col1" (QStackedWidget) [0x30]
         │       │    ├─ "myBooksWidget" (MyBooksHomeWidget) [0x34]
         │       │    └─ "topPicksWidget" (TopPicksHomeWidget) [0x38]
         │       └─ "row2col2" (QStackedWidget) [0x3c]
         │            └─ "quickTourWidget2" (QuickTourWidget) [0x40]   (+ dynamic tiles)
         └─ [index 2] "row3" (QWidget) [0x44]   property regularSpacing = true, mainLayout stretch 1
              └─ "row3Layout" (QHBoxLayout) [0x48]   stretch 1:1
                 ├─ "browseKoboContainer" (GenericContainerWidget) [0x4c]
                 │    └─ "browseKoboLayout" (QVBoxLayout) [0x50] → "browseKoboWidget" (BrowseKoboWidget) [0x54]
                 └─ "smartLinkContainer" (GenericContainerWidget) [0x58]
                      └─ "smartLinkLayout" (QVBoxLayout) [0x5c] → "smartLinkWidget" (SmartLinkWidget) [0x60]
```

`mainLayout.setStretch(2, 1)`: row3 absorbs the leftover height. Rows 1 and 2
take their size hints.

## What NickelHome does to this tree (config on this reader)

- `hide_home_row1col2_enabled:1` → `row1col2` hidden (`setVisible(false)`), and
  `HomePageView::configureTopRight` is no-op'd so nothing re-shows it.
- `hide_home_row2_enabled:1` → `row2` made transparent and disabled **but keeps
  its layout space** (opacity effect), and `configureMiddleRight` is no-op'd.
- `hide_home_row3_enabled:1` → `row3` hidden (`setVisible(false)`).

So today the visible home page is row1 only, followed by an invisible row2-sized
gap. A tile row inserted at `mainLayout` index 1 sits directly under the
current read and above that gap, which is where it should be.

## Re-configuration paths (what could disturb an inserted row)

`HomePageController::configureWidgets()` calls, in order:
`getPrioritizedWidgetList`, `configureCurrentReads(QVector<Volume>)`,
`configureMyBooks(QVector<Volume>, int)`, `configureMiddleRight(HomePageWidgets)`,
`configureTopRight(HomePageWidgets)`, `toggleParentalControlSensitiveViews()`.
`HomePageView::configureWidget(HomePageWidgets)` creates
`RecommendationsHomeWidget` / `WishlistHomeWidget` on demand and calls
`configureTopPicks` / `TopPicksHomeWidget::configure`. All of these operate on
the `row1col2` / `row2col2` stacks and the existing widgets; none touch
`mainLayout` itself. `HomePageView::showEvent` only bumps a visit counter and
may show first-time dialogs. A widget inserted into `mainLayout` therefore
survives every reconfigure; only destruction of the whole view removes it, and
the constructor hook recreates it then.

## `GenericHomeWidget` (Nickel's own home tile frame)

`Ui_GenericHomeWidget::setupUi`:

```
"GenericHomeWidget" (QWidget)   property styleSheetFile = "GenericHomeWidget"
└─ "homeWidgetLayout" (QVBoxLayout)
   └─ "homeWidgetTopContainer" (GenericContainerWidget)
      └─ "homeWidgetTopLayout" (QVBoxLayout)
         ├─ "topBorder" (QWidget)
         ├─ "widgetContainer" (GenericContainerWidget) → "widgetContainerLayout" (QVBoxLayout)
         ├─ "homeWidgetLabel" (ElidedLabel)
         ├─ "homeWidgetMetaLabel" (QLabel, align 0x21, word wrap)
         └─ "homeWidgetBottomSpacer" (QWidget)
```

Constructor: `QWidget(parent)`, `setupUi`, `setTitleMaxLines(1)`,
`setTopBorderVisible(false)`, hides the meta label, activates the layout.
`TappableHomePageWidget` derives from it plus `GestureReceiver`, grabs a tap
gesture and exposes `tapped()` / `longPress()` signals and `showHitState(bool)`.
Its object size is not recoverable from exported symbols alone (no direct
`new`+ctor pair found in the traced functions), so the skeleton uses a plain
`QWidget` tile and leaves "native" tiles as a Phase 4 option that would need
the size from a disassembly of a subclass constructor caller.

## Launch mechanism

NickelMenu's `cmd_spawn` is `QProcess::startDetached("/bin/sh", ["-c", cmd], "/")`.
The skeleton does exactly that on tap (gated by `nt_actions`), with the same
script paths the reader's NickelMenu entries already use:

| Tile | Command |
|---|---|
| KOReader | `exec /mnt/onboard/.adds/koreader/koreader.sh` |
| Plato | `exec /mnt/onboard/.adds/plato/plato.sh` |
| Cobalt | `/mnt/onboard/.adds/cobalt-launch.sh` |
| Kobold | `/mnt/onboard/.adds/kobold/launch.sh` |

## Object names visible in `libnickel` strings that were not in `setupUi`

`homeLayout`, `homeOptions`, `homePage`, `homeTapped`, `homeTileSelected`,
`homeWidgetClicked`, `leftColumn`, `rightColumn`, `myBooks`, `topPicks`,
`homeButton`, `homeHelp`, `homeRadioBtn`. These belong to other views (settings,
nav footer, older layouts) and are not needed.
