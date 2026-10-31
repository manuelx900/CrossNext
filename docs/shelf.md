# The shelf

Pomodoro has configurable focus/break durations, cycle length and automatic starts. It pauses on exit/sleep; see [pomodoro.md](pomodoro.md).

Othello uses PocketGamesActivity for its bounded menu/save lifecycle and pure rules in PocketGamesCore. See [pocket-games.md](pocket-games.md) for controls, generation semantics, save versions and physical-device checks.

How to add a game or an app to this fork, and the rules that keep the
navigation from rotting. Read [LOCAL_SCOPE.md](../LOCAL_SCOPE.md) first for why
the fork exists at all.

## The shape

```
Home
  Browse Files / Recent Books / File Transfer / Reading Stats / Games / Apps / Settings
  Games >    chess, battleship, connections, solitaire
  Apps  >    flashcards, calendar, clock, quotes, counter, notes, ...
```

Two folders, siblings, selected by the shared Home menu registry. **Uniformly two taps to
anything.**

The offline app collection and supported import formats are described in [study-suite.md](study-suite.md).

## Menu presentation

Games and Apps share the [pixel menu](menus.md): a black header, a single
bordered list, icons on the left, and a marked row while navigating with buttons.
The Games footer shortcut to PLAYER has been removed. Numbered page controls
remain at the bottom; the gear in the header opens the show/hide selector.
Device names and multiplayer identity files keep their existing format.

Games and Apps are siblings rather than Apps holding Games, and that was a
correction. Nesting made a game three taps and an app two, for a reason no user
could name, and it put a folder in the same list as loose leaf rows -- which is
the inconsistency that makes a menu feel arbitrary.

The naming works because they stand together. "Apps" on its own means
"everything that is not reading", which is a leftover rather than a category;
that is how the previous base's Apps menu ended up holding Reading Stats and the
WiFi transfer page next to Sokoban. "Apps" beside "Games" means the useful ones
beside the fun ones, and needs no explanation.

## Pages, not scrolling

A folder that does not fit is drawn a page at a time, with previous/current-total/next
controls at the bottom and the page beside the folder's name in the header.
Tapping an available arrow steps one page; a **horizontal** swipe steps one page;
the two side keys step one page. All three go through
`ShelfFolderActivity::showPage`, so no route can move by a different amount than
another.

**The reason is touch, not taste.** The list component's 3px overflow track is
drawn but not tappable, so before paging existed every row past the ninth could
be reached only with the physical buttons, on a device whose games are
touch-only on purpose. Scrolling was not worse-looking; it was unreachable.

### Horizontal paging

Swipe left for the next page and right for the previous page, including gestures
starting at the left edge. On the first page, a right swipe returns Home, or
closes the show/hide chooser first. The visible header arrow performs Back from
any page. Games and Apps route these swipes inside their folder activity so the
global edge-Back gesture cannot skip a page. Opening an app restores that app's own gesture policy.
Vertical swipes do not change the folder page. The page controls use the same
previous/current-total/next component as Murdle's How to Play.

Seven things follow, and each was got wrong once:

- **The page is derived from the current selection.** One row owns both the
  visible page and the button cursor while the folder is open. Apps and Games
  start on row zero whenever they are entered; old saved row positions are
  ignored. No position is saved for button steps, paging, exit or sleep.

- **The screen is handed a slice, not the folder plus an offset.** The list
  component clamps `topIndex` to `count - visible` so its last screen is always
  full (`list.h:164`), which is right for scrolling and wrong for paging: page
  two of twelve showed items four to eleven, repeating half of page one. A page
  is a short list, so it is passed as one, and the component never learns pages
  exist. `ListItem::actionValue` still carries the absolute index, so a tap
  reports which game it is rather than which row.
- **The pager is previous / current-total / next.** The two arrow targets step one page; the middle box is a readout. Disabled ends do nothing. The same shared component is used by other discrete pickers, so large catalogues do not create a strip of numbered buttons.

- **The page is said twice, in the footer and header.** `GAMES 2/3` beside the
  folder name makes the current page clear after button, swipe or pager input.

