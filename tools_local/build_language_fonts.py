#!/usr/bin/env python3
"""Build microSD-only interface glyphs for the installed translation catalogues."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import urllib.request

from fontTools import merge, subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from shape_language_thai import add_clusters, cluster_codepoints

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT / "lib/EpdFont/builtinFonts/source"
DEFAULT_OUTPUT = ROOT / "assets_local/language-fonts"
JP_REVISION = "66a36c8c94b1a5d992ee4e7f392fccfe4945767c"
JP_URL = f"https://raw.githubusercontent.com/google/fonts/{JP_REVISION}/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf"
JP_SHA = "c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f"
JP_LICENSE = f"https://raw.githubusercontent.com/google/fonts/{JP_REVISION}/ofl/notosansjp/OFL.txt"
JP_LICENSE_SHA = "1c05c68c34f9708415aada51f17e1b0092d2cea709bf4a94cd38114f9e73d7d9"
EXTRA_SOURCES = {
    "NotoSansThai": ("notosansthai/NotoSansThai%5Bwdth%2Cwght%5D.ttf",
                    "5a1c559bb539583c8a1fd99d1c5b9491e5e14478c9cd2bd0970d5c3096cc9ef8",
                    "2e98fd23a52d253db8612cd5942c8f2ff4111b21d2367050fdca91d8ccc374a0"),
    "NotoSansSC": ("notosanssc/NotoSansSC%5Bwght%5D.ttf",
                  "a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da", JP_LICENSE_SHA),
    "NotoSansTC": ("notosanstc/NotoSansTC%5Bwght%5D.ttf",
                  "864727d210d54f2537bbe23b3a839436c3992af72de9322af5270897246bd44f", JP_LICENSE_SHA),
}


def download(url, digest, target):
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(url, target)
    if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        raise ValueError(f"Source checksum changed: {target.name}")


def build(output=DEFAULT_OUTPUT, cache=None):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    cache = Path(cache or ROOT / "datos.local/language-font-sources").resolve()
    download(JP_URL, JP_SHA, cache / "NotoSansJP.ttf")
    download(JP_LICENSE, JP_LICENSE_SHA, cache / "OFL.txt")
    for family, (file, digest, license_digest) in EXTRA_SOURCES.items():
        base = f"https://raw.githubusercontent.com/google/fonts/{JP_REVISION}/ofl/"
        download(base + file, digest, cache / f"{family}.ttf")
        download(base + file.split('/')[0] + "/OFL.txt", license_digest, cache / f"{family}-OFL.txt")
    spec = importlib.util.spec_from_file_location("i18n", ROOT / "scripts/gen_i18n.py")
    i18n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(i18n)
    characters = set(range(32, 0x250))
    repertoire = ROOT / "assets_local/language-fonts/BASE_REPERTOIRE.txt"
    for line in repertoire.read_text("utf-8").splitlines():
        if not line.startswith("#"):
            characters.update(int(value, 16) for value in line.split())
    characters.update((0x2026, 0xFFFD, 0x02BC, 0x0E4F, 0x0E5A, 0x0E5B))
    # Contextual forms are emitted by MiniBidi after translating Arabic/Persian.
    for first, last in ((0x400, 0x52F), (0x590, 0x6FF), (0x750, 0x77F),
                        (0xFB1D, 0xFDF9), (0xFE70, 0xFEFF), (0x1E00, 0x1EFF)):
        characters.update(range(first, last + 1))
    used = set()
    texts = []
    for path in (ROOT / "lib/I18n/translations").glob("*.yaml"):
        for key, text in i18n.parse_yaml_file(str(path)).items():
            if key.startswith("STR_WEB_"):
                continue
            used.update(ord(c) for c in text if ord(c) >= 32)
            texts.append(text)
    keyboard_source = ROOT / "assets_local/language-keyboards/SOURCE.json"
    if keyboard_source.exists():
        for spec in json.loads(keyboard_source.read_text(encoding="utf-8")).values():
            for layer in ("rows", "shift"):
                for row in spec[layer]:
                    used.update(ord(c) for c in row if ord(c) >= 32)
            for text in spec.get("alts", {}).values():
                used.update(ord(c) for c in text if ord(c) >= 32)
    characters.update(used)
    pinyin_source = ROOT / "assets_local/language-keyboards/PINYIN.json"
    if pinyin_source.exists():
        for entries in json.loads(pinyin_source.read_text(encoding="utf-8")).values():
            for text in entries.values():
                used.update(map(ord, text))
        characters.update(used)
    # Kana names may compose a voiced mark after any basic syllable.
    characters.update(range(0x3040, 0x3100))
    wordle_glyphs = ROOT / 'assets_local/wordle-languages/GLYPHS.txt'
    if wordle_glyphs.exists():
        used.update(map(ord, wordle_glyphs.read_text(encoding='utf-8')))
        characters.update(used)
    thai_bank = ROOT / 'assets_local/wordle-languages/TH.cpwb'
    if thai_bank.exists():
        data = thai_bank.read_bytes()
        header, stride = (64, 108) if int.from_bytes(data[4:6], 'little') == 2 else (32, 104)
        texts.extend(data[at:at+97].split(b'\0',1)[0].decode('utf-8') for at in range(header,len(data),stride))
    thai_clusters = cluster_codepoints(texts, input_alphabet=keyboard_source.exists())
    characters.update(thai_clusters.values())
    characters.update(ord(char) for text in thai_clusters for char in text)
    records = []
    variants = {"": ("NotoSansJP", "NotoSansSC", "NotoSansTC"),
                "_SC": ("NotoSansSC", "NotoSansTC", "NotoSansJP"),
                "_TC": ("NotoSansTC", "NotoSansSC", "NotoSansJP")}
    for suffix, cjk in variants.items():
      with tempfile.TemporaryDirectory(prefix="crossnext-language-fonts-") as folder:
        scratch = Path(folder)
        faces = {}
        for weight, style in ((400, "Regular"), (700, "Bold")):
            remaining = set(characters)
            parts = []
            for family in ("NotoSans", "NotoSansArabic", "NotoSansHebrew", "NotoSansThai", *cjk):
                source = cache / f"{family}.ttf" if family in (*EXTRA_SOURCES, "NotoSansJP") else SOURCES / family / f"{family}-{style}.ttf"
                face = TTFont(source)
                wanted = remaining.intersection(face.getBestCmap())
                if not wanted:
                    face.close()
                    continue
                options = subset.Options()
                options.layout_features = []
                options.drop_tables += ["GSUB", "GPOS", "GDEF", "BASE", "kern", "DSIG", "vhea", "vmtx", "VORG"]
                sub = subset.Subsetter(options=options)
                sub.populate(unicodes=wanted)
                sub.subset(face)
                if "fvar" in face:
                    axes = {axis.axisTag: axis.defaultValue for axis in face['fvar'].axes}
                    axes['wght'] = weight
                    face = instantiateVariableFont(face, axes, inplace=True)
                part = scratch / f"{family}-{style}.ttf"
                face.save(part)
                face.close()
                parts.append(str(part))
                remaining.difference_update(wanted)
                records.append({"variant": suffix or "default", "style": style, "source": f"{family}/{source.name}",
                                "sha256": hashlib.sha256(source.read_bytes()).hexdigest()})
            combined = merge.Merger().merge(parts)
            add_clusters(combined, cache / 'NotoSansThai.ttf', weight, thai_clusters)
            combined.save(scratch / f"Interface-{style}.ttf")
            faces[style.lower()] = scratch / f"Interface-{style}.ttf"
            if remaining:
                # Control marks/emoji are not interface glyphs. Keep the exact audit list.
                (output / f"missing{suffix}-{style.lower()}.json").write_text(json.dumps(sorted(remaining)), encoding="utf-8")
                missing_used = remaining.intersection(used) - {0x200C, 0x200D, 0x200E, 0x200F, 0x2028, 0x202A, 0x202B, 0x202C, 0xFEFF}
                if missing_used:
                    raise ValueError(f"Missing catalogue glyphs in {style}: {sorted(missing_used)}")
        ranges = ",".join(f"(0x{cp:X}-0x{cp:X})" for cp in sorted(characters))
        spec = importlib.util.spec_from_file_location("converter", ROOT / "lib/EpdFont/scripts/fontconvert_sdcard.py")
        converter = importlib.util.module_from_spec(spec)
        sys.path.insert(0, str(ROOT / "lib/EpdFont/scripts"))
        spec.loader.exec_module(converter)
        intervals = converter.resolve_intervals(ranges)
        for size in (8, 10, 12):
            converter.generate_cpfont_multistyle({0: str(faces["regular"]), 1: str(faces["bold"])},
                                                size, intervals, str(output / f"Interface{suffix}_{size}.cpfont"))
    licenses = output / "licenses"
    licenses.mkdir(exist_ok=True)
    for family in ("NotoSans", "NotoSansArabic", "NotoSansHebrew"):
        shutil.copy2(SOURCES / family / "OFL.txt", licenses / f"{family}.txt")
    shutil.copy2(cache / "OFL.txt", licenses / "NotoSansJP.txt")
    for family in EXTRA_SOURCES:
        shutil.copy2(cache / f"{family}-OFL.txt", licenses / f"{family}.txt")
    report = {"license": "OFL-1.1", "modified": True,
              "modifications": "UI subsets merged; Thai catalogue clusters pre-shaped as chained ligatures; rasterized at 150 DPI into CPFONT v5 (extended ligature count).",
              "thaiClusters": len(thai_clusters), "shaper": f"HarfBuzz {hb_version()}",
              "japaneseSource": {"url": JP_URL, "sha256": JP_SHA}, "sources": records,
              "files": [{"file": p.name, "bytes": p.stat().st_size,
                         "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(output.glob("*.cpfont"))]}
    (output / "MANIFEST.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def hb_version():
    import uharfbuzz
    return uharfbuzz.version_string()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--source-cache", type=Path)
    args = parser.parse_args()
    build(args.output_dir, args.source_cache)
