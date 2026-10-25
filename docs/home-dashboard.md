# Home dashboard

The list-style Home uses a black date header, a framed Continue Reading card,
and a bordered menu with an inverted selected row. The date is the device's
local calendar date, formatted as `15 OCT 2026`; it is not a fixed build date.
Home polls the clock once per second and requests a repaint only when the date
changes. If the clock is unset, the header shows `-- --- ----`. Set the clock
and timezone in Settings to correct it.

Browse Files, Recent Books, File Transfer, Settings, Games and Apps keep their
existing destinations. The Recent Books row opens the existing library. An
installed plugin or configured OPDS server adds the existing library-service
row. Touch activates rows on release; mapped buttons and vertical swipes move
the selection, Confirm opens it, and Back resumes the latest book. Home starts
on Browse Files unless navigation restores a specific row.
Home tries to acquire the render lock without waiting. If the lock is busy
when Home handles an input frame, it skips that frame.

The card shows one recent book. Without one, it shows the existing translated
empty-state guidance and is not interactive. Portrait places the card above
the menu; landscape places it to the left. Layout uses oriented screen bounds
and bezel insets. The optional Cover Grid home retains its existing design.
ASCII labels use the existing Jersey pixel fonts. When visible labels or book
metadata contain UTF-8, the body uses the standard UI fonts so accents and other
supported characters remain readable. Long labels and metadata wrap or truncate
inside their bounds; titles have at most three lines.
The Carousel theme uses a slightly smaller 18pt Jersey title cut, instead of
the 20pt cut, so more of a long title fits. The author and navigation sizes
are unchanged. Regenerate this cut with
`python tools_local/toybox/gen_home_title_font.py`, then run
`./bin/clang-format-fix -g`; it uses the bundled, openly licensed Jersey source
and leaves all other cuts intact.

`HomeActivity` owns navigation, recent-book loading and the existing regional
cover snapshot. `src/apps_local/home/` owns the layout, date formatting and
FreeInkUI host. Its model, layout, bitmap reader and interaction tables are
fixed-size activity members, kept off the render-task stack. The existing
snapshot allocation caches only the cover art and is released on exit or a
geometry change. Selection outlines are drawn after the snapshot is captured
or restored, since byte alignment can extend it beyond the art. SD decoding
uses `HalStorage`/`HalFile` and the existing thumbnail cache. No new settings, persistent data format, network activity,
framebuffer or background task is introduced. Runtime heap and binary-size
measurements still require a firmware build and device.

## Verification

Run `./host-tests/home/run.sh` with the SDK initialized and a C++17 compiler.
It exercises the actual FreeInkUI builder and hit routing, both screen shapes,
six through eight menu rows, empty recents, bounded UTF-8 labels and date
formatting. `FREEINK_SDK_ROOT` may point to a reference SDK for host checks;
this does not validate the firmware's pinned SDK revision.

On an X4 Pro, verify:

1. Open every menu row with touch, then mapped buttons; return from Games and
   Apps and check the restored selection. Try with and without an OPDS/plugin
   row and without recent books.
2. Resume the card and Back shortcut. Try a long title, accented metadata and
   both orientations. Covers must stay inside their frame when selection moves.
3. Leave Home visible across local midnight. The date should update without
   repeated idle refreshes. Change the timezone and return Home; test an unset
   clock as well.
4. Repeatedly enter and leave Home while logging free heap and the largest
   free block. Check for growth, disappearing cover art or invalid taps after
   a refresh. Build `x4pro`, `sticky` and the simulator through the local check
   script before flashing a candidate.
5. Exercise Cover Grid entry with input while the renderer holds its lock.
   Input handled during that busy period must be skipped rather than queued
   until the refresh finishes. Check normal book/tab taps after the refresh.

Carousel side covers now clip their top/bottom corners with the same integer
column insets as decoded artwork, including uncached placeholders and failed
reads. The former triangle raster skipped its first scanline, leaving a thin
line above the tapered placeholder. The exact mask removes that residue and
avoids two temporary polygon-node allocations per side cover. Pixel scaling,
source image bytes and reader image quality are unchanged.

Charging state is forwarded from the existing shared header status to the quick
settings sheet, which uses the shared battery indicator and its lightning mask.
Home and pixel headers retain enough light fill for their dark bolt to be visible
at low charge; the numeric percentage remains the real value. No additional
power polling, timer or task is introduced. Test plugged/unplugged at low/full
charge and open/close the quick sheet in each orientation.

For the cover fix, generate a fresh thumbnail from a book without a cached
cover, observe the placeholder and completed image in the Carousel, then return
to Home and change books. No detached line should appear above either side cover.
Hardware panel residue and USB detection still require an X4 Pro check. Run
`host-tests/quick-cover/run.sh` for charging-mask and exact-corner regressions.