- **Nothing wraps.** Forward from the last page and physical/page-control
  backward navigation from the first do nothing at all. A right swipe on the
  first page performs Back. A refused page step does not repaint: a full-panel refresh that redraws
  the same eight rows is half a second of blink saying a step was taken when
  none was. `shelfui::pageStepClamped` is the shelf's step;
  `shelfui::pageStep` still wraps and Hacker News's story list still uses it.

  A wrap is only safe where the user can see where it put them. Every page draws
  its rows at the same eight screen positions, so a page arrived at by accident
  is indistinguishable from the page that was wanted until something opens --
  and walking forward off the last page is the one step nobody ever means. It
  was reached by an agent who wanted the PREVIOUS page, could not find a
  backward gesture, and pressed on hoping to come round.

  The current/total readout and header make the boundary explicit. The far end is reached with repeated arrows, horizontal swipes or the side keys.

- **A folder always reopens on its first page.** Short side-button presses and
  holds change pages only, including after leaving an app or game. They do not
  mark or open a row. Tap an item to open it. Hidden items remain hidden.

- **Side keys do not create a button cursor.** Entry does not mark an old item. A hold
  still changes pages once per existing hold interval and consumes its release;
  touch and swipe keep their existing routes. Existing shelf files, app progress
  and wake targets retain their formats.

The shared page indicator shows the current page and total between previous/next arrows. End arrows are inert. This keeps large catalogues readable without a growing strip of numbered buttons.

## Choosing what a folder shows

Twenty games is a lot of games, and not everybody wants all of them. **Touching
the header band** turns the list into a chooser: every item the folder holds,
each with a box, hidden ones included. Tap a row to put it on the list or take
it off. `DONE`, or Back, closes the mode; the folder then draws, pages and
resumes over what is left, and the rest is simply not there.

In GAMES, the **settings gear in the upper-right corner** opens this chooser.
Checked games appear in the folder; unchecked games are hidden. Each change is
saved immediately and survives restarting the device.

```
GAMES                2/3  (settings)      GAMES              2/3  [ DONE ]
  (crown) CHESS                             (crown) CHESS             [x]
  (spade) SOLITAIRE                         (ship) BATTLESHIP         [ ]
  ...                                      (spade) SOLITAIRE         [x]
            1  2  3                                    1  2  3
```

**It is a listing, not an uninstall.** Every item is still built into the
firmware and still works. A hidden game still resumes on wake if it was open
when the device slept, and still answers to `CROSSPLAY_AUTOSTART` -- a device
that woke up having forgotten the game on its own screen would be worse than a
long list.

Seven things this cost, each of which was wrong in a draft:

- **The corner icon opens the chooser.** GAMES uses a settings gear; APPS
  keeps its folder mark. The whole header band answers the same tap, so the
  target is larger than the 32px icon. `DONE` replaces the icon while choosing
  and closes the mode.

  The shared pixel header measures the counter, mark and `DONE` separately
  from the safe right edge so their widths cannot overlap the title.

- **The chooser lists EVERYTHING, and the browsing list only what is shown.**
  That is the one thing that cannot be otherwise: a chooser over the shown items
  could not show you what you had hidden. It also means the chooser's rows ARE
  the registry, in registry order, so hiding something never moves the row under
  your finger. The list that shrinks is the other one, and it is not on screen
  while you are choosing.

- **Neither folder has a footer shortcut or chooser caption.** Browsing and
  choosing use the same row geometry; only the checkbox appears on the right.
  Numbered page controls reserve space when the item count needs multiple
  pages. The visible list can have fewer pages after items are hidden.

- **Back closes the mode instead of leaving the folder**, and that is the only
  exception to the two Back rules below. It is named here rather than left to be
  discovered: on this device Back is also the only way out of an app, so a Back
  that walked out of the chooser would read as "undo what I was doing" on the
  one screen where it cannot be.

