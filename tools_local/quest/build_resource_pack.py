#!/usr/bin/env python3
"""Export the canonical lossless Quest sprites to the SD-card CNART001 pack."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
ENCODED = ROOT / "assets_local/quest/encoded"
GROUPS = (
    ("QuestFigures.h", "figures", 200),
    ("QuestFigures.h", "figuresMasks", 200),
    ("QuestArt.h", "pets", 50),
    ("QuestArt.h", "petsMasks", 50),
    ("QuestArt.h", "petIcons", 50),
    ("QuestArt.h", "petIconMasks", 50),
    ("QuestAuraArt.h", "motifs", 50),
    ("QuestAuraArt.h", "motifsMasks", 50),
    ("QuestOutfitIcons.h", "icons", 100),
    ("QuestOutfitIcons.h", "iconsMasks", 100),
)


def sheet(path):
    text = path.read_text(encoding="utf-8")
    nodes = [tuple(map(int, p)) for p in re.findall(r"\{(\d+),(\d+)\}",
             re.search(r"codebook\[\]=\{(.*?)\};", text, re.S)[1])]
    arrays = {name: bytes(int(v.strip(), 0) for v in body.split(",") if v.strip())
              for name, body in re.findall(r"uint8_t (\w+)\[\]=\{(.*?)\};", text, re.S)}
    groups = {}
    for group, body in re.findall(r"Sprite (\w+)\[\]=\{(.*?)\};", text, re.S):
        refs = []
        for name, w, h, root, predictor in re.findall(
                r"\{(\w+),codebook,sizeof\(\w+\),(\d+),(\d+),(\d+),(\d+)\}", body):
            refs.append((arrays[name], int(w), int(h), int(root), int(predictor)))
        groups[group] = refs
    return nodes, groups


def decode(nodes, packed, width, height, root, predictor):
    """Decode the existing Huffman/RLE/predictor format without changing pixels."""
    cursor = 0
    def symbol():
        nonlocal cursor
        node = root
        for _ in range(24):
            if cursor >= len(packed) * 8:
                raise ValueError("Truncated encoded sprite")
            node = nodes[node][(packed[cursor // 8] >> (7 - cursor % 8)) & 1]
            cursor += 1
            if node >= 256:
                return node - 256
        raise ValueError("Invalid Huffman tree")
    assert 0 < width <= 280 and 0 < height <= 512 and predictor in range(4)
    stride = (width + 7) // 8
    distance = 1 if predictor == 3 else predictor * stride
    raw = bytearray()
    while len(raw) < stride * height:
        control = symbol()
        literal = control < 128
        length = control + 1 if literal else control - 127
        repeated = 0 if literal else symbol()
        if len(raw) + length > stride * height:
            raise ValueError("RLE extends past sprite dimensions")
        for _ in range(length):
            value = symbol() if literal else repeated
            raw.append(value ^ (raw[-distance] if distance and len(raw) >= distance else 0))
    return bytes(raw)


def build(output):
    sheets = {name: sheet(ENCODED / name) for name, _, _ in GROUPS}
    images, groups = [], []
    for filename, name, count in GROUPS:
        nodes, bank = sheets[filename]
        assert len(bank[name]) == count, (filename, name)
        start = len(images)
        for packed, w, h, root, predictor in bank[name]:
            images.append((decode(nodes, packed, w, h, root, predictor), w, h))
        groups.append({"name": name, "first": start, "count": count})
    offset = 24 + 16 * len(images)
    index, data = bytearray(), bytearray()
    for raw, width, height in images:
        index.extend(struct.pack("<IHHII", offset, width, height, len(raw), zlib.crc32(raw)))
        data.extend(raw)
        offset += len(raw)
    payload = index + data
    header = struct.pack("<8sIIII", b"CNART002", len(images), offset, zlib.crc32(payload), zlib.crc32(index))
    binary = header + payload
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    manifest = {"format": "CNART002", "file": output.name, "bytes": len(binary),
                "sha256": hashlib.sha256(binary).hexdigest(), "sprites": len(images),
                "pixelSha256": hashlib.sha256(data).hexdigest(), "groups": groups}
    output.with_name("manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Quest resources: {len(images)} sprites, {len(binary):,} bytes, {len(zlib.compress(binary, 9)):,} compressed")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "resources/apps/reading-adventure/graphics.bin")
    build(parser.parse_args().output)
