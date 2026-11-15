# File Formats

These formats describe the SD-card cache files under `/.crosspoint/epub_<hash>/`.
All POD fields are written in the ESP32 little-endian representation used by
`Serialization.h`; strings are length-prefixed UTF-8.

## `book.bin`

### Version 10

`book.bin` stores EPUB metadata plus lookup tables for spine and TOC entries.
The current firmware writes this version from `BookMetadataCache`.

ImHex pattern:

```c++
import std.mem;
import std.string;
import std.core;

#define EXPECTED_VERSION 10
#define MAX_STRING_LENGTH 65535

struct String {
    u32 length [[hidden, comment("String byte length")]];
    if (length > MAX_STRING_LENGTH) {
        std::warning(std::format("Unusually large string length: {} bytes", length));
    }
    char data[length] [[comment("UTF-8 string data")]];
} [[sealed, format("format_string"), comment("Length-prefixed UTF-8 string")]];

fn format_string(String s) {
    return s.data;
};

struct Metadata {
    String title [[comment("Book title")]];
    String author [[comment("Book author")]];
    String language [[comment("Book language code")]];
    String coverItemHref [[comment("Path to cover image")]];
    String textReferenceHref [[comment("Path to guided first text reference")]];
};

struct SpineEntry {
    String href [[comment("Resource path")]];
    u32 cumulativeSize [[comment("Cumulative uncompressed spine size through this entry")]];
    s16 tocIndex [[comment("Index into TOC, or inherited/previous TOC index when no direct entry exists")]];
};

struct TocEntry {
    String title [[comment("Chapter/section title")]];
    String href [[comment("Resource path")]];
    String anchor [[comment("Fragment identifier")]];
    u8 level [[comment("Nesting level")]];
    s16 spineIndex [[comment("Index into spine (-1 if none)")]];
};

struct BookBin {
    u8 version;
    if (version != EXPECTED_VERSION) {
        std::error(std::format("Unsupported version: {} (expected {})", version, EXPECTED_VERSION));
    }

    u32 lutOffset [[comment("Offset to lookup tables")]];
    u16 spineCount;
    u16 tocCount;

    Metadata metadata;

    u32 currentOffset = $;
    if (currentOffset != lutOffset) {
        std::warning(std::format("LUT offset mismatch: expected 0x{:X}, got 0x{:X}", lutOffset, currentOffset));
    }

    u32 spineLut[spineCount] [[comment("Spine entry offsets")]];
    u32 tocLut[tocCount] [[comment("TOC entry offsets")]];

    SpineEntry spines[spineCount];
    TocEntry toc[tocCount];
};

BookBin book @ 0x00;

u32 fileSize = std::mem::size();
u32 parsedSize = $;
if (parsedSize != fileSize) {
    std::warning(std::format("Unparsed data detected: {} bytes remaining at offset 0x{:X}", fileSize - parsedSize, parsedSize));
}
```

## `section.bin`

Whole-card preparation also creates `sections/<spine>.p<8-hex-signature>.bin`.
These files use the same section header and page format as ordinary layouts;
the signature covers the font/content, viewport and reading layout (including
paragraph indentation). The reader validates every header field before reuse.
They remain outside the four ordinary `<spine>.bin[.1-.3]` slots. Preparation
copies a matching existing slot to its named path, including valid partial files.
Normal reading keeps a primary mirror for existing page-count/LUT progress probes;
the named layout remains available after normal reader-slot eviction. Copies use
a `.prep.tmp` staging file and the existing `.swap` recovery convention.
Cancellation keeps saved named layouts; cache clearing removes them with the
book's other section files. A `.part` suffix is an uncommitted build, as usual.
Preparing more families or layouts consumes additional card space, not more
simultaneously loaded fonts. The binary section version is unchanged.


### Version 75

Version 75 uses percentage line heights (100, 110, 120, 130, 145, 160 and 175%
of the font size). The settings key is `lineHeight`; the old `lineSpacing`
indices migrate to 110, 120, 130 and 145%, respectively. Old section caches
rebuild automatically. Reading progress, statistics and goal journals are not
changed.

After the TextBlock block-font ID and before `hasWordFonts`, a `u8 hasDropCap`
flag optionally adds `s32 fontId; u16 scale256; s16 inset, baseline, height;
u8 prefixBytes, style`. The complete NFC first word and its original source
range remain in the text arena. This annotation only changes glyph rendering
and the selection rectangle; it does not remove the initial from copied text.
Its allocation is included in the speculative page-read budget.

### Version 74

