# EPUB chapter event cache

After a chapter has been parsed completely, reflow can read its raw Expat
callbacks from SD instead of tokenizing the cached XHTML again. Text layout,
CSS resolution, images, hyphenation and page serialization still use the
existing engine. Font, size, spacing and viewport changes reuse the same
events. Ordinary page turns use the existing page cache and next-page prefetch.
There is no new user setting.

## Ownership and limits

`Section` supplies the book/spine cache path. `ChapterHtmlSlimParser` owns the
optional cache for its parse lifetime and dispatches replay through its
existing semantic callbacks. `ChapterEventCache` owns two reusable 4 KiB
workspaces in one PSRAM-only allocation, its file and temporary writer lease.
The pointer table is bounded to 32 attribute pairs. No chapter-sized RAM buffer,
new task or second framebuffer is added. Allocation failure uses XHTML; the
workspace is freed on finish, suspension, error or destruction. Cache-path
strings are cold setup data, not per-event allocations.

Only complete successful parses publish a stream. A suspended large chapter
can keep its existing partial page cache, but its unfinished event cache is
discarded. This means the optimization may never apply to a giant chapter
that the reader never finishes building. Invalid/stale headers fall back
immediately. A bad checked block makes Section preserve completed partial
pages and restart from XHTML with replay disabled for that attempt. There is
no retry loop over the same damaged stream.

Event files live in `/.crosspoint/epub_<hash>/events-v1/`. Layout-only cache
clearing preserves them. Clearing the book's full cache removes them with its
cached XHTML. Back up progress before clearing a book's full cache; never
delete all of `/.crosspoint/` merely to benchmark a chapter. Size and FAT
modification time detect ordinary source changes; a deliberately edited XHTML
with identical size and timestamp requires removing its event cache manually.
See [file format](file-formats.md#events-v1spineevc).

## Measurements and tradeoff

On this Windows host, the actual firmware parser/Section code was compiled to
WASM with the same flags and fixed SD input before/after. Three alternating
runs used eight render specs twice per book (portrait/landscape, character
spacing, normal/bold); the first build per run is excluded from warm medians.
These are host measurements, not X4 Pro stopwatch or energy measurements.

| Workload | First page before / after | Whole chapter before / after |
| --- | --- | --- |
| Alice, spine 1 | 27.58 / 22.04 ms (20.1% less) | 63.83 / 63.33 ms |
| 80 KiB mixed CSS/RTL/ruby/table/link fixture | 27.20 / 23.43 ms (13.8% less) | 1242.98 / 1252.06 ms |

There is no clear whole-chapter throughput improvement. The 20% target is
reached only for Alice's first-page availability, not generally. XML work and
buffer boundaries are reduced on the critical first-page path; word layout
and page writes remain. First-time recording adds SD writes (4,890 bytes for
Alice; 92,386 for the fixture). Do not expect a different e-ink refresh time or
a dramatic visible change from a few saved milliseconds. This small benefit
has to justify maintaining the persistent event format and fallback path.

Validation artifacts and scripts are in
`qa-artifacts/epub-event-cache-2026-10-25/`. Sixteen full section outputs match
after normalizing only unused bytes after NUL in existing fixed link/note
fields; all effective text/style/link data, page positions and lookup tables
match. Corruption and suspend/resume checks use the real incremental builder.
The standalone cache suite is `host-tests/epub-event-cache/run.sh` (C++20 host
compiler); this Windows environment also runs the same production cache source
and tests through Emscripten/Node with undefined-behavior checks.

## X4 Pro device check

1. Back up SD state/progress. Keep the prior diagnostic firmware for an A/B
   comparison. Use the same book, SD card and reading settings for both builds.
2. Open a chapter and let its background build finish. With the new diagnostic
   build and debug logging, `EVC ... Committed` confirms recording; an early
   exit intentionally leaves no complete event file.
3. Change font size or orientation, then return to the reader. Expect
   `EVC ... Replay` and identical text, links and reading position. Repeat at
   least ten times across all four orientations and a built-in plus an SD font.
   Compare the interval from cached-HTML rebuild start to `ERS ... Rendered
   page`, and retain the complete logs. Full-chapter parse logs are a separate
   measure and are not expected to show the same improvement.
4. Compare a recording of the same input/first-page display for both versions.
   E-ink and menu delays may obscure the small host CPU saving. Treat no
   repeatable device gain as a reason to reconsider enabling this cache.
5. After a book is closed, rename only its `.evc` file or truncate a backed-up
   copy. Reopen/reflow: XHTML must rebuild without missing text or a restart;
   a new complete event cache can replace the damaged one. Restore the backup
   if needed. Never modify an open cache file on the device.
6. Leave a long chapter before completion and reopen it. Previously completed
   pages must stay readable while layout catches up. Check TXT/Markdown reading,
   a table, ruby, an image and footnote navigation as well.
7. Repeat book open/close and font changes twenty times. Record internal/PSRAM
   free bytes and largest blocks in the existing memory diagnostics. Watch for
   progressive loss, OOM, watchdog resets, missing pages or incorrect progress.

Hardware speed, heap/fragmentation, power and long-run SD behavior remain for
the device tester to verify. Neither host tests nor a successful build proves
those properties.
