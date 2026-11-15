from pathlib import Path
import os
import shutil
import subprocess

repo = Path(__file__).resolve().parents[2]
out = repo / 'datos.local/performance-complete-nov4'
out.mkdir(parents=True, exist_ok=True)
em = repo / '.venv/emsdk/upstream/emscripten/em++.exe'
includes = ['test/library_index_file/stubs', 'lib/Memory', 'lib/I18n', 'lib/EpdFont',
            'freeink-sdk/libs/ui/FreeInkUI/include', 'freeink-sdk/libs/assets/Icons/include']
sources = ['host-tests/performance-complete/test.cpp',
           'lib/I18n/I18n.cpp', 'lib/I18n/I18nStrings.cpp',
           'src/apps_local/ui/PixelMenu.cpp',
           'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
flags = ['-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter']
if em.exists():
    compiler = str(em)
    binary = out / 'test.cjs'
    flags += ['-sENVIRONMENT=node', '-sEXIT_RUNTIME=1', '-sALLOW_MEMORY_GROWTH=1']
    runner = [str(next((repo / '.venv/emsdk/node').glob('*/node.exe')))]
else:
    compiler = os.environ.get('CXX', shutil.which('c++') or 'c++')
    binary = out / ('test.exe' if os.name == 'nt' else 'test')
    runner = []
subprocess.run([compiler, *flags, *['-I' + str(repo / p) for p in includes],
                *[str(repo / p) for p in sources], '-o', str(binary)], check=True)
subprocess.run([*runner, str(binary)], check=True)
