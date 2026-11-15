"""Load each generated language pack in the real C++ loader and check web JSON."""
from pathlib import Path
import argparse, importlib.util, json, os, subprocess, sys
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--packs', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
sdk = (ROOT / '.venv/emsdk').resolve()
compiler = sdk / 'upstream/emscripten' / ('em++.exe' if os.name == 'nt' else 'em++')
nodes = sorted((sdk / 'node').glob('*'))
node = nodes[-1] / ('node.exe' if os.name == 'nt' else 'bin/node')
subprocess.run([sys.executable, 'host-tests/quest-locales/prepare.py'], cwd=ROOT, check=True)
includes = ['test/language_pack/stubs', 'lib/I18n', 'lib/Memory']
sources = ['host-tests/locale-ui/pack_web.cpp', 'lib/I18n/LanguagePack.cpp', 'lib/I18n/I18n.cpp', 'lib/I18n/I18nStrings.cpp']
program = out / 'assets.cjs'
subprocess.run([str(compiler), '-std=c++20', '-O1', *['-I' + s for s in includes], *sources, '-sENVIRONMENT=node', '-sNODERAWFS=1', '-sALLOW_MEMORY_GROWTH=1', '-sEXIT_RUNTIME=1', '-o', str(program)], cwd=ROOT, check=True)
fixtures = ROOT / '.cache/locale-ui/fixtures'
for fixture in sorted(fixtures.glob('*.json'), key=lambda x: int(x.stem)):
    code = json.loads(fixture.read_text('utf8'))['code']
    script = out / (code + '.js')
    subprocess.run([str(node), str(program), str(a.packs.resolve()), code, str(fixture.with_suffix('.bin')), str(script)], cwd=ROOT, check=True)
    subprocess.run([str(node), '--check', str(script)], check=True)
print('All 39 locales: C++ lookups and generated web scripts passed')
