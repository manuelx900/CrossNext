# Reader sheets and quick settings

Quick settings opens with a downward swipe beginning at the top screen edge. A tap on the status/header band does not open it. Existing explicitly configured physical shortcuts remain supported. The default four actions are Dark mode, Refresh, Rotate and Light off/on. Display → Quick settings assigns each slot from the eleven choices documented in [Settings](settings.md#quick-settings-shortcuts), including Warm and the power actions. Selecting an action already in another slot swaps the two. The four stable action IDs persist in settings; absent older settings use defaults and malformed values are normalized. Lock uses the existing sleep/wake path. The Bluetooth shortcut toggles the saved BLE opt-in; Wi-Fi, exclusive storage transfers and deep sleep take priority over its radio service. See [Bluetooth controls](bluetooth-controls.txt). Wi-Fi retains the panel-owned session cleanup. Frontlight sliders and their persisted values retain their existing HAL ownership.

In a reflowable book, an upward swipe from the bottom edge opens Contents, Font, Layout and More. Contents has four chapters per portrait page (two in landscape), the current chapter highlight and current/total page controls. Chapter titles keep their original text, without an added ordinal, and can wrap to two lines. Opening it starts on the current chapter's page. Font keeps built-in/SD families, point size and regular/bold selection. Layout uses stepped bars for spacing, margins, paragraph indentation and all five alignment choices (justified, left, centre, right and the book’s style). More → Style shows Focus reading, Hyphenation, Embedded style and Text antialiasing inside the same sheet, with its tabs and Done button retained. Changes apply to the book beneath the sheet and are saved after the changed page is presented and a short idle interval; exit or sleep also flushes pending preferences and position. The inline back arrow or Back button returns to More; Done closes the sheet at the same content location. Done closes the whole sheet, including the family picker.

More contains actions with icons: Style, Search in book, Add/Remove bookmark (according to the current page), Bookmarks and quotes, Go to page or percentage, Book statistics and Sync progress when KOReader credentials are configured. Text antialiasing is off by default for new settings and for settings files without that option. Existing saved choices are preserved. Enable it in More → Style → Text antialiasing or Settings → Reader → Text settings → Style when smoother text edges are preferred over the extra refresh cost. Image grayscale handling is unchanged. The collections action offers bookmarks or saved quotes. Selecting an anchored quote opens its exact saved text offset; older unanchored quotes open their saved text. Locations can select a page within the current chapter, whose pagination is available, or a percentage of the whole book. No invented whole-book page total is shown. Book statistics flushes the current session before opening the existing cumulative book detail and returns directly to the reader.

Search incrementally reads each ZIP spine through HAL-backed `ZipEntryReader` and the existing Expat parser, skipping head/style/script content. Each loop submits at most 1 KiB to the parser, then returns to input polling. The existing inflater may decode ahead into its fixed 32 KiB window. TXT/Markdown uses bounded raw-text reads and returns to the document start; encrypted ZIP entries are skipped and reported as an incomplete search. It matches case-insensitively for ASCII and Spanish Latin-1 letters. It retains the first matching excerpt in each of at most 64 matching chapters, roughly 14 KiB of fixed result storage inside the fallible activity allocation. Its reusable parser is reset per chapter and freed on exit. The current chapter owns one open HAL file, a 1 KiB ZIP input buffer, a 1 KiB output buffer and the existing inflater state/window; closing, selection or cancellation releases them; it creates no task, network connection, SD index or framebuffer. Results open the last navigable block ID when one exists, otherwise the chapter start; converter-injected span IDs are ignored to match the reader anchor policy. All non-selection exits are cancelled results. Unsupported/malformed chapter XML or read errors produce an incomplete-search notice. Search again is available after the scan. This bounded chapter search does not enumerate every occurrence. Device heap, parser latency and peak stack remain unmeasured.

While the reading surface is active and touch reader controls are enabled, tapping the lateral 15% of the page turns one page: by default, left goes back and right advances, including swipe-only gesture preferences. Settings → Reading → Reading → Manga → Invert XTC/XTCH direction reverses both horizontal page swipes and page tap zones for XTC/XTCH only; it defaults to Off. See [library and fixed-layout controls](library-performance.md). The top and bottom 32 pixels remain available to edge gestures. Swipes and the existing central tap preferences remain supported. Sheet controls retain priority while open.

Fixed-layout XTC keeps only controls the format supports and removes dark mode from its lower sheet. Global quick settings owns that toggle. Existing reader page/percent progress text remains part of reading status; discrete menu/picker pagination uses `PageIndicator.h`: previous, current / total, next, with non-interactive ends.

Simulator and host checks do not establish X4 Pro touch/display timing. Before a device release, test top taps versus top swipes, lower-sheet gestures, long/multibyte chapter names, first/last pages, SD font families, all alignment choices, actual book searches, bookmark/clipping navigation, jump boundaries, stats return, light state across sleep, custom shortcut persistence and Wi-Fi teardown.

The chapter and font pagers use compact arrows and two-pixel outlines. Family rows are 44 pixels high, with the smaller menu lettering, keeping touch targets while avoiding oversized cards. The shared jump/sleep selector uses a framed panel, a thin track with a square thumb, separate minus/plus buttons and Confirm. The thumb retains drag routing, exact-step controls and the existing cancellation/result semantics.

Saved bookmarks and quotes use book title/author, passage previews, locations, explicit pagination and Done. Quote previews retain an eight-record window; their bounded location captions add 768 bytes inside the fallible activity allocation, with no new framebuffer, worker or independent store. Bookmark actions and quote delete confirmation remain available by holding a row.

Saved-collection pages are stable slices of absolute row indexes. Arrows, swipes and held side keys select the first record of the destination page; individual side-key presses move one record. The last page keeps its own records when navigating in either direction. Extra paragraph spacing is marked as not visible in the single-paragraph settings sample; its setting continues to apply to books.

Closing bookmarks or quotes returns to the four-tab lower sheet on More, after redrawing the reading page. A selected anchored record still navigates directly to its saved position. Empty quotes also retain the collection screen.

On X4 Pro, a single Home tap always returns to Home, including after updating settings with an old tap assignment. Settings → Controls → Home Button Gestures configures double tap and long press. Reader bottom sheet opens the same format-aware lower sheet as the bottom-edge swipe and only acts on an active book. It is the default long-press action. Lock/unlock touchscreen toggles the global touchscreen lock from any screen; physical keys remain usable. Settings → Controls → Touch → Lock touchscreen controls the same persistent lock. While locked, holding the circular Home button always unlocks, including when its saved action is Ignore or Reader bottom sheet. This safety override does not overwrite that saved action. The separate Touch controls setting retains its reader-only meaning. A contact already held when locking or unlocking is suppressed until released.

The quick panel light and warmth toggles and configurable shortcut choices are described in [Settings](settings.md#quick-settings-shortcuts).


The EPUB reader offers seven line heights: 100%, 110%, 120%, 130%, 145%,
160% and 175% of the font size. Existing Tight/Normal/Wide/Extra wide settings
are migrated to 110/120/130/145%. A page layout rebuild after updating is normal;
reading progress, quotes, statistics and goals are preserved.

EPUB images and covers also accept non-interlaced GIFs. Only the first frame
is displayed; there is no animation or background timer. Compressed GIFs are
limited to 16 MiB, scanline width to 3072 pixels and source area to 8 megapixels
for page images. Oversized, interlaced or malformed images fall back to the
reader's normal unavailable-image behavior.

CSS floating initials wrap the opening lines while retaining the full word for
dictionary lookup and saved quotes. Unsupported initials, right-to-left text
and ruby keep their ordinary inline presentation.

## Black-and-white EPUB images

Settings → Reading → Images → Black and white draws image shades as a monochrome dot pattern. The normal Display choice retains grayscale images. The existing image pixel cache is reused; switching the setting may regenerate the affected chapter layout cache, but does not modify book progress, clippings, reading statistics or goals. Image pages in this mode follow the ordinary page-refresh interval instead of forcing the image grayscale waveform. Text antialiasing and explicitly requested/scheduled cleanup refreshes still apply. The renderer restores its image mode after drawing, including error exits, so home covers and other activities keep their own rendering policy.

## Text settings for each book

Changes made from a book's lower Text panel, or its text-settings screen, apply
to that book. The font family, size, weight, spacing, indentation, alignment,
margins and text style are restored when the book opens again. Settings →
Reading → Text settings edits the general defaults for books without a saved
text profile. The general settings panel labels its scope as General
defaults. Layout pages adapt to the available space, including extra paragraph spacing and indentation. The general settings screen reserves a legible sample above the same controls used in the book.

Text profiles use two checksum-validated records (`text.0.bin` and `text.1.bin`)
in the book's cache directory. Clearing that entire directory also resets the
book's text profile; clearing only its layout sections preserves it.

Dictionary and translation definitions support the usual left-edge swipe to go
back. Horizontal swipes within the definition continue to turn definition pages.

See [EPUB formatting work reuse](epub-format-speed.md) for exact-quality reflow, persistence timing and verification limits.
Configurable top and bottom reading bars, including session-based book/chapter time estimates, are documented in [Reading bars](reader-bars.md). Existing layouts are preserved until personalization.


## Opening the lower text panel

Settings → Controls → Open text panel selects one opening method: swipe up
from the bottom edge (the default), tap the middle third of the reading page,
hold the center Home key, hold the left side key, hold the right side key, or
hold either side key. Side holds fire once after 700 ms; releasing them does
not also turn a page or execute their ordinary assignment. Short presses keep
their existing assignments. Center taps respect EPUB links and footnotes.
These overrides apply only to a ready foreground reader without an overlay;
other apps and locked touch retain their existing controls. The dedicated
Home/remote Reader menu action remains available independently.

In general Text settings, Use as default is checked when books share the
general text settings. Reading changes then update those shared settings.
Turning it off restores per-book text profiles; previously saved profiles are
retained, and books without one start from the general defaults. Both toggle
directions display an explanation after a successful save. Failed saves show
the existing error instead. Both reader-panel appearances reserve the same
space; alignment controls extend into the available lower area.


## Choose how book settings open

In **Settings → Controls → Buttons → Open book settings**, choose one opening
method: swipe up from the bottom (the default), tap the page center, hold the
center button, hold either chosen side button, **Tap bottom edge**, or **Tap top
edge**. Edge taps use the first/last 48 logical screen pixels and follow the
current reading orientation. Tap controls must be enabled. Links, an open
reading overlay and an unfinished page retain their existing input priority;
a page outside the selected band keeps its usual tap behavior. A top-edge tap
opens book settings; swiping down from the top continues to open quick settings.
Existing saved opening methods keep their meaning after this update. These
choices add no task, cache or framebuffer.

The EPUB bottom sheet reads only the visible chapter titles on its Contents tab. It looks up the current chapter once for that frame and uses it to place the initial page and selection. Font sizes and More actions are prepared when their tabs open. Opening another overlay over an existing one or dismissing a picker reuses the existing clean-page snapshot while it is valid. The page underneath still uses the selected image, font and antialiasing settings.
