#!/usr/bin/env python3
"""Merge pinned Wiktionary guesses into CPWB v2 banks without changing answers.

Host-only: requires regex (requirements-language-fonts.txt). The input bank set
is immutable and must match BASELINE.json. Never publish the staging directory
until all 38 banks and their manifest have been produced successfully.
"""
import argparse
from concurrent.futures import ProcessPoolExecutor
import gzip
import hashlib
import json
import mmap
from pathlib import Path
import re
import shutil
import struct
import unicodedata as ud
import urllib.request

import regex

from build_wordle_languages import lower, MAX_GUESSES_PER_LENGTH

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT / 'tools_local/wordle_sources'
STRIDE = 108
HEADER = 64
LEXICAL_POS = frozenset(('adj', 'adv', 'adv_phrase', 'article', 'conj', 'det',
                        'intj', 'noun', 'num', 'particle', 'postp', 'prep',
                        'prep_phrase', 'pron', 'verb', 'phrase'))
EXCLUDED_TAGS = frozenset(('romanization', 'transliteration', 'table-tags',
                           'inflection-template', 'class', 'error-misspelling',
                           'misspelling', 'incorrect', 'typo', 'error-unknown-tag'))
LANGUAGE = {'bs': 'sh', 'ca-valencia': 'ca', 'pt-BR': 'pt', 'pt-PT': 'pt',
            'zh-CN': 'zh', 'zh-TW': 'zh'}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def fnv(data, value=2166136261):
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def require_digest(path, expected):
    if sha(path) != expected:
        raise ValueError(f'Pinned input changed: {path}')


def tokens_from_snapshot(code, tag, snapshot):
    result = {}
    for value in snapshot['outputs'][code]:
        token = ud.normalize('NFD', lower(value, tag))
        if token:
            result.setdefault(token[0], set()).add(token)
    return result


def typeable(word, tokens):
    text = ud.normalize('NFD', word)
    reachable = {0}
    for at, char in enumerate(text):
        if at in reachable:
            for token in tokens.get(char, ()):
                if text.startswith(token, at):
                    reachable.add(at + len(token))
    return len(text) in reachable


def normalized(raw, tag):
    # Wiktionary Cyrillic stress marks and Arabic vowel points are pronunciation
    # annotations, not part of the unvocalized spellings used by these keyboards.
    if tag in ('be', 'bg', 'ru', 'uk'):
        raw = raw.replace('\u0301', '').replace('\u0341', '')
    if tag in ('ar', 'fa'):
        raw = ''.join(c for c in raw if not 0x64b <= ord(c) <= 0x652
                      and ord(c) not in (0x640, 0x670))
    return lower(raw, tag)


def eligible(raw, tag, tokens, glyphs=None, thai_clusters=None):
    word = normalized(raw, tag)
    if not word or len(word) > 48 or not all(ud.category(c)[0] in 'LM' for c in word):
        return None
    units = regex.findall(r'\X', word)
    if not 2 <= len(units) <= 6 or len(word.encode()) > 96 or any(len(u.encode()) > 15 for u in units):
        return None
    if tag == 'ja' and not all('ぁ' <= c <= 'ゖ' or 'ァ' <= c <= 'ヺ' or c == 'ー' for c in word):
        return None
    if tag.startswith('zh') and not all(0x3400 <= ord(c) <= 0x9fff for c in word):
        return None
    if tag in ('ar', 'fa', 'be', 'bg', 'kk', 'ru', 'uk', 'th') and any('LATIN' in ud.name(c, '') for c in word):
        return None
    if not typeable(word, tokens):
        return None
    if glyphs is not None and any(ord(c) not in glyphs for c in word + word.upper()):
        return None
    if tag == 'th' and thai_clusters is not None:
        for match in regex.finditer(r'[\u0e01-\u0e2e][\u0e31\u0e34-\u0e3a\u0e47-\u0e4e]*\u0e33?', word):
            cluster = match.group()
            if any(cluster[:n] not in thai_clusters for n in range(2, len(cluster) + 1)):
                return None
    return word, units


def lexical_forms(entry, language):
    if entry.get('lang_code') != language or entry.get('pos') not in LEXICAL_POS:
        return
    senses = entry.get('senses', ())
    if senses and all(EXCLUDED_TAGS.intersection(s.get('tags', ())) for s in senses):
        return
    word = entry.get('word')
    if isinstance(word, str):
        yield word
    for form in entry.get('forms', ()):
        value = form.get('form')
        if isinstance(value, str) and not EXCLUDED_TAGS.intersection(form.get('tags', ())):
            yield value


def encode(word, units):
    raw = word.encode('utf-8')
    ends = []
    at = 0
    for unit in units:
        at += len(unit.encode('utf-8'))
        ends.append(at)
    record = raw.ljust(97, b'\0') + bytes((len(units),)) + bytes(ends).ljust(6, b'\0')
    return record + struct.pack('<I', fnv(record))


