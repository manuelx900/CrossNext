# Reader and library resource reuse

EPUB idle preparation now accepts bounded image-page metadata as well as text.
The existing 24 KiB PageReadBudget charges the PageImage, ImageBlock, temporary
paths and owned path copies before speculative allocation. Path reads validate
lengths, remaining input and positive dimensions. Oversized pages retain the
ordinary foreground path.
Speculative preparation also applies the foreground screen bounds, so malformed
page dimensions cannot trigger an oversized pixel-cache write.

After the page is visible for 400 ms, its successor can be loaded on the existing
render worker. Glyphs are warmed in batches of at most 16 codepoints, followed by
JPEG/PNG pixel-cache preparation using the existing exact-dimension, grayscale
and dithering configuration. The job never presents the screen or writes pixels
to the visible framebuffer. Its image origin includes page position and oriented
margins so absolute Bayer quantization matches foreground decoding. It owns no
new task or framebuffer. A 1500 ms work
deadline limits speculation; new touch contact/release, buttons, remote input or
a newer repaint cancels it. Cancellation is cooperative at ZIP output chunks and
decoder callbacks. It does not claim a hard response bound inside a decoder
between callbacks. Cold GIFs keep their normal foreground path because their
decoder lacks the same cancellation contract.

Cold image preparation additionally requires 128 KiB free internal RAM, a 64 KiB
internal block and 512 KiB free PSRAM before allocating a decoder. Low-memory and
non-PSRAM devices retain the bounded page read but defer image decoding to the
ordinary foreground path.

Extraction failures and cancellation close and remove partial extracted files.
JPEG/PNG cancellation aborts the partial pixel cache, allowing the foreground
loader to retry normally. Completed caches keep their existing on-card format.
The prefetched Page is admitted only when the original book, section, page and
render signature still match. Format changes and section resets release it.

## Image drawing

The X4 Pro can retain three decoded pixel payloads during a page render instead
of repeatedly streaming additional images for each grayscale plane. Each slot
uses at most 96 KiB of PSRAM, with no internal-RAM fallback and 512 KiB of PSRAM
headroom at admission. They are image resources, not another screen framebuffer.
The render-completion guard releases the internal-RAM fallback; bounded PSRAM
slots survive for redraws and idle preparation. Reader exit, child transitions,
resource changes and memory pressure release them. Without PSRAM, the original
single chunked slot remains available; a full or failed cache streams the same
pixel values from SD. The shared 208-byte row scratch for chunk boundaries has
static storage under the same serialized render ownership.

Image quality settings, scaling, antialiasing, waveform selection and physical
refresh policy are unchanged. Direct grayscale and changes to cleanup waveforms
require panel measurements and are not enabled by this optimization.

## Fonts and size changes

The SD font system may retain one previous main TTF size from the same family,
sharing the source files/bytes with the current size. Returning to that size can
reuse its FreeType faces, glyphs and kerning instead of loading the size again.
A new retained size requires at least 1 MiB free PSRAM, 96 KiB free internal RAM,
a 32 KiB internal block, and at most 256 KiB of existing glyph/kerning/prepared-pair
cache capacities on the face being parked. Larger working sets use the original
in-place resize. Cache hits also discard the parked size when it exceeds that
capacity limit or working headroom has fallen. FreeType metadata is additional
to the 256 KiB capacity check and still requires device heap measurements.

Both sizes remain registered with the renderer, so its existing memory-pressure
sinks can release their rebuildable caches. Family/source invalidation removes
the previous size before releasing borrowed sources. The font system owns these
resources across reader activity changes, like its existing active font.

Main reader TTF caches survive transient UI prewarm scopes. Other UI fonts retain
their previous reset behavior. An explicit FontCacheManager reset still clears
all caches; memory-pressure release also retains its original contract. Reader
faces with at least a 128 KiB glyph budget can remember 4096 exact kerning pairs,
rather than 512. Small faces keep 512. The extra pair arrays are bounded at
36 KiB per large face, allocated through checked reserves and included in the
parked-size accounting. Kerning values and text layout calculations are unchanged.

Text adjustments start after the current adjustment touch/key is released, without a fixed 450 ms wait. Held adjustments coalesce. The correct page uses cooperative composition; valid normalized input, metrics and CSS can survive between changes. Preferences and page payloads normally persist after presentation and idle time; cache capacity and callback limits can require earlier I/O. The existing operation trace measures the full request, including lock wait and panel time. See [EPUB formatting work reuse](epub-format-speed.md) for bounds and device checks.

