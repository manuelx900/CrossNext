"""Build Reading Adventure's grayscale catalogue and two-tile backgrounds."""
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import struct
import zlib

from PIL import Image
from build_whole_art import whole_cells
from build_campaign_art import enemies

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets_local/quest/redesign"
MASTERS = SOURCE / "masters"
NATIVE = SOURCE / "native"


def fit(source, size):
    picture = source.copy()
    picture.thumbnail((size[0] - 4, size[1] - 4), Image.Resampling.LANCZOS)
    output = Image.new("RGBA", size, (255, 255, 255, 0))
    output.alpha_composite(picture, ((size[0] - picture.width) // 2, size[1] - 2 - picture.height))
    return output


@lru_cache(maxsize=16)
def atlas(name, columns, rows):
    return whole_cells(MASTERS / name, columns, rows)


def heroes():
    result = []
    for page in range(10):
        cells = atlas(f"heroes-{page}.png", 5, 4)
        for n in range(10):
            row, column = (0 if n < 5 else 2), n % 5
            result.extend((cells[row * 5 + column], cells[(row + 1) * 5 + column]))
    approved = whole_cells(MASTERS / "heroes-approved.png", 3, 2)
    for outfit, column in ((0, 0), (59, 1), (81, 2)):
        result[outfit * 2] = approved[column]
        result[outfit * 2 + 1] = approved[3 + column]
    # Wings and capes use wider frames instead of reducing the hero's height.
    return [fit(picture, (max(128, (round(picture.width * 204 / picture.height) + 7) // 4 * 4), 208))
            for picture in result]


def pets():
    catalogue = json.loads((ROOT / "assets_local/quest/catalog_v4/catalog.json").read_text(encoding="utf-8"))
    originals = []
    for page in range(5):
        originals.extend(atlas(f"companions-{page}.png", 5, 2))
    approved = whole_cells(MASTERS / "companions-approved.png", 3, 2)
    for ident, cell in ((5, 0), (3, 1), (0, 2), (6, 3), (29, 4), (47, 5)):
        originals[ident] = approved[cell]
    result = []
    for item, picture in zip(catalogue["pets"], originals):
        height = max(96, min(190, item["height"]))
        scale = min(188 / picture.width, (height - 4) / picture.height)
        width = min(192, (round(picture.width * scale) + 11) // 8 * 8)
        result.append(fit(picture, (width, height)))
    return result


def backgrounds():
    result = [Image.open(MASTERS / "background-00.png").convert("RGB")]
    for start in range(1, 49, 6):
        with Image.open(MASTERS / f"backgrounds-{start:02d}.png") as sheet:
            for n in range(6):
                col, row = n % 3, n // 3
                # Trim the atlas boundary without moving the scene's shared camera.
                box = (round(col * sheet.width / 3) + 2, round(row * sheet.height / 2) + 2,
                       round((col + 1) * sheet.width / 3) - 2, round((row + 1) * sheet.height / 2) - 2)
                result.append(sheet.crop(box).convert("RGB"))
    result.append(Image.open(MASTERS / "background-49.png").convert("RGB"))
    output = []
    for picture in result:
        picture = picture.resize((480, 320), Image.Resampling.LANCZOS)
        # Fixed lower contrast keeps scenery behind the black character contours.
        picture = Image.blend(Image.new("RGB", picture.size, "white"), picture, 0.65)
        output.append(picture.convert("RGBA"))
    return output


def background_icons():
    """Catalog emblems retain strong contours; scenery keeps its softer palette."""
    output = []
    for page in range(2):
        for cell in atlas(f"background-icons-{page}.png", 5, 5):
            cell.thumbnail((108, 136), Image.Resampling.LANCZOS)
            canvas = Image.new("RGBA", (112, 144), "white")
            canvas.alpha_composite(cell, ((112 - cell.width) // 2, (144 - cell.height) // 2))
            output.append(canvas)
    return output


def sword_frames():
    source = Image.open(MASTERS / "combat-sword-v3.png").convert("RGBA")
    bounds = source.getchannel("A").point(lambda value: 255 if value >= 160 else 0).getbbox()
    assert bounds, "Combat sword has no opaque illustration"
    source = source.crop(bounds)
    source.thumbnail((80, 164), Image.Resampling.LANCZOS)
    frames = []
    for angle in (-18, -62, -112):
        pose = source.rotate(angle, Image.Resampling.BICUBIC, expand=True)
        frame = Image.new("RGBA", (192, 192), (0, 0, 0, 0))
        frame.alpha_composite(pose, ((192 - pose.width) // 2, (192 - pose.height) // 2))
        frames.append(frame)
    return frames


def decorations():
    result=[]
    content=json.loads((ROOT/'assets_local/quest/campaign/story.json').read_text('utf-8'))
    for item in content['decorations']:
        picture=Image.open(MASTERS/f"decoration-{item['asset']}-v1.png").convert('RGBA')
        assert picture.getchannel('A').getextrema() == (0,255)
        bounds=picture.getchannel('A').point(lambda a:255 if a>=160 else 0).getbbox()
        assert bounds
        result.append(fit(picture.crop(bounds),(112,144)))
    assert len(result)==16
    return result


def coverage(picture, mask=False):
    alpha = picture.getchannel("A").tobytes()
    gray = Image.alpha_composite(Image.new("RGBA", picture.size, "white"), picture).convert("L").tobytes()
    stride = (picture.width * 2 + 7) // 8
    output = bytearray(stride * picture.height)
    for y in range(picture.height):
        for x in range(picture.width):
            at = y * picture.width + x
            level = 0 if alpha[at] < 160 else 3 if mask else min(3, (255 - gray[at] + 42) // 85)
            output[y * stride + x // 4] |= level << (6 - (x % 4) * 2)
    return bytes(output)


def build(output):
    NATIVE.mkdir(parents=True, exist_ok=True)
    figures, creatures, scenes = heroes(), pets(), backgrounds()
    icons = [fit(p, (64, 80)) for p in creatures]
    clothes = [fit(figures[n * 2], (64, 80)) for n in range(100)]
    sprites = []
    def group(pictures, name, mask=False):
        for n, picture in enumerate(pictures):
            sprites.append((picture, name + str(n), mask))
    group(figures, "figure-"); group(figures, "figure-mask-", True)
    group(creatures, "pet-"); group(creatures, "pet-mask-", True)
    group(icons, "pet-icon-"); group(icons, "pet-icon-mask-", True)
    group([p.crop((0, 0, 240, 320)) for p in scenes], "background-left-")
    group([p.crop((240, 0, 480, 320)) for p in scenes], "background-right-")
    group(clothes, "outfit-"); group(clothes, "outfit-mask-", True)
    # A distinct count rejects legacy packs whose background bank contains aura masks.
    sprites.append((Image.new("RGBA", (8, 2), (0, 0, 0, 255)), "background-format-1", False))
    group(background_icons(), "background-icon-")
    assert len(sprites) == 951
    adversaries = enemies()
    group(adversaries, "enemy-"); group(adversaries, "enemy-mask-", True)
    assert len(sprites) == 1029
    prologue = Image.open(MASTERS / "campaign-prologue.png").convert("RGBA").resize((480, 320), Image.Resampling.LANCZOS)
    group([prologue.crop((0, 0, 240, 320)), prologue.crop((240, 0, 480, 320))], "prologue-")
    for name in ("peace-male", "peace-female", "liora", "king", "journey-female"):
        picture = Image.open(MASTERS / f"prologue-{name}.png").convert("RGBA").resize((480, 320), Image.Resampling.LANCZOS)
        group([picture.crop((0, 0, 240, 320)), picture.crop((240, 0, 480, 320))], f"prologue-{name}-")
    assert len(sprites) == 1041
    for n in range(12):
        picture = Image.open(MASTERS / f"saga-scene-{n:02d}.png").convert("RGBA").resize((480, 320), Image.Resampling.LANCZOS)
        group([picture.crop((0, 0, 240, 320)), picture.crop((240, 0, 480, 320))], f"saga-scene-{n:02d}-")
    assert len(sprites) == 1065
    swords = sword_frames()
    group(swords, "combat-sword-")
    group(swords, "combat-sword-mask-", True)
    assert len(sprites) == 1071
    ornaments=decorations()
    group(ornaments,'decoration-')
    group(ornaments,'decoration-mask-',True)
    assert len(sprites) == 1103
    index, pixels, records = bytearray(), bytearray(), []
    offset = 24 + 16 * len(sprites)
    for ident, (picture, name, mask) in enumerate(sprites):
        assert 0 < picture.width <= 280 and 0 < picture.height <= 512
        raw = coverage(picture, mask)
        index.extend(struct.pack("<IHHII", offset, picture.width, picture.height, len(raw), zlib.crc32(raw)))
        pixels.extend(raw); offset += len(raw)
        records.append(dict(id=ident, name=name, width=picture.width, height=picture.height, mask=mask,
                            pixelSha256=hashlib.sha256(raw).hexdigest()))
        if not mask:
            picture.save(NATIVE / (name + ".png"))
    for n, picture in enumerate(scenes):
        picture.save(NATIVE / f"background-{n:02d}.png")
    payload = index + pixels
    binary = struct.pack("<8sIIII", b"CNART003", len(sprites), offset, zlib.crc32(payload), zlib.crc32(index)) + payload
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    assert len(binary) <= 8 * 1024 * 1024
    manifest = dict(format="CNART003", artVersion=3, sprites=len(sprites), bytes=len(binary),
                    sha256=hashlib.sha256(binary).hexdigest(), images=records,
                    backgrounds=50, backgroundIcons=50, outfits=100, figures=200, companions=50, decorations=16, enemies=13, enemyStates=39,
                    provenance="assets_local/quest/redesign/provenance/")
    output.with_name("manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    for meta in (SOURCE / "provenance").glob("*.json"):
        record = json.loads(meta.read_text(encoding="utf-8"))
        master = MASTERS / (record["name"] + ".png")
        record["sha256"] = hashlib.sha256(master.read_bytes()).hexdigest()
        meta.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(f"Reading Adventure: 50 backgrounds, 200 figures, 50 companions; {len(binary):,} bytes on SD")
    return manifest


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "resources/apps/reading-adventure-v2/graphics.bin")
    build(parser.parse_args().output)
