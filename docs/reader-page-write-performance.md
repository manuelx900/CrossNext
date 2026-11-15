# EPUB page-cache write batching

Chapter builds collect small serialized fields in one reusable 4 KiB PSRAM
buffer. Page, element and block serializers accept Arduino `Print`; reads
still use `HalFile`. `PageCacheWriter` forwards every physical file operation
through `HalFile`, preserving storage locking. This reduces HAL calls and
mutex acquisitions; it does not imply that each old call wrote an SD sector.
SdFat already has its own sector cache.

`Section::BuildContext` owns the writer for one incremental build. The buffer
is too large for the task stack; a static buffer would reserve memory outside
builds and introduce shared ownership. `HalMemory::allocatePsram` never falls
back to internal RAM. Failed allocation keeps direct writes, including the
same write-failure latch. No page-sized allocation, extra task, framebuffer,
setting, or persistent cache format is introduced. Existing section version 57
and event caches remain compatible.

Every page drains before its count/offset becomes available to the reader.
Final lookup tables and header patches now reuse this same buffer; see
[index writes](reader-index-write-performance.md).
Destruction discards buffered bytes instead of writing during cleanup. A short
write or serialization failure stops the build before finalization, removes
its temporary section, and preserves any older partial cache. This may discard
completed work from that failed attempt; reopening can rebuild it.

## Measurements and checks

These figures isolate page-body batching before the later index-write change.

Validation scripts and raw results live in
`qa-artifacts/page-write-batching-2026-10-25/`. The isolated candidate starts
from the delivered chapter-event-cache implementation. Host timings use the
real EPUB/Section/layout/serialization sources with Emscripten and Node MEMFS.
They exclude display rendering, zip loading outside section setup and user
input; they are not measurements of X4 Pro SD throughput or e-ink latency.
Instrumented profiling and uninstrumented before/after runs are separate.

The fixed workloads are Alice spine 1 and an 80 KiB mixed CSS/RTL/ruby/table/
link fixture, each across eight viewport/spacing/bold configurations. Warm
medians exclude the first sample in each of three alternating runs (45 samples
per workload and version). Results (host milliseconds):

| Workload | First page before / after | Whole chapter before / after |
| --- | --- | --- |
| Alice | 21.351 / 7.728 (63.8% less) | 63.109 / 12.814 (79.7% less) |
| Mixed fixture | 24.236 / 4.135 (82.9% less) | 1247.900 / 72.807 (94.2% less) |

These large host gains include the cost of each MEMFS/proxied write call.
Native SD and storage-mutex costs differ. The profile's unchanged variant 0
writes 14,080 bytes for Alice and 239,676 for the fixture. HAL call counts fall
from 1,955 to 8 and from 46,754 to 69 respectively, with zero-size calls
eliminated. The serialized byte counts are unchanged. Timing profiler scopes
are QA-only and are absent from the delivered firmware.

The focused C++20 suite is `host-tests/page-write-batching/run.sh`. On this
Windows host the same production writer and tests run through Emscripten/Node
with undefined-behavior checks. Checks cover page-boundary visibility, mixed
write lengths, one allocation per build, release/discard, short writes during
full-buffer and final drains, and direct fallback. Full-builder checks compare
effective page contents and lookup tables (16 text variants plus 16 image/rule
variants), exercise partial suspend/resume, and inject short writes on early
and final pages while an older partial exists. Existing unused bytes
after NUL in fixed link/footnote fields are normalized only for comparison.

## Device comparison

1. Back up the SD card and use the supplied before/after diagnostic builds on
   the same X4 Pro, card, books, fonts and settings. Keep complete serial logs.
2. Open a long chapter and let layout finish. Alternate two font sizes or
   orientations to force reflow while retaining the HTML/event caches. Check
   `EVC ... Replay` in both builds so the comparison uses the same parse path.
3. Repeat the same sequence at least ten times per build. Compare the interval
   from `EVC ... Replay` to the first `SCT ... Page 0 processed` message,
   and the existing `EHP ... Time to parse and build pages (events)` log
   separately. That parser log excludes trailing-page finalization. Background
   pauses and serial logging also affect this measure;
   do not mix uninterrupted builds with sessions spent reading a page.
4. Record the same input and first visible page on video. Expect less waiting
   while pages are generated or settings trigger reflow. Ordinary turns through
   an already-built chapter and the panel refresh waveform are unaffected.
