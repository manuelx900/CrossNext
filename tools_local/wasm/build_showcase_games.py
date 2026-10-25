"""Rebuild host-generated example saves; normal simulator builds only copy them."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ["chess/puzzles/ChessPuzzleCore.cpp", "chess/puzzles/ChessPuzzleData.cpp",
           "chess/ChessCore.cpp", "chess/ChessEngine.cpp", "picross/PicrossCore.cpp",
           "carnivoro/CarnivoroCore.cpp", "wordle/LocalWordle.cpp", "wordle/WordleCore.cpp",
           "game2048/Game2048Core.cpp", "game2048/Game2048Codec.cpp", "pomodoro/PomodoroCore.cpp",
           "intercept/InterceptCore.cpp", "intercept/InterceptData.cpp", "reading/ReadingStatsCore.cpp"]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", nargs="+", default=["c++"])
    args = parser.parse_args()
    build = ROOT / ".pio/build/showcase-fixtures"
    card = build / "card"
    data = card / ".crosspoint"
    data.mkdir(parents=True, exist_ok=True)
    (card / "CROSSNEXT-SIMULATOR-CARD.json").write_text('{"format":1}\n', encoding="utf-8")
    if not (ROOT / "src/apps_local/wordle/WordleWords.generated.h").exists():
        subprocess.run([sys.executable, "tools_local/wordle/build_words.py"], cwd=ROOT, check=True)
    if not (ROOT / "lib/I18n/I18nKeys.h").exists():
        subprocess.run([sys.executable, "scripts/gen_i18n.py", "lib/I18n/translations", "lib/I18n",
                        "--builtin-langs", "en,es"], cwd=ROOT, check=True)
    executable = build / "seed-games.exe"
    command = args.compiler + ["-std=c++17", "-Wno-nullability-completeness", "-Ilib/I18n", "-Ilib/EpdFont",
                              "tools_local/wasm/seed_showcase_games.cpp"]
    command += ["src/apps_local/" + source for source in SOURCES]
    subprocess.run(command + ["-o", str(executable)], cwd=ROOT, check=True)
    subprocess.run([str(executable), str(data)], cwd=ROOT, check=True)
    output = ROOT / "assets_local/simulator/showcase-progress"
    output.mkdir(parents=True, exist_ok=True)
    inventory = []
    for path in sorted(data.iterdir()):
        shutil.copy2(path, output / path.name)
        inventory.append(dict(file=path.name, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    (output / "manifest.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
