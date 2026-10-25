#!/usr/bin/env python3
"""Build The Intercept's bounded CNART002 microSD illustration pack."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zlib

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets_local/intercept/png"
NAMES = tuple(f"scene{n:02}" for n in range(46)) + ("Document", "Disk", "DocumentRestart")


def build(output):
    records, index, data = [], bytearray(), bytearray()
    offset = 24 + 16 * len(NAMES)
    for n, name in enumerate(NAMES):
        source = SOURCE / (name + ".png")
        with Image.open(source) as original:
            image = original.convert("L")
        # Match the SD renderer's bounded row scratch without changing aspect ratio.
        image.thumbnail((280, 512), Image.Resampling.NEAREST)
        image = image.point(lambda v: 255 if v >= 128 else 0, "1")
        raw = bytes(v ^ 255 for v in image.tobytes())
        width, height = image.size
        assert len(raw) == ((width + 7) // 8) * height
        index += struct.pack("<IHHII", offset, width, height, len(raw), zlib.crc32(raw))
        data += raw
        offset += len(raw)
        records.append(dict(id=n, name=name, width=width, height=height,
                            sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                            pixelSha256=hashlib.sha256(raw).hexdigest()))
    binary = struct.pack("<8sIIII", b"CNART002", len(NAMES), offset,
                         zlib.crc32(index + data), zlib.crc32(index)) + index + data
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    manifest = dict(format="CNART002", sprites=len(NAMES), bytes=len(binary),
                    sha256=hashlib.sha256(binary).hexdigest(), images=records)
    output.with_name("manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    shutil.copy2(ROOT / "assets_local/intercept/LICENSE.txt", output.parent / "LICENSE.txt")
    print(f"Intercept resources: {len(NAMES)} illustrations, {len(binary):,} bytes")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "resources/apps/intercept/graphics.bin")
    build(parser.parse_args().output)
