"""Prepare a disposable simulator card with the current SD resource collection."""
import importlib.util
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "tools_local/wasm/sdcard"
ADDITIONS = ROOT / "assets_local/sd-resources"
CARD = ROOT / ".pio/build/wasm/resource-card"
MARKER = "CROSSNEXT-SIMULATOR-CARD.json"


def prepare():
    card = CARD.resolve()
    if CARD.is_symlink() or not card.is_relative_to(ROOT.resolve()) or card.parent != (ROOT / ".pio/build/wasm").resolve():
        raise ValueError("Simulator card must stay inside its build directory")
    if card.exists():
        marker = card / MARKER
        if not marker.is_file() or json.loads(marker.read_text("utf-8")).get("format") != 1:
            raise ValueError("Refusing to replace a simulator card without its generation marker")
        shutil.rmtree(card)
    card.mkdir(parents=True)
    (card / MARKER).write_text('{"format": 1}\n', encoding="utf-8")
    def ignore(directory, names):
        return [name for name in names if name == "__pycache__" or
                (Path(directory).resolve() == BASE.resolve() and name in ("fonts", "dictionaries", "study"))]
    shutil.copytree(BASE, card, dirs_exist_ok=True, ignore=ignore)
    for source in (ROOT / "assets_local/reader-fonts/sd-card/fonts", ADDITIONS / "fonts"):
        shutil.copytree(source, card / "fonts", dirs_exist_ok=True)
    for source in (BASE / "dictionaries", ADDITIONS / "dictionaries"):
        for directory in source.iterdir():
            if not directory.is_dir():
                continue
            name = "Spanish Wiktionary" if directory.name == "Espanol Wikcionario" else directory.name
            shutil.copytree(directory, card / "dictionaries" / name, dirs_exist_ok=True)
    shutil.copytree(ADDITIONS / "licenses", card / "licenses", dirs_exist_ok=True)
    for name in ("THIRD-PARTY-NOTICES.txt", "CUSTOMIZATION.txt"):
        shutil.copy2(ADDITIONS / name, card / name)
    for directory in (card / "dictionaries").iterdir():
        attribution = ADDITIONS / "attribution" / (directory.name + ".txt")
        if attribution.is_file():
            shutil.copy2(attribution, directory / "ATTRIBUTION.txt")
            shutil.copy2(ADDITIONS / "licenses/CC-BY-SA-4.0.txt", directory / "LICENSE-CC-BY-SA-4.0.txt")
            if list(directory.glob("*.css")):
                shutil.copy2(ADDITIONS / "licenses/GPL-3.0.txt", directory / "LICENSE-GPL-3.0.txt")
    settings_path = card / ".crosspoint/settings.json"
    settings = json.loads(settings_path.read_text("utf-8")) if settings_path.exists() else {}
    settings.update(dictionaryName="", translationDictionaryName="", dictionaryAuto=True, translationDictionaryAuto=True)
    settings_path.parent.mkdir(exist_ok=True)
    settings_path.write_text(json.dumps(settings, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    spec = importlib.util.spec_from_file_location("app_resources", ROOT / "tools_local/build_app_resources.py")
    apps = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(apps)
    apps.build(card / "apps")
    inventory = {"format": 1, "apps": list(apps.APPS), "fonts": sorted(p.name for p in (card / "fonts").iterdir() if p.is_dir()),
                 "dictionaries": sorted(p.name for p in (card / "dictionaries").iterdir() if p.is_dir()),
                 "dictionaryDefaults": "automatic by interface language"}
    (card / MARKER).write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
    from seed_showcase import seed
    inventory["showcase"] = seed(card)
    inventory["dictionaries"] = sorted(p.name for p in (card / "dictionaries").iterdir() if p.is_dir())
    (card / MARKER).write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
    print("CrossNext showcase:", inventory["showcase"], flush=True)
    return card


if __name__ == "__main__":
    print(prepare())
