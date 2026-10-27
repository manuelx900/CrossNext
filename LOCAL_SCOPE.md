# CrossNext scope

CrossNext builds on [CrossPlay](https://github.com/ma-r-s/crossplay) and
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
This release targets **Xteink X4 Pro** (ESP32-S3, 16 MB flash, 8 MB PSRAM,
480 × 800 touch display and frontlight). Other inherited board environments
remain in the source tree but are outside this release's support and checks.
See [button handling](docs/buttons.md) for the device input model.

**This file overrides [SCOPE.md](SCOPE.md) where the two disagree.** SCOPE.md is
upstream's document and is kept verbatim so it merges cleanly. It says
interactive apps and games are out of scope. For this fork they are the point.

## What this fork is for

Reading is the device's primary purpose. CrossNext adapts the inherited reader,
navigation and study workflows, and adds focused apps and games. Reader changes
should preserve bounded resource use, save compatibility and clear ownership;
keep changes to shared code small and document their integration seams below.

`src/apps_local/Shelf.cpp` is the list of what is here, and it is the only one.
No total is written down in this file, for the same reason the count of
upstream files we own is not: a number in prose is a claim nobody re-derives,
and this sentence sat at "twenty-one apps, seventeen games" while the shelf
grew past both. The shelf itself never had that problem, because `Folder`
computes its `count` from its own table rather than being told.

## The one rule that keeps this sustainable

Upstream moves fast and we want its work. Every change we make is measured by
how much upstream-owned code it touches.

- **Everything we add lives in new files**: `src/apps_local/`, `host-tests/`,
  `scripts_local/`, `tools_local/`, and our own `docs/`. New files never
  conflict.
- **A countable set of upstream files knows we exist**, and it is counted
  rather than remembered. Part of it is the fork's identity (README, LICENSE,
  templates, workflows); the code seams are in the table below, and none of
  them grows when we add an app. `sync.sh` keeps no copy of the list: it
  computes the conflict set from git, so it cannot go stale the way the
  hand-kept list did (it sat at seven entries while the truth grew past
  twenty). Do not write the total down here either, for the same reason.
- **Before editing any other upstream file, stop and ask whether it can be done
  in `src/apps_local/` instead.** If it genuinely cannot, keep the edit as small
  and as structurally stable as possible, and add it to the table below.

The whole fork is the diff from the newest upstream commit `xteink` already
contains:

```bash
git diff --stat $(git merge-base HEAD crosspoint/develop)..HEAD
```

Keep reading that number. It is written against `merge-base` rather than
against `base` on purpose: `base` is a local branch that has to be
fast-forwarded by hand, it is not pushed to `origin`, and when it lags the
diff quietly reports upstream's own progress as though it were ours.

The language-learning flow also owns three narrow reader seams:
`ActivityResult.h` carries the selected sentence, `ClipSelectionActivity.cpp`
collects its bounded page-window context, and `DictionaryWordSelectActivity` /
`DictionaryDefinitionActivity` offer explicit saving of a successful result to local vocabulary
storage and host its save/undo control. The existing selection toolbar uses
Translate, Define and Save quote, while the dictionary engine remains shared. `EpubReaderActivity` forwards source
metadata and accepts the local Words passage-opening factory. The dictionary
engine and FSRS remain shared; app/store/rendering code lives in
`src/apps_local/words`. See [Words](docs/apps/words.md).

Dictionary presentation adapts StarDict HTML locally in
`src/apps_local/words/DictionaryHtml`: pronunciation metadata is omitted from
these small definition screens and block boundaries remain explicit. The shared
`src/util/Dictionary.{cpp,h}` lookup streams adjacent same-headword index entries
into the existing 64 KiB output budget, including synonym and sampled-index
boundary cases. It does not assume their data offsets are contiguous.

The X4 Pro resource profile retains Noto Serif as the built-in reader family,
the small UI fonts, and English/Spanish hyphenation tables. Its narrow seams are
`src/main.cpp` (font registration), `CrossPointSettings` and the Reader font
selectors (retired built-in family fallback while preserving SD font names),
`UiSliderDialog.h` and `apps_local/wikipedia` (existing font references), and
`lib/Epub/Epub/hyphenation/LanguageRegistry.cpp` (the compiled language set),
`Hyphenator.cpp` (emergency apostrophe-word wrapping without a language table),
and `Section.cpp` (layout-cache version).
See [SD card fonts](docs/sd-card-fonts.md) for user-visible behavior.

### The upstream files we own

The idle-wait optimization owns the main-loop wait policy, HAL input
wake semaphore and `InputManager::nextServiceDelayMs`. Activity deadlines are
optional: unadapted games/screens retain their existing 10/50 ms polling and
three-second CPU policy. Home, EPUB and Pomodoro opt in to bounded interruptible
waits; Pomodoro includes screen/phase/save deadlines. `HomeButtonInput` and
`MappedInputManager::nextActionDelayMs` contribute pending logical Home actions;
their dispatch counts as user activity before navigation. The semaphore is static
internal storage; no task, heap buffer, framebuffer or automatic light-sleep
mode is added. See [idle waits](docs/idle-wait.md) for compatibility and checks.

The startup optimization loads RecentBooksStore, KOReaderCredentialStore
and OpdsServerStore on first singleton access through PersistableStore's guarded
load helper, including access by writers. EPUB idle prefetch retains one finalized
text page within a 24 KiB allocation budget. Page/Section/TextBlock's optional
budget checks precede speculative allocations; images, ruby and oversized pages
use the ordinary loader. EpubReaderActivity owns invalidation and release; no
worker, framebuffer, font prewarm or persistent format is added. Activity and
ActivityManager expose cancellable idle work on the existing render task;
ordinary repaint requests retain their own notification flag and completion.
Input polling stays on the main task while the speculative SD read runs. See
[reader startup and prefetch](docs/reader-prefetch-performance.md).

