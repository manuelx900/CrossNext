"""Export real Wordle keyboard outputs; pin their code, packs and font coverage."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import sys

from prepare import prepare

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.cache/wordle-vocabulary'
sys.path.insert(0, str(ROOT / 'tools_local'))
from build_wordle_languages import catalogue
from shape_language_thai import cluster_codepoints


def export(target, thai_baseline):
    prepare(OUT)
    wasm = os.name == 'nt'
    compiler = str(ROOT / '.venv/emsdk/upstream/emscripten/em++.exe') if wasm else os.environ.get('CXX', 'c++')
    output = OUT / ('repertoire.cjs' if wasm else 'repertoire')
    sources = ['host-tests/wordle-vocabulary/repertoire.cpp', 'src/activities/util/EmbeddedKeyboard.cpp',
               'src/activities/util/KeyboardLayoutSet.cpp', 'src/activities/util/LanguageKeyboard.cpp',
               'src/activities/util/PinyinInput.cpp', 'src/apps_local/ui/KeyboardAlternates.cpp',
               'src/apps_local/ui/PixelMenu.cpp', 'lib/Utf8/Utf8.cpp',
               'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
    includes = [OUT, ROOT, ROOT / 'host-tests/keyboard-language/stubs', ROOT / 'host-tests/study-suite/stubs',
                ROOT / 'lib/Utf8', ROOT / 'freeink-sdk/libs/ui/FreeInkUI/include',
                ROOT / 'freeink-sdk/libs/assets/Icons/include']
    args = [compiler, '-std=c++20', '-fno-char8_t', '-O2', '-Wno-comment',
            *['-I' + str(p) for p in includes], *sources, '-o', str(output)]
    if wasm: args += ['-sENVIRONMENT=node', '-sNODERAWFS=1', '-sALLOW_MEMORY_GROWTH=1', '-sEXIT_RUNTIME=1']
    subprocess.run(args, cwd=ROOT, check=True)
    runtime = [str(ROOT / '.venv/emsdk/node/24.19.0_64bit/node.exe')] if wasm else []
    raw = subprocess.check_output([*runtime, str(output)], cwd=ROOT).decode('utf8')
    outputs = {}
    for line in raw.splitlines():
        code, value = line.split('\t')
        outputs.setdefault(code, []).append(bytes.fromhex(value).decode('utf8'))
    for code, suffix in [('ZH_CN', 'sc'), ('ZH_TW', 'tc')]:
        data = (ROOT / f'assets_local/language-keyboards/pinyin-{suffix}.bin').read_bytes()
        assert data[:4] == b'CPPY'
        count = struct.unpack_from('<H', data, 6)[0]
        values = set()
        for i in range(count):
            offset, n = struct.unpack_from('<IH', data, 16 + i * 16 + 8)
            values.update(chr(struct.unpack_from('<I', data, offset + j * 4)[0]) for j in range(n))
        outputs[code] = sorted(values)
    assert {code for _, code in catalogue()}.issubset(outputs)
    # Read actual serialized face intervals, not source-font cmap promises.
    fonts = list((ROOT / 'assets_local/language-fonts').glob('*.cpfont'))
    assert len(fonts) == 9
    common = None
    for path in fonts:
        data = path.read_bytes()
        for style in range(data[12]):
            toc = 32 + 32 * style
            count, glyphs = struct.unpack_from('<II', data, toc + 4)
            start = struct.unpack_from('<I', data, toc + 24)[0]
            covered = set()
            for i in range(count):
                first, last, offset = struct.unpack_from('<III', data, start + i * 12)
                for cp in range(first, last + 1):
                    index = offset + cp - first
                    assert index < glyphs
                    width, height = struct.unpack_from('<BB', data, start + count * 12 + index * 16)
                    if width and height: covered.add(cp)
            common = covered if common is None else common & covered
    # Reconstruct stable semantic Thai clusters from the pinned previous bank.
    data = thai_baseline.read_bytes()
    texts = [data[i:i + 97].split(b'\0', 1)[0].decode() for i in range(64, len(data), 108)]
    for path in (ROOT / 'lib/I18n/translations').glob('*.yaml'):
        texts.extend(re.findall(r'^STR_[^:]+:\s*"(.*)"\s*$', path.read_text(encoding='utf8'), re.M))
    clusters = cluster_codepoints(texts, input_alphabet=True)
    # Verify each semantic sequence in every face; private-use IDs are not a
    # coverage guarantee if a previous generator assigned them differently.
    for path in fonts:
        data = path.read_bytes()
        for style in range(data[12]):
            toc = 32 + 32 * style
            intervals, glyphs = struct.unpack_from('<II', data, toc + 4)
            kl, kr = struct.unpack_from('<HH', data, toc + 17)
            count = struct.unpack_from('<H', data, toc + 28)[0]
            offset = struct.unpack_from('<I', data, toc + 24)[0] + intervals * 12 + glyphs * 16 + (kl + kr) * 3
            offset += data[toc + 21] * data[toc + 22]
            pairs = dict(struct.unpack_from('<II', data, offset + i * 8) for i in range(count))
            for text, expected in clusters.items():
                cp = ord(text[0])
                for c in text[1:]: cp = pairs.get(cp << 16 | ord(c), 0)
                assert cp == expected and cp in common, (path.name, style, text)
    paths = set(sources)
    for folder in ['src/activities/util', 'src/apps_local/ui', 'lib/Utf8',
                   'freeink-sdk/libs/ui/FreeInkUI/include', 'assets_local/language-keyboards',
                   'assets_local/language-fonts', 'lib/I18n/translations']:
        paths.update(p.relative_to(ROOT).as_posix() for p in (ROOT / folder).rglob('*') if p.is_file())
    paths.add('src/apps_local/wordle/WordleCase.h')
    inputs = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in sorted(paths)}
    result = {'inputs': inputs, 'outputs': outputs, 'glyphs': sorted(common), 'thaiClusters': sorted(clusters)}
    target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(f'{len(outputs)} game repertoires; {len(common)} common glyphs; {len(clusters)} Thai clusters')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--thai-baseline', required=True, type=Path)
    args = parser.parse_args()
    export(args.out, args.thai_baseline)
