"""Generate disposable promotional saves and the simulator-only control catalog."""
import base64
import brotli
import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def prepare(card):
    card = Path(card).resolve()
    if card != (ROOT / ".pio/build/wasm/resource-card").resolve() or not (card / "CROSSNEXT-SIMULATOR-CARD.json").is_file():
        raise ValueError("Promotional saves are restricted to the generated simulator card")
    build = ROOT / ".pio/build/promotion-fixtures"
    paths = {"pet": "/.crosspoint/pet-v2/state-a.bin", "petHistory": "/.crosspoint/pet-v2/lives-a.bin",
             "quest": "/.crosspoint/reading-adventure-v2/reading-quest-v10.dat"}
    stores = {"pet": "pet/PetStore.cpp", "petHistory": "pet/PetHistory.cpp", "quest": "quest/QuestStore.cpp"}
    for key, source in stores.items():
        if '"' + paths[key] + '"' not in (ROOT / "src/apps_local" / source).read_text("utf-8"):
            raise ValueError("Update the simulator fixture path for " + source)
    build.mkdir(parents=True, exist_ok=True)
    windows = os.name == "nt"
    compiler = str((ROOT / ".venv/emsdk/upstream/emscripten/em++.exe").resolve()) if windows else os.environ.get("CXX", "c++")
    node = shutil.which("node") or str((ROOT / ".venv/emsdk/node/24.19.0_64bit/node.exe").resolve())
    flags = ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1", "-sSTACK_SIZE=1048576"] if windows else []
    binary = build / ("seed.cjs" if windows else "seed")
    sources = ["tools_local/wasm/seed_promotion.cpp", "src/apps_local/pet/PetState.cpp", "src/apps_local/pet/PetHistory.cpp",
               "src/apps_local/quest/QuestCore.cpp", "src/apps_local/quest/QuestCampaign.cpp",
               "src/apps_local/reading/ReadingStatsCore.cpp", "src/apps_local/focus/FocusCore.cpp",
               "src/apps_local/pomodoro/PomodoroCore.cpp", "src/apps_local/tools/ToolState.cpp"]
    subprocess.run([compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Ihost-tests/study-suite/stubs", *sources, *flags, "-o", str(binary)], cwd=ROOT, check=True)
    epoch = int(dt.datetime.combine(dt.date.today(), dt.time(16, 30), dt.timezone.utc).timestamp())
    subprocess.run([*([node] if windows else []), str(binary), card.as_posix(), build.as_posix(), str(epoch)], cwd=ROOT, check=True)
    from build_showcase_games import SOURCES
    game_binary = build / ("games.cjs" if windows else "games")
    subprocess.run([compiler, "-std=c++17", "-O2", "-Ilib/I18n", "-Ilib/EpdFont",
                    "tools_local/wasm/seed_showcase_games.cpp", *["src/apps_local/" + source for source in SOURCES],
                    *flags, "-o", str(game_binary)], cwd=ROOT, check=True)
    subprocess.run([*([node] if windows else []), str(game_binary), (card / ".crosspoint").as_posix()], cwd=ROOT, check=True)
    spec = importlib.util.spec_from_file_location("promotion_languages", ROOT / "tools_local/quest/simulator_languages.py")
    languages = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(languages)
    destination = ROOT / "site/showcase"
    destination.mkdir(parents=True, exist_ok=True)
    locales = languages.export_languages(destination)
    wordle = destination / "wordle"
    wordle.mkdir(exist_ok=True)
    for source in (ROOT / "assets_local/wordle-languages").iterdir():
        if source.is_file():
            if source.suffix == ".cpwb":
                (wordle / source.name).write_bytes(brotli.compress(source.read_bytes(), quality=6))
            else:
                shutil.copy2(source, wordle / source.name)
    for locale in locales:
        bank = locale["setting"] + ".cpwb"
        locale["wordle"] = bank if (wordle / bank).is_file() else None
    source = (ROOT / "src/apps_local/Shelf.cpp").read_text("utf-8")
    groups = {}
    for key, table in (("games", "kGames"), ("apps", "kApps")):
        body = source.split("constexpr shelf::Item " + table + "[] = {", 1)[1].split("};", 1)[0]
        groups[key] = re.findall(r'\{"([^"\n]+)"', body)
    encoded = lambda path: base64.b64encode(path.read_bytes()).decode("ascii")
    catalog = {"format": 1, "firmwareVersion": "2.0.0", "epoch": epoch, "languages": locales, **groups,
               "paths": paths, "petHistory": encoded(card / paths["petHistory"].lstrip("/")),
               "pet": {path.stem: encoded(path) for path in sorted(build.glob("pet-*.bin"))},
               "quest": {path.stem: encoded(path) for path in sorted(build.glob("quest-*.bin"))},
               "settings": json.loads((card / ".crosspoint/settings.json").read_text("utf-8")),
               "inventory": json.loads((card / "SHOWCASE.json").read_text("utf-8"))}
    assert len(catalog["pet"]) == 15 and len(catalog["quest"]) == 6
    (destination / "catalog.json").write_text(json.dumps(catalog, ensure_ascii=False, separators=(",", ":")), "utf-8")
    result = {"petProfiles": 15, "questProfiles": 6, "languages": len(locales), "apps": len(groups["apps"]),
              "games": len(groups["games"]), "epoch": epoch}
    (card / "PROMOTION.json").write_text(json.dumps(result, indent=2), "utf-8")
    return result