- **The box is drawn, not blitted.** A filled slab with a tick knocked out of
  it, or a hairline outline -- the same filled-versus-outlined pair the page
  marks below it already use for "this one of these". Lucide's `square-check` is
  a hairline tick inside a hairline box, and down a column of ten rows the two
  states read as the same grey smudge. Only the tick is an asset, because a
  hand-drawn tick is what goes wrong (solitaire's pips, minesweeper's flag,
  three attempts each).

- **The whole row toggles, not the box on it.** Two hit regions per row would be
  two of the screen's twenty-four for a control the finger is already on, and
  the row past the twenty-fourth stops answering in silence.

- **Hiding everything is allowed, and the empty folder is its own way back.**
  The whole empty body is a second hit region that opens the chooser, and it
  says `NOTHING HERE` above the sentence that says so. The band above it would
  have done, but it is 400px away at the top of an 800px panel and a person who
  has just found an empty folder is looking at the middle of it. A rule refusing
  the last game would be the device arguing with its owner.

### The file, and the two units

The set lives in `/.crosspoint/shelf-hidden.cfg`, beside `shelf.cfg` and
`player.cfg`, one item TITLE per line, only the hidden ones. So the ordinary
file is absent or empty and the ordinary parse is nothing at all.

**Titles, not row indices**, for the reason the wake-resume already learned:
indices move whenever a game is added, this card outlives firmware updates, and
an index would hide a DIFFERENT game after the next release with nobody able to
say why. A title in the file that matches no item today is KEPT on save, so a
build that does not know about a game is not the reason the build after it
forgets that the game was hidden. Everything else is dropped quietly -- blank
lines, whitespace, a stray carriage return, a duplicate, a line longer than any
title can be, anything past `MAX_HIDDEN` -- because there is nothing anybody
could do about any of it, and because the whole format fails OPEN: a file that
is missing, short, truncated or full of junk shows everything.

**A row is not an item, and the conversion is one named thing.** The card stores
the ITEM the folder was standing on; `shelf::resumeRowIn()` answers in SHOWN
rows and `shelf::rememberRowIn()` takes one, so the file holds one unit and the
folder counts in the other. The folder does hold items -- at the two points
where it must, opening one and hiding one -- and it gets them from
`ShelfFolderActivity::itemAtRow` and nowhere else. The arithmetic itself is in
`ShelfHidden.h` rather than beside the registry, because `Shelf.cpp` cannot be
built off a device and this is the part that must be tested: see
`host-tests/shelfhidden`. They are both small ints in the same range,
so a caller holding one while believing the other is a mistake nothing would
report -- and the symptom is this screen's oldest one: the header says page 2,
the rows sit where page 2's rows sit, and the tap opens page 1's game.

## The three rules

**1. A folder holds items, never folders.** There is nowhere in `shelf::Folder`
to put a `Folder`. The depth cap is structural, not a convention someone has to
remember, and on a panel that repaints in half a second a third tap is a real
cost.

**2. Back follows the launch origin.** A Home shortcut returns Home; a folder
launch returns to that folder. A folder itself returns Home. While a folder is
choosing, Back closes the chooser and stays put.

**3. No app names its own destination.** It calls `shelf::leave()`.

Rule 3 is the one with scar tissue. The previous base carried a
`setReturnHere()` / `takeReturnHere()` breadcrumb that had to be _redeemed by a
different activity on entry_, so a missed redemption stranded you in the wrong
menu. It existed only because upstream's own games each hardcoded `goToApps()`
and could not be edited. Every app here is ours, so none of them should ever
name a destination. When the four games moved onto CrossPoint, this rule made
each one a single-line change.

The current folder is module state in `Shelf.cpp` rather than something each app
carries. `replaceActivity` means exactly one activity exists at a time, so there
is exactly one current location: one fact, stored once. Threading a parent
through every factory would make six apps each keep their own copy of something
the device only has one of, and any app that forgot to store it could not leave.

## Adding a game or an app

**1. Copy the template.**

```bash
cp -r src/apps_local/sample src/apps_local/study
```

`sample` is sixty-six lines and is not registered, so it builds but never shows
up on the device. Do not start from a real app: the smallest is solitaire at
nineteen hundred lines.

