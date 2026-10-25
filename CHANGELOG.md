# CrossNext changes

## CrossNext v1.1.0 — 2026-10-25

- Add PET: three companions, reading-based growth, care, four minigames,
  240 decorations and 50 progressively discovered memory types.
- Refresh the album and accessory previews; allow removing furnishings while
  keeping ownership. Fit English/Spanish stage names on one line.
- Store The Intercept artwork on microSD. PET and The Intercept show the same
  missing-content installation guidance as Reading Quest.
- Integrate reader startup/prefetch, EPUB event caching, batched page/index writes,
  idle deadlines, glyph drawing and Home return fixes. No device speed or battery
  figures are claimed without physical measurements.
- Review PET's English/Spanish wording and update the bilingual README with
  the maintainer's PET illustration.
- Prepare X4 Pro application, factory and OTA release files with version 1.1.0.

See [release notes](docs/releases/CrossNext-v1.1.0.md).

## Local release candidate — 2026-10-21

- Replace Words' Anki shortcut and small gear with a full-width Settings bar;
  Anki importing remains in the dedicated Flashcards app.
- Remove the redundant divider before Words' labeled example sentence.
- Prepare high-resolution Anki images at import; open that store and allocate its
  4 KiB read band only after Zoom plus. Normal cards keep their small preview.
- Use the same built-in Latin fonts in Words and Flashcards. Question-side Words
  cards show only the term; answers retain bold Word/Definition/Example labels.
- Smart sizing probes one page before reducing, keeps a readable 12 pt floor for
  ordinary long text and paginates; manual 10 pt and oversized-word fallback remain.
- Queue Counter taps and button presses while rendering, and merge their updates.
- Toggle Calendar, Clock and Anki image rotation between two positions.
- Bold quote title/author metadata; buffer quote scanning and reuse its index.
- Reuse Notes/Tasks page starts, debounce Pomodoro interaction saves, and accelerate
  Habits checksums without changing its verified two-slot persistence.
- Reading Quest resources use a verified index and per-image checksums instead of
  reading the entire artwork pack at startup; history scanning reads eight records
  per SD operation. Install the updated Resources ZIP for the artwork improvement.
- Reserve margins above/below Wallpapers pagination and retain swipes/buttons.
- Build and package this delivery for Xteink X4 Pro only.

- Add separate question/word and content text sizes (10, 12, 14, 16, 18 pt)
  to Flashcards and Words, with Smart sizing enabled by default.
- Label vocabulary word, definition and example clearly, retain full pagination,
  and reuse page positions instead of measuring the complete entry on every turn.
- Make Pomodoro's physical button toggle Start/Pause and simplify timeline arrows.
- Keep Library cover generation off the input path and merge pending selection
  changes before presenting the final cursor position.
- Prepare CrossNext source, device and microSD packages and a populated simulator.

See [the fork summary and upstream credits](docs/crossnext-changes.md) for the
inherited CrossPoint/CrossPlay foundation and other local interface changes.
This candidate still requires physical-device verification before public release.
