# Words / Palabras

Words connects an explicit EPUB dictionary lookup to the existing FSRS-5 and
learning-step engine. It is available in Apps as its own destination, independently of Flashcards.
Imported CSV/Anki decks keep their existing content identity and history.

Select one word to choose Translate or Define. A selection of two or more words offers only Save quote; an automatically hyphenated word remains one word when both adjacent source fragments are selected.
Translate uses the bilingual dictionary chosen in Settings > Reading; Define
uses the monolingual dictionary chosen there. Install local StarDict folders
under `/dictionaries` first. Dictionary folder names must fit 31 UTF-8 bytes. No online translator is contacted and the reader
does not choose the correct sense automatically. Choose dictionaries appropriate
to those two purposes; the firmware does not infer dictionary type from its name.
An unconfigured operation reports which dictionary to choose.

A successful query opens its result under a black header with white text and an
optional compact white Save word button with a thick black border. Nothing
is stored until you tap it or press Confirm. Save keeps the selected form,
headword, meaning, dictionary, book language, passage, author, chapter and
position. It reports Saved for review and offers Undo. Back returns to the
reader's unchanged position. Failed queries create nothing. Allocation, length
and SD failures are reported as failed saves. Save quote uses the existing
clippings store, independently of Words.

Words opens with saved/due counts, Vocabulary (definitions), Languages
(translations), and a four-page illustrated How it works guide. The compact
collection shows five word names per page, using a larger cut of the same regular
Noto Sans typeface as Files. Meanings and translations remain available by opening a word.
Review words and Review difficult share a row. Review all saved words
and Export follow below. View list beside Your vocabulary opens a full screen,
names-only list with the same regular typeface: ten entries in portrait and five
in landscape. Selecting a name
opens its usual detail; Back restores the same list page, then the collection page.
An empty collection explains how to save a word and links to the guide.
The two collections retain their original context identities and scheduling data.
A dictionary/headword shared by both retains one FSRS history.

The full-width Settings bar on Words Home opens its text settings. Anki import
belongs to the separate Flashcards app: put APKG, COLPKG, Anki SQLite or
text/CSV/TSV exports under `/FLASHCARDS` and open Flashcards. Imported decks retain
their own content identity and review history; they do not fabricate book passages
or enter the Words capture store. Local import does not sign in to or sync an Anki account.

The local VCodex reference was inspected, but its second dictionary engine,
global checkpoint caches and short lookup-history list were not imported.

## Learning and context

There is one scheduling record per dictionary and canonical headword. Repeated
explicit saves increment its encounter count without changing its FSRS state or the
meaning already studied. Legacy “already known” flags remain excluded from scheduled reviews; explicitly reviewing a word from its detail and rating it includes it again.

Each learning purpose and book/chapter has its own first encountered form, sentence and position for the same word. Additional encounters in that chapter retain the first passage; a different chapter or book adds another context. Details and revealed cards use bold labels followed by the word, definition or translation and original sentence in regular type. Details also show the book in regular type. The unrevealed review face shows only the word. Source chapter, page, dictionary and lookup counters remain internal. Long content is paginated, including non-Latin scripts using the selected reading font.

The three actions are View in book, Delete and Review. Delete asks for confirmation and removes the selected saved context from this collection. Other saved passages and its shared scheduling record remain; saving the same word later keeps its learning history. Returning from a detail preserves the collection page. Collection keys change the page; holding one opens View List. Home retains its physical-key focus indicator.

Context comes from the selectable page window. Complete sentence boundaries are
retained when available, including following pages. A sentence crossing the
beginning/end of that window, or exceeding 1535 UTF-8 bytes, is explicitly
labelled “Visible passage”; it is never presented as a reconstructed quote.
The selection and source identity must fit whole; definitions above 2047 bytes
are shortened at a UTF-8 boundary and labelled. The full dictionary entry still
remains available in the dictionary viewer. Tokenization and inflection
fallbacks remain those of the existing reader/dictionary; Japanese segmentation
is not added by vocabulary capture.

“Review words” selects due/new entries in this collection. “Review difficult” selects due entries with Again/Hard as their last rating, high FSRS difficulty, repeated lapses, or failures with low stability, prioritized by difficulty and then due time. Both call the same StudyScheduler and FSRS-5 implementation as Flashcards, with the same default learning steps and one persistent scheduling record per canonical word. Hard never increments the failure counter. The review face hides the answer until Show answer, then offers Again, Hard, Good and Easy with the scheduler's actual preview intervals. Intervals are not fixed UI examples.

