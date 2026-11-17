"""Build one current-story illustration per campaign path in a bounded CNART003 pack."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets_local/quest/campaign/paths-v2"
SCENES = 120


def pixels(picture):
    gray = picture.convert("L").tobytes()
    data = bytearray(len(gray) // 4)
    for at, value in enumerate(gray):
        data[at // 4] |= min(3, (255 - value + 42) // 85) << (6 - at % 4 * 2)
    return bytes(data)


def build(output):
    catalog = json.loads((SOURCE / "catalog.json").read_text("utf-8"))
    story = json.loads((SOURCE.parent / "paths.json").read_text("utf-8"))
    canonical_story = json.dumps(story, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    assert catalog["sourceStorySha256"] == hashlib.sha256(canonical_story).hexdigest()
    scenes = catalog["jobs"]
    assert catalog["version"] == 1 and catalog["storyVersion"] == 2
    assert len(scenes) == SCENES and [s["id"] for s in scenes] == list(range(SCENES))
    assert all((s["saga"], s["step"]) == divmod(s["id"], 10) for s in scenes)
    records, index, payload, originals, rendered = [], bytearray(), bytearray(), set(), set()
    offset = 24 + SCENES * 2 * 16
    for scene in scenes:
        assert scene["master"] == f"path-{scene['id']:03d}.png"
        path = SOURCE / "masters" / scene["master"]
        original = path.read_bytes()
        digest = hashlib.sha256(original).hexdigest()
        provenance = json.loads((SOURCE / "provenance" / path.with_suffix(".json").name).read_text("utf-8"))
        assert provenance["sha256"] == digest and provenance["id"] == scene["id"]
        assert provenance["generated"] and provenance["tool"] == "built-in image_gen"
        assert provenance["guardian"] == scene["guardian"] and provenance["prompt"] == scene["prompt"]
        with Image.open(path) as source:
            assert source.width * 2 == source.height * 3, "Path illustrations must preserve the complete 3:2 scene"
            picture = source.convert("RGB").resize((480, 320), Image.Resampling.LANCZOS)
        raw = pixels(picture)
        image_hash = hashlib.sha256(raw).hexdigest()
        assert digest not in originals and image_hash not in rendered, "Duplicate story illustration"
        originals.add(digest)
        rendered.add(image_hash)
        records.append(dict(id=scene["id"], book=scene["saga"], step=scene["step"], guardian=scene["guardian"],
                            master=scene["master"], masterSha256=digest, pixelSha256=image_hash))
        for x in (0, 240):
            tile = pixels(picture.crop((x, 0, x + 240, 320)))
            index.extend(struct.pack("<IHHII", offset, 240, 320, len(tile), zlib.crc32(tile)))
            payload.extend(tile)
            offset += len(tile)
    content = index + payload
    binary = struct.pack("<8sIIII", b"CNART003", SCENES * 2, offset, zlib.crc32(content), zlib.crc32(index)) + content
    assert len(binary) == 4611864 and len(binary) <= 8 * 1024 * 1024
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    manifest = dict(format="CNART003", storyVersion=2, scenes=SCENES, sprites=SCENES * 2, bytes=len(binary),
                    sha256=hashlib.sha256(binary).hexdigest(), images=records)
    output.with_name("story-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Reading Adventure story: {SCENES} original scenes, {SCENES * 2} tiles, {len(binary):,} SD bytes")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "resources/apps/reading-adventure-v2/story.bin")
    build(parser.parse_args().output)