The EPUB reflow optimization owns `Section` and
`ChapterHtmlSlimParser`'s optional `ChapterEventCache`. It stores raw Expat events
under each book's `events-v1/`, independent of fonts and viewport. One reusable
8 KiB PSRAM-only workspace belongs to each active parser; no whole chapter,
worker or framebuffer is retained. Only complete streams are promoted. Invalid
headers use XHTML immediately; damaged blocks preserve completed partial pages
and retry XHTML once with replay disabled. Standalone sections keep their
existing path. The format/version and its corruption tests are a local
maintenance responsibility. Measured benefit is limited to first-page
availability during reflow; full-chapter throughput has no clear gain in host
measurements. See [chapter event cache](docs/reader-event-cache.md).


The page-cache write optimization owns `PageCacheWriter` and the
write-only Print interfaces of Page/TextBlock/ImageBlock and Serialization.
Each Section build owns one reusable 4 KiB PSRAM-only buffer; allocation failure
keeps direct HAL writes. Completed pages drain before publication. Failed writes
discard the temporary section and preserve older partial pages. Section/event
formats, tasks and framebuffers are unchanged. Chapter indexes and header
patches reuse that buffer, draining before physical offsets/seeks and checking
all reported write/seek/close failures. The FAT removal/rename window still requires
rebuilding after an interruption. See [page-cache writes](docs/reader-page-write-performance.md)
and [index writes](docs/reader-index-write-performance.md).

Reader page rendering opts into bounded TrueType cache retention through
`FontCacheManager::ScopeCache::Reader`; failed glyph faces are discarded for
retry at page boundaries, while valid empty glyphs remain cached. Transient UI
scopes retain their reset contract. Explicit reset, reader exit, size/family reload and memory-pressure
eviction still release or invalidate the vector caches. Home opts into a borrowed
4 KiB BMP read-ahead buffer, held within its existing reusable row allocation.
Packed monochrome rows and local box sampling avoid expanding these cached
images to 2bpp. Three art-region snapshots share a bounded 64,512-byte PSRAM
arena across Home activity lifetimes; source metadata and view identity guard
reuse. No original-image regeneration is added. See
[Home cover rendering](docs/home-cover-rendering.md).


The code seams, each a deliberate, commented edit:

`ActivityManager` consumes an existing deferred repaint when an immediate or
waiting request notifies the render task. This merges the same pending state
instead of displaying it again at the end of the activity loop. Later changes
and blocking progress frames continue to request their own renders. No timer,
extra buffer, task or persistent state is added.
Local file/library completion callbacks, bookmark deletion and menu orientation
changes defer their final repaint to the current loop tail, where the popup or
activity-return repaint is already merged. Blocking progress screens keep their
immediate/waiting requests.