Keep the app-name prefix on every file (`StudyActivity.h/.cpp`), matching
upstream convention: grep and crash logs stay unambiguous.

**2. Split it three ways.** This is the part that pays off later:

| Layer         | Example             | Knows about                               |
| ------------- | ------------------- | ----------------------------------------- |
| Rules / state | `ChessCore.h`       | nothing -- freestanding C++17             |
| Screens       | `ChessScreens.h`    | FreeInkUI and Toybox tokens, nothing else |
| Activity      | `ChessActivity.cpp` | the renderer, storage, input, the shelf   |

The first two are host-testable on macOS with no device. The third is the only
part that needs hardware, and it should be thin. See
[building-apps.md](building-apps.md) for the method in full.

**3. Register it.** One row in `src/apps_local/Shelf.cpp`:

```cpp
constexpr shelf::Item kApps[] = {
    {"STUDY", &icon_study_32, &StudyActivity::create},
};
```

That is the whole registration. No `ActivityManager` method, no `UIIcon` enum
variant, no i18n key, no `AppId` bit.

**Icons come from Lucide, not from `UIIcon`.** Add a line to
`tools_local/toybox/icons.txt` and run `./tools_local/toybox/gen_toybox_icons.sh`:

```
study = graduation-cap
```

That regenerates `src/apps_local/ui/ToyboxIcons.h`, which is committed because
generating needs librsvg and a checkout should build without it. Upstream's
palette is thirteen icons and growing it costs two upstream files per app;
Lucide ships 1735 and costs a line in a manifest.

A row with no icon does not compile -- `Shelf.cpp` static_asserts it, because a
blank icon gutter is silent otherwise. Pick for silhouette rather than
literalness: the label already says the name, so the icon's job is to be
distinct at a glance in a 62px row.

**A folder has no size limit.** It used to: the folder activity read the whole
registry into fixed arrays of sixteen and clamped in silence, so a seventeenth
game simply did not appear, with no log and nothing to grep for. The screen is
now handed one page at a time, so only a page is ever copied and the arrays are
sized by the tallest band this panel can draw rather than by how many games
Mario has promised people.

**4. Leave through the shelf.**

```cpp
if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
  shelf::leave(renderer, mappedInput);
  return;
}
```

**5. Write its tests before you believe it.** `host-tests/<app>/run.sh`, plus a
block in `host-tests/ui/test_ui.cpp` for the screens. Then break the code on
purpose and check the tests notice -- the shelf's own footer test passed against
a mutant that drew an invisible button, because asserting "the name is not
drawn" is not the same as asserting "the control is not there".

## What is not host-tested, and why

`ShelfScreen` is, like every screen here. `Shelf.cpp` is not: it exists to swap
activities, so it pulls in `ActivityManager` and cannot be built freestanding.

Its whole job is five facts -- which folder is open, which folder Home should
select, which page each folder should reopen on, which item was open when the
device went to sleep, and which items the folders are not showing -- and those
are verified in the simulator instead.

The arithmetic underneath the third one is not in that exception:
`shelfui::rowForPage` and `shelfui::resumeRowFor` are pure and live in the `ui`
suite, where the round trip (a page stored as a row comes back as the same page)
and the shrunken-folder rule are properties rather than examples.

Neither is the fifth one's file format: `ShelfHidden` is freestanding, and
`host-tests/shelfhidden` asks it what a file written by a later firmware does,
what a half-written one does, and what a card full of junk does -- the three
cases whose failures are all silent on the device, because each of them is a
game that appears or vanishes with nothing to say why. The chooser's own screen
is in the `ui` suite with the rest: that the corner chip routes, that a row
toggles instead of opening, that the page counter clears the chip, that both
modes fit the same games on the same page, and that an empty folder is tappable
everywhere.

