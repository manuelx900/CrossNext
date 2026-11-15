# Wordle vocabulary resources

Wordle checks guesses against the selected game's language and length (2–6
Unicode graphemes). The optional microSD bank is `/languages/wordle/CODE.cpwb`.
English and Spanish retain resident fallback dictionaries; their SD banks also
extend accepted guesses in unfinished resident games. Ook retains its resident
English game and has no separate natural-language dictionary.

The vocabulary expansion adds accepted guesses from English Wiktionary's
multilingual entries and inflection tables, extracted by
[Kaikki.org](https://kaikki.org/dictionary/) and
[Wiktextract](https://github.com/tatuylonen/wiktextract#format-of-the-extracted-data).
This does not mix dictionaries between languages. It preserves every prior
guess record and every answer record, including answer ordering. Existing
statistics and unfinished games remain compatible. The unchanged answer pools
avoid turning uncommon newly admitted inflections into surprise solutions.

The additional data excludes proper-name entries, extraction metadata, known
misspellings, transliterations, spaces, punctuation and unavailable keyboard
outputs. Spanish keeps its existing vowel-accent folding and distinct ñ.
Turkish/Azeri keep dotted/dotless I. Cyrillic stress marks and Arabic vowel
annotations are removed where the game uses the ordinary unvocalized spelling.
Japanese keeps Kana input; Chinese uses characters reachable through its
existing Pinyin candidate tables. Catalan/Valencian and Brazilian/European
Portuguese keep separate bank files and unchanged answer pools, with a shared
language source for additional guesses. Bosnian uses the Latin entries of the
Serbo-Croatian extraction.

Every addition must have its glyphs in all shipped interface-font faces. Thai
clusters must already be shaped in all faces, so this update does not replace
fonts, change old bitmaps, or introduce unshaped combinations. This is broader
dictionary coverage, not a guarantee that every real regional, specialist or
newly coined word is included. Native editorial review remains desirable.

## Installation and hardware checks

Exit Wordle and shut down before replacing microSD resources. Extract the
resource archive at the card root, preserving its `languages/wordle/` paths.
Replace the 38 `.cpwb` files and their accompanying manifest/notices together.
Keep `/languages/keyboards/` and `/languages/fonts/` from the matching language
resource bundle installed. Do not erase books, settings or `/.crosspoint/`.
Install the matching X4 Pro firmware from this vocabulary release as well. It
retains the unified firmware changes and adds 15 uppercase-to-lowercase mappings
needed by newly admitted words, including Vietnamese vowel forms and macrons.
Older firmware can load the banks, but does not recognize every uppercase form.

Resume an existing game and check its board and statistics. Start games of each
available length in the languages you use. Submit one newly admitted word from
the packaged manifest, with the matching length; it should be scored instead
of reported unknown. Hold alternate keys for accents and Cyrillic letters,
check Thai marks and Pinyin candidates, and repeat in portrait and landscape.
Verify that an arbitrary nonexistent string remains rejected. Missing bank,
missing glyphs, lost progress or an unusually slow guess check are failure
signs. Device latency, heap and power still require physical measurements.

## Reproducing the expansion

Use the repository Python environment and its existing `regex`, fontTools and
HarfBuzz dependencies. No dependency is added to the firmware. Preserve the
previous resource bundle as an immutable baseline; its archive SHA-256 and all
38 bank digests are pinned in `wordle-vocabulary-baseline.json`.

```text
python host-tests/wordle-vocabulary/export.py --out tools_local/wordle_sources/wordle-repertoire.json --thai-baseline PREVIOUS_BANKS/TH.cpwb
python tools_local/expand_wordle_languages.py --base PREVIOUS_BANKS --out NEW_STAGING_DIRECTORY --cache SOURCE_CACHE --case-header src/apps_local/wordle/WordleCase.h --download
./bin/clang-format-fix -g
python host-tests/wordle-vocabulary/export.py --out tools_local/wordle_sources/wordle-repertoire.json --thai-baseline PREVIOUS_BANKS/TH.cpwb
python test/language_pack/test_wordle_expansion.py -v
python host-tests/wordle-vocabulary/run.py --base PREVIOUS_BANKS --banks NEW_STAGING_DIRECTORY
```

The exporter compiles the actual game keyboard and held-choice implementation.
Its test HAL only supplies storage, memory and language-selection contracts.
The exported output repertoire is pinned to the current keyboard source,
language packs, Pinyin tables, case map and serialized fonts. Any dependency
change invalidates the snapshot until it is regenerated and reviewed.
Regenerate it after the expansion updates the generated case header. Compile
the firmware with that header; the added constant pairs live in flash and do
not introduce new runtime buffers or allocations.

`wiktionary-sources.json` records each compressed source URL, extraction date,
size, SHA-256 and attribution. `--download` downloads absent snapshots; a changed
upstream file is an error, never an automatic vocabulary update. The compressed
source cache is approximately 2.1 GB; extraction streams JSONL without storing
the much larger decompressed files. The four default host workers use dynamic
sets outside the device; lower `--jobs` to reduce desktop memory use.

The staging directory must be new and separate from the baseline. A failed run
may leave partial staging files; they are not a release. Publish only after the
38-bank manifest exists and validation succeeds. Counts over the runtime limit
of 1,000,000 guesses per length fail the build. Existing records are copied
verbatim, additional guesses are UTF-8 byte-sorted, and checksums are rebuilt.
The base frequency-list builder remains the way to create an initial bank set;
running it alone does not reproduce this merged expansion.

CPWB v2 and the firmware lookup path are unchanged: opening validates a 64-byte
header and guesses use on-card binary search with record checksums. Larger SD
files do not create larger firmware buffers or whole-bank loads. At the existing
one-million-per-length bound a lookup needs at most 20 record comparisons;
this is a bound, not a measured device response time.

The resource archive includes Wiktionary contributor attribution and source
snapshot metadata under CC BY-SA 4.0, along with the retained FrequencyWords,
Leipzig and JMdict notices. See the source corpus
[copyright terms](https://en.wiktionary.org/wiki/Wiktionary:Copyrights).
