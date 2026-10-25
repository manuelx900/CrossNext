#!/usr/bin/env python3
"""Convert the approved celebration illustration to an optional CNART002 SD pack."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


def build(output=ROOT / "resources/apps/pet/collection.bin"):
    source = ROOT / "assets_local/pet/collection/trophy-laurels.png"
    with Image.open(source) as image:
        image = image.convert("L").resize((280, 210), Image.Resampling.NEAREST)
        image = image.point(lambda value: 255 if value >= 128 else 0, "1")
    # FreeInk BW1 set bits are ink, opposite to Pillow's monochrome white bits.
    data = bytes(value ^ 255 for value in image.tobytes())
    index = struct.pack("<IHHII", 40, 280, 210, len(data), zlib.crc32(data))
    binary = struct.pack("<8sIIII", b"CNART002", 1, 40 + len(data), zlib.crc32(index + data),
                         zlib.crc32(index)) + index + data
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    info = dict(format="CNART002", sprites=1, bytes=len(binary), sha256=hashlib.sha256(binary).hexdigest(),
                sources=[dict(file=source.name, sha256=hashlib.sha256(source.read_bytes()).hexdigest())],
                images=[dict(id=0, name="trophy-laurels", width=280, height=210)])
    output.with_name("collection-manifest.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    print(f"PET celebration: 280 x 210, {len(binary)} SD bytes")
    return info


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "resources/apps/pet/collection.bin")
    build(parser.parse_args().output)
