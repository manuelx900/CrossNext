"""Encode the selected celebration masters into a bounded-row SD artwork pack."""
from pathlib import Path
import hashlib, json, struct, zlib
from PIL import Image
ROOT = Path(__file__).resolve().parents[2]
MASTERS = ROOT / 'assets_local/quest/milestones'
OUT = ROOT / 'resources/apps/reading-adventure-v2'

def build(output=OUT):
    records = bytearray()
    pixels = bytearray()
    masters = []
    offset = 24 + 6 * 16
    for level in (50, 100, 150):
        source = MASTERS / f'level-{level}.png'
        with Image.open(source) as image:
            image = image.convert('L').resize((480, 320), Image.Resampling.LANCZOS)
            for x in (0, 240):
                gray = image.crop((x, 0, x + 240, 320)).tobytes()
                tile = bytearray(len(gray) // 4)
                for at, value in enumerate(gray):
                    tile[at // 4] |= min(3, (255 - value + 42) // 85) << (6 - at % 4 * 2)
                records += struct.pack('<IHHII', offset + len(pixels), 240, 320, len(tile), zlib.crc32(tile))
                pixels += tile
        masters.append({'level': level, 'source': source.relative_to(ROOT).as_posix(),
                        'sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
    content = records + pixels
    data = struct.pack('<8sIIII', b'CNART003', 6, offset + len(pixels), zlib.crc32(content), zlib.crc32(records)) + content
    output.mkdir(parents=True, exist_ok=True)
    (output / 'celebrations.bin').write_bytes(data)
    (output / 'celebrations.manifest.json').write_text(json.dumps({'format': 'CNART003', 'images': 6,
        'sha256': hashlib.sha256(data).hexdigest(), 'masters': masters}, indent=2) + '\n', encoding='utf8')
    print(f'Celebrations: {len(data)} bytes, 3 scenes / 6 tiles')
if __name__ == '__main__':
    build()
