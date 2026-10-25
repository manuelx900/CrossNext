# App and game rendering audit

## Section-navigation update

Statistics now uses its initial summary traversal to collect recents and the
first finished-book page. Its eight-record read-ahead also supports reverse
passes. A 148-record simulator fixture makes 20 journal reads on entry, then no
journal reads while switching the cached sections and Recent filter. Home returns
with one settled presentation in the prepared fixture. These are simulator
operation counts, not physical timing claims.

Fixed queues keep physical navigation while the render mutex is unavailable in
the shelves, Statistics, Quotes, Habits, Notes/Tasks, Pomodoro and Reading Quest.
The main loop captures first and tries the mutex; commands change state only
after acquiring it. The queues coalesce adjacent steps in one direction while
preserving opposite steps and Back. Touch activation still uses the existing
surface-reveal gates.

Unchanged page limits no longer request help/puzzle refreshes in Checkers, Go,
Minesweeper, Picross, Chess Puzzles, Carnivoro and The Intercept; Pomodoro's stage
pager likewise skips an unchanged boundary. Ordinary game moves, AI thinking
states and timer updates retain their existing semantics.

Wallpapers paints a preparation view before discovery/conversion, caches source
metadata and target geometry, and processes one new thumbnail per loop. Warm
entry bypasses whole-library prewarming. Page rendering reads prepared RAM
thumbnails; selection is a fixed 32-byte bitset. The random-set copy operation
reports progress separately from gallery rendering.

## Full-entry audit

The audit covers all 28 entries currently registered in `src/apps_local/Shelf.cpp`:
16 games and 12 apps. Source branches and their drawing helpers were inspected;
each app's settled entry screen was also repainted once in portrait and landscape
in the actual WASM firmware. Both orientations produced exactly one presentation
per requested repaint. A short idle window produced no unsolicited second frame.
The current update repeated this check for all 28 entries in both actual geometries. Populated Quest, Notes, Habits and Pomodoro fixtures also retained 13 physical steps with an injected 250 ms render delay. These checks do not measure physical E-Ink refresh or exhaust every game state.

| Entry | Render owner | Presentations, portrait / landscape |
| --- | --- | --- |
| Chess | [`ChessActivity`](../src/apps_local/chess/ChessActivity.cpp) | 1 / 1 |
| Checkers | [`DraughtsActivity`](../src/apps_local/checkers/DraughtsActivity.cpp) | 1 / 1 |
| Go | [`GoActivity`](../src/apps_local/go/GoActivity.cpp) | 1 / 1 |
| Minesweeper | [`MinesweeperActivity`](../src/apps_local/minesweeper/MinesweeperActivity.cpp) | 1 / 1 |
| Picross | [`PicrossActivity`](../src/apps_local/picross/PicrossActivity.cpp) | 1 / 1 |
| Wordle | [`WordleActivity`](../src/apps_local/wordle/WordleActivity.cpp) | 1 / 1 |
| 2048 | [`Game2048Activity`](../src/apps_local/game2048/Game2048Activity.cpp) | 1 / 1 |
| The Intercept | [`InterceptActivity`](../src/apps_local/intercept/InterceptActivity.cpp) | 1 / 1 |
| Battleship | [`BattleshipActivity`](../src/apps_local/battleship/BattleshipActivity.cpp) | 1 / 1 |
| Solitaire | [`SolitaireActivity`](../src/apps_local/solitaire/SolitaireActivity.cpp) | 1 / 1 |
| Hearts | [`HeartsActivity`](../src/apps_local/hearts/HeartsActivity.cpp) | 1 / 1 |
| Murdle | [`MurdleActivity`](../src/apps_local/murdle/MurdleActivity.cpp) | 1 / 1 |
| Carnivoro | [`CarnivoroActivity`](../src/apps_local/carnivoro/CarnivoroActivity.cpp) | 1 / 1 |
| Connect Four | [`ConnectFourActivity`](../src/apps_local/connectfour/ConnectFourActivity.cpp) | 1 / 1 |
| Sudoku | [`SudokuActivity`](../src/apps_local/sudoku/SudokuActivity.cpp) | 1 / 1 |
| Othello | [`PocketGamesActivity`](../src/apps_local/pocketgames/PocketGamesActivity.cpp) | 1 / 1 |
| Reading Quest | [`ReadingQuestActivity`](../src/apps_local/quest/ReadingQuestActivity.cpp) | 1 / 1 |
| Habits | [`HabitsActivity`](../src/apps_local/habits/HabitsActivity.cpp) | 1 / 1 |
| To Do List | [`NotesActivity`](../src/apps_local/notes/NotesActivity.cpp) | 1 / 1 |
| Notes | [`NotesActivity`](../src/apps_local/notes/NotesActivity.cpp) | 1 / 1 |
| Pomodoro | [`PomodoroActivity`](../src/apps_local/pomodoro/PomodoroActivity.cpp) | 1 / 1 |
| Wallpapers | [`WallpapersActivity`](../src/apps_local/wallpapers/WallpapersActivity.cpp) | 1 / 1 |
| Quotes | [`QuotesActivity`](../src/apps_local/quotes/QuotesActivity.cpp) | 1 / 1 |
| Flashcards | [`FlashcardsActivity`](../src/apps_local/flashcards/FlashcardsActivity.cpp) | 1 / 1 |
| Words | [`WordsActivity`](../src/apps_local/words/WordsActivity.cpp) | 1 / 1 |
| Calendar | [`ToolsActivity`](../src/apps_local/tools/ToolsActivity.cpp) | 1 / 1 |
| Clock | [`ToolsActivity`](../src/apps_local/tools/ToolsActivity.cpp) | 1 / 1 |
| Counter | [`ToolsActivity`](../src/apps_local/tools/ToolsActivity.cpp) | 1 / 1 |

