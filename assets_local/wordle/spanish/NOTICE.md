# Spanish Wordle vocabulary

RLA-ES Spanish spelling dictionary, by Santiago Bosio and contributors, obtained
from https://github.com/wooorm/dictionaries/tree/main/dictionaries/es . Original
`index.dic`, `index.aff` and `license` are retained. RLA-ES permits MPL 1.1 or any
later version; this distribution selects MPL 2.0 (`MPL-2.0.txt`).

`tools_local/wordle/spanish_stems.py` extracts lowercase literal dictionary stems
and merges the curated answer lists. It does not invent inflections. Accented
vowels are normalized to base vowels; Ñ remains distinct as internal token `[`. 
The resulting `guesses-*.txt` and `SpanishWords.generated.h` are modifications
covered by MPL 2.0. Their editable sources, extraction script and header generator
ship with this repository. The curated everyday answer files are original local
vocabulary lists. English data remains separate with its existing notices.
