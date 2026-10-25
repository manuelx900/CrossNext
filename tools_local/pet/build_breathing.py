#!/usr/bin/env python3
"""Register AI-edited sleep atlas cells and pack their masks for PET's optional SD animation."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import zlib

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets_local/pet/animation"
SPECIES = ("silva", "nox", "ember")
AGES = ("newborn", "baby", "juvenile", "adult")
MOODS = ("normal", "happy", "hungry", "tired", "sick")


def helpers():
    spec = importlib.util.spec_from_file_location("pet_mask_tools", Path(__file__).with_name("build_habitat.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def segments(image, axis, count):
    """Find the largest empty gutters: generated sheets need not have mathematically equal cells."""
    ink = ImageOps.invert(image.convert("L"))
    occupied = []
    for i in range(image.width if axis == 0 else image.height):
        line = ink.crop((i, 0, i+1, image.height) if axis == 0 else (0, i, image.width, i+1))
        if line.getbbox():
            occupied.append(i)
    if not occupied:
        raise ValueError("Empty animation atlas")
    gaps = sorted(((b-a-1, a+1, b) for a, b in zip(occupied, occupied[1:]) if b-a > 1), reverse=True)
    if len(gaps) < count-1 or any(g[0] < 6 for g in gaps[:count-1]):
        raise ValueError("Animation atlas has no clear cell gutters")
    cuts = sorted((a+b)//2 for _, a, b in gaps[:count-1])
    limits = [0, *cuts, image.width if axis == 0 else image.height]
    return list(zip(limits, limits[1:]))


def append_body_frames(tools, sprites, records, provenance):
    for species in SPECIES:
        poses = {}
        for mood in MOODS:
            path = SOURCE / "moods" / f"{species}-{mood}-atlas.png"
            with Image.open(path) as image:
                sheet = tools.monochrome(image)
            provenance.append({"file": "moods/"+path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
            for age, (top, bottom) in zip(AGES, segments(sheet, 1, 4)):
                row = sheet.crop((0, top, sheet.width, bottom))
                frames, bounds = [], []
                for left, right in segments(row, 0, 3):
                    frame, box = tools.crop_ink(row.crop((left, 0, right, row.height)))
                    frames.append(frame)
                    bounds.append([left+box[0], top+box[1], left+box[2], top+box[3]])
                reference_name = f"home-{species}-{age}-sick" if mood == "sick" else f"{species}-{age}-{mood}"
                with Image.open(ROOT / "assets_local/pet/png" / (reference_name+".png")) as original:
                    reference, _ = tools.crop_ink(tools.monochrome(original))
                max_w, max_h = max(f.width for f in frames), max(f.height for f in frames)
                ratio = min(reference.width/max_w, reference.height/max_h)
                size = max(2, round(max_w*ratio)), max(2, round(max_h*ratio))
                registered = []
                for frame in frames:
                    cell = Image.new("1", (max_w, max_h), 1)
                    cell.paste(frame, (0, max_h-frame.height))
                    registered.append(cell.resize(size, Image.Resampling.NEAREST))
                poses[age, mood] = registered, bounds
        for age in AGES:
            for mood in MOODS:
                frames, bounds = poses[age, mood]
                for phase, image in enumerate(frames):
                    for packed, mask in ((tools.silhouette(image), True), (image, False)):
                        sprites.append((packed.width, packed.height, tools.bits(packed, mask)))
                        records.append(dict(id=len(sprites)-1, name=f"{species}-{age}-{mood}-{phase}"+("-mask" if mask else ""),
                                            width=packed.width, height=packed.height, source_bounds=bounds[phase]))


def append_egg_frames(tools, sprites, records, provenance):
    path = SOURCE / "egg-wobble-atlas.png"
    with Image.open(path) as image:
        sheet = tools.monochrome(image)
    provenance.append({"file": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    for species, (top, bottom) in zip(SPECIES, segments(sheet, 1, 3)):
        row = sheet.crop((0, top, sheet.width, bottom))
        frames, bounds = [], []
        for left, right in segments(row, 0, 3):
            frame, box = tools.crop_ink(row.crop((left, 0, right, row.height)))
            frames.append(frame)
            bounds.append([left+box[0], top+box[1], left+box[2], top+box[3]])
        with Image.open(ROOT / "assets_local/pet/png" / f"{species}-egg.png") as original:
            reference, _ = tools.crop_ink(tools.monochrome(original))
        max_w, max_h = max(f.width for f in frames), max(f.height for f in frames)
        ratio = min(reference.width/max_w, reference.height/max_h)
        size = max(2, round(max_w*ratio)), max(2, round(max_h*ratio))
        for phase, frame in enumerate(frames):
            registered = Image.new("1", (max_w, max_h), 1)
            registered.paste(frame, ((max_w-frame.width)//2, max_h-frame.height))
            registered = registered.resize(size, Image.Resampling.NEAREST)
            for image, mask in ((tools.silhouette(registered), True), (registered, False)):
                sprites.append((image.width, image.height, tools.bits(image, mask)))
                records.append(dict(id=len(sprites)-1, name=f"{species}-egg-wobble-{phase}"+("-mask" if mask else ""),
                                    width=image.width, height=image.height, source_bounds=bounds[phase]))


def build(output=ROOT / "resources/apps/pet/breathing.bin"):
    tools = helpers()
    sprites, records, provenance, heads = [], [], [], []
    for species in SPECIES:
        path = SOURCE / f"{species}-sleep-atlas.png"
        with Image.open(path) as source:
            sheet = tools.monochrome(source)
        provenance.append({"file": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        for age, (top, bottom) in zip(AGES, segments(sheet, 1, 4)):
            row = sheet.crop((0, top, sheet.width, bottom))
            frames = []
            bounds = []
            for left, right in segments(row, 0, 3):
                cell = row.crop((left, 0, right, row.height))
                frame, box = tools.crop_ink(cell)
                frames.append(frame)
                bounds.append([left+box[0], top+box[1], left+box[2], top+box[3]])
            # All phases share one scale. Registration removes sheet drift while preserving breathing.
            max_w, max_h = max(f.width for f in frames), max(f.height for f in frames)
            reference = ROOT / "assets_local/pet/png" / f"home-{species}-{age}-resting.png"
            with Image.open(reference) as original:
                original, _ = tools.crop_ink(tools.monochrome(original))
            ratio = min(original.width/max_w, original.height/max_h)
            size = max(2, round(max_w*ratio)), max(2, round(max_h*ratio))
            if size[0] > 280 or size[1] > 512:
                raise ValueError("Oversized breathing frame")
            for phase, frame in enumerate(frames):
                registered = Image.new("1", (max_w, max_h), 1)
                registered.paste(frame, (0, max_h-frame.height))
                registered = registered.resize(size, Image.Resampling.NEAREST)
                name = f"{species}-{age}-breath-{phase}"
                silhouette = tools.silhouette(registered)
                head_box = (0, 0, max(2, size[0]*{"silva": 56, "nox": 60, "ember": 58}[species]//100),
                            max(2, size[1]*78//100))
                # Crop the full opaque mask too: a cut neck must not make the face transparent.
                if phase == 0:
                    heads.append((silhouette.crop(head_box), registered.crop(head_box), name, bounds[phase]))
                for image, mask in ((silhouette, True), (registered, False)):
                    payload = tools.bits(image, mask)
                    sprites.append((image.width, image.height, payload))
                    records.append(dict(id=len(sprites)-1, name=name+("-mask" if mask else ""),
                                        width=image.width, height=image.height, source_bounds=bounds[phase]))
    for silhouette, head, name, bounds in heads:
        for image, mask in ((silhouette, True), (head, False)):
            payload = tools.bits(image, mask)
            sprites.append((image.width, image.height, payload))
            records.append(dict(id=len(sprites)-1, name=name+"-head"+("-mask" if mask else ""),
                                width=image.width, height=image.height, source_bounds=bounds))
    assert len(sprites) == 96
    append_body_frames(tools, sprites, records, provenance)
    assert len(sprites) == 456
    append_egg_frames(tools, sprites, records, provenance)
    assert len(sprites) == 474
    index, data = bytearray(), bytearray()
    offset = 24 + len(sprites)*16
    for width, height, payload in sprites:
        index += struct.pack("<IHHII", offset, width, height, len(payload), zlib.crc32(payload))
        data += payload
        offset += len(payload)
    binary = struct.pack("<8sIIII", b"CNART002", len(sprites), offset, zlib.crc32(index+data), zlib.crc32(index))+index+data
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    info = dict(format="CNART002", sprites=474, poses=72, frames_per_pose=3, head_offset=72, head_frames_per_pose=1,
                body_offset=96, body_moods=list(MOODS), egg_offset=456, egg_phases=["left", "center", "right"], bytes=len(binary),
                sha256=hashlib.sha256(binary).hexdigest(), sources=provenance, images=records)
    output.with_name("breathing-manifest.json").write_text(json.dumps(info, indent=2)+"\n", encoding="utf-8")
    print(f"PET motion: 216 breathing frames, 9 egg frames, {len(binary)} bytes")
    return info


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "resources/apps/pet/breathing.bin")
    build(parser.parse_args().output)
