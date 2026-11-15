# Correct or customize an interface translation

CrossNext Resources contains `languages/*.cplang`, fallback glyph fonts,
keyboards and Wordle banks. English and Spanish are also built in. The selected
language pack is loaded into PSRAM; text lookups do not read the card every time.

The release includes **readable YAML translation sources** in
`languages/translation-sources/`, plus `languages/template.csv` and `README.txt`.
They are provided so users can fix contextual translation errors and share them.
Binary `.cplang` files are not editable text documents.

1. Copy `template.csv` and retain its key column. Fill the translation column
   using the included guide; preserve placeholders such as `%s`, `%d` and
   `%llu`, escaped newlines and any required formatting.
2. With Python and the source checkout's requirements installed, compile it:

   ```sh
   python tools_local/language_packs.py build corrected.csv --code fr --name "Français" --output fr.cplang
   ```

3. Replace `/languages/fr.cplang` on the microSD, safely eject and restart the
   reader. Keep `/languages/fonts/` for non-Latin characters. Test the wording
   in its actual screen and orientation; a correct sentence may still be too long.
4. For a repository contribution, edit the matching file under
   `lib/I18n/translations/`, run `python scripts/gen_i18n.py`, then rebuild the
   language bundle with `python tools_local/language_packs.py bundle OUTPUT`.
   Generated C++ translation tables are not edited or committed by hand.

Include the language, translation key, screen context and proposed wording when
reporting a problem. An unavailable/damaged external pack falls back to English.
This system customizes interface text; it does not translate a book or expand a
Wordle dictionary automatically. Font and dictionary licences remain independent.
