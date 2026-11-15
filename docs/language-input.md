# Language input

The initial system keyboard follows the selected interface language, including
when a different keyboard selection was saved previously. The language key
still allows a Latin layout; URL fields start with the Latin layout.

Install the complete Languages bundle at `/languages` on the microSD. Optional
layouts live in `keyboards`; interface fonts and Wordle vocabularies are in
`fonts` and `wordle`. Missing Wordle resources do not select an English game.

PET and Reading Quest show the name field and keyboard within their own screen.
Their naming screen retains the illustrated companion/hero card, outlined name
field with a live character count, rectangular keys and a full-width confirmation
button. The hero preview follows the selected sex. The language, alternate,
symbol and pinyin controls use the same keyboard input owner as the confirmation
button; changing the presentation does not change the saved name format.
Names retain the existing 24-byte storage limit and a maximum of 12 supported
letter units; combining marks on a letter count with that letter. The byte
limit can be reached earlier with multi-byte scripts. The counter and short
Delete use the same units; held Delete clears the field. Hold a character for
its alternatives. Japanese uses twelve kana groups with small/voiced forms and
a hiragana/katakana selector. Chinese uses the included per-syllable Pinyin
character candidates, not predictive phrases or Zhuyin. Vietnamese vowels
offer their precomposed tone series, including the modified vowels.

PET names outside the legacy uppercase-and-space grammar use the PET9 profile
tag; Reading Quest uses RQUA. Existing fields and profiles remain the same size. Older firmware can reject these tags or name characters added by later versions. Back up the microSD before
testing a downgrade.

Wordle retains its board, letter-state legend and letter keyboard in every
language. Numbers, spaces and the system keyboard's symbol controls are omitted.
Longer alphabets need additional letter rows; kana and Thai retain a Shift key.
See [Wordle](apps/wordle.md) for optional vocabulary limits and save behavior.

The package has technical coverage checks. Native editorial review of the
translations and optional vocabulary lists, and physical keyboard/layout tests
on the reader, remain necessary.

Run `python host-tests/name-entry/run.py` for naming geometry and confirmation
checks across all supported languages in portrait and landscape. On the reader,
check both onboarding flows and renaming, all four rotations, an accented name,
the language key, held Delete, Back and physical-button navigation. Empty names
must not submit; an accepted name must remain after leaving and reopening the app.
