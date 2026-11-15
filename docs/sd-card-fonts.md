# SD Card Fonts

CrossPoint supports loading additional fonts from the SD card, including fonts
with extended Unicode coverage (CJK, Cyrillic, Greek, etc.).

If CrossPoint enables external RAM on your device, you can load `.ttf`, `.otf`,
and `.ttc` files directly. Otherwise, convert them to `.cpfont` files first.
All devices can use `.cpfont` files.

Resources includes Noto Serif for book text at 12, 14, 16 and 18 pt, with regular,
bold, italic and bold italic styles, in `fonts/Noto Serif/`. Copy this complete
folder to the root of the microSD along with the other Resources folders.
The full reader family is loaded from `.cpfont` files rather than stored in
firmware flash. `tools_local/build_reader_fonts.py` regenerates these files
from the existing licensed source fonts using the shared converter.

An existing built-in Noto Serif reading selection automatically becomes the SD
Noto Serif selection; the point size and reading progress are preserved. Other
named SD families remain selected. If a font folder is missing or damaged,
the reader uses the resident Ubuntu 12 pt face without deleting the desired
family or size. Reinstall the complete folder and restart to restore it.
Ubuntu can also be selected explicitly as the built-in reading fallback.
Chapter layout caches regenerate once for the external reader fonts. Replacing
these generated files also invalidates layouts using their embedded content fingerprint.

Noto Serif remains available in **Appearance > Interface font**. Those small,
independent interface cuts stay in flash, together with Noto Sans, Ubuntu and
Jersey 25. Changing a reading font does not change the interface font.

Automatic language-specific hyphenation is included for English and Spanish.
Books in other languages remain readable; explicit hyphens and the emergency
long-word wrapping fallback still work, without those languages' pattern tables.
These book-language rules are separate from the interface language.
Cached EPUB chapter layouts regenerate after this update to use the current
line-breaking rules. Book files and saved reading progress are retained.

### Japanese reading: Noto Serif JP

Resources also includes `fonts/Noto Serif JP/NotoSerifJP[wght].ttf` and its
`OFL.txt`. Copy the complete folder to the microSD, restart, then select
**Noto Serif JP** from the book's **Font > Font family** menu. It works with
both EPUB engines on the X4 Pro and other devices with PSRAM/TTF support.
It is independent of the interface language and its fonts.

The original variable face supplies regular (400) and bold (700) weights;
italic emphasis uses the reader's synthesized oblique. EPUB text sizes are
rasterized on demand through the existing reading-font caches. The full TTF
stays on the microSD and is streamed, with the existing optional 1 MiB prefix
cache in PSRAM and bounded glyph caches. It adds no font data to firmware flash.
Japanese uses horizontal text flow; this font does not add vertical writing or
ruby layout support.

Regenerate the resource with
`python tools_local/build_japanese_reader_font.py`. The downloader checks a
pinned upstream revision and SHA-256 values, preserves the original font and
license, and updates `assets_local/sd-resources/font-sources.json`.
The normal resource and simulator builders include this folder automatically.

### Chinese and Thai reading

Resources includes **Noto Serif SC** (Simplified Chinese), **Noto Serif TC**
(Traditional Chinese) and **Noto Serif Thai**. Copy each complete family folder
under `/fonts/`, restart, and select it from the book's **Font > Font family**.
They work with both EPUB engines on PSRAM/TTF devices such as the X4 Pro, at
on-demand text sizes, independently of the interface language. Regular and bold
use 400/700 weights; italic and bold italic use synthesized oblique forms.
Large SC/TC files are streamed; the smaller Thai sources may be retained in
PSRAM when space permits. SC and TC retain the full, unmodified upstream variable fonts, including their
regional Han glyph forms. No font bytes are added to firmware flash.

