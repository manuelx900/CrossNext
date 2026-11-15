#!/usr/bin/env python3
"""Build the bundled Noto Serif reader family using the shared CPFONT converter."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "lib/EpdFont/builtinFonts/source/NotoSerif"
DEFAULT_OUTPUT = ROOT / "assets_local/reader-fonts/sd-card/fonts/Noto Serif"


def build(output=DEFAULT_OUTPUT):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    command = [sys.executable, str(ROOT / "lib/EpdFont/scripts/fontconvert_sdcard.py"),
               "--name", "Noto Serif", "--sizes", "12,14,16,18",
               "--intervals", "builtin,latin-ext,greek,(0x0250-0x02FF),(0x1F00-0x1FFF)",
               "--pnum",
               "--output-dir", str(output)]
    sources = []
    for style, filename in (("regular", "Regular"), ("bold", "Bold"),
                            ("italic", "Italic"), ("bolditalic", "BoldItalic")):
        path = SOURCE / f"NotoSerif-{filename}.ttf"
        command += [f"--{style}", str(path)]
        sources.append({"path": path.relative_to(ROOT).as_posix(),
                        "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    subprocess.run(command, check=True, cwd=ROOT)
    shutil.copy2(SOURCE / "OFL.txt", output / "OFL.txt")
    return [{"path": "fonts/Noto Serif/" + path.name,
             "bytes": path.stat().st_size,
             "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
             "license": "OFL-1.1", "modified": True,
             "modifications": "Rasterized at 150 DPI, four styles, CPFONT v4; Original reader coverage: Latin, combining marks, Cyrillic, IPA, Greek and symbols; proportional numerals.",
             "sourceFiles": sources} for path in sorted(output.glob("*.cpfont"))]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    records = build(args.output_dir)
    catalog = ROOT / "assets_local/sd-resources/font-sources.json"
    if args.output_dir.resolve() == DEFAULT_OUTPUT.resolve():
        data = json.loads(catalog.read_text(encoding="utf-8"))
        data["files"] = [item for item in data["files"] if not item["path"].startswith("fonts/Noto Serif/")]
        data["files"].extend(records)
        catalog.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(records, indent=2))
