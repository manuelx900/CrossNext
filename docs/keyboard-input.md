# Text keyboards and reader controls

On X4 Pro, typing surfaces share white rounded character keys and black number,
delete and return keys. Shift is white when inactive and black when active.
Letters, numbers and symbols use separate pages. The globe retains the enabled
language layouts, including optional SD keyboard packs. Packs retain the CPKB
format; install the matching Languages resources to obtain the current mobile
distributions. Built-in languages use their firmware distribution even if an
older SD override exists. The shared geometry also keeps wide non-Latin rows
from overlapping Shift or Delete.

The shared text-entry keyboard refreshes after a key action changes the content, rather than refreshing the pressed-key highlight before inserting the character. This removes an intermediate panel update from a brief tap; it does not change the display waveform or promise an instantaneous physical refresh. Holds still open alternatives or clear the field using their existing thresholds. The same input path serves every built-in and SD language layout. Embedded keyboards already avoid press-only refresh requests and keep their existing routing.

When the built-in touch layout stays the same, typing and deleting reuse the
visible keys and their touch targets. Panels with windowed refresh update the
area above the keys. Changing layouts or opening the alternate chooser repaints
the keys.

Wi-Fi's **Show password** choice applies while editing as well as on the review screen. Returning from the keyboard preserves its chosen visibility; leaving cursor mode does not hide a visible password. Cancelling keeps the previous password and visibility. New password prompts start hidden. This is a transient UI choice: it does not add a stored setting, change credential storage or normalize password bytes. The changes add no heap allocation or framebuffer.

Hold a character to open its variants. Slide onto an option and release, or lift
the finger and tap the option. Language-specific variants precede the shared
accent fallback. English and Spanish retain their established key positions and
choice order. Nordic letters occupy their native rows; Czech, Hungarian,
Bosnian, Slovak and Slovenian use QWERTZ. Vietnamese vowels offer their complete
tone series. Japanese uses a twelve-key kana grid: tap a group for its first
kana, or hold it to choose a syllable, small kana or voiced form. Its Shift
label selects hiragana/katakana; there is no flick gesture or kana-to-kanji
conversion. Arabic, Persian, Cyrillic and Hebrew use their script-specific
variants. Existing Thai and other pack alternates remain available. Shift uses
the active layout's uppercase variants. Chinese continues using Pinyin spelling
and candidate selection; holding a spelling letter does not commit a Latin
accent. URL letters retain ASCII entry. A key without alternatives types its
normal character on hold. Holding Delete clears the input. Space, Return and the
letter-page label are localized for all shipped interface languages. Native
digits remain available by holding the corresponding ASCII digit.

KeyboardEntryActivity and EmbeddedKeyboard each own their chooser. Wikipedia
opens KeyboardEntryActivity; confirming returns to its result list, while
cancelling preserves the old query. It reuses the owner's interaction table and
framebuffer; there is no worker, SD access, persistent setting or additional
allocation per hold. Its bounded workspace is below 2 KiB per owner (measured by
the host input test), including up to twenty-four 15-byte outputs and eight
queued contacts. EmbeddedKeyboard holds it in its existing checked PSRAM
workspace. Missing keyboard/font packs keep the existing fallback/error paths.
Alternative glyphs use the existing interface fonts; install the matching SD
language resources for non-Latin input.



Input observed before the chooser is painted cannot select an unseen option.
An early release leaves it open. On closing, contacts that saw the chooser are
consumed until the normal keyboard has appeared; a fresh contact after that
paint can type immediately. Physical navigation cycles choices, Confirm selects,
and Back closes the chooser. These rules matter on e-ink, where painting may take
longer than the user's finger gesture.

Settings option lists retain their boxed sections, headings, values, checkboxes
and order. They add the silhouettes supplied in the design reference, through
SettingsRowIcon and generated SettingsOptionIcons masks. SVG sources and crop
provenance live in assets_local/settings-icons. Regenerate the masks with
`python tools_local/settings_art/build_option_icons.py`; no original screenshot
is needed for that step.

Settings > Reading > Reading > Manga groups the existing rotate-XTC-cover
and invert-XTC-direction options. Their stored values, JSON keys, cover rotation
behavior and reading gestures are unchanged; Back returns to the Reading list.

Reader bottom panels and Settings > Reading > Text settings accept horizontal
swipes on paginated font/layout/style views as well as their arrow buttons.
The Layout alignment box has larger icons and uses the space above the pager,
while retaining the reader text visible above the panel.

## Device checks

- In each orientation, open book search, rename a collection and enter a PET
  name. Confirm keys stay within the safe screen and the field remains visible.
- Switch languages with the globe. Hold a/e/n in Spanish, Shift+A, and a key
  with an explicit alternate in each installed non-Latin layout. Check that no
  character appears until a visible option is chosen, and it appears once.
- Release during a slow refresh, select by dragging, cancel with Back, then type
  immediately after the keyboard returns. Look for duplicate or missing input.
- Enter numbers, both symbol pages, currency, and backtick by holding ~. Check
  Pinyin candidates, long-delete, and existing name/byte limits.
- Swipe both Layout pages in a book and global Text settings. Check alignment
  icons/text and the upper reading preview in portrait and landscape.
