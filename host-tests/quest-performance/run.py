from pathlib import Path
import argparse
import os
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', type=Path)
parser.add_argument('--node', type=Path)
args = parser.parse_args()
compiler = str(args.compiler or os.environ.get('CXX') or shutil.which('c++') or 'c++')
wasm = 'em++' in compiler
out = root / 'qa-artifacts/quest-performance'
out.mkdir(parents=True, exist_ok=True)
base = ['src/apps_local/quest/QuestCore.cpp', 'src/apps_local/quest/QuestCampaign.cpp',
        'src/apps_local/reading/ReadingStatsCore.cpp']
for name in ['performance', 'requests', 'collections', 'expansion']:
    includes = ['host-tests/study-suite/stubs']
    sources = [f'host-tests/study-suite/test_{name}.cpp', *base, 'src/apps_local/quest/QuestStore.cpp']
    if name == 'performance':
        includes = ['host-tests/sdart/stubs', 'freeink-sdk/libs/ui/FreeInkUI/include',
                    'freeink-sdk/libs/assets/Icons/include']
        sources = ['host-tests/quest-performance/test.cpp', *base, 'src/apps_local/ui/SdBitmapPack.cpp',
                   'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
    binary = out / (name + ('.cjs' if wasm else '.exe' if os.name == 'nt' else ''))
    command = [compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
               *['-I' + str(root / p) for p in includes], *[str(root / p) for p in sources]]
    if wasm:
        command += ['-sENVIRONMENT=node', '-sNODERAWFS=1', '-sEXIT_RUNTIME=1',
                    '-sALLOW_MEMORY_GROWTH=1', '-sSTACK_SIZE=1048576']
    subprocess.run([*command, '-o', str(binary)], check=True, cwd=root)
    runner = [str(args.node or 'node')] if wasm else []
    subprocess.run([*runner, str(binary)], check=True, cwd=root)
