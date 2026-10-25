#!/usr/bin/env python3
"""Build PET's CNART002 microSD packs from the supplied monochrome artwork."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import shutil
import zlib

ROOT = Path(__file__).resolve().parents[2]
SPECIES = ("silva", "nox", "ember")
STAGES = ("newborn", "baby", "juvenile", "adult")
MOODS = ("normal", "happy", "hungry", "tired")
OBJECTS = ("bed", "hammock", "shelter", "plant", "lamp", "leaf-mobile", "ball", "tunnel", "ring",
           "leaves", "stars", "stones")


def names():
    return ([f"{species}-egg" for species in SPECIES]
            + [f"{species}-{stage}-{mood}" for species in SPECIES for stage in STAGES for mood in MOODS]
           + [f"object-{name}" for name in OBJECTS] + ["ui-petting-hand"])


def home_names():
    return ([f"home-{species}-egg" for species in SPECIES]
            + [f"home-{species}-{stage}-{pose}" for species in SPECIES for stage in STAGES
               for pose in ("awake", "resting", "sick")])


def background_images():
    """Keep the supplied panorama intact; split only to fit the existing pack row limit."""
    from PIL import Image, ImageOps
    source = ROOT / "assets_local/pet/backgrounds"
    for species in SPECIES:
        path = source / f"{species}.png"
        with Image.open(path) as original:
            height = max(1, round(original.height * 448 / original.width))
            gray = original.convert("L").resize((448, height), Image.Resampling.NEAREST)
            if height < 144:
                gray = ImageOps.expand(gray, (0, 144 - height, 0, 0), fill=255)
            gray = gray.crop((0, gray.height - 144, 448, gray.height))
            image = gray.point(lambda p: 0 if p < 160 else 255, mode="1")
        checksum = hashlib.sha256(path.read_bytes()).hexdigest()
        for half in range(2):
            piece = image.crop((half * 224, 0, (half + 1) * 224, 144))
            pixels = ImageOps.invert(piece.convert("L")).convert("1").tobytes()
            yield f"background-{species}-{half}", pixels, checksum


def ink_region(image):
    """Pack original ink bits with an eight-pixel breathing margin, without resampling."""
    width, height = image.size
    raw = image.tobytes()
    stride = (width + 7) // 8
    left, top, right, bottom = width, height, -1, -1
    for y in range(height):
        for x in range(width):
            if not raw[y * stride + x // 8] & (0x80 >> (x % 8)):
                left, top = min(left, x), min(top, y)
                right, bottom = max(right, x), max(bottom, y)
    if right < left:
        raise ValueError("Supplied artwork must contain visible ink")
    left, top = max(0, left - 8), max(0, top - 8)
    right, bottom = min(width, right + 9), min(height, bottom + 9)
    packed_width, packed_height = right - left, bottom - top
    packed_stride = (packed_width + 7) // 8
    pixels = bytearray(packed_stride * packed_height)
    for y in range(packed_height):
        for x in range(packed_width):
            source_x, source_y = left + x, top + y
            if not raw[source_y * stride + source_x // 8] & (0x80 >> (source_x % 8)):
                pixels[y * packed_stride + x // 8] |= 0x80 >> (x % 8)
    return packed_width, packed_height, pixels, [left, top, packed_width, packed_height]


def build(output, source=None, home=False):
    images, manifest = [], []
    for image_id, name in enumerate(home_names() if home else names()):
        path = source / f"{name}.png" if source else None
        hand = name == "ui-petting-hand"
        if hand and source:
            path = ROOT / "assets_local/pet/ui/petting-hand.png"
        supplied = bool(path and path.is_file())
        bounds = None
        if supplied:
            from PIL import Image
            with Image.open(path) as image:
                if hand:
                    from PIL import ImageOps
                    image = image.convert("L").point(lambda p: 0 if p < 160 else 255, mode="1")
                    box = ImageOps.invert(image.convert("L")).getbbox()
                    if box is None:
                        raise ValueError("The petting hand must contain visible ink")
                    image = image.crop(box)
                    image.thumbnail((144, 100), Image.Resampling.NEAREST)
                if image.mode != "1":
                    raise ValueError(f"{path}: artwork must already be a monochrome 1-bit PNG")
                width, height = image.size
                if not 1 < width <= 280 or not 1 < height <= 512:
                    raise ValueError(f"{path}: image must fit 280 x 512 pixels")
                width, height, pixels, bounds = ink_region(image)
        else:
            width, height, pixels = 1, 1, b"\x00"
        images.append((width, height, pixels))
        manifest.append({"id": image_id, "name": name, "supplied": supplied, "width": width, "height": height})
        if bounds is not None:
            manifest[-1]["source_bounds"] = bounds
        if hand and supplied:
            manifest[-1]["source_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            manifest[-1]["conversion"] = "threshold 160; ink crop; nearest fit 144x100"
    if home:
        for name, pixels, checksum in background_images():
            images.append((224, 144, pixels))
            manifest.append({"id": len(images) - 1, "name": name, "supplied": True,
                             "width": 224, "height": 144, "source_sha256": checksum,
                             "conversion": "nearest resize width 448; bottom crop/pad height 144; threshold 160; no ink crop"})
    offset = 24 + len(images) * 16
    index, data = bytearray(), bytearray()
    for width, height, pixels in images:
        index += struct.pack("<IHHII", offset, width, height, len(pixels), zlib.crc32(pixels))
        data += pixels
        offset += len(pixels)
    payload = index + data
    header = struct.pack("<8sIIII", b"CNART002", len(images), offset, zlib.crc32(payload), zlib.crc32(index))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(header + payload)
    info = {"format": "CNART002", "sprites": len(images), "bytes": offset,
            "sha256": hashlib.sha256(header + payload).hexdigest(), "images": manifest}
    output.with_name("home-manifest.json" if home else "manifest.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    print(f"PET: {len(images)} stable sprite slots, {sum(item['supplied'] for item in manifest)} illustrations, {offset} bytes")
    return info


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "resources/apps/pet/graphics.bin")
    parser.add_argument("--source", type=Path, default=ROOT / "assets_local/pet/png",
                        help="Folder of final 1-bit PNG artwork (default: supplied PET artwork)")
    parser.add_argument("--home", action="store_true", help="Build the optional Home pose pack")
    args = parser.parse_args()
    output = args.output.with_name("home.bin") if args.home and args.output.name == "graphics.bin" else args.output
    build(output, args.source, args.home)
    readme = ROOT / "assets_local/pet/README.md"
    if readme.resolve() != (output.parent / "README.md").resolve():
        shutil.copy2(readme, output.parent / "README.md")
