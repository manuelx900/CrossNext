#!/usr/bin/env python3
"""Convert the three original farewell illustrations into an optional masked SD pack."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import zlib

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets_local/pet/farewell"


def build(output=ROOT / "resources/apps/pet/farewell.bin"):
    spec = importlib.util.spec_from_file_location("pet_farewell_masks", Path(__file__).with_name("build_habitat.py"))
    tools = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tools)
    sprites, records, sources = [], [], []
    png = SOURCE / "firmware-png"
    png.mkdir(exist_ok=True)
    for species in ("silva", "nox", "ember"):
        source = SOURCE / f"{species}-farewell.png"
        with Image.open(source) as original:
            image, bounds = tools.crop_ink(tools.monochrome(original))
        ratio = min(208/image.width, 240/image.height, 1)
        image = tools.monochrome(image, (max(2, round(image.width*ratio)), max(2, round(image.height*ratio))))
        image, _ = tools.crop_ink(image)
        image.save(png / source.name)
        sources.append(dict(file=source.name, sha256=hashlib.sha256(source.read_bytes()).hexdigest(), ink_bounds=bounds))
        for frame, mask in ((tools.silhouette(image), True), (image, False)):
            payload = tools.bits(frame, mask)
            sprites.append((frame.width, frame.height, payload))
            records.append(dict(id=len(sprites)-1, name=species+('-mask' if mask else ''), width=frame.width, height=frame.height))
    index, data = bytearray(), bytearray()
    offset = 24 + len(sprites)*16
    for width, height, payload in sprites:
        index += struct.pack("<IHHII", offset, width, height, len(payload), zlib.crc32(payload))
        data += payload
        offset += len(payload)
    binary = struct.pack("<8sIIII", b"CNART002", len(sprites), offset, zlib.crc32(index+data), zlib.crc32(index))+index+data
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    manifest = dict(format="CNART002", sprites=6, bytes=len(binary), sha256=hashlib.sha256(binary).hexdigest(), sources=sources, images=records)
    output.with_name("farewell-manifest.json").write_text(json.dumps(manifest, indent=2)+'\n',encoding='utf-8')
    print(f"PET farewell: 3 illustrations, {len(binary)} bytes")
    return manifest


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,default=ROOT / "resources/apps/pet/farewell.bin")
    build(parser.parse_args().output)
