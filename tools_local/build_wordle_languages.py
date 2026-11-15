#!/usr/bin/env python3
"""Build optional UTF-8 Wordle banks from pinned, attributed frequency lists."""
import concurrent.futures
import hashlib
import json
import gzip
import shutil
from pathlib import Path
import struct
import unicodedata as ud
import urllib.request
import xml.etree.ElementTree as ET
from functools import lru_cache
import regex
from build_language_keyboards import definitions

ROOT = Path(__file__).resolve().parents[1]
REVISION = '525f9b560de45753a5ea01069454e72e9aa541c6'
BASE = f'https://raw.githubusercontent.com/hermitdave/FrequencyWords/{REVISION}/content/2018'
ALIASES = {'ca-valencia':'ca', 'nb':'no', 'pt-BR':'pt_br', 'pt-PT':'pt', 'zh-CN':'zh_cn', 'zh-TW':'zh_tw'}
WM_REVISION = '2c05d4a419a983724ca2479c87f04e712bc5559d'
MIN_ANSWERS = 2000
MAX_GUESSES_PER_LENGTH = 1000000
JMDICT_URL = 'https://www.edrdg.org/pub/Nihongo/JMdict_e.gz'
JMDICT_SHA = '490d2fef9cd8dfcdfcd618863c827fe2efd52992b002fb502679d4e1b7fb039d'


def fnv(data):
    h=2166136261
    for byte in data: h=((h^byte)*16777619)&0xffffffff
    return h

def catalogue():
    result=[]
    for p in sorted((ROOT/'lib/I18n/translations').glob('*.yaml')):
        meta={line.split(':',1)[0]:json.loads(line.split(':',1)[1]) for line in p.read_text(encoding='utf-8').splitlines() if line.startswith(('_bcp47:', '_language_code:'))}
        if meta['_bcp47'] != 'ook': result.append((meta['_bcp47'],meta['_language_code']))
    return result

def source(tag, cache):
    if tag in ('az', 'be', 'kk'):
        p = cache / (tag + '-full.txt.gz')
        url = f'https://raw.githubusercontent.com/wasi-master/wmkeyboard-data/{WM_REVISION}/data/{tag}/{tag}_full.txt.gz'
        if not p.exists(): p.write_bytes(urllib.request.urlopen(url, timeout=90).read())
        data = p.read_bytes()
        return [line.split()[0] for line in gzip.decompress(data).decode('utf-8').splitlines() if line.strip()], {
            'source': url, 'sourceSha256': hashlib.sha256(data).hexdigest(), 'license': 'CC-BY-4.0',
            'attribution': 'Leipzig Corpora Collection, packaged by WM Keyboard; see SOURCE.txt'}
    code=ALIASES.get(tag,tag)
    for suffix in ('full','50k'):
        p=cache/(code+'-'+suffix+'.txt')
        url=f'{BASE}/{code}/{code}_{suffix}.txt'
        try:
            if not p.exists(): p.write_bytes(urllib.request.urlopen(url,timeout=90).read())
            break
        except urllib.error.HTTPError as exc:
            if exc.code!=404: raise
    else: raise ValueError(f'No word source for {tag}')
    data=p.read_bytes()
    return [line.split()[0] for line in data.decode('utf-8').splitlines() if line.strip()], {'source':url, 'sourceSha256':hashlib.sha256(data).hexdigest(), 'license':'CC-BY-SA-4.0', 'attribution':'Hermit Dave / FrequencyWords, derived from OpenSubtitles 2018'}

LAYOUTS = definitions()
NATIVE = {
 'en':'abcdefghijklmnopqrstuvwxyz',
 'es':'abcdefghijklmnñopqrstuvwxyz',
 'ar':'ابتثجحخدذرزسشصضطظعغفقكلمنهويىةءآأإؤئ',
 'he':'אבגדהוזחטיכלמנסעפצקרשתךםןףץ',
 'ru':'абвгдеёжзийклмнопрстуфхцчшщъыьэюя',
 'be':'абвгдеёжзійклмнопрстуўфхцчшыьэюя',
 'uk':'абвгґдеєжзиіїйклмнопрстуфхцчшщьюя',
 'kk':'абвгдеёжзийклмнопрстуфхцчшщъыьэюяәғқңөұүһі',
 'fr':'abcdefghijklmnopqrstuvwxyzàâæçéèêëîïôœùûüÿ',
 'de':'abcdefghijklmnopqrstuvwxyzäöüß',
}
PINYIN = json.loads((ROOT/'assets_local/language-keyboards/PINYIN.json').read_text(encoding='utf-8'))
@lru_cache(maxsize=40)
def alphabet_for(tag):
    if tag.startswith('zh'):
        return set(''.join(PINYIN['sc' if tag=='zh-CN' else 'tc'].values()))
    if tag in LAYOUTS:
        layout=LAYOUTS[tag]
        alphabet=''.join(layout['rows']+layout['shift'])+''.join(layout.get('alts',{}).values())
        if tag=='ja': alphabet+='ー'
    else: alphabet=NATIVE[tag]
    return {ud.normalize('NFD',lower(char,tag)) for char in alphabet}

