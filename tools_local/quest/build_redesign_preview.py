"""Export unmodified atlas crops for the Reading Adventure design review."""
from pathlib import Path
import hashlib
import json
import shutil
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
from build_whole_art import whole_cells

MASTERS = ROOT / "REDISEÑOS/AVENTURA/assets/v1"
OUTPUT = ROOT / "site/qa-reading-adventure/assets"


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    figures = whole_cells(MASTERS / "heroes.png", 3, 2)
    pets = whole_cells(MASTERS / "companions.png", 3, 2)
    names = ("apprentice-m", "mage-m", "celestial-m", "apprentice-f", "mage-f", "celestial-f")
    for name, picture in zip(names, figures):
        picture.thumbnail((260, 430), Image.Resampling.LANCZOS)
        picture.save(OUTPUT / (name + ".png"))
    for name, picture in zip(("dog", "cat", "hamster", "fox", "dragon", "ancient-dragon"), pets):
        picture.thumbnail((360, 360), Image.Resampling.LANCZOS)
        picture.save(OUTPUT / (name + ".png"))
    with Image.open(MASTERS / "auras-approved.png") as atlas:
        for i, name in enumerate(("dew", "reading", "celestial")):
            # White remains opaque. The preview multiplies this background layer.
            picture = atlas.crop((round(i * atlas.width / 3), 0, round((i + 1) * atlas.width / 3), atlas.height))
            picture.thumbnail((560, 560), Image.Resampling.LANCZOS)
            picture.save(OUTPUT / ("aura-" + name + ".png"))
    for weight in ("Regular", "Bold"):
        shutil.copy2(ROOT / f"assets_local/sd-resources/fonts/Ubuntu/Ubuntu-{weight}.ttf", OUTPUT)
    if (MASTERS / "background-forest.png").exists():
        with Image.open(MASTERS / "background-forest.png") as source:
            source.thumbnail((960, 640), Image.Resampling.LANCZOS)
            source.save(OUTPUT / "background-forest.png")
    manifest = json.loads((MASTERS / "provenance.json").read_text(encoding="utf-8"))
    for entry in manifest["assets"]:
        entry["sha256"] = hashlib.sha256((MASTERS / entry["file"]).read_bytes()).hexdigest()
    (MASTERS / "provenance.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Exported 6 heroes, 6 companions and 3 aura examples; production catalogue unchanged.")


if __name__ == "__main__":
    main()