def header(counts, answers, digest):
    raw = struct.pack('<4sHH10II', b'CPWB', 2, STRIDE, *counts, *answers, digest)
    return raw + struct.pack('<I', fnv(raw)) + bytes(8)


def collect(source_path, language, tag, tokens, glyphs, thai_clusters):
    groups = {n: set() for n in range(2, 7)}
    with gzip.open(source_path, 'rt', encoding='utf-8') as stream:
        for line in stream:
            entry = json.loads(line)
            for raw in lexical_forms(entry, language):
                parsed = eligible(raw, tag, tokens, glyphs, thai_clusters)
                if parsed:
                    word, units = parsed
                    groups[len(units)].add(word)
    return groups


def merge_bank(base, target, entry, additions):
    """Copy every existing record, merging only new guesses in byte-sort order."""
    temporary = target.with_suffix('.part')
    with base.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        if data[:8] != struct.pack('<4sHH', b'CPWB', 2, STRIDE) or fnv(data[:52]) != struct.unpack_from('<I', data, 52)[0]:
            raise ValueError(f'Invalid v2 header: {base}')
        previous = list(struct.unpack_from('<5I', data, 8))
        answers = list(struct.unpack_from('<5I', data, 28))
        if previous != entry['counts'] or answers != entry['answers'] or len(data) != HEADER + STRIDE * (sum(previous) + sum(answers)):
            raise ValueError(f'Baseline layout differs: {base}')
        counts, added, examples, characters = [], [], [], set()
        digest = 2166136261
        with temporary.open('w+b') as out:
            out.write(bytes(HEADER))
            offset = HEADER
            for n, count in enumerate(previous, 2):
                new = iter(sorted(w.encode() for w in additions[n]))
                pending = next(new, None)
                total = delta = 0
                for i in range(count):
                    record = data[offset + i * STRIDE:offset + (i + 1) * STRIDE]
                    word = record[:97].split(b'\0', 1)[0]
                    while pending is not None and pending < word:
                        text = pending.decode()
                        raw = encode(text, regex.findall(r'\X', text))
                        digest = fnv(raw, digest); out.write(raw)
                        characters.update(text)
                        if len(examples) < 24: examples.append(text)
                        total += 1; delta += 1
                        pending = next(new, None)
                    if pending == word:
                        pending = next(new, None)
                    digest = fnv(record, digest); out.write(record); total += 1
                while pending is not None:
                    text = pending.decode()
                    raw = encode(text, regex.findall(r'\X', text))
                    digest = fnv(raw, digest); out.write(raw)
                    characters.update(text)
                    if len(examples) < 24: examples.append(text)
                    total += 1; delta += 1
                    pending = next(new, None)
                if total > MAX_GUESSES_PER_LENGTH:
                    raise ValueError(f'{entry["code"]}: {n}-letter bank exceeds runtime bound')
                counts.append(total); added.append(delta)
                offset += count * STRIDE
            original_answers = data[offset:]
            digest = fnv(original_answers, digest)
            out.write(original_answers)
            out.seek(0); out.write(header(counts, answers, digest))
        temporary.replace(target)
    return {**entry, 'counts': counts, 'totalGuesses': sum(counts), 'bytes': target.stat().st_size,
            'sha256': sha(target), 'previousSha256': entry['sha256'],
            'addedGuesses': added, 'totalAddedGuesses': sum(added),
            'answersSha256': hashlib.sha256(original_answers).hexdigest(),
            'answersUnchanged': True, 'newGuessExamples': examples}, characters


def expand_one(task):
    entry, source, base, out, cache, snapshot = task
    tag, code = entry['tag'], entry['code']
    path = cache / source['file']
    tokens = tokens_from_snapshot(code, tag, snapshot)
    groups = collect(path, LANGUAGE.get(tag, tag), tag, tokens, set(snapshot['glyphs']), set(snapshot['thaiClusters']))
    report, characters = merge_bank(base / (code + '.cpwb'), out / (code + '.cpwb'), entry, groups)
    report['supplements'] = list(entry.get('supplements', ())) + [source]
    print(f'{code}: +{report["totalAddedGuesses"]:,}; {report["totalGuesses"]:,} guesses', flush=True)
    return report, characters