The header retains the CrossNext forced-bold and spacing fields. Completed and
partial caches from older versions rebuild automatically; metadata, bookmarks,
reading progress, goals and statistics are not reset. The reader layout signature
also changes because styled text can wrap differently.

After the existing serialized `BlockStyle` fields, each TextBlock adds an `s32`
block font ID (`0` means the section font), then a `u8 hasWordFonts` flag. When set,
three `s32` font IDs and `wordCount` slot bytes follow. Slot zero uses the block
font; slots 1–3 select the font table. The high bit records small caps. These bytes
are included in the speculative page-load memory budget.

Page tags 1–3 retain their meaning. Tag 5 is a bordered/shaded block slice:
`s16 x, y; u16 width, height;` four pairs of `u8 width, style` (top, right,
bottom, left), followed by `bool shaded`. Tag 4 decodes an initial-letter glyph:
`s16 x, y; s32 fontId; u16 scale256; u8 style, byteLength; char text[byteLength]`.
Initial letters currently remain in normal logical text rather than generating
tag 4, preserving lookup, copying and source ranges.

Chapter event-cache version 2 excludes chapters containing tables. Their column
planning needs the raw XML parser; ordinary chapters still replay cached events.
Saved page caches for table chapters are reused normally.

### Version 57

The binary structure is unchanged. English/Spanish-only hyphenation patterns and
language-neutral emergency splitting of apostrophe/explicit-hyphen words change
line breaks. Completed and partial chapter caches rebuild automatically so old
and new layouts cannot mix. Book metadata and reading progress remain intact.

### Version 56

The section header adds `forceBold` after `wordSpacingPercent`. The resolved
reader weight participates in cache validation and the clipping-layout signature.
Completed and partial caches from older versions rebuild automatically; book
metadata and reading progress remain intact.

### Version 55

Each TextBlock adds a uint16 `paragraphStartWord` after `textBytes`. It is the
visual index of the paragraph's first logical word, or `UINT16_MAX` for a
continuation line. Clipping uses this marker independently of source-offset
gaps. Older completed and partial section caches rebuild automatically;
book metadata and reading progress are kept.

### Version 54

The serialized layout is unchanged. Word source ranges and split offsets now
include codepoints absorbed by NFC composition. The high bit of each word's
style byte marks a discretionary hyphen, so clipping can remove it independently
of source length. Rebuild completed and partial section caches to correct
clipping spaces and anchors for decomposed text.

### Version 53

Each TextBlock arena starts with one 8-byte source range per word (two uint32
chapter-visible Unicode-codepoint offsets, start inclusive and end exclusive).
Ranges follow words through BiDi ordering and line wrapping. This version also
includes the version 52 redaction layout changes. Older completed and partial
section caches are rebuilt automatically; book and progress files are kept.

### Version 52

The serialized layout is unchanged. Missing full-block (`U+2588`) and black-square
(`U+25A0`) symbols now use font-sized solid rectangles instead of replacement
glyphs. Rebuild older sections so cached line breaks and word positions match
their new widths.

### Version 50

The header adds `paragraphIndentSpaces` after `extraParagraphSpacing`. The value
participates in cache validation, so sections with different indentation settings
are rebuilt. Version 49 was used by pre-release builds with a different header
layout and is skipped to prevent reuse of those caches.

### Version 48

Version 48 keeps the version 47 serialized layout unchanged. It was bumped
because Hangul text no longer has implicit line-break opportunities between
syllables: Korean words wrap at spaces (like CSS `word-break: keep-all`), and
with hyphenation enabled a word may also split at the end of a line wherever the
CJK line-breaking rules allow, without an inserted hyphen. Justification stretches only word spaces. Cached line breaks and word
positions from version 47 no longer match.

### Version 47

The section header adds signed `characterSpacing` (pixels) and unsigned
`wordSpacingPercent` after `focusReadingEnabled`; both participate in cache
validation. Each TextBlock's BlockStyle stores only `characterSpacing` after
`directionDefined`. Word spacing is resolved into cached word positions during
layout. Sections from earlier versions are rebuilt.

### Version 46

Version 46 keeps the version 45 serialized layout unchanged. It was bumped
because ordered lists now number their items, `list-style-type: none`
suppresses list markers, and `<ul>`/`<ol>` containers contribute their own
margins and padding to child block insets, changing cached word contents and
page layout.

### Version 45

Version 45 keeps the version 44 serialized layout unchanged. It was bumped
because internal EPUB links now preserve CSS superscript and subscript styles,
changing their cached word-style flags and page layout.

### Version 44

Each file in `sections/*.bin` stores one laid-out spine section. The header is
also the cache-busting key: if any layout-affecting setting differs from the
current reader settings, the section is discarded and rebuilt.