Thai's two static TTF files retain the original character repertoire and add
build-time shaped consonant/vowel/tone clusters in a `CPLG` table. They require
the accompanying firmware, which reads these substitutions once per source
into PSRAM and reuses them for the lifetime of each loaded point size.
Regular and oblique share a source table; bold uses its own source table.
The table is bounded to 4096 pairs (32 KiB) per source and never falls back to
internal RAM. A malformed table or unavailable PSRAM rejects that font load,
allowing the existing reader-font fallback. Glyph caches and text-size handling
remain shared with the other reading fonts. Text remains Unicode for searching
and dictionary lookup. Emergency line wrapping protects Thai clusters without
inserting Latin hyphens; dictionary-based Thai word segmentation is not added.
These fonts use horizontal flow; they do not add vertical layout or ruby.

Regenerate all three with
`python tools_local/build_multilingual_reader_fonts.py` (fontTools and
uharfbuzz are build-time dependencies). Downloads use a pinned Google Fonts
revision and verified SHA-256 values. The resource manifest records upstream
and prepared-file hashes, and each folder includes its OFL license and notes.
The SC/TC/Thai font files total approximately 43 MiB on the microSD.

## Installing Fonts

There are three ways to install `.cpfont` fonts. Direct TTF/OTF/TTC loading uses
the manual SD card copy method below.

### Option 1: Download from device (recommended)

1. Connect your CrossPoint reader to Wi-Fi
2. Go to **Settings > Reader > Manage Fonts**
3. Browse available font families and tap to download
4. Downloaded fonts appear immediately in **Settings > Reader > Font Family**

### Option 2: Upload via web browser

1. Start **File Transfer** and connect through **Join Network** or **Create Hotspot**
2. Open the web interface URL shown on the reader
3. Navigate to the **Fonts** tab
4. Upload `.cpfont` files using the upload form

### Option 3: Manual SD card copy

