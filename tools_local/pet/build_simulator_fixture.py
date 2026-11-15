"""Build the PET simulator's species/stage scenarios with production codecs."""
from pathlib import Path
from datetime import datetime, timedelta, timezone
import base64
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'outputs/pet-simulator-catalog'
OUT.mkdir(parents=True, exist_ok=True)
windows = os.name == 'nt'
compiler = str(ROOT / '.venv/emsdk/upstream/emscripten/em++.exe') if windows else os.environ.get('CXX', 'c++')
binary = OUT / ('seed.cjs' if windows else 'seed')
flags = ['-sENVIRONMENT=node', '-sNODERAWFS=1', '-sEXIT_RUNTIME=1', '-sSTACK_SIZE=1048576'] if windows else []
subprocess.run([compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                '-Ihost-tests/habits/stubs', 'src/apps_local/pet/PetHistory.cpp',
                'tools_local/pet/seed_simulator.cpp', 'src/apps_local/pet/PetState.cpp',
                'src/apps_local/reading/ReadingStatsCore.cpp', *flags, '-o', str(binary)], cwd=ROOT, check=True)
utc_offset_quarters = -12
now = datetime.now(timezone(timedelta(minutes=15 * utc_offset_quarters)))
runtime = [shutil.which('node') or str(ROOT / '.venv/emsdk/node/24.19.0_64bit/node.exe')] if windows else []
subprocess.run([*runtime, str(binary), str(OUT), str(int(now.timestamp())), str(int(now.utcoffset().total_seconds()))], cwd=ROOT, check=True)
species = [('silva', 'Silva'), ('nox', 'Nox'), ('ember', 'Ember')]
ages = [('egg', 'Huevo'), ('newborn', 'Recién nacido'), ('baby', 'Cría'), ('juvenile', 'Juvenil'), ('adult', 'Adulto')]
scenarios = [('normal', 'Normal'), ('hungry', 'Con hambre'), ('dirty', 'Para limpiar'), ('sick', 'Para curar'), ('evolution', 'Evolución'), ('death', 'Muerte')]
def encoded(path):
    return base64.b64encode(path.read_bytes()).decode('ascii')
catalog = dict(version=1, generatedAt=now.isoformat(), utcOffsetQuarterHours=utc_offset_quarters,
               species=species, ages=ages, scenarios=scenarios,
               history=encoded(OUT / 'history.bin'),
               journals={age: encoded(OUT / (age + '-journal.bin')) for age, _ in ages}, states={})
for kind, _ in species:
    for age, _ in ages:
        for scenario, _ in scenarios:
            if age == 'egg' and scenario != 'normal':
                continue
            catalog['states'][':'.join((kind, age, scenario))] = encoded(OUT / ('-'.join((kind, age, scenario)) + '.bin'))
target = ROOT / 'site/qa-pet-redesign/catalog.json'
target.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
print(f'Generated {len(catalog["states"])} presets with five shared journals: {target.stat().st_size:,} bytes')