| File                                                                                                                                                                                                                                                                                      | Why                                                                                                                                                                                                                                                                                                                                                                                                                      | Size                                                                      |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------- |
| `lib/Serialization/PersistableStore.h`, `src/RecentBooksStore.h`, `src/OpdsServerStore.h`, `lib/KOReaderSync/KOReaderCredentialStore.h`, `src/main.cpp` | First-access initialization of secondary stores, including writers | one static guard per store |
| `lib/Epub/Epub/{Page,Section}.{h,cpp}`, `lib/Epub/Epub/blocks/TextBlock.{h,cpp}`, `src/activities/reader/EpubReaderActivity.{h,cpp}` | Optional budgeted next-page decode and reader-owned transfer/invalidation | one graph within a 24 KiB request budget |
| `src/activities/Activity.h`, `src/activities/ActivityManager.{h,cpp}` | Cancellable idle work on the existing render task, separate from repaint completion | scalar notification state |
| `freeink-sdk/libs/ui/FreeInkUI/include/FreeInkUICore.h`, `components/controls/button.h`, `FreeInkApp.h`, `src/apps_local/ui/{ToyboxScreen.h,ToyboxTokens.h}`, `src/components/SideButtonNavigation.h` | Semantic visual-order focus, solid contrasting selection, and per-screen side-button routing | bounded interaction-table policy |
| `src/DevMode.{h,cpp}`, `src/activities/network/WifiSelectionActivity.cpp`, `src/activities/settings/SettingsActivity.cpp`, `src/activities/util/FrontlightPanelActivity.{h,cpp}` | Preserve the live Wi-Fi on/off choice when leaving and revisiting management; preserve explicit off when resuming Developer Mode | existing session radio-permission flag; no parent shutdown for management |
| `src/activities/library/LibraryListActivity.{h,cpp}` | Reuse the existing decoded row window for selection-only repaints; invalidate on data/view changes or failed reads | scalar cache identity |
| `src/activities/reader/ReaderToolbarUi.cpp` | Filled style checkboxes with a contrasting tick | one list-style flag |
| `src/activities/util/KeyboardEntryActivity.cpp` | Shared text/search entry uses the existing black pixel header and back target | one header renderer call |
| `lib/GfxRenderer/FontCacheManager.{cpp,h}`, `src/activities/reader/EpubReaderActivity.cpp` | Retain already bounded TrueType glyphs/kerning across reader pages; keep transient/explicit cleanup | scope policy |
| `lib/EpdFont/TtfEpdFont.{cpp,h}` | Retry a face containing failed glyphs at reader page boundaries; retain successful glyphs and valid whitespace | per-face retry flag |
| `lib/GfxRenderer/Bitmap.{cpp,h}` | Batched headers, borrowed row read-ahead and normalized packed 1-bit rows for local Home sampling; other decoders unchanged | borrowed scratch |
| `src/util/BookCacheUtils.{cpp,h}` | Notify retained Home artwork of transfer/cache mutations, including non-book cover files with synthetic FAT dates | one atomic revision hook |
| `lib/hal/HalFrontlight.{h,cpp}`, `lib/hal/HalPowerManager.cpp` | Prepare the light for deep sleep through the existing SDK park/wake contract | thin HAL lifecycle hook |
| `lib/FsHelpers/ContentVisibility.h`, `lib/LibraryIndex/LibraryBuilder.cpp`, `lib/LibraryIndex/LibraryFormat.h`, `src/activities/library/LibraryListActivity.cpp` | Hide root resource folders and reconcile one old index using reserved header byte 0 as the visibility policy version | shared filter and cache marker |
| `src/activities/reader/DictionaryDefinitionActivity.{h,cpp}`, `src/util/DictHtmlPages.cpp` | Shared dictionary chrome, explicit save/undo, arrows/swipes and readable left-aligned definitions | local Words screen helper and reader seam |
| `src/activities/reader/ReaderToolbarUi.cpp` | Bind the Contents prose font in the family picker and selected family label | one font-slot binding |
| `src/activities/home/HomeActivity.{h,cpp}` | Repaint cached carousel steps once; repaint again only after missing artwork or a loading notice changed the frame | existing thumbnail load result |
| `src/util/Dictionary.{cpp,h}` | Bounded same-headword aggregation so separate StarDict senses all reach the existing viewer | one streaming helper; fixed 64-byte index reads |
| `src/CrossPointSettings.{cpp,h}`, `src/SettingsList.h`, `src/activities/settings/LanguageSelectActivity.cpp`, `src/main.cpp` | Persist automatic/manual dictionary ownership; apply installed EN/ES defaults at boot and language selection through `apps_local/words/DictionaryDefaults` | two booleans and narrow hooks |
| `test/library_builder/stubs/HalStorage.h` | Include `<cstdint>` directly so the storage test stub compiles on GCC 15 and newer | 1 include |
| `src/activities/home/HomeActivity.{cpp,h}`                                                                                                                                                                                                                                                | The shelf seam: Games and Apps as rows on Home, plus the `upstreamMenuRows()` count                                                                                                                                                                                                                                                                                                                                      | 4 hooks + 1 method                                                        |
| `src/components/themes/BaseTheme.h`                                                                                                                                                                                                                                                       | Two values appended to the `UIIcon` palette: Games, Apps                                                                                                                                                                                                                                                                                                                                                                 | 1 block, appended                                                         |
| `src/components/themes/lyra/LyraTheme.cpp`                                                                                                                                                                                                                                                | Two cases mapping them to bitmaps                                                                                                                                                                                                                                                                                                                                                                                        | 4 lines                                                                   |
| `src/activities/ActivityManager.cpp`                                                                                                                                                                                                                                                      | `Frontlight.present()` guard on the light-panel gesture (the stack `#ifndef` seam retired: upstream adopted it in 1.6.0rc); plus `noteSurfaceBuilt()` around the render dispatch, so no screen can forget to stamp its play surface                                                                                                                                                                                      | 1 guard + 1 line                                                          |
| `src/MappedInputManager.{h,cpp}`                                                                                                                                                                                                                                                          | `swallowCurrentTouch()`: one-line wrapper over the SDK suppression latch, for apps that time their own holds (Minesweeper's flag)                                                                                                                                                                                                                                                                                        | 1 method                                                                  |
| `src/network/OtaUpdater.cpp`, `ReleaseSources.h`, `FirmwareBoardTag.h` | CrossNext X4 Pro OTA queries only `manuelx900/CrossNext` GitHub Releases and requests the stable application alias `CrossNext-X4PRO-OTA.bin`; existing streamed download and chip/board/slot checks remain | one release source + one exact asset name |
| `lib/hal/HalStorage.{h,cpp}`                                                                                                                                                                                                                                                              | `openFileForAppend()`: `openFileForWrite` carries `O_TRUNC`, so nothing could add to an existing file                                                                                                                                                                                                                                                                                                                    | 1 method                                                                  |
| `lib/GfxRenderer/GlyphBitmap.h` | Group horizontal 1-bit glyph pixels into destination-byte masks in either direction; retain scalar/2-bit paths for other frames | allocation-free, existing framebuffer |
| `lib/GfxRenderer/GfxRenderer.cpp`                                                                                                                                                                                                                                                         | Thick lines thicken across their direction, not always downward: vertical paths drew 1px; plus `paintclock::notePainted()` at the five places pixels reach the panel                                                                                                                                                                                                                                                     | 1 fix + 5 one-liners                                                      |
| `lib/GfxRenderer/PaintClock.h`                                                                                                                                                                                                                                                            | New file. A count of completed paints, and the `RevealGate` latch built on it, so a tap is never routed against a screen the panel has not shown yet; see `src/apps_local/ui/ToyboxScreen.h`. Lives beside the renderer because the renderer is what knows the fact, and is freestanding so the toybox headers stay host-testable                                                                                        | new file                                                                  |
| `src/components/UiAppHost.{h,cpp}`                                                                                                                                                                                                                                                        | The `uiReady` handshake waits for the panel, not just for the rebuild: `resetUi()` arms a `RevealGate` and routing stays closed until one paint lands. Screen entry only, so ordinary repaints still route                                                                                                                                                                                                               | 3 lines + 1 member                                                        |
| `lib/GfxRenderer/RevealedInteractions.h`                                                                                                                                                                                                                                                  | New file. `RevealedInteractions<N>` (the table digest, lifted out of `ToyboxScreen.h` and made capacity-generic so the 17- and 48-slot components can use it) and `SurfaceGate` (the same rule for a board hit-tested against geometry, which never reaches `route()`)                                                                                                                                                   | new file                                                                  |
| `src/activities/Activity.h`                                                                                                                                                                                                                                                               | `surfaceMeaning()` / `surfaceRevealed()` / `noteSurfaceBuilt()` and a `SurfaceGate` member: the opt-in every geometry-hit-tested play surface inherits, so the games that hit-test a play surface do not each grow their own                                                                                                                                                                                             | 3 methods + 1 member                                                      |
| `src/components/OptionPopup.h`                                                                                                                                                                                                                                                            | Holds a `RevealedInteractions` rather than a raw `InteractionBuffer`, and asks the gate BEFORE routing so a suppressed tap cannot fall through to the dismiss-on-outside-tap branch                                                                                                                                                                                                                                      | 3 lines + 1 type                                                          |
| `src/activities/util/KeyboardEntryActivity.{h,cpp}`                                                                                                                                                                                                                                       | A `RevealGate` armed on screen entry, so a tap during the first paint belongs to the screen that pushed the keyboard rather than to a key. Entry only, deliberately: digesting this table would gate every layer change, and shift swaps the whole layout                                                                                                                                                                | 3 lines + 1 member                                                        |
| `lib/PngToBmpConverter/PngToBmpConverter.*`                                                                                                                                                                                                                                               | `...FitWithin()`: contain, not cover, for bounding downloaded images                                                                                                                                                                                                                                                                                                                                                     | 1 method                                                                  |
| `lib/KOReaderSync/KOReaderCredentialStore.cpp`                                                                                                                                                                                                                                            | A comment saying out loud that sync stays on upstream's server, and why                                                                                                                                                                                                                                                                                                                                                  | comment only                                                              |
| `src/components/UITheme.cpp`                                                                                                                                                                                                                                                              | `getScreenSafeArea()` starts from the bezel's viewable insets, so upstream screens stop drawing under the glass; see `docs/bezel-insets.md`                                                                                                                                                                                                                                                                              | 1 block                                                                   |
| `lib/Epub/Epub/Section.{h,cpp}` | A second constructor for a standalone document (an HTML file on the card, a cache directory, the anchors that start fresh pages, no Epub), and `!epub` guards at the six places `startBuild` asked the Epub; the Wikipedia app lays out an article through it. Also makes `Section` constructible with no zip at all | 1 constructor + 6 guards |
| `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.{h,cpp}` | `setCaptureFootnotes(false)`: every internal link was also recorded as a footnote entry (288 bytes a page) for the reader's popup; a document with no popup keeps only its link rectangles | 1 setter + 1 condition |
| `lib/Epub/Epub/Page.h` | `MAX_LINKS_PER_PAGE` 32 to 96: a Wikipedia lead runs one link per five words and a link past the cap silently became plain text. Serialized as a count, so older caches still load | 1 constant |
| `src/main.cpp`, `src/CrossPointSettings.h`, `src/SettingsList.h`, `src/activities/settings/OtaUpdateActivity.cpp`, `src/activities/settings/SdFirmwareUpdateActivity.cpp`, `src/network/HttpDownloader.cpp`, `src/apps_local/bridge/BridgeHttp.cpp`, `src/apps_local/study/StudySync.cpp` | The device report's hooks: `devreport::begin()` after the settings load, one settings field and its row, the install attempt/failure notes on both install screens (the SD one is the OTA one's twin), and `devreport::headersFor()`/`delivered()` around each request in the three transports that reach CrossPlay's own hosts; the module itself is `src/network/DeviceReport*.{h,cpp}`. The device header and event implementation lives in `src/network/DeviceReport*.{h,cpp}`. | 1 call + 1 field + 1 row + the notes on 2 screens + 2 calls per transport |
| `src/activities/boot_sleep/SleepActivity.cpp`, `src/CrossPointSettings.h`, `src/SettingsList.h`, `lib/I18n/translations/english.yaml` | A note on the sleep screen: one `SLEEP_SCREEN_MODE::NOTE` value appended before the count, its label, its string, and one `case` in the sleep screen's dispatch that calls `notes::drawAsleep()` and falls back to the default screen. The module is `src/apps_local/notes/NotesSleep.{h,cpp}`; the Wallpapers core mirrors the enum and static_asserts it | 1 enum value + 1 label + 1 string + 1 case |
| `src/main.cpp` (again)                                                                                                                                                                                                                                                                    | `finishWifiSessionWithoutRestart()`: a touch device ends a WiFi session by tearing the stack down in place rather than rebooting, so the externally powered touch and frontlight rails keep their state. It lives INSIDE upstream's `silentRestartTo()` because upstream folded its three callers into that helper in 1.6.5 -- and the 2026-09-19 sync auto-merged the whole seam away with no conflict marker, because the lines it hung on were the ones upstream deleted. Check this one by name after every sync                                                    | 1 function + 1 guard                                                      |
| `.gitignore`                                                                                                                                                                                                                                                                              | Ignore `qa-artifacts/` and the simulator's SD cards                                                                                                                                                                                                                                                                                                                                                                      | 3 lines, append-only                                                      |
| `platformio.ini`                                                                                                                                                                                                                                                                          | One `extra_configs` line pulling in `platformio.sim.ini`                                                                                                                                                                                                                                                                                                                                                                 | 1 line                                                                    |
| `SCOPE.md`                                                                                                                                                                                                                                                                                | One-line pointer here; it is the file that says "no games"                                                                                                                                                                                                                                                                                                                                                               | 2 lines                                                                   |