def lower(word,tag):
    if tag in ('tr','az'): word=word.replace('I','ı').replace('İ','i')
    word=ud.normalize('NFC',word.lower())
    # Spanish plays vowels without accents; ñ remains a distinct letter.
    if tag=='es': word=word.translate(str.maketrans('áéíóúü','aeiouu'))
    return word

@lru_cache(maxsize=40)
def tokens_for(tag):
    result={}
    for token in alphabet_for(tag):
        if token: result.setdefault(token[0],[]).append(token)
    return result

def typeable(word,tag):
    tokens=tokens_for(tag)
    text=ud.normalize('NFD',word)
    reachable={0}
    for at in range(len(text)):
        if at not in reachable: continue
        for token in tokens.get(text[at],()):
            if text.startswith(token,at): reachable.add(at+len(token))
    return len(text) in reachable

def eligible(word, tag):
    word=lower(word,tag)
    if not word or not all(ud.category(c)[0] in 'LM' for c in word) or not typeable(word,tag): return None
    if tag=='ja' and not all('ぁ'<=c<='ゖ' or 'ァ'<=c<='ヺ' or c=='ー' for c in word): return None
    if tag.startswith('zh') and not all(0x3400<=ord(c)<=0x9fff for c in word): return None
    # Avoid foreign Latin tokens inside non-Latin banks.
    if tag in ('ar','fa','be','bg','kk','ru','uk','th') and any('LATIN' in ud.name(c,'') for c in word): return None
    units=regex.findall(r'\X',word)
    if not 2<=len(units)<=6 or len(word.encode())>96 or any(len(u.encode())>15 for u in units): return None
    return word,units

def additional_guesses(tag, cache):
    if tag != 'ja': return [], []
    snapshot=ROOT/'tools_local/wordle_sources'/f'JMdict_e_{JMDICT_SHA}.gz'
    path=snapshot if snapshot.exists() else cache/'JMdict_e.gz'
    if not path.exists(): path.write_bytes(urllib.request.urlopen(JMDICT_URL,timeout=90).read())
    data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=JMDICT_SHA:
        raise ValueError('JMdict snapshot changed; review and pin the new source before rebuilding')
    words=[]
    with gzip.open(path,'rb') as stream:
        for _,entry in ET.iterparse(stream,events=('end',)):
            if entry.tag=='entry':
                words.extend(node.text for node in entry.iter('reb') if node.text)
                entry.clear()
    return words,[{'source':JMDICT_URL,'sha256':JMDICT_SHA,'license':'CC-BY-SA-4.0',
                   'attribution':'JMdict, Electronic Dictionary Research and Development Group; kana readings only'}]