- Open Reading > Manga, toggle both XTC options and return. Reopen Manga and
  restart to confirm the saved choices. Check the original Controls list no
  longer duplicates the XTC direction option.
- Inspect Display, Controls, Library and Connectivity in both header themes for
  missing icons or text/value overlap. No cache deletion or migration is needed.

Host tests verify geometry, font coverage and input ordering; they do not measure
physical touch latency or e-ink appearance.

## Verified symbol coverage

The shared number/symbol pages retain printable ASCII punctuation. Hold `?` or
`!` for inverted Spanish punctuation; `0` for degree/ordinal signs; quotes,
hyphen, period, arithmetic operators and currency keys for their related forms.
Arabic/Persian add their own punctuation and marks, Hebrew adds geresh,
gershayim and combining dots, and Japanese/Chinese offer their punctuation.
Hold an ASCII digit for the corresponding Arabic-Indic, Persian or Thai digit
when that language is active. Thai also retains its native digit keys; Persian
digits are reached through the corresponding ASCII-digit holds.

Vietnamese vowel holds include the modified vowels and their tone series:
18 choices for a/o and up to 24 choices in the bounded chooser. There is no
separate row of â/ă/ê/ô/ơ/ư keys or floating tone marks. Catalan l
also offers l·l. French û/ü are offered from u; the old pack aliases on q/w are
ignored when choosing variants. The SD pack format and files remain compatible.

The automated coverage audit checks every shipped language table, both letter
layers and symbol pages, and actual chooser outputs against the original pack
manifest and built-in tables. It also checks geometry in four orientations and
real interface-font metrics. This verifies the firmware's published characters;
it does not make the firmware a complete implementation of every iOS input
method. Japanese remains Kana entry and Chinese retains its existing Pinyin
candidate path, with no new prediction, handwriting or transliteration engine.
Existing language fonts must be installed for the matching non-Latin glyphs.

## Wordle game keyboards

Wordle's multilingual game keyboard shares the English/Spanish game's square
key faces, body type, Enter treatment, erase icon and correct/present/absent
marks. Display labels use uppercase where the script supports it, including
Turkish/Azeri dotted I and native Cyrillic/Vietnamese letters; the original UTF-8 key outputs and dictionary rules are
unchanged. Its letter widths use a common unit so Enter/Delete span two letters
for both resident and SD layouts; longer Enter labels can use more units. Script-specific rows, Shift, Pinyin candidates
and alternate-letter marks remain available. Held choices use square uppercase
game tiles. General typing and PET/Quest naming retain their existing rounded
keyboard and chooser presentation.

The erase key in every Wordle mode, including classic English/Spanish, uses the
same Lucide delete bitmap as the general keyboard, centered inside the game key.

## Keyboard layout selection

Settings > Controls > Keyboard layouts lists the ten built-in layouts and the
28 distinct optional language layouts. Ook shares English and does not add a
duplicate QWERTY entry. Optional layouts stay visible when their microSD file is
missing, marked Unavailable and disabled. If a native name cannot be rendered
without the interface font bundle, its stable language code is shown instead. Install the matching Languages bundle
to enable them. A selected interface-language layout and at least one resident
Latin keyboard remain enabled; existing saved mask assignments do not change.

The chooser is a vertical scrolling list with 52-pixel minimum rows, the smaller
interface font and 26-pixel checkboxes. There is no page bar or page counter.
Vertical swipes move by at most three rows, preserving overlap; button navigation
uses the same list. Scrolling keeps the button selection in the visible viewport.
Opening or scrolling does not write settings; actual edits save once on exit.
Only visible rows are measured and drawn. Missing resources never become active
keyboard choices merely because their catalogue entry is visible.

Persian heh offers its heh-plus-hamza ezafeh form alongside the retained legacy
forms. General text entry in Persian offers Half-space by holding minus on the symbol
page. Its output is U+200C, which the existing Arabic shaper already understands;
the popup uses a translated visible label with a measured key width. URL fields
and the letter-only PET/Quest/Wordle inputs keep their existing input grammar and
do not offer this separator. No font data or saved-name format changes.

On the device, verify all four rotations and both header themes. Scroll to the
last optional layout, toggle it, return, reopen and cycle to it with the keyboard
globe. With a missing card file its row must say Unavailable and refuse a toggle.
Swipe across an initially pressed checkbox: scrolling must not also toggle it.
After touch scrolling, Confirm must act on a visible row; test remapped buttons
and held navigation too. In a Persian search enter text with Half-space and
check its joining and preservation on confirmation.

Ukrainian exposes all 33 letters in both cases, including ґ/Ґ by holding г/Г.
The symbol-page apostrophe also offers the modifier apostrophe ʼ, accepted by
the shared naming validator and preserved by both app codecs. Thai retains
Kedmanee's four character rows and Shift layer; all 87 assigned Thai block
characters are reachable. Hold nikhahit ํ for yamakkan ๎, and paiyannoi ฯ for
fongman ๏, angkhankhu ๚ and khomut ๛. Mark choices show a dotted-circle carrier.
The matching interface font bundle adds those punctuation glyphs and 46
pre-shaped consonant/yamakkan clusters, preserving the metrics, bitmaps and
kerning of every previous glyph and semantic Thai cluster.
