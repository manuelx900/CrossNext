# Configurable reading bars

Open Settings > Reading > Customise status bar. Top reading bar and Bottom
reading bar each have three left slots, one centre slot and three right slots.
Choose None, Clock, Battery, Book time left, Chapter time left, chapter pages,
book percentage, book title or chapter title. Each bar can be hidden without
losing its slots, and has an independent book/chapter progress line and thickness.
Changes save immediately. Back returns from a bar editor to the overview.
Restore previous layout returns to the pre-existing status-bar settings.
The web interface's legacy status-bar controls edit that previous layout;
customized bars are edited on the device.

Existing installations retain their original renderer and layout until a bar is
changed. Personalizing either bar copies the previous bottom-bar choices into
the new editor and starts with an empty top bar. Hidden bars reserve no space.
EPUB, TXT and Markdown reflow around the visible bars using the existing viewport
cache validation. Book preparation uses the same dimensions. The reader keeps
its existing content-position handling when it rebuilds a layout.

Time left is an estimate learned from two qualifying forward page turns in the
current reader session. Until then it shows --. Samples shorter than two seconds,
longer than five minutes, backwards turns and jumps over five percent of the book
are excluded. Menus and sleep break the timing anchor. Repaints of the same page
do not train the estimate. Chapter estimates use the current EPUB spine section;
the publisher's table of contents may group sections differently. The pace is
kept only while this reader activity is alive; reopening a book learns again.
No historical statistics, saved positions or book files are modified by this
estimator. Estimates based on an unfinished chapter's page count can change
while indexing continues.

The XTC option hides or shows the configured bars on monochrome XTC/XTCH pages.
Their top and bottom placements come from the bar editor. Once personalized,
use this editor rather than the old XTC placement/percentage controls. Grayscale XTC/XTCH keeps
its existing image presentation. A chapter estimate is unavailable when an XTC
book has no chapter metadata. Auto Page Turn continues to show its active status
in the bottom bar while enabled. A clock without an available RTC shows --.

The design takes CrossInk's independent seven-slot reader bars as a reference:
https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/util/ReaderStatusBarConfig.h
The CrossNext renderer and pace estimator live in src/apps_local/readerbars.

## Resources and verification

Configuration occupies 21 bytes and the pace state 24 bytes in the host build.
The existing fallible reader activity owns one fixed 504-byte text/width workspace,
reused for both bars. EPUB keeps a 64-byte chapter-title cache; XTC keeps a
64-byte book-title cache, filled on first use only when a Book title slot is
visible. Normal page-local bar drawing and pace updates allocate no heap and do
not access SD. A selected EPUB chapter title refreshes through the existing
metadata API on chapter changes, which can read SD and allocate temporary strings.
It is then reused for the remaining pages in that chapter. No parsing, task,
framebuffer or scheduled repaint is added. Pace updates run only when a visible
bar includes a time-left item and the reading page is uncovered.
Titles are cached on chapter changes; clipping is bounded to a 63-byte UTF-8
prefix. Truncated fields end in three dots. Settings writes happen only when
an option changes. No firmware speed or battery improvement is claimed.

Run host-tests/reader-bars/run.py for malformed-configuration, pace, wraparound
and production-renderer bounds checks. On X4 Pro, test both bars in all four
orientations, long titles, seven populated slots, hidden bars, progress-only
bars, Auto Page Turn and save/reboot. A restored original layout should match the
previous reading surface. Change bar heights and reopen the book: no text should
sit under a bar, and the same passage should remain reachable. Compare page-turn
latency with the original layout and with time estimates enabled using the same
book, font, cached pages and refresh interval. Physical latency, heap headroom,
refresh quality and battery behavior still require device measurements.
