# Dictionary

Look up individual words while reading an EPUB using an offline StarDict dictionary stored on the SD card. Selecting two or more words offers Save quote instead.

## Supported Format

The reader supports **StarDict** dictionaries. When searching for dictionaries online, look for "StarDict format" or files with `.dict`, `.idx`, and `.ifo` extensions.

A dictionary folder must contain:

- `.idx` — word index (required, **must be uncompressed** — a `.idx.gz` will not work; decompress it on your computer with `gzip -d` first)
- `.dict` or `.dict.dz` — definition data (`.dict.dz` is supported as-is; entries are decompressed on the fly during lookup)
- `.syn` — synonym index (optional; maps alternate spellings and irregular forms to their headword)
- `.ifo` — metadata (optional)

Not supported: dictionaries with 64-bit index offsets (`idxoffsetbits=64` in the `.ifo` — rare, and rejected with an error).

> [!NOTE]  
> If you use .dict.dz and Crosspoint failed to load your dictionary with error message "Not enough memory", try unzipping it.

## Setting Up a Dictionary

1. Copy your dictionary folder(s) to `/dictionaries/` on the SD card — one dictionary per folder, e.g. `/dictionaries/webster/webster.idx` + `webster.dict.dz`. A hidden `/.dictionaries/` folder (dot-prefixed) works the same way, for keeping it out of the file browser.
2. Open **Settings → Reading** on the device.
3. Choose **Define: monolingual dictionary** for definitions and **Translate: bilingual dictionary** for translations. Each selector can use a different dictionary, or **None** to disable that action.

The two selectors are always visible. If no usable dictionaries are installed, selecting either shows installation instructions. After copying a dictionary, reopen Reading settings to refresh the list. Folders containing more than one dictionary (multiple `.idx` stems) are skipped as ambiguous. Folder names must fit 31 UTF-8 bytes.

CrossNext fills an unassigned dictionary role from installed bundled resources.
An existing saved dictionary name is retained, including names saved with an old
automatic-selection flag. Explicit **None** remains disabled. Definitions and
translations are independent; changing the interface language never replaces an
assigned name. See [SD resources](sd-resources.md) for the available defaults.

A dictionary result does not automatically add a card. Choose **Save word** to
keep its meaning, original sentence and book location in Words; **Undo** removes
that capture. Define uses Learn words, while Translate uses Learn languages.
Failed lookups cannot create empty cards. An SD write failure is reported.

## Looking Up Words

Two ways to start a lookup while reading:

- Open the reader menu (**Confirm**) and choose **Look Up**.
- Or set **Settings → Controls → Long-press Menu** to "Dictionary", then hold **Confirm** (~0.4s) on the reading page.

### Selecting Text

**On button-driven devices:**

1. A selection cursor appears on a word. Use **Left/Right** to navigate between words in reading order, and the side **Up/Down** buttons to jump between lines or flip across nearby pages.
2. Press **Confirm** (**Select**) to anchor the start of the selection.
3. **Single word:** Press **Confirm** again to open the selection actions.
4. **Phrase / multiple words:** Use **Left/Right** or **Up/Down** to extend the selection, then press **Confirm** to open the actions.
5. A single word offers **Define** and **Translate**. Two or more selected words offer only **Save quote**.
6. **Back** first clears an anchored selection; press it again to return to reading.

**On touchscreen devices:**

- Hold a word to show **Define** and **Translate**. Extend the selection to two or more words to show only **Save quote**. The touch toolbar and physical-button popup use the same rule.

On the very first lookup with a dictionary (and again whenever the `.idx` or `.syn` source file changes), the reader shows *"Indexing dictionary…"* while it builds small sidecar files next to them — a `.qidx` for the word index, and a `.sidx` when a `.syn` synonym file is present. Each sidecar is rebuilt independently, only when its own source changes. This takes a few seconds for large dictionaries and makes all subsequent lookups fast. The sidecars can be deleted safely at any time — they will simply be rebuilt.

### How Lookup Works

1. **Direct match** — the selected word is searched as-is (case-insensitive) in the dictionary index. Leading and trailing punctuation and whitespace are stripped automatically.
2. **Synonyms** — on a miss, if the dictionary ships a `.syn` file, alternate spellings and irregular forms recorded there are resolved to their headword (e.g. `oxen` → `ox`, `colour` → `color`). This step is skipped if the `.sidx` sidecar could not be built (e.g. transient low memory during indexing); the dictionary otherwise stays usable, and the build is retried the next time it is opened.
3. **Stemming** — still no match: common English word forms are retried automatically: possessives and plurals (`dogs` → `dog`, `stories` → `story`) and verb endings (`walked` → `walk`, `running` → `run`, `making` → `make`).
4. **Not found** — a short popup appears and you return to word selection.

## The Definition Screen

When a word is found, the definition screen shows the matched headword at the top and the definition text below, with a page counter for long definitions.

HTML dictionaries that declare `sametypesequence=h` use the EPUB text-layout engine for semantic formatting such as headings, bold, italics, lists, and line breaks. Images and CSS styling are ignored. Definitions that are too large or cannot be laid out within the available memory fall back to plain text.