The rest are identity, not seams: `README.md`, `LICENSE`, `.github/`
templates, funding and workflows, `.skills/README.md`. They are this fork's
front matter and merge trivially or not at all. `GOVERNANCE.md` and
`ROADMAP.md` are not on that list because the fork does not carry them at all:
c8360519 deleted both rather than owning them, upstream's governance file
having routed harassment reports to a maintainer who never agreed to receive
them.

None of the seams grows when an app is added -- the two theme edits are per
_folder_, and there are two folders. Both are appends: values at the end of an
enum keep every number above them, and a case in a switch merges as an
addition. `HomeActivity.cpp`'s hooks likewise append after upstream's rows, so
their indices never shift.

The navigation-menu presentation also owns these narrow seams. Layout, fonts,
row formatting and framed chrome live in `src/apps_local/ui/PixelMenu*`.

| File | Seam |
| --- | --- |
| `lib/hal/HalStorage.{h,cpp}`, `scripts_local/sim_catchup.py` | Checked directory enumeration for bulk book-position sync; distinguish SD read failure from EOF, with a matching simulator overload |
| `lib/KOReaderSync/KOReaderDocumentId.cpp` | Bulk sync reuses both document identifiers; bounded hash scratch and exact signed reads fail safely on damaged SD data |
| `src/MappedInputManager.{cpp,h}`, `src/activities/ActivityManager.cpp` | bottom-edge reader settings shortcut takes priority over Home only on the active reader |
| `src/activities/Activity.h`, `src/activities/reader/ReaderActivity.h`, `EpubReaderActivity.{cpp,h}`, `XtcReaderActivity.{cpp,h}` | Type-safe settings gesture capability and format-aware entry; existing EPUB repagination and XTC rendering retain ownership |
| `src/CrossPointSettings.{cpp,h}`, `lib/Epub/Epub/ReaderRenderSpec.h`, `Section.cpp`, `parsers/ChapterHtmlSlimParser.{cpp,h}` | saved Regular/Bold reader weight, resolved before measurement; versioned section layout/cache and clipping signature include the weight |
| `src/activities/reader/ReaderToolbarUi.{cpp,h}` | Fork-owned partial-height text/page sheet over the current page, shared UI host/actions and fixed workspace; existing font browser remains unchanged |
| `src/activities/util/FrontlightPanelActivity.{cpp,h}` | Fork-owned QuickSettingsScreen renders the existing control-center actions; HAL/light persistence stays shared; negative tile values close the sheet |
| `src/activities/settings/SettingsActivity.{cpp,h}` | Seven categorized root rows via local SettingsHome, shared pixel header and reusable section rows; existing setting actions and web API categories remain shared |
| `src/activities/home/FileBrowserActivity.{cpp,h}` | Shared pixel menu and lazy display-row formatting; raw filenames, storage operations and navigation stay upstream-owned |
| `src/activities/network/NetworkModeSelectionActivity.{cpp,h}` | Shared pixel transfer menu; existing mode choices and callbacks |
| `src/activities/library/LibraryListActivity.{cpp,h}` | Shared pixel Library header, tabs and row formatting over the existing visible window; index, grouping, metadata and navigation stay upstream-owned |