Version 44 appends the internal-link rectangles produced during text layout to
each serialized page. The reader uses these rectangles for touch navigation;
older caches are rebuilt because they contain no link geometry.

Version 43 keeps the version 42 serialized layout unchanged. It was bumped
because paragraph base direction now excludes direction changes from inline
elements.

Version 42 keeps the version 41 serialized layout unchanged. It was bumped
because closing a block now strips inherited vertical margins and padding.

Version 41 keeps the version 40 serialized layout unchanged. It was bumped
because simple HTML table rows are now laid out as positioned columns rather
than flattened paragraphs with synthetic row/cell labels.

Version 40 keeps the version 39 serialized layout unchanged. It was bumped
because ruby groups now remain intact when large text blocks are soft-flushed.

Version 39 keeps the version 38 serialized layout unchanged. It was bumped
because image top margins are now clamped to keep full-height images within the
page viewport.

Version 38 keeps the version 37 serialized layout unchanged. It was bumped
because Focus Reading now permits line breaks at visible hyphens and dashes
and hyphenates focus-split words as a whole, changing cached page layout.

Version 37 increases the fixed-size footnote href field from 96 to 256 bytes.
This changes each serialized footnote record from 128 to 288 bytes, so older
section caches must be discarded and rebuilt.

Version 36 keeps the version 35 serialized layout unchanged. It was bumped
because ruby and justified text positioning and CJK line breaking now use
corrected word measurements, so version 35 cached page layouts no longer match.

Version 35 adds a header offset and a `uint32_t` entry per page for the
visible-text offset LUT. The other section LUTs remain unchanged.

Version 34 is binary-identical to version 33. The version was bumped because
word-gap suppression was narrowed to tokens glued together in the source: v33
dropped the gap between any two words meeting at a CJK break opportunity, which
collapsed the spaces between Hangul words, so v33 word positions no longer match
what the layout engine now produces.

Version 30 is binary-identical to version 29. The version was bumped because
Arabic contextual shaping changed text measurement (`getTextAdvanceX` now
measures the shaped visual text), so word positions cached by v29 no longer
match what `drawText` renders.

Version 28 introduced serialized word style bits for underline, strikethrough,
superscript, and subscript. The format also includes:

- cache-busting fields for paragraph alignment, hyphenation, embedded CSS,
  image rendering mode, and Focus Reading
- page offset LUT
- per-page visible-text offset LUT (zero-based Unicode codepoints in `<body>`)
- anchor-to-page map for fragment and footnote navigation
- paragraph and list-item LUTs retained for navigation and legacy sync fallback
- optional per-word Focus Reading split metadata
- per-page footnote entries
- serialized word style bits for underline, strikethrough, superscript, and
  subscript
- flat TextBlock word storage (v29): per-word arrays plus one shared
  NUL-terminated text blob, replacing v28's length-prefixed word strings. The
  on-disk order mirrors the in-RAM arena so the firmware reads a whole block
  payload with a single allocation and a single SD read

ImHex pattern:

