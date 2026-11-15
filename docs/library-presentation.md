# Library entry and battery rendering

In cover mode, Library shows its header, the Recent/Collection/Filter strip and
shared default book placeholders with a loading window before checking the index.
The pager uses the same numeric painter as the finished page, with no gray bar.
Recent entry counts supported formats from the already resident recent-book
store, bounded to thirty entries. A finished-book or metadata filter that
requires SD keeps its existing deferred preparation; the header and notice
appear immediately and the pager appears with the exact total once available.
No gray bar or speculative page total is shown. Deferred pruning can correct
the resident total when a book has been removed outside the firmware.
In list mode, it prepares real row text before the first displayed frame, without
gray text bars or a loading window. The existing render worker prunes missing
recent entries, opens or rebuilds the index, resolves pinned books and filters
the list. No task is added.
A held launch Confirm release is consumed while this work is pending.

A cover page first checks its existing local grid cache and retained complete
Library scene. It also copies valid prepared native covers directly from the
existing prefetch arena. When a cover still needs SD access or BMP decoding,
it draws placeholders for those slots and **Loading covers...**.
Only after that frame has been presented does it schedule the unchanged native
or BMP drawing path. Missing thumbnails use the existing generation queue; the
window stays up until all visible slots have either loaded or been attempted.
A fully prepared native page completes in one grid frame without the cover
notice. Mixed pages reuse their ready native tiles while deferring other work.
Actual books without a cover keep their normal title/icon placeholder. Lists
and empty grids never start the cover-loading stage. New page, sort, orientation,
inversion or library/art revision invalidates the page state.

The notice is passive: Back/Home and navigation remain available; book
activation waits for the final grid. The existing parser/thumbnail generation
calls are synchronous under the render lock, so cancelling during an individual
uncached decode can still wait for that call to return. Entry feedback avoids
waiting for the index and covers; it cannot shorten the panel waveform itself.
Cold cover entries first present this shell without opening a cover file or inventing
titles. Known resident counts supply default covers and the final numeric pager;
unknown counts show the header and notice until deferred metadata is ready.
A selected collection uses its resident catalogue count in cover mode, or
prepares actual rows before painting in list mode. Automatic cold-entry
indexing keeps this provisional body instead of replacing it with a full-screen
loading popup. Explicit index rebuilds retain their progress message.
A retained, validated index can instead
supply the final first frame without SD reads when storage, media, library revision
and metadata/visibility settings still match. Finished-book filtering keeps the
ordinary deferred path because it can read SD. Resident complete cover scenes
and prepared native tiles bypass the cover notice; pending covers still show it.
Dropping a snapshot-only index first reopens and checks its backing file.

There are no new pixel buffers, arenas or worker tasks. The entry and cover
states belong to the activity; the render worker owns transitions and HAL I/O.
Status icons share the original Framed/Carousel painter, with the same 30x19
pixels and charge bolt. The reader footer scales it to its existing small slot.
FreeInkUI owns positioning/labels and accepts an optional icon painter; other
SDK clients and battery-bar styles retain their default path.

## Verification

`python host-tests/library-first-frame/render.py` verifies frame/I/O ordering
using production methods with host adapters and renders the actual components.
Use `--baseline` for exact old/new final-grid byte comparison. The native cover
and prefetch suites verify the real bitmap/pixel paths independently.

On X4 Pro, test each orientation and day/night mode with:

- List view and an empty shelf: the first frame contains the Library header
  together with the actual rows or empty state; no gray bars or cover notice.
- Cover grid with existing thumbnails: the window appears before cold SD cover
  loading, then disappears once the grid is ready, without changed detail,
  crop, dithering or contrast.
- A fresh EPUB/XTC and a book with no cover: generation ends normally; no-cover
  books keep their placeholder and the window must disappear after attempts.
- Library → Home → Library and multiple pages: retained scenes require no cover
  reads; page/sort/orientation changes must never show an old page as final.