Three deliberate near-misses, worth knowing so nobody "fixes" them:

- **`platformio.sim.ini`, not `platformio.local.ini`.** Upstream's `.gitignore`
  reserves `*.local*` for personal overrides. The simulator is not personal:
  every checkout needs it. Naming it differently keeps it tracked without
  touching an ignore rule.
- **Screenshots go to `qa-artifacts/`, which costs the one ignore line.** They
  could have gone to `$TMPDIR` for free. They did not, because a directory you
  will actually open beats a number in this table.
- **We never edit `src/activities/apps/`.** It does not exist upstream, and
  recreating CrossPoint's old Apps menu is how the previous base ended up with a
  junk drawer.

## CrossPlay's settings sort below CrossPoint's

A reader looking for a CrossPoint setting must never scroll past one of ours to
reach it. Ours go at the bottom of their category, under every upstream row.

Declaration order is not render order, and that is what made this invisible.
`SettingsList.h` declared the fork's two System rows last, with a comment saying
so, while `SettingsActivity::rebuildSettingsLists()` appended eight upstream
ACTION rows (WiFi, KOReader, OPDS, Clear cache, Check for updates, SD update,
Language, Keyboards) after building that list. Dev Mode rendered 6th of 14,
above WiFi and Language, for as long as those actions had existed. The comment
asserting a position the declaration does not decide was the tell.

`host-tests/settingsorder/` holds the line. It reads the append order out of the
activity rather than the declaration, and decides which rows are ours by asking
`crosspoint/develop` rather than naming them, so it still means something after
the third fork setting is added.

## The shelf

Home gains two rows, **Games** and **Apps**, as siblings. Each opens a folder.
That is the entire hierarchy, and it is capped at two levels by the type system
rather than by discipline.

Read [docs/shelf.md](docs/shelf.md) before adding anything. The short version:

- A folder holds items, never folders.
- Back follows launch origin: a Home shortcut returns Home; a folder launch returns to its folder, and a folder returns Home. The launcher preserves origin across sleep.
- **No app names its own destination.** It calls `shelf::leave()`.

## Deliberate deviations from upstream's rules

All scoped to `src/apps_local/`.

| Upstream rule                                    | What we do in local apps                                  | Why                                                                                                                                                                                                                                                        |
| ------------------------------------------------ | --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| All user-facing text uses `tr()`                 | Raw `const char*` titles and strings                      | Routing through `tr()` means editing `lib/I18n/translations/*.yaml` per app, which is per-app churn in an upstream file                                                                                                                                    |
| Icons are `UIIcon` enum variants added per app   | Shelf items carry a `freeink::Icon` generated from Lucide | An asset the shelf resolves gives us Lucide's 1735 icons instead of an enum that grows per app. The two _folder_ rows are the exception: upstream's Home menu accepts only a `UIIcon`, so Games and Apps are appended to that palette once and never again |
| Apps register via `ActivityManager::goTo<App>()` | A function-pointer factory in the shelf                   | Avoids editing `ActivityManager.{h,cpp}` per app                                                                                                                                                                                                           |
| All rendering through the `GUI`/UITheme macro    | Apps draw their own surface via FreeInkUI                 | A board or a grid is the app's own material; chrome still goes through Toybox, which is a FreeInkUI theme                                                                                                                                                  |

Everything else still applies: the resource protocol, `makeUniqueNoThrow`,
HAL-only access, no hardcoded screen dimensions, free in `onExit()` what you
allocate in `onEnter()`.

## Host tests live in `host-tests/`, never under `src/`

PlatformIO's build filter is `+<*>`, so **every file under `src/` is compiled
into the firmware, on every environment**. A test file with its own `main()`
placed under `src/` links into the firmware and replaces the real one.

So pure-logic modules stay in `src/apps_local/<app>/`, and their host tests live
in `host-tests/<app>/`. Keeping app logic freestanding (no Arduino, no renderer,
no heap) is what makes that split possible, and it is the only way to get real
coverage without a device.