```c++
import std.mem;
import std.string;
import std.core;

// Historical version-56 inspection pattern; see version 75 above for current additions.
#define EXPECTED_VERSION 56
#define MAX_STRING_LENGTH 65535
#define FOOTNOTE_NUMBER_LEN 32
#define FOOTNOTE_HREF_LEN 256

struct String {
    u32 length [[hidden, comment("String byte length")]];
    if (length > MAX_STRING_LENGTH) {
        std::warning(std::format("Unusually large string length: {} bytes", length));
    }
    char data[length] [[comment("UTF-8 string data")]];
} [[sealed, format("format_string"), comment("Length-prefixed UTF-8 string")]];

fn format_string(String s) {
    return s.data;
};

enum PageElementTag : u8 {
    TAG_PageLine = 1,
    TAG_PageImage = 2,
    TAG_PageHorizontalRule = 3
};

enum WordStyle : u8 {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3,
    UNDERLINE = 4,
    STRIKETHROUGH = 8,
    SUP = 16,
    SUB = 32
};

enum TextAlign : u8 {
    JUSTIFIED = 0,
    LEFT_ALIGN = 1,
    CENTER_ALIGN = 2,
    RIGHT_ALIGN = 3,
    NONE = 4
};

struct BlockStyle {
    TextAlign alignment;
    bool textAlignDefined;
    s16 marginTop;
    s16 marginBottom;
    s16 marginLeft;
    s16 marginRight;
    s16 paddingTop;
    s16 paddingBottom;
    s16 paddingLeft;
    s16 paddingRight;
    s16 textIndent;
    bool textIndentDefined;
    bool isRtl;
    bool directionDefined;
    s8 characterSpacing;
};

struct TextBlock {
    u16 wordCount;
    u8 hasFocus;
    u16 textBytes [[comment("Total size of text[], including one NUL per word")]];

    if (wordCount > 0) {
        u16 textOff[wordCount] [[comment("Byte offset of word i's text within text[]")]];
        s16 wordXPos[wordCount];
        if (hasFocus != 0) {
            u16 wordFocusSuffixX[wordCount] [[comment("Suffix x offset from word start")]];
        }
        WordStyle wordStyle[wordCount];
        if (hasFocus != 0) {
            u8 wordFocusBoundary[wordCount] [[comment("UTF-8 byte boundary between bold prefix and suffix")]];
        }
        char text[textBytes] [[comment("All words back to back, each NUL-terminated")]];
    }

    BlockStyle blockStyle;
};

struct ImageBlock {
    String imagePath;
    String srcPath;
    s16 width;
    s16 height;
};

struct PageLine {
    s16 xPos;
    s16 yPos;
    TextBlock block;
};

struct PageImage {
    s16 xPos;
    s16 yPos;
    ImageBlock image;
};

struct PageHorizontalRule {
    s16 xPos;
    s16 yPos;
    u16 width;
    u8 thickness;
};

struct PageElement {
    PageElementTag pageElementType;
    if (pageElementType == TAG_PageLine) {
        PageLine pageLine [[inline]];
    } else if (pageElementType == TAG_PageImage) {
        PageImage pageImage [[inline]];
    } else if (pageElementType == TAG_PageHorizontalRule) {
        PageHorizontalRule horizontalRule [[inline]];
    } else {
        std::error(std::format("Unknown page element type: {}", pageElementType));
    }
};

struct FootnoteEntry {
    char number[FOOTNOTE_NUMBER_LEN];
    char href[FOOTNOTE_HREF_LEN];
};

struct Page {
    u16 elementCount;
    PageElement elements[elementCount] [[inline]];

    u16 footnoteCount;
    FootnoteEntry footnotes[footnoteCount];
};

struct AnchorEntry {
    String anchor;
    u16 page;
};

struct AnchorMap {
    u16 count;
    AnchorEntry entries[count];
};

struct ParagraphLut {
    u16 count;
    u16 paragraphIndex[count];
};

struct SectionBin {
    u8 version;
    if (version != EXPECTED_VERSION) {
        std::error(std::format("Unsupported version: {} (expected {})", version, EXPECTED_VERSION));
    }

    s32 fontId;
    float lineCompression;
    bool extraParagraphSpacing;
    u8 paragraphIndentSpaces;
    u8 paragraphAlignment;
    u16 viewportWidth;
    u16 viewportHeight;
    bool hyphenationEnabled;
    bool embeddedStyle;
    u8 imageRendering;
    bool focusReadingEnabled;
    s8 characterSpacing;
    u8 wordSpacingPercent;
    bool forceBold;

    u16 pageCount;
    u32 pageLutOffset;
    u32 anchorMapOffset;
    u32 paragraphLutOffset;
    u32 listItemLutOffset;
    u32 visibleTextLutOffset;

    Page pages[pageCount];

    u32 currentOffset = $;
    if (currentOffset != pageLutOffset) {
        std::warning(std::format("Page LUT offset mismatch: expected 0x{:X}, got 0x{:X}", pageLutOffset, currentOffset));
    }

    u32 pageLut[pageCount] [[comment("Page data offsets")]];

    if (anchorMapOffset != 0) {
        AnchorMap anchorMap @ anchorMapOffset;
    }

    if (paragraphLutOffset != 0) {
        ParagraphLut paragraphLut @ paragraphLutOffset;
    }

    if (listItemLutOffset != 0 && paragraphLutOffset != 0) {
        u16 listItemIndex[paragraphLut.count] @ listItemLutOffset;
    }

    if (visibleTextLutOffset != 0) {
	u32 visibleTextOffset[pageCount] @ visibleTextLutOffset;
    }
};

SectionBin section @ 0x00;

u32 fileSize = std::mem::size();
u32 parsedSize = $;
if (parsedSize != fileSize) {
    std::warning(std::format("Unparsed data detected: {} bytes remaining at offset 0x{:X}", fileSize - parsedSize, parsedSize));
}
```

## CLX1 — library index (`.crosspoint/library.idx`)

Written by `lib/LibraryIndex/LibraryBuilder.cpp`, read by `LibraryIndexFile`. One
file describing every book on the card, so the shelf can sort and search
thousands of titles without opening any of them.