## Library covers

Recently opened uses the resident recent-book records directly, without loading
or scanning the whole index on entry. A retained index snapshot can still be
reused. Other sorts and filters enable the existing PSRAM browsing cache before
resolving index records; its 2 MiB ceiling and allocation-failure fallback remain.
Positive recent-path existence results are reused until the library revision,
path or source/cache removal changes; hidden paths are always checked and missing
results are not cached. Removal invalidates only the presence memoization without
loading/saving recent history or forcing an index rebuild.

A fully drawn cover grid can now reuse the existing three-slot Home art-scene
cache across page changes and activity returns. This adds no new scene arena.
The key includes the visible book paths in order, indexed sizes/timestamps where
available, page geometry, orientation, inversion and XTC cover rotation. The
shared library-content and cover-scene revisions invalidate changes to books or
cache resources. Only complete artwork is retained; pages with missing covers,
fallback labels or index read errors use the normal drawing path. Current touch
targets and selection borders are rebuilt on every presentation. Restoring pixels
already includes their inversion treatment and must not invert the art again.

The one-page activity-owned grid cache remains the first path for repeated
selection changes. Missing cover generation keeps its existing idle scheduling.
The library index, thumbnails and saved preferences keep their formats.

After 80 ms without input, Library also reads existing next-page BMP thumbnails
on the existing render worker through its non-painting idle-work route. Each job
reads at most 4 KiB of thumbnail bytes; touch, held buttons, pending repaints and activity transitions
stop further jobs. This work never extracts EPUB artwork, generates thumbnails,
requests a display refresh. A next page already in
the shared art-scene cache needs no thumbnail prefetch.

The optional next-page arena holds at most 128 KiB in PSRAM and nine complete
thumbnails. It is allocated once on idle, with 512 KiB of PSRAM headroom, and
released on Library exit. Partial files are never served. Page identity,
geometry, orientation, inversion, XTC rotation, library revision and cover-scene
revision guard reuse. BMP parsing, cropping, scaling and polarity compensation
use the same renderer for SD and memory sources. Missing, oversize, truncated
or unavailable data follows the ordinary drawing path.
Speculation uses either the RAM index or resident Recently opened records, so
calculating the next page's identity and paths does not add SD scans to an idle job.

The X4 Pro also stores one optional native monochrome cover raster after the
BMP's declared source bytes. The original BMP prefix is unchanged. The 40-byte
`CNAT001` footer records the geometry/orientation key, lengths and checksums of
the source, native pixels and footer. Validation rejects torn writes, replaced
source pixels and damaged native data. Regenerating the BMP truncates the derived
trailer. Only one current geometry is retained per thumbnail, limiting SD growth
to 16 KiB plus the footer per prepared thumbnail; books and the index format do
not change. Payloads use neutral polarity and tight physical rows, so relocation
and inversion do not require another saved variant.

Native hits copy packed pixels instead of scaling and painting individual runs.
A cold hit still reads and validates source and native bytes; an idle-prefetched
native hit needs no foreground cover reads. A newly rasterized next-page preload
also replaces its raw BMP bytes with native pixels in the same bounded arena,
before that page's first visit, without rereading the source. Preparation uses the existing worker
and render lock: it temporarily paints only the cover rectangle, captures its
pixels and restores the original rectangle and clip without refreshing the panel.
Painting checks cancellation between source rows; subsequent source verification
and writes advance in at most 4 KiB chunks, publishing the footer last. Home can
prepare the first Recent page after 180 ms idle, and Library prepares visible and
next-page covers after 80 ms idle. Explicit batch book preparation also creates
native covers; automatic single-book preparation does not add this work.

Each active owner reuses one optional 32 KiB PSRAM arena, with at least 512 KiB
free headroom and a 256 KiB largest free block before admission. Home releases it
when preparation finishes; Library and batch preparation release it on exit.
Canonical sources above 128 KiB, tiles above 16 KiB, unsupported BMPs, low memory,
restricted clipping and unavailable/corrupt trailers use the ordinary renderer.
No additional task or framebuffer is introduced. First use of an unprepared
thumbnail still requires ordinary painting; a new orientation or tile geometry
also needs preparation. Device timing is required to assess the SD/CPU tradeoff.