## Repeated work removed

- Carnivoro: removed an unconditional complete UI scan for prewarming built-in
  fonts before the real build. It now builds its UI once per frame.
- Words: built-in content builds once. SD-font prewarming is reserved for a new
  entry or text size; ordinary page changes reuse layout and font-cache state.
- Reading Quest: the missing-resources branch previously built the same notice,
  cleared it and built it again. The existing builder already owns that branch.
- Othello: removed a font binding immediately overwritten by the actual binding.
- Flashcards: reuse measured face size/page counts; plain text has 128 indexed
  page starts. Rich text no longer measures each candidate width twice.
- Words, Quotes and The Intercept reuse bounded page starts in their existing
  wrapping workspaces. Pages beyond cache capacity remain accessible.

The native plain-flashcard fixture requires 2,234 width measurements without its
index, down from 10,249 before bounded prefix fitting; cached pages 0, 42 and 83
require 27, 27 and 6 respectively. A long Words fixture requires 2,418 uncached
measurements, down from 13,825, and 29, 37 and 10 for cached pages 0, 51 and 101.
These are operation counts for synthetic fixtures, not device speed multipliers.

Counter captures a bounded queue of releases while rendering, then applies them
under the existing render lock. A test with a deliberate 250 ms render delay
retains all 12 touch taps and 12 physical releases. Notes/Tasks reuse derived page
starts; Quotes reuses its index while paging and reads clippings in 512-byte blocks.
Pomodoro debounces interaction saves for 350 ms and flushes on exit/sleep; Habits
uses a flash CRC table with its existing verified save format.

The CNART002 Reading Quest pack opens by validating its index, then validates each
sprite before first use. The shipped 900-sprite fixture reads 14,424 bytes at open
with PSRAM, compared with 2,448,642 for the legacy whole-file check. The PSRAM
cache is 18,496 bytes and Quest borrows its own 3,072-byte history read-ahead.
Legacy packs still use whole-file validation. Healthy renders build once; an
asset failure during drawing replaces the frame with the resource error screen.

## Intentional work retained