Format version 4. Normal opening rejects older indexes; reconciliation can read version 3 to preserve arrival history before rebuilding.

### Layout

| Section | Offset | Contents |
|---|---|---|
| Header | 0 | 64 bytes, `ClixHeader` |
| Folders | `folderStart` | length-prefixed paths, one per folder |
| Records | `recordStart` | `bookCount` × 128-byte `ClixRecord` |
| Permutations | `permStart` | `bookCount` u16 entries in each of five orders: author surname, arrival, author given name, series/volume, genre |
| Name blob | `nameStart` | per record: path hash, name, canonical author, title, source author, series, genre, volume key (see below) |

The arrival permutation runs oldest first using `firstSeen`, the preserved discovery counter. A newly copied book can retain an old FAT modification time, so that timestamp is used for metadata freshness rather than arrival ordering. Fold version 5 restores this discovery ordering; rebuilding preserves each existing first-seen value. Fold version 4 preserves leading articles in title sort and search keys.

Sections are 512-byte aligned so each starts on an SD block boundary.

Format version 4 adds three permutations and the series, genre and volume blob fields. Normal opening requires version 4. Version 3 is accepted only during reconciliation to preserve first-seen history, including for an empty index; it is then rebuilt. Metadata reuse requires the current format version. The index is disposable and separate from books and reading saves.

### Records are exactly 128 bytes

A fixed stride is what lets the reader seek straight to record *n* without an
offset table, and read a screenful in one 4 KB block. `static_assert` enforces it.

Each record carries `fold[96]`, the title normalised for search and sorting —
accents stripped, case dropped, leading articles preserved — and `authorKey[12]`,
the author's words folded and sorted so that "Victor Hugo" and "Hugo Victor" group as
one person. `authorKey` is a GROUPING key, not an ordering one: the shelf orders by
surname, derived separately from the display name.

The byte before the folded title records metadata extraction status: not
attempted, extracted, or failed. The final four bytes contain the packed FAT
modification date and time returned by SdFat. A zero timestamp is not trusted.
These fields occupy the alignment and reserved bytes from version 1, so the
record remains exactly 128 bytes.

The header's reserved byte 0 records the content-visibility policy. Reserved byte 1 is the card-signature version (1); bytes 2..9 and 10..17 store little-endian 64-bit, order-independent mixed and summed fingerprints of eligible path hashes and sizes. The record count participates in the comparison. This is a cache-freshness check, not a content checksum: edits retaining the same path and size require a manual refresh or an app dirty marker. Older indices without the signature are reconciled once; the header remains 64 bytes. An unavailable directory check preserves the existing index for retry.

The header records whether EPUB metadata extraction was enabled for the build.
This prevents a metadata-disabled rebuild from making filename fallbacks look
fresh to a later metadata-enabled build.

### The name blob

Per record, at `nameStart + nameOff`:

```text
[u64 pathHash]    FNV-1a fingerprint of the complete path
[nameLen bytes]  filename, without the directory
[u8][author]     display author, one spelling chosen per authorKey across the library
[u8][title]      the book's own title, or length 0 if it never gave one
[u8][source]     cleaned author spelling before the library-wide spelling vote
[u8][series]     EPUB series name (at most 95 UTF-8 bytes)
[u8][genre]      first nonempty EPUB subject (at most 95 UTF-8 bytes)
[u8=8][volume]   eight ASCII hexadecimal digits encoding a sortable IEEE-754 float key; FFFFFFFF means unknown
```

The filename must stay the first textual field and stay the filename: `readPath`
rebuilds a book's path from it, so writing the display title there makes the book
impossible to open. That was a real defect, and it is why title has its own field.

The source author is separate from the displayed canonical author so a later
rebuild can repeat the spelling vote after books are added or removed. Existing
display reads still stop at the author or title fields and retain their offsets.

### Freshness and unchanged rebuilds

Reconciliation treats the persisted 64-bit complete-path fingerprint as the
book identity. Metadata is reused only when the fingerprint, size, nonzero FAT
timestamp, fold version, metadata mode, and expected extraction status agree.
EPUBs with a zero timestamp or a previous extraction failure are parsed again.

If every current record reuses metadata, the old and new counts agree, and no
unreadable entry was seen, the staging files are discarded and the live index is
left byte-for-byte unchanged. A normal rebuild action is therefore a freshness
check, not a forced metadata reread.

### Header flags

`RANKS_DEGRADED` says one or more orders fell back to walk order because a
checked sort allocation failed. Title and author each use a phase-local
`SortKey[bookCount]` allocation (14 bytes per book, 57,344 bytes at the 4,096-book
format ceiling); the first array is released before the second is requested.
Sorting is therefore best effort through the full format limit rather than
being disabled at an arbitrary library size.