5. Check all four orientations, a built-in and SD font, images, ruby, tables,
   links, footnotes, TXT and Markdown. Text, pagination and saved position must
   agree. Leave a long chapter early, reopen it, and navigate its partial pages.
6. Repeat open/close and font changes twenty times. Record internal/PSRAM free
   bytes and largest blocks with the existing memory diagnostics. The extra
   4 KiB belongs only to an active build and must be released on completion,
   cancellation and error. Watch for growing memory loss, restarts, missing
   pages or progress jumps. The fallback log indicates unavailable PSRAM and
   means batching is inactive for that build.

Physical speed, fragmentation, power and long-run SD behavior remain unverified
until tested on the device. Host percentages must not be presented as device
speed guarantees. If there is no repeatable device improvement, reconsider the
copying and maintenance cost of this buffer.


## Sequential reads and bounded reuse

Section owns a reusable 4 KiB PSRAM read buffer, serialized by its read mutex.
Both committed-page reads and reads during a partial build use it. The borrowed
ReadBuffer stream passes through every page/block decoder; direct HalFile entry
points remain available. Allocation failure preserves direct reads. Its logical
cursor is restored before returning, including decode failures. Read-ahead is
bounded to the page LUT extent, so short pages do not fetch adjacent payloads. Cache bytes,
read budgets, pagination and renderer behavior are unchanged.

Existing layout profiles are read in place. Eight fixed in-memory chapter
histories choose a least-recently-used alternate slot on a miss. Cache hits make
no FAT rename or cursor-file write; a miss preserves the primary in an alternate
slot before rebuilding. Existing profile formats and interrupted-swap recovery
remain readable. The histories are process-local and do not survive reboot.

SD-font prewarm coalesces contiguous metadata and bitmap runs. Metadata runs
read directly into the existing mini arena when source and
destination are both contiguous; other records keep single-glyph reads. Bitmap
runs read directly into the bitmap arena. This adds no font scratch allocation.
Sparse glyph sets never read the gaps between runs.

Each modern chapter parser may own an exact 512-slot word-width cache (at most
48 KiB PSRAM, leaving 2 MiB headroom). Keys compare text bytes, font/style
identity, tracking, spacing and
focus emphasis. Long words, fallback-font words, unavailable PSRAM, unstable SD
advance tables and
uninitialized or failed vector styles use ordinary measurement. Vector load
identities prevent reuse across face reloads or recycled object addresses. A
failed glyph disables word memoization for that vector font until reload.

An Epub owns a mutex-protected catalog with at most 128 ZIP entry names/stats
(about 34 KiB PSRAM with 1 MiB headroom), directory details and a scan cursor.
Separate ZipFile objects can reuse it. Archive path, file size and nonzero storage/media revisions
must match. Any storage mutation conservatively invalidates the catalog; it
never bypasses local-header or payload validation or retains an idle SD handle.

The reader retains its existing three bounded PSRAM image slots across redraws.
Exact resource paths, dimensions and nonzero storage/media stamps validate hits;
slots used by the current render are pinned across its planes. Each allocation
holds at most 96 KiB of pixels plus 1,025 path bytes. The 512 KiB free-PSRAM floor,
reader exit, child-activity handoff and explicit memory-pressure release discard
optional images. The child transition calls Activity::prepareForChild under the
existing RenderLock; the reader releases payloads there, after drawing stops. Chapter builds also shed retained images below 2 MiB free PSRAM.
Internal-RAM fallback chunks remain transient. All planes, pixel values,
antialiasing, dithering and display waveforms retain their existing behavior.

Host checks: `host-tests/reader-io/run.py` uses the production read/compose/font/
ZIP/index code; set CROSSNEXT_GTEST to an existing googletest source directory.
Windows uses Emscripten and a Node filesystem mount. Image regressions run with
`host-tests/reader-speed/run.py`. Host I/O counts are not physical SD transaction
counts or device timing. For hardware, compare the same books/fonts/card with
cold and warm caches: input-to-first-frame, input-to-complete-content, p50/p95,
read calls/bytes, internal/PSRAM low-water and repeated back/font/redraw cycles.
Check four orientations, three render planes, ruby, focus, accents, ligatures,
partial builds, truncated caches and memory-pressure fallback. No hardware test
or rank against stock is inferred from host checks.