When Library closes its index, including when opening a book or a confirmation,
a valid snapshot of at most 512 KiB can transfer into one retained
slot, without copying or retaining its SD handle. Retention requires at least
2 MiB of remaining PSRAM; larger indexes keep the existing per-open 2 MiB cache
and are released on exit. On reentry, the normal open still reads and validates
the index header. Only matching path, complete header and library-content revision
can take back the retained snapshot. Transfers and rebuilds change that revision;
USB-drive mode and card remounts restart the firmware. This bounds persistent
memory use while removing repeated index-body reads on eligible warm entries
and cancelled-confirmation returns. Rebuilds release the retained snapshot before
the builder allocates its working data.

## Resource validity and further reuse

The HAL keeps the general content revision and a separate resource revision.
Saving `/.crosspoint/state.json` or canonical per-book `progress.bin` files still
changes the general revision, but no longer discards unchanged images, ZIP
directories, cover scenes or index snapshots. Settings, unknown paths, renames,
source writes and media changes retain conservative invalidation. Existing
directory checks do not count as resource mutations. The extra revision costs
four bytes; writable handles retain their mutation classification and, for
derived cover tails, a source-byte boundary.

Native thumbnail tails and the disposable Library packs use explicit HAL write
methods. Only reserved paths qualify. Writing or truncating a thumbnail before
its source-byte boundary restores ordinary resource invalidation for that
handle. This avoids invalidating every cover when preparing another cover,
while preserving invalidation for actual BMP changes. These methods retain the
storage mutex; there is no global suppression of other writers.

The desktop simulator keeps zero resource/media revisions, since host files
can change outside its HAL. Its shadow storage API forwards derived writes to
ordinary file operations and conservatively disables these cache reuse paths.

Paragraph layout can remember exact inter-word gap measurements during its
dynamic-programming search. One optional, parser-owned 16 KiB PSRAM array holds
up to 4096 gaps, alongside the existing word cache, with 2 MiB free headroom.
It is reused between paragraphs and released with the parser. Font identity,
style, fallback availability and metric changes guard each value. Larger
paragraphs, unavailable PSRAM and unstable metrics use direct measurement.
Line costs, tie breaks, kerning, rounding, ruby and final positions are unchanged.

Background section composition yields between parser steps after an 8 ms work
slice, in addition to the existing page limit and input cancellation. This is a
cooperative budget: a single large paragraph or finalization step can exceed it.
Foreground composition and the saved layout format remain unchanged.

Idle image work can also load already-converted pixel caches into the existing
three PSRAM slots. Reads advance in cancellable 4 KiB pieces, validate resource
and media revisions before publication, and never paint the framebuffer.
Foreground image reads retain their bulk path. No extra pixel arena is added.

The reader may retain the current deserialized Page graph after drawing it,
avoiding another page-file read and allocation on a redraw. The key includes
the section, page, full render specification and resource/media revisions.
The current graph and speculative next graph share a conservative 64 KiB cap;
the next graph also keeps its 24 KiB cap. Admission retains the established
internal-heap reserves and largest-block checks. Notes and links remain owned
by the reader. No graph copy or second framebuffer is made. Section resets,
pressure, reader exit and child transitions clear retention; heavy child/setup
paths release it under the render lock before allocating their own data.

Complete Library grids can spill from the existing art slots to at most eight
`/.crosspoint/library-grid-<0..7>.pack` files. Each holds at most 48 KiB of exact
art pixels plus a 64-byte header. Descriptors add about 1 KiB to the existing
PSRAM art arena; there is no additional pixel buffer. The existing idle worker
writes at most 4 KiB of payload per step, borrowing an unchanged art slot.
Only a completed header and successful close publish a pack. Geometry, content,
art and media revisions and a payload checksum validate it before drawing.

Pack descriptors are trusted only during the current boot. After reboot, the
normal cover validation path rebuilds them; these files do not promise fast
first use of an unprepared library. A verified foreground pack uses one open
and two reads (header and payload), after the existing black loading notice.
Next-page idle prefetch can instead read the pack in cancellable chunks into
an existing art slot, so subsequent presentation uses RAM. RAM scene hits retain
their immediate path. Missing, partial, corrupt or stale packs use ordinary
cover rendering, with unchanged artwork, selection and hit targets.

