# Resource sources and redistribution

Verified on 2026-10-20. The resource collection retains independent component
licenses; it does not claim that the resources are public domain or relicense
them under the firmware's license. Keep the ZIP's `THIRD-PARTY-NOTICES.txt`,
`SOURCES.json`, full licenses and per-folder attribution when redistributing.

## Font selection

The six additions are Literata, Atkinson Hyperlegible, Merriweather, EB Garamond,
PT Serif and Inter. They complement the existing Bitter and Lexend Deca families.
Each family has four static TTF faces: regular, bold, italic and bold italic.
The six new families retain upstream binary bytes and their original OFL files;
no subsetting, variable-font instancing or internal renaming was performed.
The existing two families retain their inherited bytes and license notices.
`SOURCES.json` distinguishes inherited provenance from directly acquired releases.

These choices reflect recurring suggestions in the consulted community threads,
not a representative poll or an objective ranking. Most explicit device reports
refer to X4; the search did not establish a separate X4 Pro preference ranking.
Direct TTF loading in this package targets the supported PSRAM devices, including
X4 Pro. An X4 firmware without vector-font support needs converted `.cpfont`
files instead; this ZIP does not provide those conversions.

Community evidence:

- [Favorite fonts, r/XTEINK](https://www.reddit.com/r/XTEINK/comments/1u33cby/whats_your_favorite_font/): Literata, Atkinson Hyperlegible and PT Serif appear among the suggestions.
- [Favorite fonts, r/xteinkereader](https://www.reddit.com/r/xteinkereader/comments/1u33erx/whats_your_favorite_font/): mentions include Literata, EB Garamond, Merriweather and Inter.
- [Fonts/settings to try](https://www.reddit.com/r/xteinkereader/comments/1u2bb3q/what_fonts_or_settings_should_i_try/): includes Atkinson Hyperlegible on X4 and Inter.
- [Fonts for Xteink](https://www.reddit.com/r/xteinkereader/comments/1vjagpo/fonts_for_xteink/): includes Merriweather.

Bookerly is also frequently recommended in those threads. It was not added:
this work did not establish an open license authorizing its redistribution.
Popularity and availability of a download are not redistribution permission.

Primary font sources:

| Family | Upstream release/source | License |
| --- | --- | --- |
| Literata | [googlefonts/literata](https://github.com/googlefonts/literata), static `fonts/ttf`, pinned commit in SOURCES.json | OFL 1.1 |
| Atkinson Hyperlegible | [Google Fonts original family](https://github.com/google/fonts/tree/main/ofl/atkinsonhyperlegible), four static TTFs | OFL 1.1 |
| Merriweather | [SorkinType/Merriweather](https://github.com/SorkinType/Merriweather), static `fonts/ttf`, pinned commit | OFL 1.1 |
| EB Garamond | [octaviopardo/EBGaramond12](https://github.com/octaviopardo/EBGaramond12), static `fonts/ttf`, pinned commit | OFL 1.1 |
| PT Serif | [Google Fonts PT Serif](https://github.com/google/fonts/tree/main/ofl/ptserif), four static TTFs | OFL 1.1 |
| Inter | [Inter 4.1](https://github.com/rsms/inter/releases/tag/v4.1), four static release TTFs | OFL 1.1 |
| Bitter / Lexend Deca | Existing CrossInk reference bundle; upstreams [Bitter](https://github.com/solmatas/BitterPro) / [Lexend](https://github.com/googlefonts/lexend) | OFL 1.1 |

The [official OFL](https://openfontlicense.org/) permits software bundling subject
to its conditions. Retain copyright/license notices, keep the fonts under OFL,
do not sell the font software by itself, and respect any Reserved Font Names
when making modified versions. The packaged OFL files contain the exact notices.

## Dictionaries

Ten collections are included: English Simple and Spanish Wiktionary definitions,
plus English to/from Spanish, French, German and Portuguese. Each direction is
a separate StarDict collection. Data/index/synonym/.ifo bytes were copied without
entry edits. Existing monolingual and EN/ES bilingual files were checked against
their official download archives as well.

- [WikDict provider and licensing](https://www.wikdict.com/page/about): Wiktionary
  data via DBnary, processed by WikDict/Karl Bartel; eight bilingual archives from
  the [official StarDict directory](https://download.wikdict.com/dictionaries/stardict/), dated 2026-06-23.
- [xxyzz Wiktionary StarDict](https://xxyzz.github.io/wiktionary_stardict/): English
  Simple and Spanish definitions, release `20261018`, snapshot `2026-10-01`.
  Its dictionary generator identifies dictionary data as CC BY-SA 4.0 in `.ifo`;
  the [source code](https://github.com/xxyzz/wiktionary_stardict) is GPL-3.0-or-later.
  The two accompanying CSS source files retain that software license; their full
  source and GPL text are included. The converter program is not in the ZIP.

Dictionary data remains under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
Attribution names Wiktionary contributors and the relevant converters/publishers,
links to source entries/contributor histories and records the release. Preserve
these notices and identify future changes. Adapted dictionary data must use the
same or a permitted compatible license; do not apply DRM or extra restrictions
that defeat recipients' licensed rights. The CC license applies to the dictionary
data, not automatically to independent code merely packaged alongside it.

The package omits separate dictionary image archives. These checks establish
provider license statements and traceable sources, not a guarantee against every
possible third-party claim or an audit of all pre-existing firmware assets.

## Evidence files

`assets_local/sd-resources/{font-sources,dictionary-sources,existing-dictionary-sources}.json`
record URLs, archive members, pinned revisions where applicable, byte counts and
SHA-256 hashes. The builder verifies these records and includes them as
`SOURCES.json`. `MANIFEST.json` covers every packaged file.

Sources are grouped above as community preferences and primary provider/license
sources. Academic research was not used: the question concerns community choices
and the providers' actual redistribution licenses. The optional Parallel search
service lacked account credit; the research used the available web search and
official provider downloads instead.