`DEDUP_DEGRADED` says a directory exceeded the fixed 1024-entry duplicate-key
buffer, or that its fallible 8 KiB allocation failed. The walk still indexes
every enumerated book; it only stops remembering additional identities for
duplicate-dirent detection, so a damaged FAT may expose duplicates but cannot
make a real book disappear.

`selfSize` is the expected file size. Comparing it against the real one is a free
truncation guard: a build cut short by a power failure cannot pass.

## Clipping store (`/.crosspoint/clippings/epub_<path-hash>.bin`)

Version 4 retains the version 3 header and page-local range fields. After each
record's layout signature it stores `startOffset` and `endOffset` (uint32 chapter
codepoint range, end exclusive; UINT32_MAX means unavailable), `syncRevision`
(uint64), `pendingUpload` (one byte), and a 65-byte NUL-terminated sync ID. The
chapter title, text length, and text follow. Versions 1–3 remain readable.

Stable IDs are saved before upload. A sibling `.deleted` file stores fixed
65-byte IDs awaiting server acknowledgement. Deletions are queued before the
local record is removed and retried on the next enabled manual sync. A `.bak`
file is recovered if power interrupted replacement of the main store.

Clipping header strings are limited to 4 KiB on both reads and writes. A failed
load leaves no usable index and disables writes until a successful load. The
index is allocated with checked, bounded growth and released on unload.

Book moves rename the store and its `.deleted` journal (plus recovery sidecars)
together. The stored source path is informational and is refreshed on the next
save; the current file path selects the store. Local book deletion cleans up all
of these sidecars but preserves the independent `My Clippings.txt` export.


## Local Wordle and Pomodoro cycle stages

Local Wordle uses two 256-byte WDL1 records at /.crosspoint/wordle-local-{a,b}.bin. Bytes0..3 are magic,4..7 generation,8 chosen length,9 active,10 result recorded,11 vocabulary data version1,12..15 seed,16..21 three u16 bank positions. Bytes24..39 store played/won/current/max as u32;40..63 hold six u32 guess-distribution counts. Byte64 is active word length,65 serialized board length,66..114 hold ANSWER GUESS... (maximum48 characters),116 is partial typing length and117..122 typing bytes. Bytes252..255 are FNV-1a over0..251. Remaining bytes are reserved and written zero. All integers are little-endian, read bytewise. The newest valid slot wins; unknown future/wholly damaged records remain read-only. Legacy files are imported without deletion. See apps/wordle.md.

Pomodoro POM2 retains the 64-byte POM1 layout and paths. Byte15 now stores the cycle stage0..2*rounds-1; even stages are Focus, odd stages are Short Break except the final Long Break. POM1 load derives a stage from phase/completed sessions and pauses, retaining remaining time where compatible. POM2 validates phase against stage. Bytes60..63 remain the FNV-1a checksum over0..59. A higher format version remains read-only. See pomodoro.md.

## Focus FCS4

`/.crosspoint/focus-a.bin` and `focus-b.bin` alternate 13,856-byte verified files. Integers are little endian. Bytes 0–3 are `FCS4`, 4–7 generation, byte 8 active-task index (255 for none), and bytes 16–79 hold a Pomodoro record. The fixed 96-byte header is followed by 12 × 52-byte project records, 96 × 72-byte task records, seven × 20-byte daily records, and 14 × 12-byte aggregate records. Task byte 70 is list ownership (0 project, 1 Today, 2 Tasks); independent lists require project index 255. Daily fields are day, milliseconds, sessions, Today milliseconds and Today sessions, each serialized as u32, with session values validated to fit u16. Aggregate records contain deleted-completed task count, planned sessions and completed sessions.

At offset 7,940 follow 96 × 61-byte study records: u32 recorded seconds, u16 fractional milliseconds, u16 skipped focus stages, u32 hourly-record day, 24 × u16 hourly seconds and a u8 legacy-history-incomplete flag. Then 14 × u32 aggregate study seconds follow. The final u32 is FNV-1a over all preceding bytes. Each task retains only its latest dated hourly breakdown and its all-time total. Paused/break time is excluded; skipped stages do not earn sessions.

FCS1 (7,636 bytes), FCS2 (7,720) and FCS3 (7,944) remain readable. FCS1/2 tasks stay in projects; V3 list ownership and aggregates survive. Earlier formats have no per-task time, so migration marks it incomplete and starts recorded time at zero without fabricating history. The next changed save writes FCS4. Future versions and wholly damaged saves remain read-only. See [Focus](apps/focus.md) for deletion retention and device checks.

