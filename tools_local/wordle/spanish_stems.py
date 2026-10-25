"""Extract accepted Spanish stems from the vendored RLA-ES dictionary.
SPDX-License-Identifier: MIT
"""
from pathlib import Path
import unicodedata
root=Path(__file__).resolve().parents[2]/'assets_local/wordle/spanish'
def normal(word):
    word=word.lower().replace('ñ','[')
    return ''.join(c for c in unicodedata.normalize('NFD',word) if unicodedata.category(c)!='Mn').upper()
words=set()
for line in (root/'index.dic').read_text(encoding='utf-8-sig').splitlines()[1:]:
    raw=line.split('/')[0].split()[0]
    if raw[:1].isupper(): continue
    word=normal(raw)
    if 4<=len(word)<=6 and all('A'<=c<='[' for c in word): words.add(word)
for length in (4,5,6):
    answers=set((root/f'words-{length}.txt').read_text().splitlines())
    guesses=sorted(answers|{w for w in words if len(w)==length})
    (root/f'guesses-{length}.txt').write_text('\n'.join(guesses)+'\n',newline='\n')
    print(length,len(answers),len(guesses))