Chess and Murdle have multiple `displayBuffer` call sites in mutually exclusive
branches with early returns. Wallpapers preview returns before the regular grid
renderer. Reading Quest's level-up dialog overlays its base screen. Loading,
generation progress, AI move previews, timer ticks and partial/full refresh
policies represent different visible states; removing them would change behavior.
An SD-font prewarm scan gathers glyphs and precedes one panel presentation.

No new worker, second framebuffer or persistent game-state format was added.
The Anki preview retains its 896-byte image band. Zoom alone opens the optional
source-resolution sidecar and allocates a fallible PSRAM band of at most 32,768 bytes; Fit/Back/exit
release it. Missing or invalid HD data falls back to the preview. Rotated and
unrotated sampling walk source bands monotonically. Progressive JPEG HD preparation uses a bounded JPEG-only decoder; the reduced
grid is retained only as an allocation-failure fallback. Existing imports retain their compact preview and can recover HD on explicit
Zoom from their verified unchanged source, preserving review history.

## Device verification

Back up the SD card. On X4 Pro, open every entry, wait, navigate and
return. Repeat in all four orientations; there should be no unexplained second
flash or delayed replay of old input. Preserve intended loading/timer/AI frames.
Page through long plain, vocabulary, cloze and CJK cards; verify complete text,
word boundaries, ruby, manual sizes and Smart sizing. Exercise image zoom,
diagonal dragging, rotation plus zoom, bounds, Fit and Back; returning must retain
the same card and rating state. Repeat open/close while monitoring free heap,
largest free block, watchdogs and SD errors. Hardware latency and resource peaks
remain unmeasured until those physical tests are completed.

Install the matching Resources ZIP and test a large PNG and baseline JPEG in a
new APKG. Compare Fit with Zoom, pan both axes and rotate twice; detail should
come from the source and returning should preserve the card. Repeat with a
missing/malformed zoom sidecar and confirm preview fallback. Test Counter bursts,
Reset/Cancel navigation, count limits and save/reopen. Check Clock/Calendar's
two-position rotation, Words/Flashcards manual 10 pt and Smart long-word paging,
Wallpapers pager margins, and Notes/Tasks cache invalidation after edits. Exercise
Pomodoro Start/Pause and sleep persistence, Habits slot recovery and Quest with
substantial history. In a disposable copy, corrupt a used Quest sprite: it must
show the resource notice without active gear controls. Track internal heap,
largest free block, PSRAM and render stack high-water over repeated cycles; watch
for cumulative loss, watchdogs, SD errors or stale input replay.


## Home navigation and existing Anki images

Home captures logical navigation before attempting the render lock. Its main-loop
queue holds 16 commands (72 bytes including indices); adjacent movements collapse
modulo the option count. Confirmation retains its position in that sequence.
Side and front navigation cycle only menu options, including the cover-grid theme;
book covers remain touch targets and Back retains the Resume shortcut. Holds start
at 400 ms and repeat every 140 ms; releases add no navigation step.

Carousel and framed dashboards repaint the old and new menu rows in the existing
single framebuffer. They do not rebuild hit tables, repaint covers or reload book
progress for focus changes. A date/time change, carousel move, orientation change
or intervening panel paint restores the full rendering path. Net-zero navigation queues no repaint; external status refreshes remain active. The e-paper refresh itself still takes time;
hardware response times have not been measured.

The compact Photo control shares the 32-pixel pager row at the card's lower right,
with the same border weight and small font. It no longer removes 52 pixels from
the text region. Both controls have separate, non-overlapping touch targets.

A deck imported before HD support may have only `images.dat`. Its first explicit
Zoom can rebuild media from the unchanged APKG/colpkg recorded in `source.txt`.
This cold path uses the existing importer in a separate `.zoom-refresh` directory;
it verifies the package fingerprint and byte-for-byte card text/order before
publishing only `images.zoom.dat`. Scheduling, history, previews and text files
are never replaced. The initial upgrade needs import time and temporary SD space,
and the existing importer's 524,320-byte PSRAM workspace; subsequent zooms use the
32 KiB PSRAM read band. Normal card navigation never enters this path. If the original
is missing/changed, verification fails, or scratch already exists, the preview
remains available. A power interruption may leave `.zoom-refresh`; preserve a
backup and remove that scratch directory before retrying. Original low-resolution
images and the bounded progressive decoder's memory fallback still limit detail; magnification cannot create missing pixels.

