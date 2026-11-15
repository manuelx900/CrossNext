"""Validate all merged records, real-core lookups, saves and bounded SD I/O."""
import argparse
from concurrent.futures import ProcessPoolExecutor
import json
import mmap
import os
from pathlib import Path
import struct
import subprocess
import sys
import unicodedata as ud

from prepare import prepare

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.cache/wordle-vocabulary'
sys.path.insert(0, str(ROOT / 'tools_local'))
import expand_wordle_languages as builder


def validate(task):
    entry, base, bank_root, snapshot = task
    code, tag = entry['code'], entry['tag']
    path = bank_root / (code + '.cpwb')
    builder.require_digest(path, entry['sha256'])
    builder.require_digest(base / path.name, entry['previousSha256'])
    tokens = builder.tokens_from_snapshot(code, tag, snapshot)
    glyphs, thai = set(snapshot['glyphs']), set(snapshot['thaiClusters'])
    examples, additions = [], []
    with path.open('rb') as nf, (base / path.name).open('rb') as of, \
            mmap.mmap(nf.fileno(), 0, access=mmap.ACCESS_READ) as new, \
            mmap.mmap(of.fileno(), 0, access=mmap.ACCESS_READ) as old:
        assert new[:8] == old[:8] == struct.pack('<4sHH', b'CPWB', 2, 108)
        assert builder.fnv(new[:52]) == struct.unpack_from('<I', new, 52)[0]
        counts = struct.unpack_from('<5I', new, 8)
        previous = struct.unpack_from('<5I', old, 8)
        answers = struct.unpack_from('<5I', new, 28)
        assert list(counts) == entry['counts'] and answers == struct.unpack_from('<5I', old, 28)
        assert len(new) == 64 + 108 * (sum(counts) + sum(answers))
        old_answers, new_answers = old[64 + 108 * sum(previous):], new[64 + 108 * sum(counts):]
        assert old_answers == new_answers
        old_at = new_at = 64
        for units, (before, count) in enumerate(zip(previous, counts), 2):
            cursor, last, added, samples = 0, b'', 0, []
            for i in range(count):
                record = new[new_at + i * 108:new_at + (i + 1) * 108]
                word = record[:97].split(b'\0', 1)[0]
                assert last < word
                last = word
                original = old[old_at + cursor * 108:old_at + (cursor + 1) * 108] if cursor < before else None
                if original is not None and original[:97].split(b'\0', 1)[0] == word:
                    assert record == original
                    cursor += 1
                else:
                    text = word.decode('utf8')
                    parsed = builder.eligible(text, tag, tokens, glyphs, thai)
                    assert parsed and len(parsed[1]) == units
                    assert record == builder.encode(*parsed)
                    added += 1
                    if len(samples) < 3: samples.append(text)
            assert cursor == before and added == entry['addedGuesses'][units - 2]
            additions.append(added)
            for text in samples:
                forms = {text, ud.normalize('NFD', text)}
                if builder.lower(text.upper(), tag) == text: forms.add(text.upper())
                examples.extend((code, units, word) for word in sorted(forms))
            old_at += before * 108; new_at += count * 108
        assert sum(additions) > 0
    print(f'{code}: {sum(additions)} additions; all old guesses and answers preserved', flush=True)
    return examples


def run(base, bank_root, jobs):
    prepare(OUT)
    subprocess.run([sys.executable, 'test/language_pack/test_wordle_expansion.py', '-v'], cwd=ROOT, check=True)
    snapshot = builder.read_json(builder.SOURCES / 'wordle-repertoire.json')
    for name, digest in snapshot['inputs'].items(): builder.require_digest(ROOT / name, digest)
    manifest = builder.read_json(bank_root / 'MANIFEST.json')
    assert len(manifest) == 38
    tasks = [(entry, base, bank_root, snapshot) for entry in manifest]
    with ProcessPoolExecutor(max_workers=jobs) as pool:
        groups = list(pool.map(validate, tasks))
    examples = [example for group in groups for example in group]
    content = 'struct ExpansionExample { Language language; unsigned units; const char* word; };\n'
    content += 'inline constexpr ExpansionExample EXPANSION_CASES[] = {\n'
    content += ''.join('{Language::' + code + ',' + str(units) + ',' + json.dumps(word, ensure_ascii=False) + '},\n'
                       for code, units, word in examples) + '};\n'
    (OUT / 'ExpansionCases.h').write_text(content, encoding='utf8')
    wasm = os.name == 'nt'
    compiler = str(ROOT / '.venv/emsdk/upstream/emscripten/em++.exe') if wasm else os.environ.get('CXX', 'c++')
    output = OUT / ('expansion.cjs' if wasm else 'expansion')
    command = [compiler, '-std=c++20', '-fno-char8_t', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-comment',
               '-I' + str(OUT), '-Ihost-tests/study-suite/stubs', '-Ilib/Utf8',
               'host-tests/wordle-vocabulary/test.cpp', 'src/apps_local/wordle/NativeWordle.cpp',
               'lib/Utf8/Utf8.cpp', '-o', str(output)]
    if wasm: command += ['-sENVIRONMENT=node', '-sNODERAWFS=1', '-sALLOW_MEMORY_GROWTH=1', '-sEXIT_RUNTIME=1']
    subprocess.run(command, cwd=ROOT, check=True)
    runtime = [str(ROOT / '.venv/emsdk/node/24.19.0_64bit/node.exe')] if wasm else []
    subprocess.run([*runtime, str(output), str(base), str(bank_root)], cwd=ROOT, check=True)
    (OUT / 'validation.json').write_text(json.dumps({'banks': len(manifest), 'allRecordsValidated': True,
        'oldGuessesPreserved': True, 'answerRecordsIdentical': True, 'runtimeExamples': len(examples),
        'guessesBefore': sum(sum(e['counts']) - e['totalAddedGuesses'] for e in manifest),
        'guessesAfter': sum(e['totalGuesses'] for e in manifest), 'deviceTested': False}, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, type=Path)
    parser.add_argument('--banks', type=Path, default=ROOT / 'assets_local/wordle-languages')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    run(args.base.resolve(), args.banks.resolve(), args.jobs)
