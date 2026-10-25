# Interface and navigation update

The release targets Xteink X4 Pro only. The single framebuffer and the existing
render mutex remain in use. No new background task owns application state.

Statistics projects recent books and the first finished-book page during its
initial summary scan. A fixed five-book workspace (1,600 bytes) retains the
finished page; its existing ten-book workspace retains recents. An activity-owned
3,072-byte read-ahead buffer reads eight journal records per storage operation
in either direction. Paging finished books beyond the cached page still scans
the journal. Closing Statistics frees these workspaces; changes made while the
activity is closed are read on its next entry.

Home no longer requests another cover-grid presentation solely because its first
layout established a thumbnail height. Missing thumbnails still produce a loading
state followed by a new picture. The installed-plugin probe stops at the first
runnable plugin, preserving root precedence. Return-to-Home still builds its UI
and reads the current book status rather than retaining a stale global screen.

Shelf, Statistics and Quotes retain physical navigation commands in fixed queues
while the display mutex is busy. State changes and rendering remain serialized.
Wallpaper selection uses a 256-bit activity-owned set; a partial chosen set copies
at most 256 bounded names only when applied. The All path borrows the existing
library list. Conversion is one image per loop with progress between images;
no second framebuffer or full-library image cache is allocated.

Wallpaper thumbnail caches include source metadata and target dimensions. The
preparation marker avoids checking/converting every image on an unchanged entry.
Selection changes require confirmation before replacing the random sleep set.
The app closes its file handles after each conversion; an in-progress single
decode or directory scan must finish before the main loop can accept Back.

## X4 Pro checks before publication

1. Return to Home repeatedly from books, Statistics and both shelves. Expect one
   settled Home screen when thumbnails already exist; verify current progress.
2. Rapidly press the side button in both shelves and Statistics, then hold it.
   Expect retained steps/page changes and no extra short step on release.
3. Open Statistics, change every tab/filter and rotate. Compare recent/finished
   totals, first/last pages and day/month/year projections with existing history.
4. Open a folder of many wallpapers cold. Expect a visible preparation screen and
   counts advancing. Reopen it unchanged: expect cached thumbnails. Add, replace
   or remove a source image and confirm the cache updates. Rotate the gallery.
5. Select across several pages, cancel, apply a subset, then apply All. Suspend
   repeatedly and confirm that only the selected images appear. Test an SD write
   failure: the app should report it and retain the previous usable set.
6. Switch brightness and warmth off/on. Off must read 0%; on restores the saved
   value. Close/reopen and sleep/wake to check persistence and wake preferences.
7. Assign different direct apps to the three Home gestures, restart and test them.
   Rotate Clock, Calendar, books and images twice in either configured direction:
   expect the original orientation after the second rotation.
8. Open an Anki image normally, then Zoom, rotate and use arrows/drag through the
   edges in both configured rotation directions. Turn the device to read the
   image: each arrow must point and pan in the direction it shows. Rotate again
   and check the upright arrows; dragging must still follow the finger.
   Normal cards must retain preview speed; leaving Zoom releases its HD
   band. Test a missing original package and a malformed image.

Serial PERF/STACK logs can separate CPU work from panel-busy time and identify
stack/resource regressions. Physical panel timing, power and peak heap remain
hardware checks; a successful native or WASM test does not establish them.

Home retains the original book typography: 18 pt titles in Carousel, 20 pt titles in Framed, and 14 pt authors in both. Framed progress
is a small outlined rectangle with a centered percentage, drawn after the cover
using the existing framebuffer.

Unapplied wallpaper marks are cleared whenever library discovery rebuilds the filename order. Preparation progress throttles from the end of the preceding blocking paint, so panel time cannot trigger another update by itself. Statistics cached sections become usable only after the summary scan finishes successfully.

When a book has no author (including whitespace-only metadata), its Home title
uses the unused author area in both Framed and Carousel. Font sizes are unchanged;
the progress control and menu keep their existing bounds. Reading and Controls
settings show a clipped preview of the next row or section when it does not fully fit.
That preview does not receive taps or count as a navigable complete row; scrolling
or physical navigation brings it fully into view.

Verify long titles with/without an author in both appearances and orientations.
At the first Reading page, Status bar should peek into view; Controls should
show the continuation of its touchscreen options below. Scroll it fully into view
before activating it. With preexisting dictionary names (including old automatic
flags), reboot and change language: both names must remain. With no assigned
names, installed defaults should fill each eligible role independently.

Clipped Settings rows retain the same four-pixel outer section contour as complete
rows. The contour is clipped at the viewport rather than closing the section
above a continuation. Partial rows remain outside touch and button navigation.
Controls keeps its existing row density and spacing. The continuation of
Invert direction remains in its touchscreen section; no extra Keyboard heading
is brought onto the initial page. Wrapped text still increases row height.
The wallpaper marker continues to show the active suspension wallpaper; paging
alone does not change that meaning or the saved selection.

Home retains one bounded percentage result (under 600 bytes) across activity
recreation. It rereads the saved progress bytes and checks book/cache size and
modification time before reuse; a changed or missing record is never reused.
No EPUB parser, book object or second framebuffer remains resident.

Quick light tiles and slider icons share one dispatcher. Verify repeated taps
on both icons and tiles: light off shows 0% without discarding the previous
brightness; warm light can be enabled while the light is off and restores its
previous warmth. An inactive control must not retain a tap highlight.

Wallpaper All only toggles the eligible thumbnail selection. Done opens the
confirmation; cancelling retains marks, and confirming writes only that set.
Corrupt or truncated bitmaps are excluded from manual and All selection, including
on later cached openings. Failed preparations are retried on the next opening.
Valid BMP dimensions need not equal the built-in wallpaper pack dimensions.
Normal gallery mode has no selection instruction; entering selection moves the
same-size thumbnails down to make room for the shared pixel-font instruction.
