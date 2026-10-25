# Flashcards screens and local history

The library follows the supplied October 2026 reference layouts: a daily total,
three deck rows per page, history/help navigation and the source-folder footer.
A deck opens its due/new counts, current Monday–Sunday activity and actions to
browse cards, inspect its history or begin reviewing. Imported titles and card
content remain in their original language. Interface text uses English/Spanish
translation keys. The final help illustration uses bars for Hard and two checks
for Easy. The completion and preparation artwork is extracted from the supplied
reference images; `tools_local/flashcards_art/generate.py` regenerates FlashArt.h.

The library scans `/flashcards` (or existing `/FLASHCARDS`) and its selected
subfolder. Converted deck folders open directly. Raw supported exports retain
the preparation/import flow and reuse their content-addressed converted cache.
Listing reads cached source identities without rehashing packages. If multiple
versions exist, it shows the most recently reviewed installed version. Opening a
source validates its complete content hash before using a cache, so changing an
export cannot inherit scheduling from another version. The legacy uppercase root
is preserved on FAT; lowercase is a fallback for case-sensitive filesystems.
The global history discovers converted decks under these roots, `/study`, and
`/.crosspoint/flashcards`. Its filter selects all discovered decks or one deck.
History shows three sessions per month/page, with stable timestamp/deck/offset
ordering. The session detail separates unique cards, answers (which can repeat),
new cards and rating totals. Reviewed cards and the whole deck can be browsed
without grading or changing scheduling state. Long questions and answers have
pages rather than an ellipsis, including older prepared decks and vocabulary or
cloze cards. Every visible field shares the page sequence; CJK deck fonts,
furigana and answer emphasis are retained. Whole lines move to the next page.
If a custom headword cut is taller than the content area, the card uses its
sentence cut. No deck conversion or scheduling reset is required.

The answer uses compact text, with deck fonts retained for characters the UI
font cannot draw. The rating instructions have been removed; in portrait their
space belongs to the answer box. A smaller page selector leaves more room for
content. The four rating buttons retain their positions and labels.

Paginated deck lists, help, history, filters, card lists and card text accept
left/up swipes to advance and right/down swipes to go back. Text swipes stay
within the current question or answer and stop at its page limits. Existing
page arrows remain available. Edge gestures keep their Back/Home/panel actions.

## Storage and timing

There is no new session database. FlashHistory streams the existing 32-byte
`revlog.dat` records. Bit 1 of the flags byte marks the first response of a new
local review session; bit 0 still means voided. New local responses record actual
`millis()` elapsed since loading their card in `tookMs`. This includes thinking
and reading the answer. Leaving an ungraded card writes no time. Undo voids its
response and preserves the session boundary. Existing Anki replay ignores the
new marker. Pre-marker logs are grouped by a 30-minute inactivity gap or a
backward clock jump, and zero times are displayed as unavailable rather than
invented. Old inferred sessions can span more than 256 cards. Overflow IDs are counted in
sorted windows of 256, with a sequential pass per window and a 1 KiB read cache,
rather than a seek for each pair of records.

Activities own bounded scratch buffers. The library/history catalog reserves at
most 128 deck records; names and paths are cold-path strings, never built in
render. History owns 256 IDs and flags, one 32-byte record, a 1 KiB block cache and a session summary;
it streams older data instead of growing a vector with every response. Four
visible card previews and the existing 8 KiB note buffer are activity members.
The retained Study parent owns one additional 1 KiB read cache; the history child
owns a second 1 KiB cache for card-state lookup. No new framebuffer or background
task is allocated. HAL file reads yield every
128 physical block reads during long history scans. Global deck discovery reads
identity metadata without rescanning scheduling records it does not display. Reference artwork is constexpr one-bit
flash data. Global history scans and legacy overflow rescans trade latency for
bounded working memory.

## Verification and device handoff

Automated checks: `host-tests/flashcards-redesign/run.sh`, study core/import tests,
and the browser simulator. `site/qa-flashcards.html` seeds three demonstration
decks written by the production converter, with real-shaped scheduling/history
records. Its counts are fixture data, not user data. Use Spanish/English, both
orientations, empty library and CSV import scenarios. Check library → deck →
question → answer → grade → undo → completion → session → reviewed cards, plus
month/filter/page navigation and long-card browsing.