The last three of those outlive a reboot, in `/.crosspoint/shelf.cfg` beside
`player.cfg`. They are plain `.bss` otherwise, and `main.cpp` deep-sleeps on the
idle timeout with wake being effectively a chip reset, so the shelf used to
forget which game you were playing every time you put the device down. Verified
by driving it twice: one run opens the third game and leaves, and a second run
from a cold boot must land Home on Games and the folder's cursor on that same
game. Both were wrong at some point and both were caught by looking:
leaving a folder put the cursor on Browse Files, and returning from the third
game put it on the first. If you touch that bookkeeping, drive it:

```bash
# Opened a game and came back: the row you opened, on its page.
./scripts_local/sim-shot.sh '2500:TAP:240,650;4500:TAP:285,687;7000:TAP:240,275;12000:BACK;15000:QUIT' \
                            '14000:qa-artifacts/returned.bmp'

# Browsed and walked away without opening anything: the page you were reading.
# This is the half that was wrong, and the half a launch-only test cannot see.
# A left swipe advances; the header arrow returns Home.
./scripts_local/sim-shot.sh '2500:TAP:240,650;4500:SWIPE:380,400,100,400;7000:BACK;9000:TAP:240,650;12000:QUIT' \
                            '11000:qa-artifacts/returned.bmp'
```

Both must come back on the page they left, and the first must come back on the
row it opened. Drive both: they were the same behaviour for as long as the stored
value was the launched item, so a run that only launches passes either way.

## Wake comes back into the app, not to Home

Deep sleep is a chip reset, so before it there is one write and after it one
read. `shelf::rememberForWake()` runs on the way into sleep and records the open
item's **title** on a second line of `shelf.cfg`; `shelf::resumeFromWake()` runs
from `setup()`'s routing block and reopens it, ahead of the reader's own resume.
Without it every sleep taken in a game woke up on Home, which is what made the
"Quick Resume" sleep screen a lie outside the reader: it leaves your game on the
panel with a small moon in the corner, and the wake then threw that game away.

Three things that look optional and are not:

- **The title, not the row index.** The card outlives firmware updates and row
  indices move whenever a game is added. An index would resume into a different
  game, silently. `findItemByTitle()` is the one place a title becomes a row,
  and a title that no longer exists falls back to Home.
- **The activity name, not `openFolderIndex`.** An item counts as open only when
  the activity it launched is the one on screen. The Home gesture leaves an app
  without passing through `leave()`, so `openFolderIndex` alone would claim a
  game you left ten minutes ago is still open and wake into it.
- **Only on a verified sleep wake.** The gate is `BootResume::SplashlessWake`,
  which needs both the one-shot `showBootScreen` flag the sleep wrote and a
  power-button wakeup reason. A cold boot must not resume off a stale card.

The simulator models the whole cycle -- a button press while asleep re-execs the
process reporting a power wake -- so drive it end to end rather than in halves:

```bash
CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE='7000:QUIT' \
CROSSPOINT_SIM_SCREENSHOTS_AFTER_WAKE='4000:qa-artifacts/after-wake.bmp' \
./scripts/sim-shot.sh '2500:TAP:240,650;4500:TAP:240,472;7000:SLEEP;10000:POWER' ''
```

The trace must show the game entered again on the second process, with no Boot
and no Home between. Do not settle for checking the two halves separately: the
gate is the _combination_ of the flag and the wakeup reason, and setting each by
hand proves neither.

What this does **not** do is restore a half-played board. Reopening the app is
the shelf's job; remembering the position is the app's, through the same save
its `onExit` already writes. Chess, Solitaire and Sudoku come back mid-game;
Checkers, Connect Four, Knucklebones, Minesweeper and Yahtzee persist only their
history, so they come back on their own menu.

## Two icon paths, opposite conventions

Worth knowing before you add an icon anywhere new, because it costs an
afternoon otherwise.

- `fui::bitmapFromIcon` + `DrawTarget::bitmap`, which everything in
  `apps_local` uses, takes **upright** bitmaps. The renderer maps logical
  coordinates onto the panel.
- `GfxRenderer::drawIcon`, which upstream's `drawButtonMenu` uses, rotates its
  input by -90. Every bitmap in `src/components/icons/` is therefore stored
  rotated the other way so it comes out upright.

