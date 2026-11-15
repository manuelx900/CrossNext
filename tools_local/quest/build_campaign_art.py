"""Extract three enemy states at a shared physical scale, with a common baseline."""
from pathlib import Path
from PIL import Image
from build_whole_art import whole_cells

ROOT = Path(__file__).resolve().parents[2]
FAMILIES = ("shadow-wolf", "root-guardian", "bell-sentinel", "fog-corsair", "abyss-beast", "echo-gargoyle",
            "sand-scorpion", "clock-automaton", "moon-wraith", "fallen-colossus", "war-revenant", "void-serpent", "demon-king-states")


def enemies():
    pictures = []
    for family in FAMILIES:
        cells = whole_cells(ROOT / "assets_local/quest/campaign/masters" / (family + ".png"), 3, 1)
        factor = min(236 / max(c.width for c in cells), 268 / max(c.height for c in cells))
        for cell in cells:
            cell = cell.resize((round(cell.width * factor), round(cell.height * factor)), Image.Resampling.LANCZOS)
            canvas = Image.new("RGBA", (248, 280), (255, 255, 255, 0))
            canvas.alpha_composite(cell, ((248 - cell.width) // 2, 274 - cell.height))
            pictures.append(canvas)
    return pictures