## CrossNext Anki zoom images

`images.dat` retains the v1 answer-only / v2 question-answer `XSTUDYI` format,
with 448×620 maximum images. Optional `images.zoom.dat` uses version 3 for
legacy one-bit data or version 4 for grayscale bytes (0 black, 255 white). Both
use the same 16-byte header and two 10-byte `<IHHH` records per card (offset,
width, height, row stride), with up to 2048×3072 pixels. V4 requires at least
`width` bytes per row; stride bit 15 is an optional high-contrast text hint,
and bits 0–14 hold the byte stride. V3 requires `(width+7)/8`. Only the explicit Zoom reader
accepts these versions. Both stores index the same cards/faces; absent entries
are zero. Normal card/Fit rendering never opens the zoom store. Import scratch
and partial files are removed on cancellation/failure.

## Reading Quest graphics, CNART002

The 24-byte little-endian header is `<8sIIII>`: magic `CNART002`, sprite count,
total size, CRC32 of index+pixels, and CRC32 of the index. Each 16-byte index
record is `<IHHII>`: offset, width, height, byte length and pixel CRC32. The
index checksum and all offsets/dimensions are checked at open; each sprite's
checksum is checked before its first draw. The 512-byte verified bitset and
activity-owned PSRAM index are discarded on close/failure. Legacy `CNART001`
retains its complete-file validation. Pixel data and sprite IDs are unchanged.


## `events-v1/<spine>.evc`

Optional EPUB chapter callback cache, independent of layout settings. The
XHTML cache remains the source of truth. All integers below are explicitly
little endian; the fixed header and records do not use native struct padding.

| Header field | Bytes |
| --- | ---: |
| Magic `EVC1` | 4 |
| Format version `2` | 2 |
| Header length `20` | 2 |
| Source XHTML size | 4 |
| Source XHTML modification time (HalFile/FAT representation) | 4 |
| CRC-32 of the preceding 16 bytes | 4 |

Each block contains a two-byte payload length (1–4096), a four-byte CRC-32 of
the payload, then the payload. CRC uses the reflected IEEE polynomial
`0xEDB88320`, initial/final XOR `0xFFFFFFFF`. A block is verified before its
records reach layout. Records can cross block boundaries; their full payload
is assembled and checked before dispatch.

Records begin with kind (one byte) and payload length (two bytes, at most
4096). Kinds are: 0 end-of-stream (empty), 1 start element, 2 end element,
3 character data, 4 default-handler data. Start-element payloads contain an
attribute-count byte, a NUL-terminated nonempty name, and NUL-terminated
attribute name/value pairs (at most 32). End-element payloads contain a
NUL-terminated nonempty name. Text is raw UTF-8 without a trailing NUL.
Nesting is bounded to 256. A complete stream ends at depth zero, with no bytes
after its end marker. Each file is capped at 8 MiB.

Writers use `.evc.part`, one writer lease, and `Storage.replaceFile` after a
successful complete parse and a source metadata recheck. FAT replacement
removes the old target before rename; power loss may lose this optional cache,
which regenerates from XHTML. Suspended, malformed or oversized sources do not
publish a stream. Source size and timestamp are a stale-cache check, not a
cryptographic content identity. A format/parser-configuration change must bump
the version/path if the recorded Expat events are no longer interchangeable.
CSS is still resolved by the live parser and is not serialized in this file.

## PET generation history (`PTL2`)

The alternating `/.crosspoint/pet-v2/lives-a.bin` and `lives-b.bin` files are 640 bytes
with up to twelve 52-byte records. Each record stores generation (u32), accumulated
reading seconds (u64), ending epoch (i64), reading days (u16), species (u8), ending
reason (u8) and a 25-byte name. Three formerly reserved bytes at record offsets
49–51 now store calendar age in days (u16, little endian) and a known-age flag
(0 unknown, 1 recorded). Age is the ending local calendar day minus the birth
calendar day, clamped to 65535 days. It is separate from days with reading.

Previous PTL2 writers zeroed those bytes, so their ages remain unknown. PTL1
records likewise migrate with unknown age. The existing file size, record stride,
checksum, alternating-save and read-back contracts remain unchanged. Older
firmware can read these PTL2 files, but drops age metadata if it rewrites them.


### PET8 food-choice compatibility

