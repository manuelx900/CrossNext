"""Exercise production cover/progress paths with in-memory storage and display."""
from pathlib import Path
import os
import subprocess

r = Path(__file__).resolve().parents[2]
out = r / 'datos.local/ui150-2026-11-02/home-cache'
out.mkdir(parents=True, exist_ok=True)
em = r / '.venv/emsdk/upstream/emscripten/em++.exe'
node = next((r / '.venv/emsdk/node').glob('*/bin/node.exe'), None) or next((r / '.venv/emsdk/node').glob('*/node.exe'))
common = ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-comment', '-O2', '-sSTACK_SIZE=1048576',
          '-sENVIRONMENT=node', '-sEXIT_RUNTIME=1', '-sALLOW_MEMORY_GROWTH=1']
includes = ['host-tests/home-cache/stubs', 'host-tests/reading-cloud/stubs', 'host-tests/reading-suite/stubs',
            'host-tests/reading-progress/stubs',
            'lib/Memory', 'lib/Performance', 'src', 'lib/GfxRenderer', 'freeink-sdk/libs/ui/FreeInkUI/include',
            'freeink-sdk/libs/assets/Icons/include']
def build(name, sources):
    path = out / (name + '.js')
    subprocess.run([str(em), *common, *['-I' + n for n in includes], *sources, '-o', str(path)], cwd=r, check=True)
    return path
def run(path, *args):
    subprocess.run([str(node), str(path), *args], cwd=r, check=True)
run(build('cache', ['host-tests/home-cache/test.cpp']))
p = build('progress', ['host-tests/home-cache/progress.cpp', 'src/util/BookProgress.cpp', 'src/apps_local/home/CoverSceneCache.cpp', 'src/apps_local/library/ScenePack.cpp'])
run(p)
run(p, 'oom')
c = build('covers', ['host-tests/home-cache/covers.cpp', 'src/apps_local/home/CoverBitmapPainter.cpp',
                      'lib/GfxRenderer/Bitmap.cpp', 'lib/GfxRenderer/BitmapHelpers.cpp'])
run(c)
run(c, 'oom')
s = build('scenes', ['host-tests/home-cache/scenes.cpp', 'src/apps_local/home/CoverSceneCache.cpp', 'src/apps_local/library/ScenePack.cpp'])
run(s)
run(s, 'oom')
