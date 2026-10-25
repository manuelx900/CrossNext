# Chapter index write batching

`Section::commitBuildFile` sends its page offsets, anchors, paragraph/list-item
indexes, visible-text offsets, partial watermark and header through the existing
`PageCacheWriter`. Its one 4 KiB PSRAM buffer remains owned by `BuildContext`.
No additional allocation, task, file format or persistent setting is added.
Allocation failure retains direct writes and the same error latch.

The writer exposes `drain()` for both completed pages and metadata boundaries.
Each table drains before the next physical file position is recorded. Pending
bytes also drain before either header seek; the valid version byte is written
and checked last. Every drain, seek and reported file-close failure cancels the temporary section.
The build's cache-write failure latch prevents suspension from retrying a failed
commit. Finalization uses the same cleanup owner to restore the older partial's
page count and readability when its file remains intact.

FAT still requires removing the old section before renaming the new one.
A failed old-file removal now stops replacement. If rename fails after removal,
the old file is gone and no pages are advertised; reopening must rebuild.
A power cut between removal and rename has the same rebuild requirement.
This change does not make replacement atomic. HAL close reports its sync result and is checked before removal; the separate
flush API is void. Physical power-loss durability still needs device testing.

## Host measurements

The base includes lazy startup, next-page prefetch, the chapter event cache and
page-body write batching. The isolated checkout from the prior task is reused;
its previous reviewed sources are preserved under that task's `final-source/`.
Current task scripts, baseline files and results are under
`qa-artifacts/section-index-batching-2026-10-25/`; `candidate-path.txt` identifies
the actual checkout. Root source files are updated only after validation.

Uninstrumented timings use the real EPUB/Section sources in Emscripten/Node
MEMFS, across eight viewport/spacing/bold settings. Three alternating runs
provide 45 warm samples per workload and version; each run's first sample is
excluded. Instrumented commit profiling is a separate run.

| Workload | Whole chapter before / after | First page before / after |
| --- | --- | --- |
| Alice, spine 1 | 14.844 / 14.533 ms | 9.039 / 11.087 ms |
| 80 KiB mixed CSS/RTL/ruby/table/link fixture | 76.056 / 48.118 ms | 5.945 / 6.813 ms |

The complex chapter uses about 36.7% less total host time. The short chapter
shows no clear total improvement. This optimization acts at finalization, so
there is no established first-page benefit; the first-page differences above
must not be attributed to index batching.

For the fixture's unchanged variant 0 (69 pages), commit writes fall from 1,008
to 7 for the same 3,155 bytes, and the profiled commit takes 27.255 / 1.471 ms.
Alice's commit writes fall from 57 to 7 for the same 265 bytes (2.243 / 0.895 ms).
These are representative instrumented samples, separate from the table's warm
medians. MEMFS write-call overhead differs from SD/SdFat and mutex costs.
Neither timing percentages nor HAL call counts prove a physical-device gain.

## Checks and device comparison

The focused writer suite uses C++20 and undefined-behavior checks through
Emscripten/Node on this Windows host. Full-builder comparisons cover 16 text
variants and 16 variants with an image/horizontal rule, including effective
contents and every section lookup table. Only pre-existing unused bytes after
NUL in fixed note/link fields are normalized for comparison. Two additional
large-anchor variants exercise tables exceeding the 4 KiB workspace.

Fault injection covers early/final-page writes, each of the seven commit
writes, both header seeks, file-close failure, direct fallback, and partial-cache suspension.
Before-replacement failures must return failure, remove the temporary file,
and preserve the old partial's exact bytes, count and actual page readability.
Removal/rename faults verify their distinct recovery behavior. Event-cache
corruption fallback, built-in 12/18-point fonts, TXT and Markdown remain covered.

For X4 Pro, use the supplied before/after diagnostic images with identical
`Index commit: pages=... partial=... time=...ms` probes. This duration includes
index/header writes, close/sync, old-file removal and rename; it excludes
logging the result. Both normal release targets omit the probe.

Use the supplied single-spine fixture, retaining HTML/event caches. Alternate
two font sizes to invalidate page layout, then use the existing reader's jump
to percentage action (e.g. 50%). That action requests a complete section build;
merely opening a chapter or waiting does not, because incremental work can
stop at the ahead-of-reader window. Repeat ten times per version with matched
settings and collect the `Index commit` values for `partial=0`. Compare medians
for the same page count. Record input-to-display video separately. The existing
EHP parse-time log precedes this commit and cannot measure the improvement.

Exercise all four orientations, links/footnotes, TOC and paragraph jumps,
images, ruby, tables, an SD font, TXT and Markdown. Exit a long chapter early
and reopen: completed partial pages must remain readable. Repeat open/close
and font changes twenty times while watching free internal/PSRAM bytes and
largest blocks. The optimization adds no buffer beyond the existing 4 KiB.
Missing pages, changed positions, growing memory loss or resets fail the check.
Device timing, fragmentation, power and real SD fault behavior remain unverified.