So the two Home folder icons exist twice: upright in `ui/ToyboxIcons.h`, which
is what `rotate_icons.py` reads, and pre-rotated in
`src/components/icons/shelfIcons.h`, which is what the theme draws.
`tools_local/toybox/gen_toybox_icons.sh` writes both; never hand-edit either.
The upright pair is the rotation's source rather than something a screen draws:
the folder header stopped carrying a mark when the chooser took that corner.

The joystick shipped lying on its side, then upside down, before this was
understood. The direction was settled by rotating a freshly generated `folder`
and comparing it against upstream's own stored `FolderIcon` -- not by
byte-equality, since the Lucide source has moved since theirs was generated, but
by looking at the shape.

## Adding a third folder

One row in `kFolders`, plus its icon in two places: a `UIIcon` value appended in
`BaseTheme.h` with a case in `LyraTheme.cpp` (Home draws it and accepts nothing
else), and a line in `tools_local/toybox/icons.txt` -- which is not for the
folder's own header, which carries no icon any more, but for the pre-rotated
copy `gen_toybox_icons.sh` writes into `src/components/icons/shelfIcons.h` for
Home's row. Those are the only per-folder edits to upstream files this fork
makes, which is affordable at two folders and would not be at ten.

Resist it until there is something that genuinely belongs in neither. Two
folders is a structure; four is a filing system, and a filing system is what you
build when you have not decided what the device is for.

## The Home seam

Home uses `src/apps_local/home/HomeMenuCore` for visible row count and identifier/index conversion. Browse Files, Recent Books, File Transfer, Reading Stats, Games, Apps and Settings are enabled by default. Settings stays enabled; the other rows can be renamed or hidden from Settings > System > Home Menu, with at most seven enabled. Optional Plugins/OPDS and selected game/app shortcuts count toward the same limit. + Add Game / + Add App open separate catalogue selectors. A shortcut uses the shelf launcher with Home origin and returns directly Home, including after sleep/reboot. See [reading-stats.md](reading-stats.md).

Recent-book cover tiles precede these menu rows and are subtracted once before dispatch. Shelf return uses stable Games/Apps identifiers, so changing visibility does not change which folder opens. `HomeShelfRows.h` is retained only as a legacy test fixture; production Home no longer derives shelf offsets from upstream row positions.

## Titles: Title Case outside, capitals inside

Touch headers reserve their left side for a Back arrow. The folder publishes
that region through the same `HeaderBackTapTarget` used by system headers;
`ToyboxScreen::headerBand()` does the same for app screens. The input manager
turns it into logical Back and excludes it from ordinary taps, so it cannot
also toggle the visibility selector or launch a row. Each activity retains its
existing Back behavior. An app's own leading header action takes precedence.
Headerless views clear the target explicitly, as xkcd's reader, Forehead's round
and Wallpapers' full-screen preview do. Scratch frames and partial redraws
preserve the visible screen's header target.

The registry stores `"Games"`. Home draws that, because Home is upstream's list
and has to look like it. `ShelfFolderActivity` shouts it into `GAMES` for its own
header, because Toybox chrome is capitals. Outside is their look, inside is ours,
and the line between them is one loop in one file.

Games and Apps share the nine-row page limit. Their folder menus page horizontally; a right swipe on the first page performs Back. Games reserve horizontal swipes for gameplay by disabling left-edge Back; the header arrow and logical Back remain available. Home follows the saved menu order; Settings remains visible and may be moved. See [interface-layouts.md](interface-layouts.md).

## Launch origin

A Home shortcut returns directly Home. A game or app launched from its folder
returns to that folder. The shelf launcher owns this distinction; individual
apps still call `shelf::leave()`. `shelf.cfg` stores an optional third line,
`HOME`, for the open shortcut so sleep/reboot preserves the return destination.
Older one/two-line saves default to folder origin. Opening a folder clears the
open shortcut and restores normal folder navigation.

Carousel 2 and Bookshelf folder menus use unboxed regular labels, gray separators and a compact footer with page controls. Their page capacity uses the same row height for drawing and navigation. Framed and Carousel retain their existing bordered rows and paginators.