On backed-up hardware, repeat import/review/undo, suspend/reopen and reboot;
expected counts and history must survive. Check a removed or corrupt deck,
missing clock and full SD: errors must replace success and navigation must stay
usable. Test both orientations and repeated activity open/close while watching
free heap/largest block and watchdog logs. Long historical logs should remain
responsive without cumulative heap loss. Host tests and successful firmware
builds do not replace these physical-device checks.

## Text sizes

The gear immediately to the left of the header status opens Text settings from
the Flashcards menu, deck, study card and history. Question and answer sizes are
independent: 10, 12, 14, 16 or 18 pt. The compact 10 pt choice uses the existing
UI face; the larger Latin choices use the built-in serif. Fixed-size custom deck
fonts are scaled in black and white, retaining CJK glyphs and ruby readings.

Smart sizing is on for a new installation. It chooses the largest size that
fits the current face. Ordinary long text stops at 12 pt and paginates instead
of shrinking indefinitely; 10 pt remains available manually and for oversized
words. Fit probes stop after the first page overflows, then the chosen layout
is indexed once.
Turn it off to use the manual sizes; their initial values are 12 pt for each
face. The manual choices remain saved while Smart sizing is on. The previews
show those manual choices. Done and Back save changed settings through the
existing settings store; a failed save keeps the settings screen open for retry.

Returning preserves the deck, card and question/answer face, and starts at its
first text page. Changing a size never grades a card or resets its FSRS history.
Resizing or rotating recalculates pagination. A page turn reuses the measured
layout; plain text stores at most 128 page starts (about 0.55 KiB per activity).
Pages beyond that bound remain accessible by scanning from the last cached start.
Rich-card wrapping reuses each candidate width instead of measuring it twice.
These reduce text-layout work; physical refresh latency still needs device checks.

## Anki image viewer

Tap PHOTO (or an image-only face) to open its image. The bottom toolbar offers
Zoom minus, Zoom plus, Rotate and Fit. Zoom advances through 1x, 2x, 4x and 8x;
drag horizontally, vertically or diagonally to inspect enlarged details. While zoomed, four arrow buttons move through the cropped range in thirds, below the image in portrait or beside it in landscape. Rotate
toggles between the original orientation and a 90-degree turn in the direction chosen under Settings → Controls → Rotate button direction while retaining zoom. Arrow icons and their panning directions turn with the image, so they remain consistent when the device is turned to read it. Returning to the original orientation restores the arrows. Dragging continues to follow the finger. Fit centers the rotated
image. Back first fits an enlarged image, then returns to the same card. Viewing
an image never grades a card. New imports retain a compact one-bit preview (up to 448×620) and an optional
source-resolution grayscale copy (up to 2048×3072). Normal cards and Fit use only
the preview. Zoom opens `images.zoom.dat` v4 and lazily allocates up to a 32 KiB PSRAM
band; Fit/Back releases both. Source gray is interpolated before monochrome
dithering at screen resolution, so enlargement does not magnify a baked-in dither
pattern. Screenshots with at least 95% near-black/white source pixels use a
clean threshold after interpolation to keep text strokes solid. Reads walk source bands in either orientation, including reverse row
order. No image decoding occurs while dragging, and there is no second display
framebuffer. The arrow buttons provide discrete panning without a helper caption occupying the image area.

Progressive JPEG HD preparation uses a JPEG-only stb_image decoder with a 6 MiB
aggregate PSRAM cap. Its temporary full coefficient/pixel workspace exists only
during import or explicit legacy recovery; allocation failure retains the older
reduced decoder fallback. Native PNG and baseline JPEG remain line-buffered.
Import needs additional time and SD space. Zoom cannot invent source detail.

On first explicit Zoom, eligible older imports rebuild optional HD data from the
unchanged APKG/colpkg recorded in `source.txt`. The package fingerprint and card
text/order must match before publishing only the zoom data. Review state, history,
text and the normal preview remain untouched. If the package is missing/changed,
the available monochrome HD/preview remains usable. A stale `.zoom-refresh`
directory after power loss must be removed from a backed-up SD copy before retry.
Missing, malformed or unreadable zoom data falls back to the preview.
