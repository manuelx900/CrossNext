#!/usr/bin/env python3
"""Bundle the full Japanese reading face as a streamed microSD font."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REVISION = "66a36c8c94b1a5d992ee4e7f392fccfe4945767c"
BASE_URL = f"https://raw.githubusercontent.com/google/fonts/{REVISION}/ofl/notoserifjp/"
FAMILY_PATH = Path("fonts/Noto Serif JP")
FILES = (
    ("NotoSerifJP%5Bwght%5D.ttf", "NotoSerifJP[wght].ttf",
     "2fd527ba12b6a44ec30d796d633360da0aeba6c5d4af1304ce12bb4dc15a7dfc"),
    ("OFL.txt", "OFL.txt", "5e0da210fb04058a8c0087985d2d456b931c2579811a49655721d3cf0c36b6d6"),
)


def build(output, cache, archive=None):
    output, cache = Path(output), Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    destination = output / FAMILY_PATH
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    for remote, name, digest in FILES:
        source = cache / name
        if not source.exists():
            temporary = source.with_suffix(source.suffix + ".download")
            urllib.request.urlretrieve(BASE_URL + remote, temporary)
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != digest:
                raise ValueError(f"Source checksum changed: {name}")
            temporary.replace(source)
        if hashlib.sha256(source.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Source checksum changed: {name}")
        shutil.copy2(source, destination / name)
        records.append({
            "path": (FAMILY_PATH / name).as_posix(), "url": BASE_URL + remote,
            "bytes": source.stat().st_size, "sha256": digest,
            "license": "OFL-1.1", "revision": REVISION, "modified": False,
        })
    (destination / "README.txt").write_text(
        "Noto Serif JP - Japanese reading font\n\n"
        "Copy this complete folder to /fonts/ on the microSD and restart the reader.\n"
        "Select Noto Serif JP in the book's Font > Font family menu.\n"
        "Requires a PSRAM device with TTF support, including the Xteink X4 Pro.\n"
        "This is a reading font; it does not replace interface/language fonts.\n\n"
        "The unmodified upstream variable face retains its full character repertoire.\n"
        "The reader selects weight 400 for regular and 700 for bold, and rasterizes\n"
        "EPUB text sizes on demand. Italic emphasis uses synthesized oblique forms;\n"
        "the upstream family has no separate italic face.\n"
        "The large TTF is streamed from the microSD using the existing bounded caches.\n"
        "No additional font is embedded in firmware flash.\n"
        "Japanese is rendered in the reader's horizontal text flow; this package\n"
        "does not add vertical writing or ruby support.\n\n"
        "Keep OFL.txt with the font when redistributing it.\n",
        encoding="utf-8", newline="\n")
    catalog = output / "font-sources.json"
    data = json.loads(catalog.read_text(encoding="utf-8")) if catalog.exists() else {"files": []}
    data["files"] = [item for item in data["files"] if not item["path"].startswith(FAMILY_PATH.as_posix() + "/")]
    data["files"].extend(records)
    catalog.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    if archive:
        archive = Path(archive)
        archive.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
            for path in sorted(destination.iterdir()):
                info = zipfile.ZipInfo(path.relative_to(output).as_posix(), date_time=(2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                bundle.writestr(info, path.read_bytes())
            info = zipfile.ZipInfo("Noto-Serif-JP-SOURCES.json", date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            bundle.writestr(info, json.dumps({"files": records}, indent=2) + "\n")
    return records


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "assets_local/sd-resources")
    parser.add_argument("--cache", type=Path, default=ROOT / "datos.local/noto-serif-jp/sources")
    parser.add_argument("--zip", type=Path)
    arguments = parser.parse_args()
    for record in build(arguments.output, arguments.cache, arguments.zip):
        print(f"{record['path']}: {record['bytes']} bytes, SHA256 {record['sha256']}")
