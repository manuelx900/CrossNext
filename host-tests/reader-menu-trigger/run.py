"""Exercise reader holds and unmodified production dispatch/default-setting seams."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.pio/host-tests/reader-menu-trigger'
OUT.mkdir(parents=True, exist_ok=True)


def method(path, signature):
    source = (ROOT / path).read_text(encoding='utf8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


(OUT / 'reader-dispatch-production.inc').write_text(method(
    'src/activities/reader/ReaderActivity.cpp', 'bool ReaderActivity::handleReaderTextSettingsGesture()'), encoding='utf8')
(OUT / 'button-routing-production.inc').write_text(method(
    'src/MappedInputManager.cpp', 'bool MappedInputManager::readButton('), encoding='utf8')
source = (ROOT / 'src/activities/settings/TextSettingsActivity.cpp').read_text(encoding='utf8')
start = source.index('      case readertext::CopyDefaults:')
(OUT / 'defaults-production.inc').write_text(source[start:source.index(
    '      case readertext::AlignmentLeft:', start)], encoding='utf8')
header = (ROOT / 'src/activities/reader/ReaderActivity.h').read_text(encoding='utf8')
start = header.index('  bool acceptsReaderTextInput() const final {')
body = header[start:header.index('\n  }', start) + 4].replace(
    '  bool acceptsReaderTextInput() const final', 'bool ReaderActivity::acceptsReaderTextInput() const', 1)
(OUT / 'reader-context-production.inc').write_text(body + '\n' + method(
    'src/activities/ActivityManager.cpp', 'bool ActivityManager::readerTextInputActive() const'), encoding='utf8')
wasm = os.name == 'nt'
compiler = str(ROOT / '.venv/emsdk/upstream/emscripten/em++.exe') if wasm else os.environ.get('CXX', 'c++')
flags = ['-sENVIRONMENT=node', '-sEXIT_RUNTIME=1', '-sSTACK_SIZE=1048576'] if wasm else []
for name in ['test', 'dispatch', 'button-routing', 'defaults', 'context']:
    target = OUT / (name + ('.cjs' if wasm else ''))
    subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-comment',
                    '-I.', '-Isrc', '-Ilib/I18n', '-Ifreeink-sdk/libs/ui/FreeInkUI/include', '-I' + str(OUT),
                    *flags, f'host-tests/reader-menu-trigger/{name}.cpp', '-o', str(target)], cwd=ROOT, check=True)
    subprocess.run(([shutil.which('node')] if wasm else []) + [str(target)], cwd=ROOT, check=True)