“Review all saved words” includes every canonical word with a saved passage in
the current collection, including legacy known words and future-due words. It
updates the same schedule with the same four ratings. Learning repeats retain
their timed steps. This mode scans in lexicographic ID batches of at most 48,
deduplicates contexts within each batch, then resumes above the last ID until the
collection ends. It never drops later batches or builds a collection-sized index.
Its progress shows the current presentation number without a misleading batch total.
An early rating can move a future review date; rating a legacy known word includes
it again in scheduled learning. The guide distinguishes these three review entry
points, name-only lists, detail actions, imports/exports and phrase quotation.

The activity holds a bounded 48-word session, deduplicated by canonical word across saved passages. Due learning/relearning cards take priority when filling that session, followed by the earliest/most difficult remaining cards; another session picks the remaining due words. Learning/relearning outcomes stay in that session. Ripe learning cards take priority; if nothing is ripe yet, a waiting screen shows the remaining minutes and refreshes when due. Leaving preserves the due state on SD, so learning resumes in a later session too. An explicit detail Review can study that word early, while Review words and Review difficult respect its due date. There is no separate sentence-completion exercise or second review calendar.

The existing scheduler has its documented FSRS-5 compatibility limits; using the shared engine does not claim byte-for-byte equivalence with every Anki release or custom deck preset.

“View in book” opens the original EPUB at its spine and visible-codepoint
offset, with the saved local page as fallback. The offset survives repagination.
A moved/deleted source book is reported; no unrelated book is opened.

## Persistence and ownership

`/.crosspoint/vocabulary/words/<id>.dat` owns scheduling and encounter counts.
`contexts/<id>.dat` owns book/chapter provenance. IDs comprise two deterministic
64-bit hashes; matching identities are checked before updating, so a collision
fails without inheriting another word's history. No CSV reimport is involved.

Word v2 adds last rating and a saturated failure count, while reading v1 as zero for those fields without changing card state or IDs. Versioned, little-endian records encode fields explicitly, with a checksum and
bounded UTF-8 buffers. No compiler padding or unaligned numeric loads are used.
Each replacement is written to `.tmp`, closed, read back, and renamed with a
`.bak` recovery copy. A valid backup can be read after an interrupted commit;
a corrupt live file cannot replace the valid backup during repair. Context is
committed before the word, so an interrupted first capture leaves at worst an
unreferenced context, never a blank review card. Duplicate/undo operations do
not overwrite review state. Directory iteration recognizes recovery copies.
All device I/O uses HalStorage/HalFile. Physical power-cut resilience remains a
hardware check; successful byte counts/readback do not guarantee a healthy SD.

The collection grows on SD with no collection-sized resident index. The app
streams directory scans in eight-entry loop budgets and holds one page, one
word and one context. Word is 2712 bytes, Context 2832 bytes on the host ABI;
wire records are 2738/2854 bytes. Legacy 2850-byte context records remain readable as Learn words, without changing their IDs or schedules. The reusable wire buffer is 4096 bytes.
Capture's temporary workspace is approximately 12 KiB, plus the 2832-byte
source context. HTML conversion temporarily uses the existing dictionary
formatter, guarded by largest-block/free-heap checks and bounded by the
dictionary's 64 KiB entry ceiling. These are cold,
fallible allocations with explicit owners, released before returning to
reading. The 48-entry session and its fixed batch cursor remain bounded, inside the activity’s existing32 KiB compile-time budget; it adds no heap container or worker. No network worker, background vocabulary cache, second framebuffer
is added. One separate bilingual dictionary name is persisted alongside the existing monolingual dictionary preference. The styled definition viewer retains its already-loaded entry (at most 16 KiB on that path) until it closes so a later explicit save can use the full result. Device heap/PSRAM peaks remain unmeasured.

“Export for Anki” streams one row per word, using the first saved context encountered that matches the selected collection and filters,
to `/WORDS/vocabulary.tsv`. The stable ID is the first field, followed by
word, meaning, sentence, book, chapter and dictionary. Map the ID to a dedicated
first field when importing into Anki to update existing notes. This exports
content, not scheduling history, and does not sync an Anki account.

Export advances in bounded steps and Back cancels it, preserving the preceding
TSV. It traverses contexts once and deduplicates word IDs in a temporary SD
hash table, with 33-byte slots and at most one-half load. Its temporary size is
`66 × matching context count + 33` bytes, plus the staged TSV; the activity
borrows its existing word/context workspace and allocates no collection-sized
RAM index. Empty collections skip the scan. Temporary files are removed after
success/cancellation and truncated on the next export after interrupted power.
Sleep, Home and activity exit close the job's handles. A completed TSV replaces
the previous one through a recoverable backup.

## Verification