PET8 keeps the fixed PET7 byte layout, length and checksum. Only the accepted
food-choice range at byte 72 expands from 0-5 to 0-8. The encoder keeps PET7 for
0-5 and writes PET8 for 6-8. The store recognizes both; unknown future versions
disable writes. Older firmware rejects PET8 rather than silently loading an
older PET7 meal balance. The required new breathing.bin has 1182 CNART003
records, appending to the original 1014 stable IDs.

## Reading Adventure campaign (`RQU6`)

`/.crosspoint/reading-adventure-v2/reading-quest-v6.dat` contains two alternating 512-byte records.
The writer verifies the new slot before publishing its sequence. First migration
uses a temporary file and rename; RQU1–RQU5 files remain available as backups.
A present corrupt newer file disables writes rather than loading an older format.

Bytes 0–315 retain the RQU5 profile fields (including name extension). Byte 316
holds the legacy-layout FNV-1a checksum, and byte 508 holds the full record FNV-1a checksum over
bytes 0–507. RQU6 magic replaces RQU5; no old writer can accept its record.
Little-endian extension offsets are:

| Offset | Value |
| --- | --- |
| 320, 328, 336, 344 | u64 observed reading cursor, adventure reading baseline, banked seconds, extra reward coins |
| 352, 356, 360 | i32 start local day, u32 seconds already read that day, u32 completed-book baseline |
| 364, 368, 372 | u32 encounter seconds, frozen victory coins, i32 claimed Monday local day |
| 376, 378 | u16 encounter index (0–719), u16 reading-XP remainder (0–5999) |
| 380–383 | u8 initialized, battle phase (reading/victory/story/complete), adventure state (active/unfinished/peace/setup), extension version (1) |
| 384–398 | 120 chapter-reward claim bits |
| 399 | u8 first adventure day contains new reading, for multi-year streak bounds |
| 400–404 | u8 introduced-region count, extension schema (5), prologue acknowledged; u16 narrated-encounter cursor |
| 405–412 | u16 decoration ownership, two equipped decoration IDs, pending chapter item |
| 413–450 | 300 request/keepsake claim bits; high four bits of byte 450 are zero |
| 451–458 | u64 observed-reading cursor when the active request was accepted; zero when none |
| 459–460 | u16 active request ID 0–299, or 300 for none |
| 461–473 | 104 ownership bits for expansion decorations and royal equipment (item IDs 216–319) |
| 474 | u8 ending acknowledged (0 or 1) |
| 475–507 | reserved zero bytes |

Schema 5 retains schema 4 requests and schema 3 decoration fields and adds earned
expansion equipment and an ending acknowledgment. The legacy empty decoration
and pending-reward sentinel 216 maps to the current sentinel 320. Optional outfit,
background and PET slots keep the serialized empty marker 200. Stable item IDs
0–215 retain their meaning. Existing completed chapters and claimed requests
restore their new decorations without awarding EXP or coins again. Completed
campaigns also restore the four royal items and permit continued ordinary reading.

Schema 4 adds finite character requests.
Earlier schemas initialise an empty collection and no active request. Decode
rejects out-of-range IDs, a claimed active request, an unintroduced/incomplete
source chapter, or an acceptance cursor outside the campaign reading cursors.
A claim bit is also ownership of that task's unique keepsake. EXP, coins and its
bit are published in the same verified record. `RQU7` uses the same 512-byte
layout for names needing the Unicode name format.

Migration retains old reward bits and XP-derived wallet semantics but does not
apply the old mission list. Campaign reading begins at the current journal total.
Retirement saves one of two ending states; restart resets only this profile and
sets a fresh baseline against the unchanged reader journal.

The active PET and Reading Adventure storage namespaces are described in [save generation 2](apps/save-generation-v2.md). Binary format compatibility does not import files from the former generation.


### Reading Adventure RQUA / QBAL 5

The current record remains 8192 bytes. QBAL revision byte 7110 is 5. Offsets 7124–7659 contain 134 little-endian uint32 Hunt win counters indexed by the stable Hunt outfit order; the existing whole-record CRC covers them. A nonzero counter requires its matching historical unique claim. Revisions 1–4 leave counters zero in memory, and an existing unique claim is interpreted as one victory when displayed or incremented. Revision 5 permits an active or paused rematch despite a historical claim; the stricter older-revision validation remains. Counters saturate at UINT32_MAX without disabling further play. Older firmware rejects CRC-valid newer QBAL records and preserves the journal. Dates, reading and reward values in the existing Hunt victory record describe the latest collected attempt. Unique equipment claims remain idempotent.

Home settings add homeCoverLoadingMask, a four-bit mask indexed Framed, Carousel, Carousel 2 and Bookshelf. Missing or invalid values use 8, enabling Bookshelf only. Zero is a valid explicit choice and is retained on save.