```bash
./scripts_local/check.sh          # every suite, both builds
./scripts_local/check.sh --tests  # suites only, fast
```

**Every suite green is the only green; nothing here is a known failure.** A
suite that is allowed to stay red stops being read, and then so do the ones
beside it.

(Inside a worktree always call `./scripts_local/`; the workspace-root
`./scripts/` symlinks resolve back to the integration tree.)

Each suite builds into a directory keyed to **this checkout**, not just the
suite name. Two worktrees once shared one build dir, and a suite whose source
was not even present reported 52 green checks against the other tree's binary.

The suites are built by CI on Linux as well as here, so they have to compile
under GCC and not only Apple clang. What that costs, and what is still papered
over, is in [docs/open-items.md](docs/open-items.md).

## Branches

- **`base`** is a pure mirror of the upstream branch. Never commit here.
  Fast-forward only.
- **`xteink`** is the integration branch. `base` merges into it, never the
  reverse.

Building `base` gives a clean upstream binary for answering "is this bug mine
or theirs". It is only as good as its last fast-forward: `base` lives on this
machine and not on `origin`, so check it is current before trusting a diff
against it.

```bash
./scripts_local/sync.sh           # report what changed upstream, change nothing
./scripts_local/sync.sh --apply   # merge and verify in a trial worktree, then land
```

### The base branch died, and this fork inherited it

Until 2026-08-14 `base` tracked **`crosspoint/feat-touch-ui`**: the X4 Pro
beta branch, the only place X4 Pro and touch support lived. Upstream deleted
that branch without merging it. Its content is not in `develop`, not in
`master`, not anywhere upstream -- `xteink` is its only living continuation,
and our pushed copy survives as `origin/feat-touch-ui`. The touch layer this
fork's apps sit on is therefore ours to carry now, not a passthrough.

`base` now tracks **`crosspoint/develop`**, the durable trunk, re-founded at
`v1.5.0` (`e00f5958`): the last develop commit fully contained in `xteink`.
That keeps both invariants true -- `base` is always an ancestor of `xteink`,
and `sync.sh`'s behind-count is the truth.