def build_one(item,cache,out):
    tag,code=item;words,credit=source(tag,cache)
    groups={n:{} for n in range(2,7)}
    for raw in words:
        parsed=eligible(raw,tag)
        if not parsed: continue
        word,units=parsed;g=groups[len(units)]
        g.setdefault(word,units)
        if len(g)>MAX_GUESSES_PER_LENGTH:
            raise ValueError(f'{tag}: {len(units)}-unit guesses exceed the runtime bound')
    # Each length keeps a frequent answer pool; the larger sorted bank accepts
    # less frequent guesses without making them surprise solutions.
    answer_groups = {n: dict(list(g.items())[:500]) for n,g in groups.items()}
    if sum(map(len,answer_groups.values())) < MIN_ANSWERS:
        for n,g in groups.items():
            for word,units in g.items():
                if sum(map(len,answer_groups.values())) >= MIN_ANSWERS: break
                answer_groups[n].setdefault(word,units)
    if sum(map(len,answer_groups.values())) < MIN_ANSWERS:
        raise ValueError(f'{tag}: only {sum(map(len,answer_groups.values()))} playable frequent words; source needs expansion')
    supplements,credits=additional_guesses(tag,cache)
    for raw in supplements:
        parsed=eligible(raw,tag)
        if not parsed: continue
        word,units=parsed;g=groups[len(units)]
        g.setdefault(word,units)
        if len(g)>MAX_GUESSES_PER_LENGTH:
            raise ValueError(f'{tag}: supplemented guesses exceed the runtime bound')
    credit['supplements']=credits
    records=bytearray();counts=[];all_words=[]
    for bank in (groups, answer_groups):
        for n,g in bank.items():
            counts.append(len(g))
            for word,units in sorted(g.items(),key=lambda x:x[0].encode()):
                encoded=word.encode();ends=[];at=0
                for unit in units: at+=len(unit.encode());ends.append(at)
                record=encoded.ljust(97,b'\0')+bytes([n])+bytes(ends).ljust(6,b'\0')
                records.extend(record+struct.pack('<I',fnv(record)))
                if bank is groups: all_words.append(word)
    if not counts[2]: raise ValueError(f'{tag}: missing four-unit words')
    header=struct.pack('<4sHH10II',b'CPWB',2,108,*counts,fnv(records))
    binary=header+struct.pack('<I',fnv(header))+bytes(8)+records
    target=out/(code+'.cpwb');target.write_bytes(binary)
    return {'tag':tag,'code':code,'counts':counts[:5],'answers':counts[5:],'totalGuesses':sum(counts[:5]),'totalAnswers':sum(counts[5:]),'bytes':len(binary),'sha256':hashlib.sha256(binary).hexdigest(),**credit},all_words

def build():
    out=ROOT/'assets_local/wordle-languages';out.mkdir(parents=True,exist_ok=True)
    cache=ROOT/'datos.local/language-ui-input/word-sources';cache.mkdir(parents=True,exist_ok=True)
    with concurrent.futures.ProcessPoolExecutor(max_workers=4) as pool:
        tasks=[pool.submit(build_one,item,cache,out) for item in catalogue()]
        result=[task.result() for task in tasks]
    report=[r for r,w in result]
    glyphs=''.join(sorted(set(''.join(w for r,words in result for w in words))))
    (out/'GLYPHS.txt').write_text(glyphs,encoding='utf-8')
    pairs={ord(c):ord(c.lower()) for c in ''.join(glyphs).upper() if len(c.lower())==1 and c.lower()!=c}
    if 'ß' in glyphs: pairs[0x1E9E]=ord('ß')
    header='#pragma once\n#include <cstdint>\n// Generated from the optional Wordle alphabets.\nstruct WordleCasePair { uint32_t upper, lower; };\ninline constexpr WordleCasePair WORDLE_CASE_PAIRS[] = {\n'
    header+=''.join(f'  {{0x{upper:X}, 0x{lower:X}}},\n' for upper,lower in sorted(pairs.items()))+'};\n'
    (ROOT/'src/apps_local/wordle/WordleCase.h').write_text(header,encoding='utf-8')
    (out/'MANIFEST.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    credit=f'Optional Wordle vocabularies. Adapted from FrequencyWords by Hermit Dave / OpenSubtitles 2018, CC BY-SA 4.0. Revision {REVISION}.\nhttps://github.com/hermitdave/FrequencyWords\nhttps://creativecommons.org/licenses/by-sa/4.0/\nAzerbaijani, Belarusian and Kazakh: Leipzig Corpora Collection frequency lists, redistributed by WM Keyboard, revision {WM_REVISION}.\nhttps://wortschatz.uni-leipzig.de/en/download\nhttps://github.com/wasi-master/wmkeyboard-data\nhttps://creativecommons.org/licenses/by/4.0/\nJapanese supplementary kana guesses: JMdict, copyright Electronic Dictionary Research and Development Group, used in accordance with its licence. CC BY-SA 4.0.\nhttps://www.edrdg.org/jmdict/j_jmdict.html\nhttps://www.edrdg.org/edrdg/licence.html\nModifications: normalized UTF-8, Spanish vowel-accent folding (preserving ñ), keyboard coverage and length filtering, frequency-ranked answer pool, sorted fixed records with checksums. Supplemental kana readings expand guesses only. No source words embedded in firmware flash.\nAnswers are selected by corpus frequency; native editorial review is still needed. Rebuild with tools_local/build_wordle_languages.py.\n'
    (out/'SOURCE.txt').write_text(credit,encoding='utf-8')
    for name in ('JMDICT-LICENSE.txt','JMDICT-DOCUMENTATION.txt'):
        shutil.copyfile(ROOT/'tools_local/wordle_sources'/name,out/name)
    print(json.dumps([{'tag':r['tag'],'counts':r['counts']} for r in report],ensure_ascii=False),flush=True)

if __name__=='__main__': build()
