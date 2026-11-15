"""Upgrade a disposable card with current thumbnails and native library rasters."""
from pathlib import Path
import argparse, hashlib, io, json, struct, subprocess, time, zipfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--previous', type=Path, required=True, help='Previous macOS distribution ZIP')
parser.add_argument('--firmware', type=Path, required=True, help='Firmware checkout used for this engine')
args = parser.parse_args()
TEST = ROOT / 'build' / ('cover-upgrade-' + str(time.time_ns()))
CARD = TEST / 'Tarjeta — 日本語'
(CARD / 'books').mkdir(parents=True)
(CARD / '.crosspoint/reading').mkdir(parents=True)
(CARD / '.crosspoint/settings.json').write_text('{"fontSize":12,"readerFontStorageVersion":1}')
(CARD / '.crosspoint/reading/stats.json').write_text('{"minutes":123}')

image = Image.new('RGB', (300, 450), 'white')
draw = ImageDraw.Draw(image)
draw.rectangle((20, 20, 280, 430), fill='#444444')
picture = io.BytesIO()
image.save(picture, 'PNG')
with zipfile.ZipFile(CARD / 'books/Portada.epub', 'w', zipfile.ZIP_DEFLATED) as book:
    book.writestr('mimetype', 'application/epub+zip')
    book.writestr('META-INF/container.xml', '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
    book.writestr('content.opf', '<package xmlns="http://www.idpf.org/2007/opf" version="2.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Portada</dc:title><meta name="cover" content="cover"/></metadata><manifest><item id="body" href="body.xhtml" media-type="application/xhtml+xml"/><item id="cover" href="cover.png" media-type="image/png"/></manifest><spine><itemref idref="body"/></spine></package>')
    book.writestr('body.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml"><body><p>Library cover upgrade.</p></body></html>')
    book.writestr('cover.png', picture.getvalue())
for gray in (False, True):
    width, height = 480, 800
    payload = width * ((height + 7) // 8) * 2 if gray else ((width + 7) // 8) * height
    header = struct.pack('<IBBHBBBBIQQQQII', 0x48435458 if gray else 0x00435458, 1, 0, 1, 0, 0, 0, 0, 0, 0, 56, 72, 0, 0, 0)
    table = struct.pack('<QIHH', 72, 22 + payload, width, height)
    page = struct.pack('<IHHBBIQ', 0x00485458 if gray else 0x00475458, width, height, 0, 0, payload, 0)
    (CARD / ('books/Portada.xtch' if gray else 'books/Portada.xtc')).write_bytes(header + table + page + bytes([0x55]) * payload)
protected = {p: p.read_bytes() for p in CARD.rglob('*') if p.is_file()}

previous = TEST / 'previous-runtime'
previous.mkdir()
with zipfile.ZipFile(args.previous) as bundle:
    for name in ['engine.cjs', 'engine.wasm', 'worker.cjs']:
        entry = next(n for n in bundle.namelist() if n.endswith('/Contents/Resources/runtime/' + name))
        (previous / name).write_bytes(bundle.read(entry))
current = ROOT / 'build/desktop-runtime'
metadata = json.loads((current / 'engine-build.json').read_text())
for name in ['src/apps_local/library/LibraryMenu.h', 'src/apps_local/library/LibraryControls.h',
             'src/apps_local/bookprep/BookPreparationGeometry.cpp']:
    assert metadata['sourceHashes'][name] == hashlib.sha256((args.firmware / name).read_bytes()).hexdigest(), name

config = TEST / 'job.json'
config.write_text(json.dumps({'card': str(CARD), 'fonts': ['Ubuntu'], 'sizes': [12], 'verify': True}))
def run(runtime, name):
    result = subprocess.run(['node', str(runtime / 'worker.cjs'), 'run', str(config)],
                            capture_output=True, text=True, encoding='utf-8', timeout=120)
    (TEST / (name + '.jsonl')).write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode == 0, result.stdout + result.stderr
    events = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    assert events[-1]['type'] == 'finished' and events[-1]['failed'] == 0, events[-1]
    assert all(p.read_bytes() == data for p, data in protected.items()), 'Books or reader state changed'
    return events[0], events[-1]

old, first = run(previous, 'previous')
assert first['completed'] == 3, first
new, upgraded = run(current, 'upgraded')
expected = {178, 168, 264, 232}
assert expected <= set(new['covers']), new['covers']
assert len(new['nativeCovers']) == 2 and not old.get('nativeCovers'), (old, new)
assert upgraded['completed'] == 3 and upgraded['alreadyPrepared'] == 0, upgraded
caches = list((CARD / '.crosspoint').glob('epub_*')) + list((CARD / '.crosspoint').glob('xtc_*'))
assert len(caches) == 3
for cache in caches:
    for height in new['covers']:
        thumb = cache / ('thumb_' + str(height) + '.bmp')
        data = thumb.read_bytes()
        actual = abs(struct.unpack_from('<i', data, 22)[0])
        # The shared converters can truncate a floating-point scale by one row.
        assert data[:2] == b'BM' and height - 1 <= actual <= height, thumb
        if height in expected:
            assert actual == height, thumb
def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value

def check_native(catalog):
    checked = []
    for cache in caches:
        for spec in catalog['nativeCovers']:
            thumb = cache / ('thumb_' + str(spec['height']) + '.bmp')
            data = thumb.read_bytes()
            footer = data[-40:]
            assert footer[:8] == b'CNAT001\0', thumb
            key, source, size, pixels, image, _, guard = struct.unpack_from('<QIIIIII', footer, 8)
            assert key == int(spec['key'], 16) and size == spec['bytes'], thumb
            assert source == struct.unpack_from('<I', data, 2)[0] and source + size + 40 == len(data), thumb
            assert checksum(footer[:36]) == guard and checksum(data[:source]) == image, thumb
            assert checksum(data[source:-40]) == pixels, thumb
            checked.append(thumb)
    return checked

native = check_native(new)
before = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in (CARD / '.crosspoint').rglob('*') if p.is_file()}
_, warm = run(current, 'warm')
assert warm['alreadyPrepared'] == 3 and warm['total'] == 0, warm
assert all((p.read_bytes(), p.stat().st_mtime_ns) == saved for p, saved in before.items()), 'Warm preparation rewrote cache'
victim = native[0]
valid = victim.read_bytes()
source = struct.unpack_from('<I', valid, 2)[0]
damaged = bytearray(valid)
damaged[source] ^= 1
victim.write_bytes(damaged)
_, repaired = run(current, 'damaged-native')
assert repaired['total'] == 1 and repaired['alreadyPrepared'] == 2 and victim.read_bytes() == valid, repaired
victim.write_bytes(valid[:source])
_, restored = run(current, 'missing-native')
assert restored['total'] == 1 and restored['alreadyPrepared'] == 2 and victim.read_bytes() == valid, restored
for orientation in (1, 2, 3):
    config.write_text(json.dumps({'card': str(CARD), 'fonts': ['Ubuntu'], 'sizes': [12], 'verify': True,
                                  'profile': {'orientation': orientation}}))
    rotated, done = run(current, 'orientation-' + str(orientation))
    assert done['completed'] == 3, done
    check_native(rotated)
report = {'oldCovers': old['covers'], 'newCovers': new['covers'], 'updatedBooks': upgraded['completed'],
          'checkedThumbnails': len(caches) * len(new['covers']), 'nativeRasters': len(native),
          'cases': ['native pixel/source/footer checksums', 'damaged native repair', 'missing native repair', 'four orientations'],
          'preservedFiles': len(protected), 'warm': warm}
(TEST / 'report.json').write_text(json.dumps(report, indent=2))
print('PASS firmware native cover upgrade, BMP dimensions, checksum repair, four orientations, preservation and warm reuse')
print(TEST)
print(json.dumps(report, indent=2))