def write_case_header(characters, target):
    pairs = {ord(c): ord(c.lower()) for c in ''.join(characters).upper()
             if len(c.lower()) == 1 and c.lower() != c}
    if 'ß' in characters: pairs[0x1e9e] = ord('ß')
    previous = (ROOT / 'src/apps_local/wordle/WordleCase.h').read_text(encoding='utf-8')
    for upper, lower_value in re.findall(r'\{0x([0-9A-F]+), 0x([0-9A-F]+)\}', previous):
        upper, lower_value = int(upper, 16), int(lower_value, 16)
        if upper in pairs and pairs[upper] != lower_value:
            raise ValueError('Existing Wordle case mapping would change')
        pairs[upper] = lower_value
    text = '#pragma once\n#include <cstdint>\n// Generated from the optional Wordle alphabets.\n'
    text += 'struct WordleCasePair {\n  uint32_t upper, lower;\n};\ninline constexpr WordleCasePair WORDLE_CASE_PAIRS[] = {\n'
    text += ''.join(f'  {{0x{upper:X}, 0x{lower_value:X}}},\n' for upper, lower_value in sorted(pairs.items())) + '};\n'
    temporary = target.with_suffix('.part')
    temporary.write_text(text, encoding='utf-8'); temporary.replace(target)


def build(base, out, cache, case_header, download=False, jobs=4):
    base, out, cache = Path(base).resolve(), Path(out).resolve(), Path(cache).resolve()
    if base == out or base in out.parents or out.exists():
        raise ValueError('Use a new staging directory separate from the immutable baseline')
    expected = read_json(SOURCES / 'wordle-vocabulary-baseline.json')
    entries = read_json(base / 'MANIFEST.json')
    if {e['code']: e['sha256'] for e in entries} != expected['banks']:
        raise ValueError('Wrong baseline bank set')
    for name, digest in expected['files'].items(): require_digest(base / name, digest)
    for entry in entries: require_digest(base / (entry['code'] + '.cpwb'), entry['sha256'])
    snapshot = read_json(SOURCES / 'wordle-repertoire.json')
    for name, digest in snapshot['inputs'].items(): require_digest(ROOT / name, digest)
    sources = read_json(SOURCES / 'wiktionary-sources.json')
    cache.mkdir(parents=True, exist_ok=True)
    for source in sources:
        path = cache / source['file']
        if not path.exists() and download:
            temp = path.with_suffix('.part')
            with urllib.request.urlopen(source['url'], timeout=120) as response, temp.open('wb') as stream:
                shutil.copyfileobj(response, stream, 1024 * 1024)
            require_digest(temp, source['sha256']); temp.replace(path)
        require_digest(path, source['sha256'])
    by_code = {s['code']: s for s in sources}
    out.mkdir(parents=True)
    tasks = [(e, by_code[LANGUAGE.get(e['tag'], e['tag']) if e['tag'] != 'bs' else 'bs'], base, out, cache, snapshot)
             for e in entries]
    # The Serbo-Croatian extraction is stored as bs; records still require sh.
    with ProcessPoolExecutor(max_workers=jobs) as pool:
        result = list(pool.map(expand_one, tasks))
    reports = [r for r, _ in result]
    if any(not r['totalAddedGuesses'] for r in reports):
        raise ValueError('Every shipped language bank must gain guesses')
    chars = set((base / 'GLYPHS.txt').read_text(encoding='utf-8'))
    for _, added in result: chars.update(added)
    (out / 'GLYPHS.txt').write_text(''.join(sorted(chars)), encoding='utf-8')
    for name in ('SOURCE.txt', 'JMDICT-LICENSE.txt', 'JMDICT-DOCUMENTATION.txt'):
        shutil.copyfile(base / name, out / name)
    with (out / 'SOURCE.txt').open('a', encoding='utf-8') as stream:
        stream.write('\nAdditional accepted guesses: English Wiktionary contributors, extracted by Kaikki.org / Wiktextract.\n'
                     'Dump 2026-10-01, extraction 2026-10-30. CC BY-SA 4.0.\n'
                     'https://kaikki.org/dictionary/\nhttps://en.wiktionary.org/wiki/Wiktionary:Copyrights\n'
                     'https://creativecommons.org/licenses/by-sa/4.0/\n'
                     'Modifications: lexical headwords and inflections only; language, keyboard, Unicode and length filtering;\n'
                     'Cyrillic stress and Arabic pronunciation annotations removed, existing Spanish accent rules retained.\n'
                     'All previous guesses retained. All answer records unchanged. No definitions or audio redistributed.\n'
                     'Source URLs and SHA-256 snapshots: WIKTIONARY-SOURCES.json. A word page and its contributor history\n'
                     'can be found at https://en.wiktionary.org/wiki/WORD and its History tab.\n'
                     'Rebuild with tools_local/expand_wordle_languages.py and the pinned previous bank set.\n')
    shutil.copyfile(SOURCES / 'wiktionary-sources.json', out / 'WIKTIONARY-SOURCES.json')
    (out / 'MANIFEST.json').write_text(json.dumps(reports, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    write_case_header(chars, Path(case_header))
    return reports


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--cache', required=True, type=Path)
    parser.add_argument('--case-header', required=True, type=Path, help='Generated source header; rebuild firmware after changing it')
    parser.add_argument('--download', action='store_true', help='Download absent pinned sources; never accept changed snapshots')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    build(args.base, args.out, args.cache, args.case_header, args.download, args.jobs)
