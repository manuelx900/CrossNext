# Translators

Below is a list of translator credits for languages with known contributors.
Official UI language support is determined by the YAML files in
`lib/I18n/translations/`; see [i18n.md](./i18n.md) for the current supported
language list.

## Contributing

If you'd like to add your name to this list, please open a PR adding yourself and your Github link. Thank you!

Translations completed with assistance still need native editorial review. Key
coverage, valid format arguments and fitting text do not establish linguistic
accuracy. Review terms in their actual screen: a chess bishop, a petting action,
a ship class and an avatar's gender need their own context.

See [translation meaning and review](translation-context.md) for game-specific
terms, source comments, rule fidelity and the recurring false interpretations.

Keep the YAML catalogues complete, in NFC, and preserve printf arguments in
their original order. `tools_local/language_packs.py bundle` validates the source
key set and includes only strings referenced by this firmware in the SD packs.
The CSV compiler validates inactive translations too before excluding them from
the runtime pack. The 8,192-entry limit and the binary pack format are unchanged.

Fit translated interface labels by measuring the actual font and available
rectangle. Prefer natural concise copy, additional lines and sufficient space;
do not remove the end of a message to hide an overflow. Loading-message dots
are punctuation and are distinct from automatic truncation. Book titles, file
names and other user content may still exceed a finite control's capacity.

The real-font host checks in `host-tests/locale-ui/README.md` cover PET,
system-message bounds and language-pack/web delivery. The separate Quest check
covers Reading Adventure. These checks supplement physical-device and native
speaker review; they do not replace either.

## French
- [Spigaw](https://github.com/Spigaw)
- [CaptainFrito](https://github.com/CaptainFrito)

## German
- [DavidOrtmann](https://github.com/DavidOrtmann)

## Czech
- [brbla](https://github.com/brbla)

## Portuguese (Brazil)
- [yagofarias](https://github.com/yagofarias)
- [Rodrigo-Matsuura](https://github.com/Rodrigo-Matsuura)

## Portuguese (Portugal)
- [victordomingos](https://github.com/victordomingos)

## Italian
- [andreaturchet](https://github.com/andreaturchet)
- [fragolinux](https://github.com/fragolinux)
- [alan0ford](https://github.com/alan0ford)

## Russian
- [madebyKir](https://github.com/madebyKir)
- [mrtnvgr](https://github.com/mrtnvgr)

## Spanish
- [yeyeto2788](https://github.com/yeyeto2788)
- [Skrzakk](https://github.com/Skrzakk)
- [pablohc](https://github.com/pablohc)
- [DaniPhii](https://github.com/DaniPhii)
- [lpla](https://github.com/lpla)

## Swedish
- [dawiik](https://github.com/dawiik)
- [steka](https://github.com/steka)

## Romanian
- [ariel-lindemann](https://github.com/ariel-lindemann)

## Catalan
- [angeldenom](https://github.com/angeldenom)
- [lpla](https://github.com/lpla)

## Finnish
- [plahteenlahti](https://github.com/plahteenlahti)

## Ukrainian
- [mirus-ua](https://github.com/mirus-ua)
- [KymAndriy](https://github.com/KymAndriy)

## Belarusian
- [Dexif](https://github.com/dexif)

## Danish
- [hajisan](https://github.com/hajisan)

## Measure the control, not the character count

Some remaining calls to `renderer.drawCenteredText` still draw a single line.
A translated label must fit its actual control after bezel margins, icons,
clock, buttons and orientation are accounted for. The portrait panel width is
not the width available to every label. Fonts are proportional and scripts can
select SD fallback faces; character count alone cannot establish a fit.

Use the real-font locale checks described above. `host-tests/i18nwidth/run.sh`
is a supplementary survey of direct single-line calls. It gates English only
and cannot certify scripts missing from its built-in face. Recovery, cache and
sync prompts now reserve wrapped bounds where appropriate; update their
contract probe when changing that geometry.

Browser messages with arguments use complete `STR_WEB_MSG_*` templates. Keep
all `{named}` markers, including repeated occurrences, but reorder them as the
language requires. They produce plain text via `uiFormat`, assigned with
`textContent` or shown in a native confirmation. The pack compiler validates
marker names; runtime formatting falls back to the English template if a
third-party pack changes them. Do not translate filenames, note contents or
other argument values, and do not interpolate the result as HTML.

Keep English context comments when editing short commands: image Fill and
Invert, document Notes, file Upload/Download and the software extension runner
must not acquire unrelated meanings.