Dictionary layout uses compact line spacing without the reader's extra paragraph spacing or focus-reading emphasis. These dictionary-specific choices do not change book typography. The header uses the shared back arrow and a separate **Save to Words** action.

The resource builder compacts all bundled HTML dictionaries in its generated copies. The monolingual dictionaries keep numbered meanings, lexical relations and examples supplied by the source. Web styling, pronunciation (including regional AFI transcriptions), syllabification, repeated headword/inflection rows and etymology metadata are omitted so the first meaning follows the word category. Examples and synonyms are never invented. Long entries can still require several pages. The bundled text renderer supplies the missing ♁ and 🜨 symbols without changing their meaning in the source.

The eight bundled WikDict translators show target-language equivalents together, grouped by the source's sense divisions. The part-of-speech labels use the destination language: EN–ES shows **Adjetivo: privado**, while ES–EN shows English equivalents and labels. Select the bilingual pair for the direction you need; the interface language does not reverse a manually selected pair. Source-language glosses and source gender metadata are omitted from these translation entries. These dictionaries supply word equivalents, not translated explanatory paragraphs or contextual sentence translation. Use a monolingual dictionary for definitions in its own language. Custom dictionaries keep their own content and language; the viewer does not automatically translate arbitrary dictionary text.

Run `python tools_local/build_resources.py` to regenerate the microSD package. Source dictionaries remain unchanged; generated `.idx` offsets and `.ifo` sizes are rebuilt while word order and `.syn` pointers remain intact. `SOURCES.json` records the source hash and the new output hash. Install the complete updated dictionary folders while the reader is powered off; merge the resource folders without removing `.crosspoint` or your saved words. The viewer spacing correction also applies to dictionaries already installed.

- **Left/Right** or side **Up/Down** — previous / next page
- **Back** — return to word selection



## Where to find dictionaries

> credit to https://github.com/koreader/koreader/wiki/Dictionary-support for the list.

- The [reader.dict](https://www.reader-dict.com) (ex "BoboTiG/ebook-reader-dict") project provides StarDict version of daily dumps of [Wiktionary](https://www.wiktionary.org/) monolingual dictionaries for a variety of languages. It also provides [non-free multilingual](https://www.reader-dict.com) dictionaries.
- The [WikDict](https://www.wikdict.com) project provides free bilingual dictionaries based on [Wiktionary](https://www.wiktionary.org/) for a lot of language pairs. StarDict versions can be [downloaded from here](https://download.wikdict.com/dictionaries/stardict/).
- The [`Vuizur/Wiktionary-Dictionaries`](https://github.com/Vuizur/Wiktionary-Dictionaries) repository contains dictionaries based on [Wiktionary](https://www.wiktionary.org/) from many languages to English, including English-English.
- The [DictInfo](https://www.dictinfo.com/) website provides outdated monolingual dictionaries based on [Wiktionary](https://www.wiktionary.org/).
- The [Firedict site](https://tuxor1337.frama.io/firedict/dictionaries.html) contains a list of freely available dictionaries.
- [wiktionary_stardict](https://xxyzz.github.io/wiktionary_stardict/): update monthly.
- [Fictionaries](https://fictionary.gumroad.com/) provides dictionaries for various speculative fiction books and series.
- [World Factbooks Archive](https://github.com/MilkMp/CIA-World-Factbooks-Archive-1990-2025) provides 36 years of CIA's World Factbook dictionaries in StarDict format.
- [StarDict-Hebrew](https://github.com/Uri-Tauber/StarDict-Hebrew) Hebrew-English StarDict versions of Babylon dictionaries.

## Definition readability

StarDict can store several adjacent index records for the same headword, for
example adjective and noun entries for “country”. Lookup includes those records
in dictionary order, separated in the viewer, within a shared 64 KiB / 64-record
budget. This also applies through a synonym or English stemming fallback.

The definition viewer adapts HTML block boundaries before layout, so a gloss,
translation and example do not run into one another. Known WikDict and Wiktionary
pronunciation-only blocks are omitted to keep the result focused on meanings.
Meaning and example content is preserved, and saving to Words
uses this cleaned content. Original dictionary files are never rewritten.
This formatting applies to dictionary results, not EPUB chapter content.

The header uses the same height as Games and Apps. Page arrows sit above a bottom
margin; horizontal swipes also turn definition pages. The configured Home action
on the circular button closes this viewer; a lookup opened from a book returns
to that book. Holding the circular button also closes the viewer when its action
is Reader bottom sheet. Other configured shortcuts and emergency touch unlock
keep their normal behavior.

Built-in Noto Serif includes Greek, polytonic Greek, and phonetic characters at
all four reading sizes and styles. An entry uses the selected reading font when
it covers the text. Otherwise the viewer chooses the nearest built-in Noto Serif
size when that supplies missing characters without losing characters supported
by the selected font. The book's font preference is not
changed. Characters absent from both fonts still use their normal replacement
glyph; this is not a universal font for every writing system.
