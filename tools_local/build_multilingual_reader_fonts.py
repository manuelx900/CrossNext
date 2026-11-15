#!/usr/bin/env python3
"""Build full streamed SC/TC fonts and pre-shaped Thai reading fonts for microSD."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import urllib.request
import zipfile

from fontTools.ttLib import TTFont, newTable
from fontTools.varLib.instancer import instantiateVariableFont

from shape_language_thai import add_clusters, cluster_codepoints

ROOT = Path(__file__).resolve().parents[1]
REVISION = "66a36c8c94b1a5d992ee4e7f392fccfe4945767c"
BASE = f"https://raw.githubusercontent.com/google/fonts/{REVISION}/ofl/"
FAMILIES = (
    ("Noto Serif SC", "notoserifsc", "NotoSerifSC[wght].ttf", "NotoSerifSC%5Bwght%5D.ttf",
     "050080d9255a86808f2945bffac582b31ef32bc36411ce29563b4961670c66f9",
     "5e0da210fb04058a8c0087985d2d456b931c2579811a49655721d3cf0c36b6d6"),
    ("Noto Serif TC", "notoseriftc", "NotoSerifTC[wght].ttf", "NotoSerifTC%5Bwght%5D.ttf",
     "0077e18f57c6908f4a000969880940bdb0dad057c0e8d98b49dc364c3d1b09c6",
     "5e0da210fb04058a8c0087985d2d456b931c2579811a49655721d3cf0c36b6d6"),
    ("Noto Serif Thai", "notoserifthai", "NotoSerifThai[wdth,wght].ttf", "NotoSerifThai%5Bwdth%2Cwght%5D.ttf",
     "34a7ad11647c845303aabdde639059806c56b84719e5d2ceb28eb038711bdf53",
     "2e98fd23a52d253db8612cd5942c8f2ff4111b21d2367050fdca91d8ccc374a0"),
)


def verified_source(cache, folder, name, remote, digest):
    source = cache / folder / name
    source.parent.mkdir(parents=True, exist_ok=True)
    if not source.exists():
        temporary = source.with_suffix(source.suffix + ".download")
        urllib.request.urlretrieve(BASE + folder + "/" + remote, temporary)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Source checksum changed: {name}")
        temporary.replace(source)
    if hashlib.sha256(source.read_bytes()).hexdigest() != digest:
        raise ValueError(f"Source checksum changed: {name}")
    return source


def thai_pairs(clusters):
    pairs = {}
    for text, output in clusters.items():
        left = ord(text[0]) if len(text) == 2 else clusters[text[:-1]]
        key = (left << 16) | ord(text[-1])
        if key in pairs:
            raise ValueError("Ambiguous Thai cluster")
        pairs[key] = output
    return sorted(pairs.items())


def prepare_thai(source, destination, weight, clusters):
    font = TTFont(source, recalcTimestamp=False)
    axes = {axis.axisTag: axis.defaultValue for axis in font["fvar"].axes}
    axes["wght"] = weight
    font = instantiateVariableFont(font, axes, inplace=True)
    add_clusters(font, source, weight, clusters)
    table = newTable("CPLG")
    pairs = thai_pairs(clusters)
    table.data = struct.pack(">4sHH", b"CNLG", 1, len(pairs)) + b"".join(struct.pack(">II", *pair) for pair in pairs)
    font["CPLG"] = table
    font["head"].modified = font["head"].created
    font["OS/2"].usWeightClass = weight
    font["OS/2"].fsSelection = (font["OS/2"].fsSelection & ~0x60) | (0x20 if weight == 700 else 0x40)
    font["head"].macStyle = (font["head"].macStyle & ~1) | (1 if weight == 700 else 0)
    style = "Bold" if weight == 700 else "Regular"
    for name_id, value in ((1, "Noto Serif Thai"), (2, style), (4, "Noto Serif Thai " + style),
                           (6, "NotoSerifThai-" + style), (16, "Noto Serif Thai"), (17, style)):
        font["name"].setName(value, name_id, 3, 1, 0x409)
    font.save(destination)
    font.close()


def build(output, cache, archive=None):
    output, cache = Path(output), Path(cache)
    records = []
    prefixes = []
    clusters = cluster_codepoints([], input_alphabet=True)
    for family, folder, name, remote, digest, license_digest in FAMILIES:
        relative = Path("fonts") / family
        prefixes.append(relative.as_posix() + "/")
        destination = output / relative
        destination.mkdir(parents=True, exist_ok=True)
        source = verified_source(cache, folder, name, remote, digest)
        license_file = verified_source(cache, folder, "OFL.txt", "OFL.txt", license_digest)
        shutil.copy2(license_file, destination / "OFL.txt")
        products = [("OFL.txt", "OFL.txt", license_digest, False)]
        if folder == "notoserifthai":
            for weight, style in ((400, "Regular"), (700, "Bold")):
                product = f"NotoSerifThai-{style}.ttf"
                prepare_thai(source, destination / product, weight, clusters)
                products.append((product, remote, digest, True))
        else:
            shutil.copy2(source, destination / name)
            products.append((name, remote, digest, False))
        for product, origin, source_digest, modified in products:
            path = destination / product
            records.append(dict(path=(relative / product).as_posix(), url=BASE + folder + "/" + origin,
                                bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                source_sha256=source_digest, license="OFL-1.1", revision=REVISION, modified=modified))
        detail = (
            f"Thai regular/bold outlines contain {len(clusters)} build-time shaped clusters.\n"
            "CPLG substitutions require the accompanying CrossNext firmware. Original Unicode text\n"
            "is retained for searching and dictionary lookup. Common dependent vowels, tones and\n"
            "Sara Am are positioned together at every point size. No runtime shaping library is loaded.\n"
            "Line wrapping protects these clusters; it does not add dictionary-based Thai word segmentation.\n"
            if folder == "notoserifthai" else
            "The full upstream variable font is unmodified, with regional Chinese glyph forms.\n"
            "Regular uses weight 400 and bold uses weight 700.\n")
        (destination / "README.txt").write_text(
            family + " - reading font\n\nCopy this complete folder to /fonts/ on the microSD, then restart the reader.\n"
            "Select it in Font > Font family. Requires a PSRAM device with TTF support, including X4 Pro.\n"
            "Works in both EPUB engines; text sizes are rasterized on demand.\n"
            "Italic emphasis uses synthesized oblique forms (no separate upstream italic face).\n"
            "Large files stream from microSD; smaller files may be held in PSRAM when space permits.\n"
            "No font bytes are stored in firmware flash. Reading and interface fonts are independent. Horizontal text only.\n\n" + detail +
            "\nKeep OFL.txt with these files when redistributing them.\n", encoding="utf-8", newline="\n")
    catalog = output / "font-sources.json"
    data = json.loads(catalog.read_text(encoding="utf-8")) if catalog.exists() else {"files": []}
    data["files"] = [item for item in data["files"] if not any(item["path"].startswith(p) for p in prefixes)] + records
    catalog.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    if archive:
        archive = Path(archive)
        archive.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
            for prefix in prefixes:
                for path in sorted((output / prefix).iterdir()):
                    info = zipfile.ZipInfo(path.relative_to(output).as_posix(), date_time=(2026, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    bundle.writestr(info, path.read_bytes())
            info = zipfile.ZipInfo("Noto-Multilingual-SOURCES.json", date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            bundle.writestr(info, json.dumps({"files": records}, indent=2) + "\n")
    return records


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "assets_local/sd-resources")
    parser.add_argument("--cache", type=Path, default=ROOT / "datos.local/noto-multilingual-2026-11-07/sources")
    parser.add_argument("--zip", type=Path)
    args = parser.parse_args()
    for record in build(args.output, args.cache, args.zip):
        print(f"{record['path']}: {record['bytes']} bytes")
