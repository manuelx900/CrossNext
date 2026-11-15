"""Validate distinct expansion originals, progression bindings and distributable packs."""
import hashlib
import json
from pathlib import Path
import struct
import zlib


def check(root):
    import copy
    import sys
    sys.path.insert(0, str(root / 'tools_local/quest'))
    from bestiary_catalog import load, validate
    bestiary_directory = root / 'assets_local/quest/bestiary'
    entries = load(bestiary_directory)
    swapped = copy.deepcopy(entries)
    swapped[0], swapped[1] = swapped[1], swapped[0]
    try:
        validate(swapped)
    except ValueError:
        pass
    else:
        raise AssertionError('Reordered bestiary IDs must be rejected')
    provenance = json.loads((bestiary_directory / 'provenance.json').read_text('utf8'))
    assert len(provenance['masters']) == 7
    for master in provenance['masters']:
        assert hashlib.sha256((bestiary_directory / master['master']).read_bytes()).hexdigest() == master['sha256']
    source = root / "assets_local/quest/expansion"
    catalog = json.loads((source / "catalog.json").read_text("utf8"))
    assert len(catalog["decorations"]) == 101 and len(catalog["scenes"]) == 120
    assert len(catalog["enemies"]) == 72 and len(catalog["jobs"]) == 289
    assert sorted(e["sprite"] for e in catalog["enemies"] if "sprite" in e) == list(range(60))
    assert {s["chapter"] for s in catalog["scenes"]} == set(range(120))
    assert all(s["beat"] in (1, 2, 3) for s in catalog["scenes"])
    assert [s["request"] for s in catalog["requestScenes"]] == [0, 1, 2]
    assert [d["item"] for d in catalog["decorations"]] == list(range(216, 317))
    for d in catalog["decorations"]:
        assert d["book"] == (d["sourceId"] // 10 if d["source"] == "Chapter" else
                             d["sourceId"] // 25 if d["source"] == "Request" else 11)
    for kind in ("Chapter", "Request"):
        rewards = [d["sourceId"] for d in catalog["decorations"] if d["source"] == kind]
        assert len(rewards) == len(set(rewards)) == 50
    hashes = set()
    active = [job for job in catalog["jobs"] if job["kind"] not in ("scene", "requestScene")]
    retired_paths = {"assets_local/quest/campaign/path-art/masters/" + entry["master"]
                     for entry in json.loads((root / "assets_local/quest/campaign/story.json").read_text("utf8"))["pathIllustrations"]}
    for job in active:
        image = (source / "masters" / job["master"]).read_bytes()
        record = json.loads((source / "provenance" / (job["id"] + ".json")).read_text("utf8"))
        digest = hashlib.sha256(image).hexdigest()
        assert digest not in hashes, job["id"]
        hashes.add(digest)
        assert image[:8] == b"\x89PNG\r\n\x1a\n"
        width, height = struct.unpack_from(">II", image, 16)
        assert width >= 512 and height >= 512
        assert (width, height) == (record["width"], record["height"])
        assert record["sha256"] == digest and record["tool"] == "built-in image_gen"
        assert all(record[key] == job[key] for key in ("id", "master", "prompt", "reference", "transparent"))
        assert record.get("references", [record["reference"]]) == job.get("references", [job["reference"]])
        # A generation reference may belong to the retired story; the generated output remains verified.
        assert (root / job["reference"]).is_file() or job["reference"] in retired_paths
    folder = root / "resources/apps/reading-adventure"
    manifest = json.loads((folder / "expansion-manifest.json").read_text("utf8"))
    assert manifest["originals"] == manifest["expectedOriginals"] == len(active) == 166 and not manifest["preview"]
    counts = {"rewards.bin": 593, "guardians.bin": 360}
    assert not (folder / "moments.bin").exists()
    assert {p["file"] for p in manifest["packs"]} == set(counts)
    payloads = {}
    for entry in manifest["packs"]:
        data = (folder / entry["file"]).read_bytes()
        assert not entry["preview"] and len(data) < 8 * 1024 * 1024
        assert hashlib.sha256(data).hexdigest() == entry["sha256"]
        magic, count, size, crc, index_crc = struct.unpack_from("<8sIIII", data)
        assert magic == b"CNART003" and size == len(data) == entry["bytes"]
        assert count == counts[entry["file"]] == entry["sprites"]
        assert zlib.crc32(data[24:]) == crc and zlib.crc32(data[24:24+count*16]) == index_crc
        pictures = []
        end = 24 + count * 16
        for n in range(count):
            offset, width, height, length, checksum = struct.unpack_from("<IHHII", data, 24+n*16)
            assert offset == end and 0 < width <= 256 and 0 < height <= 320
            assert length == ((width * 2 + 7) // 8) * height
            pixels = data[offset:offset+length]
            assert len(pixels) == length and zlib.crc32(pixels) == checksum and any(pixels)
            pictures.append(pixels)
            end += length
        assert end == len(data)
        payloads[entry["file"]] = pictures
    decorations = payloads["rewards.bin"][:101]
    bestiary = payloads["rewards.bin"][257:425]
    assert len(bestiary) == len(set(bestiary)) == 168
    for ink, mask in zip(bestiary, payloads["rewards.bin"][425:593]):
        assert all((a >> shift & 3) == 0 or (b >> shift & 3) > 0
                   for a, b in zip(ink, mask) for shift in (0, 2, 4, 6))
    assert len(set(decorations)) == 101
    for ink, mask in zip(decorations, payloads["rewards.bin"][101:202]):
        assert all((a >> shift & 3) == 0 or (b >> shift & 3) > 0
                   for a, b in zip(ink, mask) for shift in (0, 2, 4, 6))
        assert sum(a >> shift & 3 == 3 for a in ink for shift in (0, 2, 4, 6)) >= 40
    guardians = payloads["guardians.bin"]
    assert len(set(guardians[:180])) == 180
    for ink, mask in zip(guardians[:180], guardians[180:]):
        assert all((a >> shift & 3) == 0 or (b >> shift & 3) > 0
                   for a, b in zip(ink, mask) for shift in (0, 2, 4, 6))
    print("Expansion artwork: 166 active originals, no retired moments pack, 180 guardian poses, provenance and all pack CRCs passed")


if __name__ == "__main__":
    check(Path(__file__).resolve().parents[2])
