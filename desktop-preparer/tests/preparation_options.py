"""Actual font capabilities, per-job EPUB engines and unchanged reader settings."""
from pathlib import Path
import hashlib
import io
import json
import shutil
import subprocess
import sys
import time
import zipfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
TEST = ROOT / 'build' / ('preparation-options-' + str(time.time_ns()))
CARD = TEST / 'Card'
EXE = ROOT / 'dist' / (sys.argv[1] if len(sys.argv) > 1 else 'CrossNext-Book-Preparer-Test.exe')
(CARD / '.crosspoint').mkdir(parents=True)
(CARD / 'books').mkdir()
(CARD / 'fonts/Vector').mkdir(parents=True)
(CARD / 'fonts/Bitmap').mkdir()
shutil.copy2(REPO / 'Resources/fonts/Ubuntu/Ubuntu-Regular.ttf', CARD / 'fonts/Vector/Regular.ttf')
for point in (12, 18):
    name = f'Noto Serif_{point}.cpfont'
    shutil.copy2(REPO / 'Resources/fonts/Noto Serif' / name, CARD / 'fonts/Bitmap' / f'Bitmap_{point}.cpfont')
settings = CARD / '.crosspoint/settings.json'
settings.write_text(json.dumps({'epubEngine': 1, 'sdFontFamilyName': 'Bitmap',
    'readerFontStorageVersion': 1, 'fontPointSize': 18}), encoding='utf-8')
cover = io.BytesIO()
Image.new('RGB', (160, 240), 'white').save(cover, format='PNG')
with zipfile.ZipFile(CARD / 'books/Test.epub', 'w', zipfile.ZIP_DEFLATED) as book:
    book.writestr('mimetype', 'application/epub+zip')
    book.writestr('META-INF/container.xml', '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
    book.writestr('content.opf', '<package xmlns="http://www.idpf.org/2007/opf" version="3.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Preparation options</dc:title><dc:creator>Test</dc:creator><meta name="cover" content="cover"/></metadata><manifest><item id="text" href="text.xhtml" media-type="application/xhtml+xml"/><item id="cover" href="cover.png" media-type="image/png"/></manifest><spine><itemref idref="text"/></spine></package>')
    book.writestr('text.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml"><body><h1>Test</h1><p>Supported sizes and engines preserve the reader settings.</p></body></html>')
    book.writestr('cover.png', cover.getvalue())
protected = {path: hashlib.sha256(path.read_bytes()).digest() for path in CARD.rglob('*') if path.is_file()}

def worker(mode, **options):
    config = TEST / 'job.json'
    config.write_text(json.dumps({'card': str(CARD), **options}), encoding='utf-8')
    result = subprocess.run(['node', str(ROOT / 'dist/runtime/worker.cjs'), mode, str(config)],
        capture_output=True, text=True, encoding='utf-8', timeout=120)
    events = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    for path, digest in protected.items():
        assert hashlib.sha256(path.read_bytes()).digest() == digest, path
    return result, events

result, events = worker('inspect')
assert result.returncode == 0, result.stdout
catalog = events[0]
families = {font['name']: font['sizes'] for font in catalog['fonts']}
assert families == {'Ubuntu': [12], 'Bitmap': [12, 18], 'Vector': [12, 14, 16, 18]}, families
assert catalog['profile']['epubEngine'] == 1 and catalog['selectedSize'] == 18

# Exercise saved and overridden engine selection through the shared worker.
# Native shell controls are covered by tests/desktop.cjs and tests/packaged.cjs.
for name, override, expected in [('saved-classic', None, 1), ('override-modern', 0, 0)]:
    options = {} if override is None else {'profile': {'epubEngine': override}}
    result, events = worker('run', fonts=['Ubuntu'], sizes=[12], formats=['epub'], verify=True, **options)
    assert result.returncode == 0, result.stdout + result.stderr
    assert any(event['type'] == 'verified' for event in events), events
    print(name, 'PASS', flush=True)

result, events = worker('run', fonts=['Vector', 'Bitmap', 'Ubuntu', 'Vector'], sizes='all',
    formats=['epub'], verify=True, profile={'epubEngine': 0})
assert result.returncode == 0, result.stdout + result.stderr
verified = [event for event in events if event['type'] == 'verified']
assert len(verified) == 7, verified
assert {(event['font'], event['size']) for event in verified} == {
    *[('Vector', point) for point in [12, 14, 16, 18]], ('Bitmap', 12), ('Bitmap', 18), ('Ubuntu', 12)}
assert all(event['pages'] > 0 for event in verified), verified
before = {path: hashlib.sha256(path.read_bytes()).digest() for path in (CARD / '.crosspoint').rglob('*') if path.is_file()}
result, events = worker('run', fonts=['Bitmap'], sizes=[15])
assert result.returncode != 0 and 'unsupported size 15' in result.stdout
assert all(hashlib.sha256(path.read_bytes()).digest() == digest for path, digest in before.items())
assert list((CARD / '.crosspoint').rglob('*.classic-p*.bin'))
print('PASS: 7 firmware-supported layouts, no duplicate work, unsupported bitmap size rejected, both engines and original settings preserved')
print(TEST)