## Verification

`python host-tests/reader-next/run.py` executes the production HAL and art-pack
code, plus extracted production composition, lock-release and reader-load paths.
It covers interleaved writers, progress saves, derived-prefix guards, partial
writes, cancellation, stale resources, allocation failure and exact restored
pixels. Its fake locks model ownership; they do not prove real task scheduling.
`python host-tests/reader-io/run.py --only chapter-layout` includes exact layout
parity and bounded Page retention. In the fixed paragraph fixture, gap queries
fall from 1773 to 439 with identical serialized lines. These are host operation
counts, not device speedup percentages.

For the new paths, record `page_reuse_hit`, `page_reuse_miss`, `cover_pack_hit`
and the existing image/SD timing counters from a debug build. On an X4 Pro,
compare progress saves and repeated redraws, font A/B/A changes, large
paragraphs, ruby, notes and link menus; then browse more than three cover grids
with short idle pauses and return to older grids. Compare against rapid swipes
without idle time and a fresh boot. Verify all four orientations, inversion,
antialiasing, same-size file replacement, interrupted writes and low-memory
fallbacks. No stale artwork, changed text layout, extra panel presentation,
lost links, deadlock or watchdog is expected. Measure internal and PSRAM free
bytes, largest blocks and task stack high-water marks across repeated
reader/menu/dictionary/Library transitions. Record input-to-content and settled
refresh P50/P95 on the same SD card and books. Physical latency and any claim
of being faster than stock require these device measurements.

`python host-tests/library-prefetch/run.py` exercises the production prefetch,
BMP reader and cover painter: chunk bounds, partial cancellation, revision and
page rejection, capacity, allocation failure and identical SD/memory pixels.
`test/library_index_file` covers retained index reuse, one-header-read reentry,
changed revisions/headers/paths, low memory and oversize fallback. These checks
measure avoided SD work, not device latency.

`python host-tests/library-native-covers/run.py` checks native pixel equality,
temporary framebuffer restoration, clipping, all orientations and inversion,
source/payload corruption, cancellation and allocation failure. It also checks
that positive recent-path reuse respects revision, path and visibility changes.

For Library A/B hardware measurements, use identical firmware settings, SD card,
books and pre-generated thumbnails. Compare cold entry, ten warm entries and
forward/backward cover pages, both with a short idle pause and rapid swipes.
Record `LIB entry prepare`, `LIST prepare/display` and the shared repaint timing
P50/P95; warm eligible entry should log `reused browse snapshot`, and prefetched
pages should report `prefetched covers`. Check fast touch/back/held buttons during
prefetch, all four orientations, inversion, filters/sorting, collections, library
rebuilds, transfers and memory pressure. No extra panel refresh or wrong/stale
cover is expected. A higher input-to-presentation P95 on the same workload is a
regression to investigate before distribution; no hardware speedup is claimed
from host checks alone.

Run `python host-tests/reader-speed/run.py` after installing the X4 Pro PlatformIO
dependencies. On Windows it uses the repository's Emscripten compiler and Node;
other hosts can select CC/CXX. It executes the production ImageBlock and JPEG/PNG
converters against a host HAL, including pixel equality with cache allocation
disabled, four orientations, three-image repeated passes, invalid metadata,
small budgets, cancellation cleanup and unchanged framebuffers during preparation.
Its test assets come from the existing synthetic EPUB fixtures.

The existing reader-font-cache, font-cache-manager and cover-scene suites cover
cache retention, explicit resets, cancellable glyph batches, repeated kerning,
shared-scene revision invalidation and allocation failure. Browser/host counts
show avoided work, not physical SD or display latency.

Before distribution, test on an X4 Pro with Bluetooth off: cold/warm image pages,
multiple images, fast navigation during preparation, forward/back changes,
opening menus or leaving the reader, font-size A/B/A and a third size, family
changes, large CJK fonts, PSRAM pressure, and valid/truncated cache files. Compare
all orientations, inversion, antialiasing and image quality. In the library,
compare entry and return, previous/next pages, sorting/filtering, cover generation,
rebuilds and same-size resource replacement through file transfer. Selection,
hit targets and current covers must remain correct. Record internal/PSRAM free
space and largest blocks, watchdogs, and input-to-content/settled-refresh P50/P95.