Upstream then **reimplemented** X4 Pro + touch support on
`feat-x4-papermono-support` and landed it in `develop` as one squashed commit
(#2983), released as `1.6.0rc` on 2026-08-17. The foreseen sit-down merge
happened on 2026-08-25: 85 conflicted files, resolved by a simple rule --
upstream's side wins everywhere it reimplemented the inherited touch layer
(reader, settings, boot flow, gestures, dark mode), and the fork's authored
seams were re-applied on top (the table above). Two seams retired in the
process: upstream adopted the `CROSSPOINT_RENDER_TASK_STACK` `#ifndef` and
grew an SDK-level touch-suppression latch that replaced the fork's
swallow machinery (now a one-line wrapper kept for Minesweeper's flag hold).

After that merge `base` fast-forwards along `crosspoint/develop` as before;
the next syncs should be routine again until upstream's next big line.

## Why this base at all

An earlier incarnation of this fork sat on a different CrossPoint fork, picked
because it was the only app-capable one with a desktop simulator. That reason
expired: the simulator is now CrossPoint's own, and the X4 Pro environment
always was. Basing on CrossPoint directly costs nothing we were using and drops
a quarter of a million lines of reading features this fork does not touch.

## Personal Home menu and reading history

The Home registry is owned by `src/apps_local/home/HomeMenu*`. Its upstream seams are `HomeActivity.{h,cpp}`, `ActivityManager.{h,cpp}` (stable menu identifiers/return selection), `CoverGridHomeUi.{h,cpp}` (the same registry in the optional cover-grid presentation), and `SettingsActivity.{h,cpp}` (one System action). It replaces fixed positional row arithmetic; it does not change reader navigation.

`ReaderActivity.cpp` flushes the existing qualified `ReaderSession` into the fork-owned reading journal before plugin delivery; `ReaderSession.{h,cpp}` exposes its qualified page count. The statistics UI, fixed records and SD repository live in `src/apps_local/reading`. No dependency is taken on `REFERENCIA FIRMWARE/`. See [reading statistics](docs/reading-stats.md) for counting rules, storage growth and the hardware checks. The shared Library seam is limited to row fitting and grouped frames.

2048 is registered in the existing shelf and lives in `src/apps_local/game2048`; its MIT notice is kept with the source. See [2048](docs/2048.md). Shared app chrome removes the separate under-header rule while retaining body reservations so game geometry stays stable.


Chess adds its Puzzles entry through the existing start menu. The pushed fork-owned ChessPuzzleActivity preserves the parent match in memory and returns with finish(), while fixed puzzle/core/screen/store units live under src/apps_local/chess/puzzles. No radio or worker task is started. The bundled catalogue is CC0 Lichess data, imported and validated by tools_local/chess/import_puzzles.py; python-chess and zstandard are development tools only. See docs/chess-puzzles.md.


## Interface customization seams

The three saved Home layouts extend the existing fork-owned HomeScreen/HomeDashboardUi and HomeActivity hooks; no theme-specific activity or background animation is added. SettingsList/CrossPointSettings persist the Home layout, while SettingsActivity adds Interface. The Library seam now includes a persisted Recent list/cover-grid toggle and a thirty-entry view cap; its cover generation and one bitmap handle remain activity-owned. Text Settings from a reader uses a local paged family list in the existing sheet, with the existing SD registry and render-locked font loader. The standalone Settings > Reader screen remains available. See docs/interface-layouts.md.

Activity carries an edge-Back policy; Shelf marks Games/folder flows as disabled, and ActivityManager propagates it to pushed children and restores it on active-screen changes. MappedInputManager applies it only to the left-edge gesture, preserving header and physical Back. These are bounded seams for game input ownership rather than per-game gesture copies. Home carousels also reserve horizontal swipes for books. The nine-row shelf cap applies to both folders.

Wordle's local bank/state/store and Pomodoro's saved cycle-stage model are app-owned. Their data versions and migration checks are documented in docs/apps/wordle.md and docs/pomodoro.md. Openly licensed font binaries and license files are bundled for SD; no runtime/build dependency on REFERENCIA CROSSINK is introduced.


The pixel UI also uses `lib/GfxRenderer/FontFallback.h` and the existing
`GfxRenderer::resolveTextFontId` seam to follow at most four fallback edges.
Toybox registers Jersey-to-built-in edges; SD font configuration continues to
own the built-in-to-SD/TTF edges. Batch prewarming skips built-in-only diversions
until it finds an SD-backed string. The four map nodes are reused on entry;
this adds no per-label buffer or image allocation.

The language-learning seam persists one bilingual dictionary folder alongside the existing monolingual folder in CrossPointSettings/SettingsList. Words context format v2 adds learning purpose while reading v1 without resetting history.

### Reader controls profile

The reader controls own the narrow bottom-sheet/More action seams in `EpubReaderActivity`, `EpubReaderMenuActivity.h`, `EpubReaderPercentSelectionActivity` (chapter page or whole-book percent), `XtcReaderActivity` (supported fixed-layout controls) and the existing top-swipe control-center entry in `ActivityManager`. The narrow `Epub::openItemReader` seam supplies unencrypted ZIP access through the resumable HAL-owned `ZipEntryReader`; the power tile queues the existing main-loop sleep protocol. Shared builders and bounded chapter search stay in `src/apps_local/readertext/`; shortcut selection and its UI stay local. `CrossPointSettings` persists the four validated shortcut IDs and `SettingsActivity` links Display to that local editor. See [reader controls](docs/reader-controls.md) for ownership, search limits and device verification.

### Settings and reader reference layouts

`src/apps_local/readertext/ReferenceSlider.h` and `ReaderSavedScreens.h` own the requested selector and saved-passage layouts. Their bounded upstream adapters are `UiSliderDialog.h`, the existing text/percent/bookmark/clipping activities and `TextSettingsPreview.cpp`. The Wi-Fi activity keeps its existing session and dev-mode pause/resume ownership; its `Manage` purpose stays on actual connection details until Back, while connection consumers still receive the existing result. Small lock/signal/check drawing helpers live in `src/apps_local/ui/WifiScreens.h`. Home renaming remains in the existing local settings activity. `Timezones.cpp` appends Punta Arenas without renumbering persisted zone IDs and uses it for an unset, non-migrated zone; explicit existing selections remain intact.

The manual clock adds one narrow `lib/hal/HalClock.{h,cpp}` API to validate local input, write the existing RTC in UTC and update system time. Its UI lives in `src/apps_local/tools/ManualClockActivity`; ClockSettings only links it. KeyboardLayoutSet automatically includes the active English/Spanish layout. No new radio task is added.

### Words styled source text

The Words detail owns a narrow FreeInkUI seam: `freeink-sdk/libs/ui/FreeInkUI/include/FreeInkUICore.h` carries italic in TextStyle, and `FreeInkUIGfxRenderer.h` maps bold/italic to the existing EpdFontFamily variants. Words owns pagination and source paragraphs under `src/apps_local/words`; dictionary capture and FSRS ownership remain unchanged. The SDK capability avoids a second local font registry or copying installed font data.

The uniform book/file lists own one further FreeInkUI list seam:
`ListItem::labelBold` in `include/components/lists/list.h` enables bold for a row
without changing its font size. The same merged text style is used for row
measurement and drawing; default false preserves other lists. Folder styling,
the Noto Sans regular/bold 10pt family, uniform two-line row metrics and
the continuous divided rectangle remain in `src/apps_local/library/LibraryMenu.h`
and the shared Toybox font registry. The book-list family adds a static regular
cut with a one-eighth-pixel outline increase at generation time, reusing the
existing bold folder face. Its font ID and standard UI fallback are registered
once through the existing maps; other regular UI text retains its original cut.
No per-row buffers, task or framebuffer are added.

Direct flashcard media import stays in `src/apps_local/flashcards/FlashMedia.*`:
it writes paired one-bit `images.dat` through HAL, reuses the existing PNG/JPEG
converters, and leaves rendering in StudyActivity/StudyImages. The sole shared
converter seam is `JpegToBmpConverter::jpegFileTo1BitBmpStreamFitWithin`, the
contain-mode counterpart of its existing crop-to-fill API. Source-path strings
are cold import-only allocations bounded by the importer's 255-byte source
path; stack/static lifetime would not satisfy ZipFile's borrowed-string API.
Images are decoded individually during import, never allocated in the render
loop. The additional import-owned 512-byte I/O buffer is reused for media-map
scanning and final pixel copying. The existing display band buffer is reused.
No network task, new setting, or second framebuffer is introduced. Peak device
heap/PSRAM, flash/OTA headroom and import latency require final build/device
checks; host/browser checks do not stand in for hardware measurements.

Flashcard import progress uses a small callback DTO in `FlashProgress.h`.
Catalog/import/media remain synchronous on the activity loop task. Only the
activity owns UI snapshots and acquires RenderLock; expensive I/O/conversion
runs outside that lock so the existing render task can show bounded progress.
There is no new worker/task, persistent state or framebuffer. Seven phase
labels are translated; percentages refer to a known current phase only.

Native flashcard v4 content uses bounded, reusable S3 PSRAM: one 524,320-byte
import workspace, and one 131,104-byte borrowed Note buffer per Study/History
activity. The core reader allocates nothing; v2/v3 keep inline compatibility.
The native card layout owns PHOTO affordance and complete text pagination.
FlashMedia resolves cloze before selecting each face's first image. Browser
presentation is serialized and may run from the existing render worker during
long imports; no native-device task/framebuffer is added. See
[compatibility](docs/apps/study-anki-compatibility.md) and
[stress validation](docs/apps/flashcards-stress-2026-10-19.md).

The Home order extends the existing HomeMenuCore config and the
verified 992-byte preference slots to HMC4, preserving customized HMC1/HMC2/HMC3 menus.
The shared menu allows eight visible entries and defaults to seven, with File
Transfer in third position. Untouched older defaults gain that entry; names,
shortcuts and custom order survive. Earlier firmware preserves HMC4 as read only.
Settings remains required but movable. HomeMenuSettingsActivity owns a
reusable order-button descriptor; the shared header keeps Back available. The
shelf state adds an optional Home launch-origin line for wake. No app carries
its own destination or adds a background task. Firmware file selection shares
the regular document rows used by Books/Library. The reader chapter sheet uses
that regular document face and four portrait rows, with its existing numeral
face and four landscape rows.

The library speed correction keeps a single bounded 40 KiB cover-region cache in PSRAM through HalMemory, owned and released by LibraryListActivity; allocation failure keeps direct rendering. It reuses Bitmap read-ahead and GfxRenderer region copies, without a new task/framebuffer or persistent catalog format. The XTC/XTCH reading-direction toggle extends the existing SettingsList/CrossPointSettings persistence and ReaderUtils touch mapping, with the format check in ReaderActivity. It reverses swipes and page tap zones only; the fixed reader sheet retains shared row rendering. See [library performance](docs/library-performance.md).

### Card text settings and page-layout work

The Flashcards/Words text settings remain local in
`apps_local/flashcards/FlashSettings*`, shared by these two apps. Six validated
bytes in CrossPointSettings/SettingsList persist two sizes and a Smart-sizing
toggle for each app. PixelMenu adds an optional action to its existing header
mark; inactive callers retain the same painter. The settings child retains its
parent card through ActivityManager and adds no worker or framebuffer.

`GfxRenderer::drawTextScaledBw` is a narrow allocation-free raster operation for
fixed-size deck/reader fonts. Ordinary drawText retains the identity-scale path.
Local StudySizedText/StudySizedDrawTarget apply the same scale to measurement,
line height, ruby and drawing. No SDK fork or global font-scale state is added.
Flashcards/Words own bounded page-start indexes and invalidate on content,
face, size and oriented-geometry changes; overflowing their caches never drops
pages. The retained parent owns its cache and the child owns its fixed controls;
all dynamic activity creation uses makeUniqueNoThrow. See the app documentation
for defaults, navigation and device verification.

The shared PagedText wrapper also reuses its existing 2 KiB workspace for up to
512 plain-text page starts in Quotes and The Intercept. Content fingerprints,
metrics and explicit workspace ownership prevent reuse after a text change or
another formatter uses the workspace. No collection-sized or per-frame buffer
is added. Words uses that same scratch storage for its typed paragraph starts.

The Anki image viewer reuses one 896-byte, 16-row member band instead of a
full image or second framebuffer. Its zoom/quarter-turn/pan state is local to
StudyActivity and has no persistent settings. Inverse sampling walks rows or
columns to keep SD band reads monotonic even after rotation. Back first fits
an enlarged image, then returns to the retained card without grading it.
The render audit removes Carnivoro's unconditional built-in-font prewarm pass,
Words' built-in/repeated page prewarm, Reading Quest's duplicate missing-assets
screen and Othello's duplicate font binding. Reading Quest's level-up overlay
and distinct generation/progress frames remain intentional compositions.

CrossNext local refinements retain one framebuffer. Anki preview and zoom data
are separate SD stores; the 4 KiB zoom band exists only while enlarged. Reading
Quest CNART002 validates its index and checks each sprite before its first draw.
Counter queues same-screen input while rendering. This delivery builds X4 Pro;
other inherited board environments are outside its release verification.

The global date-order preference is persisted by CrossPointSettings through
SettingsList and exposed by ClockSettingsActivity. The freestanding
src/util/DateFormat.h formatter supplies PET memory and historical-life dates
through the render model; PET binary save formats are unchanged.

The X4 Pro landscape layout owns small presentation seams in LibraryListActivity
(adaptive grid capacity and thumbnail invalidation), KeyboardEntryActivity
(compact keys and a cursor-centered text viewport), CrossPointWebServerActivity
and CalibreConnectActivity (bounded side-by-side network content), and
OtaUpdateActivity (version details below the shared header). Geometry lives in
local LibraryMenu, KeyboardViewport and NetworkLandscape helpers. No save or
network protocol changes, continuous framebuffer or task is introduced. See
[interface layouts](docs/interface-layouts.md).

The Home Button Gestures interval picker extends the existing global input
state machine and settings store: one byte selects five fixed windows, starting
at the unchanged 350 ms default. The input object retains one 32-bit interval;
there is no additional task, timer, dynamic buffer or per-gesture SD write.


The end-of-book adapter in `src/activities/reader/EndOfBookOptions` uses the existing
UiAppHost lifecycle and render/input publication gate. Its local
`apps_local/readertext/EndOfBookScreen.h` owns the illustration, Home action and
last-page arrow. No directory scan, suggestion list, worker or framebuffer is added.
ReaderActivity caches the durable completion state, and both reader More menus
switch between finishing and reopening. The statistics journal appends a zero-time
Reopen record instead of rewriting reading history. Reserved bytes in each 384-byte
record hold the finished local day, reversal day and explicit
unfinished flag. Older firmware ignores the new record kind and cannot display
this reversible status correctly; keep the current firmware when using it.
Statistics builds a non-owning completion-event index during its existing summary
scan. Its single PSRAM allocation is sized from the record-count upper bound,
capped at 32,768 entries (768 KiB), and released with the activity. Books without
completion events need no entry; page scans resolve current finishes in memory
without nested SD reads. A failed allocation or full index reports unavailable
statistics and leaves the journal untouched. Period projections retain
bounded cancellation counters so reverse scans remove completions from their
original period. The unified projection honors explicit local reversals, counts
finished books once across devices and preserves cumulative reading totals.