On X4 Pro, rapidly tap and hold both logical navigation directions in carousel,
framed and cover-grid homes. The cursor must stop on the final menu option and
never enter a book; confirm must open that option. Repeat after remapping, in each
orientation, and after returning from a pushed screen. Check a minute change,
carousel swipes and touch-opened covers. For a legacy Anki deck, retain its original
package, back up the SD, use Zoom once, then Fit/Zoom again; compare fine detail
and verify review counts/history are unchanged. Try missing/modified originals,
SD-full/failed upgrade and a forced interruption on a disposable card copy.


Zoom stores native grayscale in v4 (one byte/pixel). The preview path is unchanged.
Only Zoom allocates at most a 32,768-byte PSRAM band. Fixed-point bilinear sampling precedes
screen-sized ordered dithering; reverse traversal uses overlapping 16-row bands
to avoid one SD read per output column. Legacy v3 HD is read-only compatible and
can be upgraded from its verified original package. Progressive JPEG HD import
uses stb_image 2.30, JPEG-only, with fallible PSRAM-only blocks capped at 6 MiB in
aggregate; it falls back to the existing decoder if unavailable. Coefficients and
gray pixels are temporary import data, never a second panel framebuffer. Normal
viewing and image panning never instantiate that decoder. Flash/heap/stack impact
must be checked on X4 Pro; native/WASM checks do not measure physical panel latency.

The importer records a text hint when at least 95% of native gray samples are
below 32 or above 223. This uses existing decoded rows without new buffers.
Zoom interpolates those samples then thresholds text at 128; photographs retain
screen-sized ordered dithering. This avoids turning text edges into enlarged
noise while preserving photographic tones. The v4 stride's high bit stores the
hint and is masked before any size validation or SD addressing.

## Minesweeper touch feedback

Grid contact tracking no longer requests a presentation on finger-down or
finger-up. The board repaints only when a dig, chord or flag changes the game.
Hold-to-flag keeps its 400 ms threshold and unconditional release suppression;
no game rules, saves, refresh waveform or extra framebuffers change. A touch that
clears an existing physical-button focus still repaints that visual change.

The actual WASM activity reproduced two presentations for an opening/dig, two
for a no-op revealed-number tap and three for a held flag before this change.
The host computation-to-present logs were 0–1 ms for that fixture, so this fix
removes redundant presentation requests rather than changing the rules engine.
Those counts do not establish physical X4 Pro panel latency.

The updated activity produced 1 opening presentation, 1 safe dig presentation,
0 no-op number presentations and 1 held-flag presentation in the same input
fixture. No extra storage or framebuffer allocation was added.

## Home return and shelf entry

BMP fixed headers are read in one 54-byte HAL operation instead of individual
byte operations. Palette and image decoding retain their existing behavior;
short fixed headers fail before their fields are used. The 54-byte scratch is
local, with no new heap owner or image cache. Flat Home cover rows calculate
vertical scaling once per source row; slanted side covers retain their existing
raster. This preserves cover pixels and the display refresh policy.

Apps and Games always enter on the first page with no remembered highlight;
the first new button tap marks the first visible option. Cursor/page browsing
no longer writes the old resume position to SD. Shared input capture, hold
interval, release suppression, touch routing and page geometry are unchanged.

Wallpaper preparation/confirmation uses the gallery's shared pixel font bindings.
Loading indicators have more clearance below the supporting text; processing
cadence and thumbnail preparation are unchanged.

Wallpapers uses the shared Jersey family across its gallery, preparation, messages, confirmations and preview actions. Longer paragraphs use the existing compact Jersey cut. This changes font bindings only, without another image buffer or job.

CrossPlay wallpaper network views and their routes were removed, including Live integration in sleep/wake and Notes. The local gallery retains its thumbnail cache and staged preparation; no replacement background service was added.
