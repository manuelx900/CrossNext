# Reader startup and next-page prefetch

Recent books, KOReader credentials and OPDS servers load at first singleton
access. The C++ static initialization guard serializes concurrent first access.
First access by a writer loads the existing file before any mutation. Legacy
format resaves keep the existing PersistableStore lock protocol. A missing file
keeps the store defaults; explicit reload and save remain available. Settings,
application state and dictionary defaults retain their existing startup order.

After a page has been visible for 400 ms, the EPUB reader may deserialize its
immediate successor once. It runs only when the section is finalized, no reader
panel or automatic turn is active, and no page-turn button or touch contact is
held. The input loop queues it after a nonblocking render-lock acquisition.
ActivityManager runs it on the existing render task under RenderLock, so SD
delays do not stop input polling. Activity changes and repaint requests cancel
queued work; a running load checks cancellation before retaining its result.
Idle work does not paint, acknowledge a waiting repaint or update reveal state.

The speculative decoder accounts for at most 24 KiB of allocation requests,
including a conservative 32 bytes per allocation for allocator overhead. This
includes Page, element storage, PageLine/TextBlock objects, text arenas, footnotes
and links. Images, ruby, more than 256 elements and over-budget pages skip
prefetch and retain their ordinary foreground decoding. Rejection frees the
partly constructed page and never removes or rewrites a section cache.

Admission requires the budget plus 64 KiB of default-capability free memory,
the budget plus 48 KiB of internal free memory, and 32 KiB contiguous blocks in
both pools. Memory is checked again after loading. Allocation-failure fallback
is the existing foreground loader. The budget describes this optional object
graph, not all renderer/font working memory or a measured device heap limit.

A matching next-page render takes ownership of the prepared Page. It performs
its ordinary glyph prewarm/render exactly once. A miss releases speculation
before loading the requested page. Section reset, layout mismatch, reader
overlay entry, memory pressure and reader exit discard the retained page. An
inactive reader retained on the activity stack may retain its bounded page
until reset, a foreground miss or exit. No framebuffer, persistent cache
format or user setting is added.

## Verification

Compare the same book, card image, font, orientation, brightness and radio
state. Distinguish cold parsing from warm resume and finalized-section turns.
Debug logging includes `Page load: ... prefetch=... time=...` and
`Prefetch ready: ... time=... internal=... max=...`, alongside existing render
phase times. Compare median and tail latency, not an isolated fast turn.

The local-session WASM harness and raw before/after captures are saved in
`qa-artifacts/lazy-start-prefetch-2026-10-24/`. It uses the actual firmware and a
fixed MEMFS card. That directory is ignored by Git and is not a maintained
project test command. Host times are not physical SD, panel BUSY or battery results.

On X4 Pro, record startup-to-first-readable-page and page-turn-trigger-to-completed
refresh for at least 30 turns with a built-in font and an SD TrueType family.
Record whether the configured long-press behavior triggers on press or release.
Include rapid turns, Back, chapter boundaries, portrait/landscape, size changes,
dictionary/menu round trips, close/reopen and low-memory fallback. Expect
identical text and progress, prefetch hits only for eligible successors, and no
repeated attempt after a page is rejected. Record free/largest internal and
PSRAM blocks before/after the session and after reader exit. Compare firmware
processing separately from the controller waveform; no panel timing changed.
