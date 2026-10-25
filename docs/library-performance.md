# Library covers and fixed-layout reading controls

The Recent cover grid retains one page of unselected artwork in an activity-owned,
40 KiB PSRAM buffer. Selecting another book restores that page and draws only the
new selection border, so it does not reopen the cover files. Changing the page,
orientation, view, query, sort or catalog invalidates the cached page. The grid
still registers every current book target on each render. No internal-RAM
fallback is used; allocation failure keeps direct drawing available. The same
buffer supplies BMP read-ahead while its cached pixels are invalid. It is released
on exit. This is a cover-region cache, not another display framebuffer.

Missing thumbnails are generated one at a time on the existing render worker,
after 250 ms without navigation. Selection navigation only tries the render
lock and never runs a cover parser. Actions that open, remove, filter or group
books still serialize with the worker to protect index and cache data. Repeated button presses update
the selection while the panel is busy, and pending repaints show the latest
selection instead of replaying each intermediate step. Thumbnail-only jobs do
not present the screen; their finished batch requests one repaint. Navigation
takes priority, cancels queued cover-only work, and changing page replaces the
queue. A queued cover job is discarded if another screen has painted meanwhile.
Pending selection presentations take priority even after the idle delay expires;
cover-only jobs cannot consume a normal repaint waiting behind a slow frame.
Failed covers are attempted once per visible page and retain the file fallback.
Existing cover files and the library index format are unchanged.

In Settings → Controls → Touch, **Invert XTC/XTCH direction** reverses horizontal
page swipes and both edge/regular page tap zones for those two fixed-layout
formats. It defaults to Off, including existing settings files without the key.
The setting uses the normal validated settings persistence. Physical page buttons,
EPUB/text reading, chapter sheets and system gestures retain their current behavior.
The XTC More sheet uses the same 70px row height and label font as Layout for refresh frequency, with its value below to preserve the label size.

On hardware, compare cold/warm library entry, repeated selection, page changes,
view/sort/search changes, catalog rebuild, and backing out during cover preparation.
Covers and strong selection borders must remain correct; use a backed-up SD with
both valid and missing covers. Repeat in all four orientations and open/close the
library while checking internal heap, PSRAM and watchdog logs. For both XTC/XTCH,
check left/right swipes and page-edge/interior taps with the option Off and On,
then reboot to verify persistence. Chapter/menu gestures and physical page keys
must keep their behavior. Browser timings and file-I/O counts are not physical
SD/display timings.

The simulator semaphore shim honors nonblocking and finite mutex timeouts,
and its task notifications coalesce pending repaints, matching the device.
A slow refresh must not stop its input polling either.
Test rapid forward/back bursts and holds during a slow refresh and while covers
are missing: the final book must match the input, with no later cursor movement
after release. Check both the row list and cover grid, including page boundaries.
