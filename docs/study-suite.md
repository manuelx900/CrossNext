# Offline study and reading tools

The CrossNext X4 Pro build includes Flashcards, Calendar, Clock, Quotes, Counter, To Do List and Reading Quest. Apps is paginated. Notes, Habits, Pomodoro and Wallpapers remain. Calculator, Study, Hacker News, XKCD, Instapaper and Wikipedia are absent from the app registry; source and conversion tools are retained where reused. Trivia, Connections, Insider, Sea Salt, Yahtzee, Knucklebones, Wavelength and Underhand are excluded from the game registry and device/simulator compilation. Existing app shortcuts resolve by title, so removed apps no longer launch from Home. Settings stays visible and last, within the six-item Home limit.

## Flashcards

The firmware creates `/FLASHCARDS` on SD mount. Copy files there, with optional subfolders. Flashcards explains this path on its help screen. Each folder displays at most 128 supported files/subfolders; divide larger collections into subfolders. The directory is excluded from Browse Files, the Library index and Recent Books, even when hidden files are enabled.

Direct input: `.apkg`, `.colpkg`, `.anki2`, `.anki21`, and comma-, tab- or semicolon-separated `.csv`, `.tsv`, `.txt`. CSV supports quoted fields/newlines, UTF-8 BOM, Anki separator comments and optional Front/Back headers. It uses the first two columns as question and answer. Anki imports use the first two note fields, basic/reversed cards and simple cloze deletions with hints. Nested clozes, oversized/empty records and unsupported cards are skipped and counted. Direct APKG import supports the first PNG/JPEG per face, storing a compact preview and an optional larger image opened only on Zoom. Custom templates, audio, arbitrary JavaScript/CSS and collection synchronization are unsupported. See [direct-import compatibility](apps/study-anki-compatibility.md#direct-apkg-import-on-crossnext-x4-pro--sticky) for formats and limits. The computer converter `tools_local/study/anki_to_deck.py` also prepares image/CJK decks; copy its output folder under `/FLASHCARDS`. Converted paths must fit the existing 63-byte deck-path limit.

APKG/collection archives may contain a legacy SQLite collection or a Zstandard-compressed `collection.anki21b`. The SQLite reader is read-only, bounded to 16 B-tree levels, UTF-8 and valid table pages. Compressed windows are capped at 4 MiB of PSRAM; collections at 256 MiB; imports at 100,000 cards. Cleaned question/answer faces are bounded to 65,535 UTF-8 bytes each, with raw joined fields capped at 256 KiB. Plain imported Flashcards text shrinks and then pages without an ellipsis. Specialized CJK, ruby and cloze layouts retain their existing rendering. No source file is overwritten. Import scratch files and the converted deck live under `/.crosspoint/flashcards/`, with metadata published last; cancelled/failed imports remove their unpublished scratch files; interrupted power loss can be retried. CSV conversion consumes at most 4096 bytes per partial record per loop, including oversized records. Hashing/archive extraction shows a preparation screen and runs synchronously; on large files Back is handled after that phase, then between conversion batches. Removing that cache removes its local scheduling history, not the original archive.

A source path and content fingerprint identify its cache. Reopening unchanged files resumes local reviews; editing/moving a source creates a different deck. Direct imports start a fresh schedule, using the firmware's existing FSRS-5 engine, four review grades, learning/relearning steps, review log, undo and per-deck study statistics/streak. They do not import Anki scheduling settings, existing due dates, review history or FSRS-6 parameters. Converted decks retain the parameters/history supplied by the converter. The Flashcards entry hides the former Study pairing/sync flow.

## Reading tools

Calendar defaults to Monday first. Its Monday/Sunday button saves a setting also used by Reading Stats. Clock shows analog or digital time and stays awake while open. Clock and Calendar toggle between the initial orientation and one landscape/portrait alternative; two Rotate presses return to the initial position. Both restore the caller's orientation on exit. Clock repaints on minute changes; it has no background timer task. Set the device clock through Settings if the clock warning is shown.

Counter supports plus, minus, one-step undo and confirmed reset, up to 999,999,999. Saves are deferred until 800 ms without input and flushed on exit/sleep. A failed save displays an error and retries less frequently.

Quotes reads the newest 512 complete entries from `/My Clippings.txt`, shows an excerpt and its book, and opens paginated details. Indexing consumes at most 4096 bytes per loop; row previews also have fixed byte/line budgets. Hold a word in a text-capable reader, drag the selection handles and save the selection using the existing highlight tools. EPUB/TXT text selection is supported by the reader; fixed raster XTC/images do not gain OCR or selectable text. The clipping archive keeps saved excerpts even if a highlight is later removed from the book.

Notes displays prose files; To Do List displays files containing task checkboxes. Both use the existing `/notes` files without migration/deletion. New lists start with an empty checkbox marker, which identifies the list without counting as a task; completing tasks is kept in the same Markdown files. Existing note/list conversion moves a file between these views.

Wallpapers shows a flat 3-by-3 gallery of BMP, JPEG and PNG files in `/sleep`, `/WALLPAPER` and `/wallpapers` on the local card. Existing image files remain readable. It has no built-in image collection. Tap to preview and hold for Set wallpaper, Delete or Slideshow. Deletion requires confirmation. See [Wallpapers](apps/wallpapers-shuffle.md).

## Reading Stats and Reading Quest

Tabs are Today, Stats, Calendar, Books. Today shows the actual device date and today's reading time, sessions, pages and completions; clock-unset devices show a warning. Session overlap at midnight is apportioned by local day. The calendar and weekly charts follow Calendar's week-start preference.

Settings > Reader > Reading formats controls collection of new statistics. EPUB, TXT and Markdown default on; XTC/XTH and images default off. Images can opt into viewing time, capped at two minutes per image without interaction, and never become completed books. App-owned image/deck folders are always excluded. There is no PDF reader in this firmware, so no PDF toggle is advertised. Existing history remains intact: old journal entries do not contain a format field and cannot be retroactively filtered reliably.

Reading Quest derives XP from saved reading: 3 XP/minute, 120/finished book, 15/reading day. Claiming a 30-minute daily quest gives 60 XP once per device date. Every 10 earned XP grants one coin before purchases: the daily reward therefore also grants six coins. Reading and one-time milestones both earn coins. Its 1,500 numbered milestones cover minutes, pages, sessions, reading days, completions and streaks, with increasing targets and one-time rewards. Level thresholds grow quadratically through level 100. Stored XP never decreases if older reading history is removed. Ready milestone rewards appear first, incomplete goals next, and claimed rewards last.

Gear has exactly three slots: Clothing, Aura and Pet. There are 100 complete outfits, each with a masculine and feminine figure unlocked by the same purchase, 50 auras and 50 pets. The shop displays level and coins above these categories, with independent simplified garment illustrations, prices, required levels, locks and equipped states. Six cards fit a portrait page and three fit landscape; numbered pages and arrows reach the entire catalog. Selecting a card opens its purchase/equip controls. The Hero page shows the equipped complete character and a gender switch. No character preview occupies the shop header.

The monochrome chibi figures are whole illustrations: headwear, clothes and boots belong to a single outfit. There are no independently joined head/body/foot regions. Outfits progress from apprentices to celestial/divine armor. The 50 distinct auras progress from sparse effects to elaborate divine forms behind opaque character and companion silhouettes. Pets progress from real animals to monsters and mythical creatures. A cat is small, while large dragons remain larger than the hero. Source scale above hero height is compressed and companion width is bounded, keeping the complete hero readable. Auras grow within a bounded frame. A common scene transform fits everything. Connected silhouettes are extracted before resizing, preserving wings, tails and clothing. Complete figures and pets are aligned at their visible ground line.

XP, spending, reward bits, gender and useful purchases migrate from RQU1–RQU4 to the two-record, checksummed RQU5 store at `/.crosspoint/reading-quest-v5.dat`. Earlier files remain intact. Historical head/body/foot rank `n` purchases unlock complete outfit `2*n`; the old equipped body selects the new outfit. Aura rank `n` maps to `n/4`, and pets retain their corresponding species. Retired moon/crystal dragons map to the new star-wyrm/ancient-dragon line. The first successful migration begins a fresh sequence epoch and is verified before installation; interrupted first migration can be retried. Level gifts add 24–220 coins per level, and consecutive-day milestones add special EXP and coin bonuses. Titles change every five levels. See [equipment and progression details](apps/quest-gear.md#progression-and-rewards) for exact rules, estimates, the level-up window, migration, generation and hardware checks. Changing the device date affects daily claims, as with offline reading streaks. Reading Quest remains an offline reading companion, without Habitica networking or accounts.

## Languages

Only English and Spanish are compiled into this profile. The UI uses the same Jersey pixel face in both languages; Spanish accents have dedicated pixel glyphs without changing the existing ASCII glyphs. Labels, prompts, rules, the illustrated story and Murdle clue templates are localized, with compact Spanish wording for narrow controls. User filenames, book metadata, proper names and imported flashcard content are retained as supplied. Wordle chooses its answer and accepted-guess dictionaries from the selected English or Spanish interface language.

## Games and appearance

2048 has a D-pad plus touch gestures. Portrait shared headers match the Games band; deliberately landscape games keep their established geometry. Chess and chess puzzles use the Classic traditional black/white silhouettes. The piece-style setting and alternate styles have been removed.

Wordle retains its 2465 English answers and durable saves. Spanish adds separate 4/5/6-letter curated answer lists and RLA-ES accepted stems; Ñ is distinct, while accented vowels use their base letters. Unknown words remain editable on the current row without spending an attempt. The 256-byte WDL1 save version 2 records selected/game vocabulary and migrates version 1 as English, so a locale switch never reinterprets an answer index. Replacing an unfinished game after a vocabulary/length change requires the existing new-game confirmation, and then counts as an abandoned game. A separate 16,323-word English guess dictionary (4/5/6 letters) accepts inflections/common guesses independently of answers. Non-words still prompt an error; valid guesses advance to the next row. Black means correct position, gray present elsewhere, white absent. Duplicate letters use the existing quantity-aware scoring. OK is a filled button.

The Intercept is a condensed, illustrated adaptation of inkle's MIT-licensed story, with 46 nodes, 46 distinct scene illustrations, conditional choices and five ending types. Each choice saves the node, flags and decision count. Continue resumes it; New Story confirms resetting this playthrough. This firmware includes the original source/license and adaptation data, not an Ink virtual machine. Illustrations are new assets created for this fork.

The Recent grid gives covers the whole card, with three columns and three rows per page and the existing 30-item Recent cap. Nonbooks use book-proportioned cards with the filename inside. Browse and Recent list use 56-pixel rows with compact type. Flashcards has its own three-deck library, due/new overview, review/rating cards, completion, monthly history and session detail following the supplied references; see [Flashcards UI](apps/flashcards-ui.md). Home offers Framed and Carousel. Carousel has one compact label per icon and a separate progress row; Framed shows progress on the cover. Appearance contains these two choices. Settings uses readable grouped rows with values beneath labels; System begins with Home Menu. Shared black header bands remain black in dark mode. Settings subflows share the black header and framed-row treatment, with action icons where pertinent. Wi-Fi replaces the touch toggle in Quick Settings and opens network selection; the panel releases Wi-Fi on exit if it owned activation. Rotate redraws Quick Settings immediately while the panel stays open.

Swipe upward from the reader's bottom edge to open Font, Layout, Contents and More. Contents selects available EPUB/XTC chapters inside the same sheet; formats without chapters say so. More contains the former reader-menu actions, with format-specific entries. Center taps follow page-navigation behavior and no longer open that menu; the physical Confirm fallback remains available. On touch devices a short power-button release requests suspension; holding it opens Sleep, Power off and Restart. Release after a long press does not trigger a second action. Actual power-latch/off and wake behavior must be tested on the physical X4 Pro.

## Ownership and resource costs

All storage uses HalStorage/HalFile. Each activity owns its models/work buffers and releases member files on exit; no new background task or framebuffer is added. The direct-import text workspace is 512 KiB plus 32 bytes in checked PSRAM, with bounded ZIP/image scratch and a checked PSRAM-only Zstd context/window. It is released before the Study activity opens. Flashcards reserves 128 directory entries, Quotes keeps a bounded 512-offset index and text/page buffers, and Quest scans the journal in 32-record batches into its fixed summary. Expanded Quest figure and shop artwork is stored as lossless BW1 records in the SD resource package, drawn with reusable 512-byte read and 35-byte row buffers. The Intercept also reads its 46 scenes and three supporting illustrations from its activity-owned CNART002 SD pack, with the same bounded streaming loader. Neither path allocates another image/framebuffer. The Intercept requires `/apps/intercept/graphics.bin`; missing or invalid resources show installation instructions before progress is loaded. See [The Intercept](apps/intercept.md). The generated `*-codec-check.json` files record exact pixel hashes, decoded bytes and compressed payload/codebook costs; the firmware size report includes descriptor tables and code too. Guess tables and story tables are constexpr. Clock orientation is activity-local; week start and reading formats are settings. Counter/Quest/Story stores have two checksummed sequence slots and protect unknown versions rather than overwriting them.

Regeneration: `python tools_local/wordle/build_words.py`, `python tools_local/intercept/build_story.py` (or `--check`), `python tools_local/quest/build_catalog.py`, `python tools_local/quest/build_figures.py`, `python tools_local/quest/build_expansion_art.py`, `python tools_local/quest/build_auras.py`, and `python tools_local/chess/gen_traditional.py`, then `python scripts/gen_i18n.py` using the project's environment. Artwork PNGs, prompts/manifests, whole silhouettes and generated headers travel together and require no build-time image tooling. Existing ignored I18n outputs are generated by the normal build hooks. Keep source YAML and generators together. See THIRD-PARTY-NOTICES.md for licenses.

## Hardware verification before publication

On a backed-up X4 Pro SD card: boot, confirm `/FLASHCARDS`, import a small legacy and modern APKG plus CSV with quoted lines, review/undo, suspend/reboot and reopen the same deck; due state and streak must persist. Confirm a large/unsupported archive reports failure/skips without reset or modifying its source. Test a malformed file, full SD and interrupted import, then retry. Measure free internal heap, largest block and PSRAM before/after import and 20 open/close cycles; watch for OOM, reset or cumulative loss. Large compressed decks can take time; test battery/watchdog behavior on hardware.

Check portrait and both landscape rotations, analog/digital minute updates, Clock exit orientation, Calendar Monday/Sunday order in both apps, Counter reset/undo/reboot, Quest claim/purchase/reboot and Story choices/endings/reboot. Check EPUB/TXT selection with accents and a long quote. Verify default XTC/image reading does not create a completion; opt-in images records time only. Confirm both wallpaper folders mix, preview PNG/JPEG, set suspension image and reboot. Existing histories/files should survive. Exercise network connect/cancel from Quick Settings and Settings, then verify Wi-Fi releases without breaking a preexisting transfer. Browser simulation verifies UI and software paths; display waveform, touch edges, real RTC, power use, SD failure recovery and heap pressure need the physical device.


## UI and game refinement profile (October 2026)

Only English and Spanish appear in Language. Both use the same compact Jersey
header and settings cuts. Updates and diagnostic submission are hidden. "Formats
counted in stats" selects which document types contribute reading records.
Brightness and warmth buttons step by ten points through 0–100. Brightness zero
turns the light off; switching it on from zero selects ten. Sliders retain fine
adjustment. Warmth zero selects the cool endpoint.

Apps begin with Reading Quest, Habits, To Do, Notes, Pomodoro, Wallpaper, Quotes,
Flashcards, Calendar, Clock and Counter. Games begin Chess, Checkers, Go,
Minesweeper, Picross, Wordle, 2048, The Intercept. D&Diagrams was
unregistered and its private sources/assets/tools were removed. Toy Battle and
Forehead have no shelf entries; their sources are not new features.
Show/hide uses left checkboxes and right shelf icons; hidden states retain stable
identifiers. Carousel reserves more space for covers. The former Focus layout is now named Carousel; the older third layout migrates to it.

Reading Quest offers 50 distinct auras progressing from simple effects to divine
forms. Earlier aura purchases map into the new catalog during RQU5 migration;
owned items can be re-equipped without paying again. Aura and Pet can be removed,
while a complete outfit always remains equipped. The scene draws aura, hero and
companion in that order and fits all three with a shared scale.

Habits provides daily date navigation, Today, meaningful preset icons, progress,
and a detail screen with four statistics and a monthly check-in calendar. Dates
before a habit was created omit it from that day's list. To Do offers free tasks
and named lists on entry. The header switch returns to the other view. Free tasks
use reserved `/notes/.crossplay-free-tasks.md`, excluded from Notes' library;
ordinary atomic note persistence is reused. This internal file cannot be renamed
or converted to prose. Counter and Clock use integer-scaled pixel digits, with
aligned analog/digital layouts in both orientations. Pomodoro keeps its existing
engine behind a framed timer, timeline and Start/Pause, Skip, Restart, Settings.
Reading Stats has Today, Stats, Calendar and Books tabs, icons, minutes cards,
three daily metrics, consistency cards and weekly/monthly charts. Browsing a day
rescans only that day; lifetime and completed-book counts are not accumulated
twice. Continue Reading uses the recent book or the file browser.

Picross has 450 puzzles: 100 each at 5x5, 8x8 and 9x9, and 150 at 10x10. All 199
previous bitmaps retain identities through `PicrossLegacyMap.h`; 251 original
geometric additions are unique and solved by the independent line solver during
generation. Numbered square tiles, four size tabs and a five-page moving window
keep every page reachable within the hit table. Version 7 uses a checksummed
168-byte `/.crosspoint/picross-v7.sav`, writes a temporary file, verifies it, then
publishes it through HAL replacement. A valid pending v7 save is recovered when replacement was interrupted and the canonical file is absent. Version 6's old 132-byte file remains intact and is migrated on read.
Unknown newer formats are read-only.

Minesweeper offers 10x10, 15x15 and 20x20 boards with a 15% mine count, a safe
first dig and complete flood reveal. Geometry is derived from the screen. The
size preference is a separate one-byte file. New history is a checksummed
420-byte full-board record, written by temporary file and rename; the old 8x10
text record remains a read fallback. Board/history buffers belong to the
activity; the flood uses bounded scans rather than an enlarged stack queue.

### Checkers variants

The default is International (10x10, twenty men). Brazilian, Spanish and English
use 8x8/twelve men. International and Brazilian men capture backwards and kings
fly; the longest capture is mandatory. Spanish men capture forwards; flying kings
and maximum quantity, then maximum captured kings, apply. English has short kings,
forward man captures, any complete capture chain and crowning ends that turn.
Taken pieces block until the chain ends. Brazilian men passing through the far
row continue as men when possible; promotion occurs when their turn ends there.

Threefold repetition and variant-specific quiet-move limits are automatic. The
International 16/5-turn endgames retain the earlier deadline when material falls.
Brazilian 15/5-turn low-material and 30/60-turn no-material-change counters are
tracked per player. Spanish uses twenty quiet king moves per player and the
Ludoteka thirteen-turn three-kings-with-long-diagonal ending convention. Regional
Spanish tournament conventions can differ. Rules sources:
[FMJD](https://www.fmjd.org/docs/Annex_1.pdf),
[IDF Brazilian](https://fmjd64.org/rules-of-the-game/),
[WCDF English](https://wcdf.net/rules/rules_of_checkers_english.pdf),
[Ludoteka Spanish](https://www.ludoteka.com/clasika/damas_clasicas.html).

Choose computer difficulty or two local players in Settings; the English network
activity remains available via its link button. Easy chooses randomly, Normal
uses position/capture/promotion weights, Hard also estimates immediate diagonal
threats and protection; this is a lightweight heuristic opponent. Rules and local
opponent apply when starting a new game; an active game keeps its variant and
opponent. Configuration uses `/.crosspoint/draughts.cfg` via temporary rename.
Ongoing boards are session-only. The iterative capture generator has an
activity-owned fixed workspace (~6.5 KiB), yields after 512 search nodes and
allocates nothing inside its search loop.

### Simulator fixtures

`tools_local/wasm/seed_demo.py` populates only the simulator preload card: Alice
plus five original readable demo books/covers, six scenic PNGs in sleep/WALLPAPER,
45 days of reading records with three completions, Habits and named/free tasks.
These demonstrate nonempty carousels, history and the image-only gallery. They
are labeled demo data and do not occupy the device application partition. Reload
restores this disposable card; none of these fixtures writes to a physical SD.


### Reference UI refinement (October 2026)

Large counters use proportional glyphs extracted unchanged from the existing
Jersey 25 font, with actual ink bounds centered in each panel. This replaces the
handmade 5x7 digits in Habits, tasks, Reading Stats, Pomodoro, Clock and Counter.
The shared numeral subset occupies 9,803 bitmap bytes in flash and draws row
runs directly through FreeInkUI; it adds no heap allocation or framebuffer.
Murdle clues use a 1-bit Noto Sans reading cut. The renderer adapter has one
additional font slot (four bytes per target), keeping prose separate from the
fixed small-label fallback. Development font generation uses the existing OFL
Noto source and tools_local/toybox/gen_ui_prose.py; dependencies are listed in
tools_local/requirements-graphics.txt.

2048, Othello, Wordle, Murdle, Minesweeper and The
Intercept use the reference square panels and icon controls. Headers retain
their agreed compact size. 2048 centers the real glyph ink within symmetrical
insets. Minesweeper has raised dithered cells and a scalable eight-spoke mine
with a highlight. Both dig/flag tools remain visible; only the unselected tool
changes mode. Settings can return to the existing board, and a new board size
applies when starting a new game. Settled boards retain the result banner and
provide separate New Game and Settings controls.

The Intercept's 46 Spanish scenes explain the missing Bombe component,
blackmail and decisions in direct prose. Its reference redesign uses the
supplied barrack, custody and suspicion illustrations, two illustrated About
pages, numbered choices and separate Menu/New story actions at an ending.
Story links and the two-slot save format are unchanged. Murdle has fixed Clues/Grid/Case File tabs, numbered clue rows and
an explicit accusation action. Reading Stats shows finished books as framed
cards with separate start/end dates and duration. The paged capacity follows
those cards, so all stored books remain reachable. Draughts settings separate
variant names, board dimensions, difficulty and opponent choices.

The five primary values on Reading Stats' Stats (Datos) tab use an 18% smaller numeral
area, centered in the same panels. Today's values and consistency counts retain
their sizes. Murdle's Case File shows the full cast on one page. In landscape,
suspects keep the full width and the other categories use separate columns;
the category with detailed traits gets extra width. The accusation button
remains on Clues and Grid. Tapping an
already active case tab does not repaint or save the case.

Carousel covers are decoded one at a time through HalFile. Each cover needs
one fallible row scratch buffer (packed decoder row plus source row), matching
the previous bitmap path's allocation model; it is freed before the next cover.
The main image fills its slot with an opaque background and centered crop;
side images compress the complete cover horizontally into a tapered plane.
Far sides paint first, near sides next, and the main cover last. Thumbnails
use the canonical book cache at each actual slot height; existing fixed-size
covers remain a fallback. Image polarity is preserved in dark mode. Home layout
is now included in the existing settings JSON so a chosen carousel
survives reopening the firmware. No save formats or book files change.

Flashcards and Words are separate Apps destinations. Flashcards lists imported
decks only. Its deck view shows due/new counts and the last two weeks of review
history. Question and answer faces share a bordered content area; revealing an
answer offers four rating tiles with the scheduler’s existing intervals. Undo
appears on the question face when a previous review can be undone. Words keeps
its own learning collections and manual dictionary-save workflow.


Carnivoro's 1,470-case campaign keeps inline Truth / Lie / Suspect notes below
each character and paginates up to three cards in portrait. Neighbour clues
explicitly name both neighbours, independently of page order. Open case resumes
or advances through the selector's progressive order. Each difficulty groups
cases by increasing culprit count, then liar count. Beginner has one culprit;
Normal, Advanced and Expert introduce one before two. Replay preserves
completion, and every case is selectable from the beginning. Reset all requires
confirmation. The version-5 save migrates versions 1–4 while preserving puzzle
IDs, notes, results and lives; displayed numbers follow the new difficulty order.
See [the Carnivoro guide](apps/carnivoro.md) for rules, limits and device checks.

Numbered puzzle catalogs use free selection: Carnivoro and Murdle no longer
require earlier completions. Picross and Chess challenges already
expose every catalog level independently of solved counts; progress marks are
records, not prerequisites. Each game's normal per-attempt rules remain.

D&Diagrams (Dungeon), absent from the Games menu, has been removed from firmware
sources together with its private puzzle/art assets, generators and tests. The
map icon shared with Reading Quest remains in use.

The [Citas and Notas redesign](apps/reading-tools-redesign.md) documents favorites, per-book quotes, local removals, note reading/options, the user-file gallery and carousel progress.