1. Download font files from the
   [crosspoint-fonts repository](https://github.com/crosspoint-reader/crosspoint-fonts)
2. Copy font family folders to one of two locations on your SD card:

   - `/.fonts/` — hidden directory (preferred; keeps the SD root tidy
     when mounted on a desktop)
   - `/fonts/` — visible directory (use this if your OS hides dot-files
     and you'd rather see the folder in your file manager)

   Both roots are always scanned at boot and the results are merged: a
   family installed in `/fonts/` shows up even when `/.fonts/` also
   exists, and vice versa. The two roots only collide if the same family
   name appears in both — in that case the copy in `/.fonts/` wins and
   the duplicate in `/fonts/` is ignored.

       SD Card Root/
       ├── .fonts/                     ← Hidden root (preferred)
       │   └── Literata/
       │       ├── Literata_12.cpfont
       │       ├── Literata_14.cpfont
       │       ├── Literata_16.cpfont
       │       └── Literata_18.cpfont
       └── fonts/                      ← Visible root (equally valid)
           └── Merriweather/
               ├── Merriweather_12.cpfont
               └── ...

3. Insert the SD card and power on your CrossPoint reader

### Direct TTF/OTF/TTC fonts

If CrossPoint enables external RAM on your device, copy a `.ttf`, `.otf`, or
`.ttc` file directly into `/fonts/` or `/.fonts/`. The filename without its
extension becomes the family name. For example, `/fonts/Bookerly.ttf` appears as
`Bookerly` in **Settings > Reader > Font Family**.

For a family with separate styles, put its files in one subfolder. The folder
name becomes the family name. For example:

    /fonts/Bookerly/Bookerly-Regular.ttf
    /fonts/Bookerly/Bookerly-Bold.ttf
    /fonts/Bookerly/Bookerly-Italic.ttf
    /fonts/Bookerly/Bookerly-BoldItalic.ttf

The reader selects regular, bold, italic, and bold italic faces from the font
files. One regular file is enough; the reader derives missing styles. A `.cpfont`
file in the same family folder takes priority over TTF/OTF/TTC files, so keep
the two formats in separate folders. If both font roots contain the same family
name, the copy in `/.fonts/` takes priority.

Direct fonts use the standard reader sizes: 12, 14, 16, and 18 pt. The **Fonts**
tab in File Transfer accepts `.cpfont` files only. Copy direct fonts to the SD
card instead. To remove a loose font file, delete that file from the SD card.
The Fonts page can delete families stored in subfolders.

## Non-Latin Scripts Fallback in the User Interface

The language resource bundle supplies `languages/fonts/Interface_8.cpfont`,
`Interface_10.cpfont` and `Interface_12.cpfont` on the microSD. These size-matched
fonts cover the translated interface, including Thai, Japanese, Chinese,
Cyrillic, Arabic and Hebrew. Chinese uses the matching `Interface_SC_*.cpfont`
or `Interface_TC_*.cpfont` files for Simplified or Traditional glyph forms;
missing regional files fall back to the shared interface fonts. Only one set
of three sizes is loaded, independently of the book's selected font.
Thai catalogue consonant/vowel/tone clusters are shaped during resource generation
and stored as chained ligatures in CPFONT v5, which extends the ligature count
while retaining the v4 glyph layout. Ordinary reading fonts remain v4 and still
work; the new language fonts require matching firmware. The device keeps
the original Unicode translations and uses its existing ligature lookup; it does
not run an additional shaping engine. Rebuild the language fonts when adding Thai
text with new clusters, using `tools_local/build_language_fonts.py` and its
`requirements-language-fonts.txt` dependencies. These are interface subsets,
not complete Thai book fonts.
Install the whole `languages` folder and restart the device. The fonts
remain available when changing the selected reading family; they are not added
to flash or offered as book fonts.

Built-in UI fonts retain their appearance for strings they can render. When a
string needs other characters, the language font is tried first. Its glyphs are
a subset for interface translations, not a complete CJK book font. The selected
SD-card reading family remains a fallback for additional book titles and file
names. Without the language fonts, this reader-family fallback still works.

The fallback is **size-matched**. The built-in UI fonts render at 8 pt
(small/author lines), 10 pt (list rows) and 12 pt (book-cover titles, headers),
so CrossPoint loads your SD family at those sizes too and maps each UI font to
its same-size SD font. Non-Latin book names therefore appear at the same size
as the Latin text around them. A `.cpfont` family must contain files at sizes
**8, 10 and 12** for this UI fallback (in addition to the reader sizes 12–18) —
**missing these is the most common reason non-Latin UI text still shows
replacement boxes even though the book itself renders fine.** Any missing UI
size keeps showing boxes for that script at that size. Direct TTF/OTF/TTC
families use the same file at all three UI sizes.

These UI sizes only take effect if the reader-size font also matches one of
CrossPoint's built-in fallback-detection codepoints — see `kFallbackProbes` in
`src/SdCardFontSystem.cpp` for the current list. A script outside that list
gets no UI fallback even with the correct `.cpfont` sizes present.

The reading-size selector uses the nearest installed sizes to 12, 14, 16 and
18 pt, removing duplicates. Small UI sizes remain available to interface
fallback without adding extra steps to the reading selector.

When converting your own font, include the UI sizes:

    python3 lib/EpdFont/scripts/fontconvert_sdcard.py \
      MyCJKFont-Regular.otf \
      --intervals cjk \
      --sizes 8,10,12,14,16,18 \
      --style regular \
      --name MyCJKFont \
      --output-dir ./MyCJKFont/

What this means in practice:

- Select a non-Latin-capable SD font under **Settings > Reader > Font Family**
  (see [Installing Fonts](#installing-fonts) and the interval presets for
  `.cpfont` under [Converting Custom Fonts](#converting-custom-fonts)). That
  selection drives book content and supplies additional size-matched fallback
  in the UI beyond the characters supplied by the language fonts.
- Pure-Latin UI strings keep the crisp built-in font; only strings that
  actually contain a character the built-in font lacks are routed to the SD
  font.
- The fallback is per *string*, not per glyph: a mixed title such as
  `三体 Vol.1` renders entirely in the SD font (including the Latin part). If
  that SD font is a `Mono` family, the Latin portion will appear half/full
  width.
- If no SD reading font is selected, translated interface text can still use
  the supplied language fonts. Characters outside those fonts need an SD
  reading family covering their script; otherwise replacement boxes remain.

## Available Pre-Built Fonts

The current list of pre-built fonts is maintained in the
[crosspoint-fonts repository](https://github.com/crosspoint-reader/crosspoint-fonts).

## Converting Custom Fonts

To make `.cpfont` files for any device, convert your TrueType/OpenType fonts:

### Prerequisites

    pip install freetype-py fonttools

### Single font (one style)

    python3 lib/EpdFont/scripts/fontconvert_sdcard.py \
      MyFont-Regular.ttf \
      --intervals latin-ext \
      --sizes 12,14,16,18 \
      --style regular \
      --name MyFont \
      --output-dir ./MyFont/

### Multi-style font

    python3 lib/EpdFont/scripts/fontconvert_sdcard.py \
      --regular MyFont-Regular.ttf \
      --bold MyFont-Bold.ttf \
      --italic MyFont-Italic.ttf \
      --bolditalic MyFont-BoldItalic.ttf \
      --intervals latin-ext \
      --sizes 12,14,16,18 \
      --name MyFont \
      --output-dir ./MyFont/

> **Using this font for non-Latin UI fallback too?** The examples above only
> include the reader sizes (12–18). To also get fallback for this font's
> script in the library, headers, and file browser (see
> [Non-Latin Scripts Fallback in the User Interface](#non-latin-scripts-fallback-in-the-user-interface)),
> add the UI sizes: `--sizes 8,10,12,14,16,18`.

### Available Unicode interval presets

| Preset | Coverage |
|--------|----------|
| `ascii` | U+0020–U+007E (Basic Latin) |
| `latin1` | U+0080–U+00FF (Latin-1 Supplement) |
| `latin-ext` | European languages (Latin + Extended-A/B + punctuation + currency + letterlike symbols such as ™ and № + minus sign + ligatures) |
| `greek` | Greek + Extended Greek |
| `cyrillic` | Cyrillic + Supplement |
| `hebrew` | Hebrew + Alphabetic Presentation Forms |
| `arabic` | Arabic + Supplement + Extended-A + Presentation Forms A/B (RTL, contextual shaping) |
| `georgian` | Georgian + Georgian Supplement |
| `armenian` | Armenian |
| `ethiopic` | Ethiopic + Extended |
| `vietnamese` | Vietnamese subset (ơ/ư and combining marks) |
| `ipa-chars` | IPA Extensions + Spacing Modifier Letters (phonetic transcription) |
| `punctuation` | General punctuation (U+2000–U+206F) |
| `cjk` | CJK Unified Ideographs + Hiragana + Katakana + Fullwidth |
| `hangul` | Korean Hangul syllables + Jamo + Compatibility Jamo |
| `cherokee` | Cherokee (historic + supplement block) |
| `tifinagh` | Tifinagh |
| `symbols` | Math, currency, arrows, box-drawing, misc symbols, dingbats |
| `reading` | Literary fiction coverage: Latin, Greek, Cyrillic, math/symbol blocks, supplemental punctuation, and CJK quote marks |
| `builtin` | Matches the firmware's built-in font conversion intervals |

Combine presets with commas: `--intervals latin-ext,greek,cyrillic`

You can also specify arbitrary Unicode ranges directly:
`--intervals 'latin-ext,(0x2190-0x21FF)'`

To list all presets with codepoint counts:

    python3 lib/EpdFont/scripts/fontconvert_sdcard.py --list-presets

### Additional options

`--force-autohint` — force FreeType's auto-hinter instead of the font's native hinting (useful when a font's built-in hints produce poor results at small sizes).

Install custom fonts via the web interface or manual SD card copy.

### Compact reading sizes

All vector families, including PT Serif, use four reading steps: 12, 14, 16 and
18 pt, matching the standard family sizes in CrossNext 1.4.0. Bitmap families
use the nearest installed sizes to those four steps, removing duplicates. The
resident emergency fallback retains its single 12 pt face. Installed font files
are preserved; only the selectable reading list is reduced.


## CrossNext 2.0.0 font distribution

Book-font families live in Resources `/fonts` rather than occupying the full
resident book-font set. Resources also supplies optional Noto Sans regular,
bold, italic and bold italic faces with their OFL licence. Install a complete
family folder. Small Noto Sans/Noto Serif UI cuts stay resident; changing the
reading font does not change the interface font. Ubuntu is the resident fallback.
