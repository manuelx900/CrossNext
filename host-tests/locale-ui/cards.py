"""Measure the actual Solitaire/Hearts help and settings with all UI locales."""
from pathlib import Path
import argparse, importlib.util, os, subprocess, sys

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--emsdk', type=Path, default=root / '.venv/emsdk')
parser.add_argument('--output', type=Path, default=root / '.cache/locale-ui/cards')
args = parser.parse_args(); sdk = args.emsdk.resolve(); out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('quest', root / 'host-tests/quest-locales/run.py')
quest = importlib.util.module_from_spec(spec); spec.loader.exec_module(quest)
subprocess.run([sys.executable, 'host-tests/quest-locales/prepare.py'], cwd=root, check=True)
suffix = '.exe' if os.name == 'nt' else ''
compiler = sdk / 'upstream/emscripten' / ('em++' + suffix)
subprocess.run([str(compiler.with_name('emcc' + suffix)), '-O1', '-c', 'lib/MiniBidi/minibidi.c', '-o', str(out / 'bidi.o')], cwd=root, check=True)
sources = ['host-tests/locale-ui/cards.cpp', 'src/apps_local/hearts/HeartsScreens.cpp', 'src/apps_local/hearts/HeartsCore.cpp',
           'src/apps_local/solitaire/SolitaireScreens.cpp', 'src/apps_local/solitaire/SolitaireCore.cpp',
           'src/apps_local/cards/CardArt.cpp', 'src/apps_local/ui/PixelMenu.cpp', 'lib/EpdFont/EpdFont.cpp', 'lib/EpdFont/EpdFontFamily.cpp',
           'lib/Utf8/Utf8.cpp', 'lib/MiniBidi/BidiUtils.cpp', 'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
subprocess.run([str(compiler), '-std=c++20', '-O1', '-Wno-comment', *['-I' + p for p in quest.INCLUDES], *sources, str(out / 'bidi.o'),
                '-sENVIRONMENT=node', '-sNODERAWFS=1', '-sALLOW_MEMORY_GROWTH=1', '-sEXIT_RUNTIME=1', '-sSTACK_SIZE=1048576',
                '-o', str(out / 'cards.cjs')], cwd=root, check=True)
node = sorted((sdk / 'node').glob('*'))[-1] / ('node.exe' if os.name == 'nt' else 'bin/node')
result = subprocess.run([str(node), str(out / 'cards.cjs'), str(root), str(out)], cwd=root, capture_output=True, text=True, encoding='utf8')
(out / 'run.log').write_text(result.stdout + result.stderr, 'utf8'); print(result.stdout + result.stderr)
raise SystemExit(result.returncode)
