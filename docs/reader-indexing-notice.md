# Indexing over the reading page

When a chapter needs a blocking layout build, the EPUB reader draws the existing
localized indexing popup over the current page **before** lending its framebuffer
to the inflater. E-ink retains that page and popup while the build reuses the
buffer. The finished page replaces them through the ordinary reader refresh.

No second framebuffer, snapshot, working buffer, task or persistent setting is
added. The existing in-place loan and its memory budget are unchanged. The reader
also checks the loan generation: it never refreshes the indexing popup from page
bytes that have been lent and returned white. A stale buffer, or a book that has
not yet presented its first page, keeps the existing panel image until the page
is ready. Initial metadata loading and optional book preparation retain their
opening progress UI; there is no reading page to overlay at that point.

The notice is now presented before a blocking chapter build rather than waiting
for a one-second deadline. Consequently even a quick build without a usable
layout cache may briefly show it. Complete cache hits and partial caches that
sufficiently cover the destination (including the existing page-number safety
margin) skip the build and the notice. Background indexing
continues without a popup. Parsing, layout caches and saved positions are unchanged.

## Verification

Run `host-tests/reader-indexing/run.py`. It extracts the production reader build
branches, section reset, child-screen invalidation, popup guard and framebuffer-loan
lifecycle into a fixture that overwrites
the loaned page and returns it white. It checks the actual presented bytes during
chapter turns, percent/anchor/text-offset jumps, image probes, cache hits, partial
extension, first-page/stale-buffer handling, failed page-load retries and build
failures. Running the same
fixture against the pre-change source reproduces a popup presented over white.
`host-tests/reader-next/run.py --only build-slices` covers incremental build pacing.
These are host checks, not panel or hardware timing measurements.

On X4 Pro, use a separate SD copy or an unprepared book with uncached chapters;
do not delete all of `/.crosspoint/`, which includes settings and reading state.
Read to a chapter boundary, jump through contents and percent, and repeat with
images, a large chapter, night mode and both orientations. Expect the last page
to remain behind the indexing box until the destination page appears, with the
same text and position. Revisit cached chapters: they should turn normally.
First opening may retain its opening progress screen until there is a page.
A white intermediate screen, a stuck box, lost progress or a reset is a failure.
Repeat with Wi-Fi and Bluetooth use before and after reading; inspect internal
heap, PSRAM and largest free blocks over repeated transitions. No additional RAM
allocation is introduced by this change.
