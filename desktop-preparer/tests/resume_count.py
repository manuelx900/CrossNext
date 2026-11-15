"""A 600/800 library resumes with 200 pending files, using real XTC cache stamps."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
source = max((ROOT / 'build').glob('integration-*'), key=lambda value: value.stat().st_mtime)
reference = source / 'Tarjeta-日本語'
test = ROOT / 'build' / ('resume-count-' + str(time.time_ns()))
card = test / 'Card'
(card / 'books').mkdir(parents=True)
(card / '.crosspoint').mkdir()
shutil.copy2(reference / '.crosspoint/settings.json', card / '.crosspoint/settings.json')
shutil.copytree(reference / 'fonts', card / 'fonts')
book = reference / 'books/Rotado.xtc'
cache = next(folder for folder in (reference / '.crosspoint').glob('xtc_*')
             if int.from_bytes((folder / '.desktop-source-v1.bin').read_bytes()[:8], 'little') == book.stat().st_size)
assert list(cache.glob('ready-v1-*.bin')), 'Run the engine integration test first'


def device_hash(value):
    data = value.encode('utf-8')
    mask, m = 0xffffffff, 0x5bd1e995
    h = 0xc70f6907 ^ len(data)
    while len(data) >= 4:
        k = int.from_bytes(data[:4], 'little') * m & mask
        k ^= k >> 24
        h = (h * m & mask) ^ (k * m & mask)
        data = data[4:]
    if data:
        h ^= int.from_bytes(data, 'little')
        h = h * m & mask
    h ^= h >> 13
    h = h * m & mask
    return h ^ (h >> 15)


for number in range(800):
    relative = f'/books/Test-{number:04d}.xtc'
    shutil.copy2(book, card / relative[1:])
    if number >= 600:
        continue
    destination = card / '.crosspoint' / ('xtc_' + str(device_hash(relative)))
    destination.mkdir()
    for generated in cache.iterdir():
        if generated.is_file():
            os.link(generated, destination / generated.name)

configuration = test / 'job.json'
configuration.write_text(json.dumps({'card': str(card), 'formats': ['xtc'], 'fonts': ['Ubuntu'], 'sizes': [12]}))
result = subprocess.run(['node', str(ROOT / 'dist/runtime/worker.cjs'), 'plan', str(configuration)],
                        capture_output=True, encoding='utf-8', timeout=60)
assert result.returncode == 0, result.stdout + result.stderr
events = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
plan = next(event for event in events if event['type'] == 'scanned')
assert plan == {'type': 'scanned', 'total': 200, 'alreadyPrepared': 600, 'found': 800}, plan
assert not any(event['type'] == 'progress' for event in events)
(test / 'report.json').write_text(json.dumps(plan, indent=2))
print(test)
print(json.dumps(plan))
