"""Export the isolated narrative-campaign mockup assets and pacing proposal."""
from pathlib import Path
import hashlib
import json
import shutil

from PIL import Image
from build_whole_art import whole_cells
from build_gray_art import backgrounds, background_icons, pets

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "REDISEÑOS/AVENTURA/campaign-v1"
TARGET = ROOT / "site/qa-reading-campaign"
BOOK_PLACES = (0, 12, 16, 24, 5, 6, 23, 31, 48, 25, 41, 49)

BOOKS = [
    ("El bosque de las voces", "Bosque", "Las voces de la aldea han quedado atrapadas en las raíces. Liora guarda la primera pista sobre el Rey del Olvido."),
    ("La ciudad sin memoria", "Ciudad", "Sus habitantes recuerdan las calles, pero han olvidado a quienes aman. Cada nombre recuperado abre una puerta."),
    ("El puerto de la niebla", "Costa", "Los barcos regresan sin tripulación. Una carta mojada señala una biblioteca bajo el mar."),
    ("La biblioteca sumergida", "Océano", "Las últimas crónicas duermen en un palacio hundido. Su guardián reconoce el sello de tu compañero."),
    ("Las montañas del eco", "Montaña", "Una voz imita todos tus pasos. En la cumbre se conserva un recuerdo que el rey no pudo borrar."),
    ("El desierto de los nombres", "Desierto", "Los viajeros pierden su nombre al cruzar las dunas. La caravana de Nadir busca un mapa escrito en piedra."),
    ("El reloj de ceniza", "Ciudadela", "Un día se repite en la ciudadela. Romper el ciclo exige descubrir quién entregó la primera página."),
    ("El reino bajo la luna", "Reino lunar", "La reina custodia un pacto antiguo. Su versión de la guerra contradice todo lo que has aprendido."),
    ("El jardín de los dioses", "Jardín divino", "Los dioses también han perdido historias. El jardín conserva una semilla del mundo anterior."),
    ("La guerra de los relatos", "Frontera", "Las regiones liberadas se unen. Tus aliados defienden sus bibliotecas mientras buscas el último sello."),
    ("El umbral de las estrellas", "Umbral", "Más allá del firmamento, un archivo conserva el origen del Olvido y el verdadero precio de cerrarlo."),
    ("La última página", "Trono", "El Rey demonio del Olvido espera. Las personas y recuerdos que has salvado decidirán qué mundo puede renacer."),
]


def build():
    assets = TARGET / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    for name in ("index.html", "mockup.css", "mockup.js"):
        shutil.copy2(SOURCE / "prototype" / name, TARGET / name)
    for name in ("root-guardian", "shadow-wolf"):
        cells = whole_cells(SOURCE / "masters" / (name + ".png"), 3, 1)
        factor = min(236 / max(c.width for c in cells), 268 / max(c.height for c in cells))
        for state, cell in zip(("ready", "hurt", "defeated"), cells):
            cell = cell.resize((round(cell.width * factor), round(cell.height * factor)), Image.Resampling.LANCZOS)
            canvas = Image.new("RGBA", (248, 280), (255, 255, 255, 0))
            canvas.alpha_composite(cell, ((248 - cell.width) // 2, 274 - cell.height))
            canvas.save(assets / f"{name}-{state}.png")
    image = Image.open(SOURCE / "masters/demon-king.png").convert("RGBA")
    image = image.crop(image.getchannel("A").getbbox())
    image.thumbnail((280, 390), Image.Resampling.LANCZOS)
    image.save(assets / "demon-king.png")
    approved = ROOT / "assets_local/quest/redesign/masters"
    for name, picture in zip(("apprentice-m", "mage-m", "celestial-m", "apprentice-f", "mage-f", "celestial-f"),
                             whole_cells(approved / "heroes-approved.png", 3, 2)):
        picture.thumbnail((260, 430), Image.Resampling.LANCZOS)
        picture.save(assets / (name + ".png"))
    for name, picture in zip(("dog", "cat", "hamster", "fox", "dragon", "ancient-dragon"),
                             whole_cells(approved / "companions-approved.png", 3, 2)):
        picture.thumbnail((360, 360), Image.Resampling.LANCZOS)
        picture.save(assets / (name + ".png"))
    for name in ("Ubuntu-Regular.ttf", "Ubuntu-Bold.ttf", "UFL.txt"):
        shutil.copy2(ROOT / "assets_local/sd-resources/fonts/Ubuntu" / name, assets / name)
    pets()[7].save(assets / "owl.png")
    scenes, emblems = backgrounds(), background_icons()
    for n in (0, 7, 10, 39, 42, 49):
        scenes[n].save(assets / f"background-{n}.png")
    for n in sorted(set(BOOK_PLACES) | {0, 7, 10, 39, 42, 49}):
        emblems[n].save(assets / f"place-{n}.png")

    # Fixed first chapter and sample encounter; scale the rest to the declared total.
    fixed = {n: value for n, value in enumerate((5, 10, 15, 20, 25, 40))}
    fixed.update({12 + n: value for n, value in enumerate((30, 45, 60, 75, 90, 150))})
    raw = [60 + b * 25 + c * 8 if e == 5 else 20 + b * 11 + c * 3 + e * 3
           for b in range(12) for c in range(10) for e in range(6)]
    scale = (72000 - sum(fixed.values())) / sum(v for n, v in enumerate(raw) if n not in fixed)
    minutes = [fixed[n] if n in fixed else max(10, round(v * scale / 5) * 5) for n, v in enumerate(raw)]
    difference = 72000 - sum(minutes)
    n = len(minutes) - 1
    while difference:
        if n not in fixed:
            step = 5 if difference > 0 else -5
            minutes[n] += step
            difference -= step
        n -= 1
    assert len(minutes) == 720 and sum(minutes) == 72000 and min(minutes) >= 5
    books = [dict(id=n, title=title, region=region, synopsis=synopsis, chapters=10, coverId=BOOK_PLACES[n],
                  baseMinutes=sum(minutes[n * 60:(n + 1) * 60]))
             for n, (title, region, synopsis) in enumerate(BOOKS)]
    design = dict(version=1, status="Interactive mockup; proposed pacing, not firmware or a fully written saga",
                  books=books, chapters=120, encounters=720, baseHours=1200, minimumHours=900,
                  maxTimeReduction=25, encounterMinutes=minutes,
                  estimates=[dict(dailyMinutes=m, yearsAtCap=round(900 * 60 / m / 365, 1),
                                  yearsWithoutBonuses=round(1200 * 60 / m / 365, 1)) for m in (15, 30, 60)])
    TARGET.joinpath("campaign.json").write_text(json.dumps(design, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    SOURCE.joinpath("pacing-proposal.json").write_text(json.dumps(design, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for path in (SOURCE / "masters").glob("*.json"):
        record = json.loads(path.read_text("utf-8"))
        record["sha256"] = hashlib.sha256(path.with_suffix(".png").read_bytes()).hexdigest()
        path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Campaign mockup: 12 books, 120 chapters, 720 encounters, 1200 base hours; 25% cap => 900 hours")


if __name__ == "__main__":
    build()