`host-tests/words/run.sh` exercises duplicates across books/dictionaries,
unchanged intervals, known words, undo, rejected empty cards, failed writes,
wire corruption, UTF-8 boundaries, growing collections and SD recovery.
Device verification must include a bilingual and monolingual dictionary,
cross-page phrases, reflow before reopening a passage, all orientations,
sleep/wake, failed SD writes and power loss around a save. After a duplicate,
review count and interval must remain unchanged. A failed write must show
“Not saved” and preserve the preceding valid record.

List notices, such as an unset clock or completed/cancelled export, occupy the
list pane until OK is tapped. Loaded words remain unchanged.
The four ratings are selected by touch. Physical keys reveal a question, then
change only the answer page; View List keys also change only the page.
Review intervals remain visible with each rating and follow the scheduler's
preview for that word.
The question keeps its larger word face. Revealed answers and definitions opened
from View List use compact text, with the detail headword kept prominent. Swipe
left/up to advance or right/down to go back in paginated collections, View List,
help and card text. At the text boundary a swipe stays on the current face;
revealing or rating still uses its own control.
Cards whose alphabet is absent from the compact fonts use the selected reading
font and paginated content, including reader families with only one installed size.

## Browser examples

`tools_local/wasm/seed_demo.py` calls `seed_learning.py` only for the disposable
browser SD image. It provides 12 translation entries (five due), six definitions,
12 installed English flashcards, an importable TSV, five clippings, nine reference
wallpapers, and a checksummed Murdle campaign case. No firmware startup path
seeds or replaces a device SD card. The obsolete simulator Murdle v1 record is
replaced by a reproducible v4 case; real unsupported/corrupt saves remain protected.

`host-tests/words-tools-redesign/run.sh` renders English/Spanish, portrait/landscape,
empty/loaded collections, guides, review/reveal/wait/delete, calendar months, clock modes
and counter limits. `test_fixtures.cpp` validates examples with the production
Vocabulary/StudyDeck/Murdle decoders and rebuilds the Murdle fingerprint.

The FreeInkUI TextStyle/GfxRenderer adapter carries bold labels and regular content through the shared font path. The host preview records requested styles; the browser firmware checks the resulting glyphs. No font copy, synthetic slant or per-label allocation is added.

The local simulator packages the same eight font families and ten dictionaries as
CrossNext-Resources.zip in an isolated generated card. English selects English Simple
and WikDict ES-EN; Spanish selects Spanish Wiktionary and WikDict EN-ES. A manual
choice, including None, remains manual. Rebuilding the simulator refreshes its demo
card without writing to a physical microSD or changing the source demo saves.

Definition and translation results have Save word (or Undo) at the upper right.
Previous/next arrows and the page counter occupy the bottom row. Swipe left or
right over the definition to page; taps on the left third go back and other body
taps go forward. The first/last arrows are disabled at the page bounds. Back uses
the header arrow or physical Back. Pronunciation-only metadata with unsupported
phonetic glyphs is omitted, including rows mixed with syllabication; meanings and
syllabication remain. Definition text uses left alignment without paragraph indent,
independent of the book's alignment. Saving still requires an explicit action.

## Card typography and paging

The Settings bar on Home opens the same two-panel text settings layout
as Flashcards. Word size and Content size are saved independently, with 10, 12,
14, 16 and 18 pt choices. The initial manual sizes are 14 pt for the word and
12 pt for its definition/example. Smart sizing is enabled by default; it expands
short text and uses 12 pt with complete pagination for long text. The 10 pt option
remains available manually and when an oversized word requires it.
Smart sizing chooses word and content sizes separately and reduces a size before
splitting a word when a smaller cut fits. Turn it off for fixed manual sizes. The built-in Latin cuts match Flashcards exactly: the compact 10 pt option uses
the UI face, and larger Latin sizes use the same serif.
Installed reader fonts still supply scripts absent from the built-in fonts and
TTF/OTF reader fonts are scaled to the selected size rather than keeping
their original cut. The retained built-in reader face follows the same local size
policy when a fallback is needed.

Details and revealed review cards label Word, Definition and Example sentence
in bold; the actual word uses regular weight. Empty examples are omitted. The question face shows only the word, without the Word label. Definitions and
examples use regular content type. Definitions and examples use the content
size, including when opening an individual word from View list. Horizontal
swipes and the existing arrows navigate within the current face's pages.

Page turns reuse a bounded 256-entry paragraph-page index in its existing 2 KiB wrap
workspace, with a small metadata record.
Longer entries remain readable beyond the cache limit. Built-in text renders
once; SD-font prewarming runs when the entry or size changes rather than on every
page turn. Capture records, book positions and FSRS scheduling are unchanged.