- Back/Home, page turns and Confirm during loading: navigation stays coherent;
  no hidden book activation or stale cover publication. A single decoder call
  may finish before the pending navigation is applied.
- Files, all Home layouts, the top sheet and reader footer at 0/50/100 percent,
  USB charging, hidden percentage, large UI and both orientations: the shared
  icon/bolt fits its slot and labels/clock do not overlap.

Development logs separate `LIB entry first frame=... ms` (including panel time),
`LIB entry prepare=... ms` (index preparation) and existing `PERF ... panel/ours`
repaint logs. Compare multiple warm and cold runs on the same card/books.
Watch heap/PSRAM and render-task stack headroom across repeated entry/exit.
A persistent window, shifted/cropped battery, stale scene, changed book pixels,
crash or falling free memory is a failure. Do not clear all `.crosspoint`, since
it also holds settings and reading state.

Library loading notices use the same popup chrome painter as the shared loading
window: a black body, white frame and white text, with square pixel-UI geometry
for Carousel 2 / Shelf. A thin black outside edge keeps the white frame visible
on white cards. The notice remains centered inside the available Library body;
header, tabs and pager stay outside it. The existing body font, translated text,
paint count and deferred-cover ownership are unchanged.


## Related appearance controls

Appearance places the four action boxes immediately below the theme previews in
portrait, or at the top of the right column in landscape. The redundant Home
theme heading is omitted. Interface theme, recent books, interface font and the
cover loading notice use equal, larger touch targets. The latter still controls
Home notices per theme; Library follows its own cover/list view.

In the reader, the selected regular or bold Aa sample uses a rounded outline
with the light Carousel 2 / Bookshelf interface, matching its other controls.
Framed / Carousel retain their square outline. The sample font, text rendering
and selection behavior are unchanged.

## Immediate list text and stable cover fallback

Library list entry and a collection in list mode load their row data before the
first display request completes. They show actual two-line titles and file-format
values, with the same file-type icons, without gray skeletons or a loading notice.
A cold library may still spend time opening its index; this is not a measured
physical-device latency guarantee.

Cover views retain deferred preparation. Unknown cover candidates keep the
existing default cover design, with its book symbol and black lower area, until
their image is ready or absence is established. Its outer border is one pixel.
The named fallback is used only for books without usable artwork. The bitmap
decode, scaling, grayscale and dithering paths are unchanged. Lists allocate no
extra row vectors: existing secondary strings carry the format value.

Collection entry uses the count already resident in the catalogue to draw only
the corresponding visible default covers and the actual page total. When an entry count is not yet known, the
header, tabs and notice appear first; no full page of invented books is drawn.
Metadata preparation then supplies the exact visible set. The default cover and
the final image occupy the same card; no intermediate named cover or thicker
loading border is introduced. The final image inset is unchanged.
This adds no allocation or SD read to the first collection frame. Catalogue
counts may be corrected when membership is loaded after an interrupted save.

The collection catalogue has three columns and two rows in portrait, and three
columns and one row in landscape. Book grids, image dimensions and thumbnail
cache keys are unchanged. The original tab height and text fonts are retained.
A book icon with a numeric count shares the bottom row with the options button;
the options touch area remains 48 pixels wide. List-mode counts keep their
translated label.
Collection cards and rows use the shared three-book collection icon; actual
filesystem folders retain their folder icon.

Home Settings uses the same gear artwork as Carousel 2, including the Framed
and Carousel menus and the Bookshelf drawer. Upright UI masks and the legacy
renderer's rotated mask are generated from the same local SVG.

On hardware, open collections with zero, one, three and more than nine books,
then compare cold and repeated entry with covers enabled and disabled. Expect
one default cover per visible book and a numeric pager from the first counted
frame, then artwork or a named no-cover fallback.
Check both orientations, page navigation, the collection options button and all
four Home layouts. Cover detail must match the preceding build; timing and
E-Ink appearance still require physical-device testing.
