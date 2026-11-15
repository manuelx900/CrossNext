#!/usr/bin/env python3
"""Run Pomodoro timer, history, persistence, and screen checks."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=("core","history","screens"))
    parser.add_argument("--output", type=Path, default=ROOT / ".pio/host-tests/pomodoro")
    parser.add_argument("--render", type=Path, help="Write seeded screen previews into this directory")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    wasm = os.name == "nt"
    compiler = str(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX", "c++")
    node = shutil.which("node")
    if wasm and not node:
        node = next((ROOT / ".venv/emsdk/node").glob("*/bin/node.exe"), None)
        node = node or next((ROOT / ".venv/emsdk/node").glob("*/node.exe"), None)
    runtime = [str(node)] if wasm else []
    common = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-comment", "-O2"]
    flags = ["-sSTACK_SIZE=1048576", "-sENVIRONMENT=node", "-sEXIT_RUNTIME=1", "-sALLOW_MEMORY_GROWTH=1", "-sNODERAWFS=1"] if wasm else []
    include = ["-Ihost-tests/bookprep/stubs", "-Ihost-tests/reading-suite/stubs", "-Ilib/Memory"]
    core = ["src/apps_local/pomodoro/PomodoroCore.cpp", "src/apps_local/pomodoro/PomodoroStore.cpp"]
    history = ["src/apps_local/pomodoro/PomodoroHistory.cpp", "src/apps_local/pomodoro/PomodoroHistoryStore.cpp",
               "src/apps_local/reading/ReadingStatsCore.cpp"]
    i18n = output / "i18n"
    i18n.mkdir(parents=True, exist_ok=True)
    for filename in ("I18n.cpp", "I18n.h", "LanguagePack.cpp", "LanguagePack.h"):
        shutil.copy2(ROOT / "lib/I18n" / filename, i18n / filename)
    with (output / "i18n-generation.log").open("w", encoding="utf-8") as log:
        subprocess.run([str(ROOT / ".venv/Scripts/python.exe") if wasm else os.environ.get("PYTHON", "python3"),
                        "scripts/gen_i18n.py", "lib/I18n/translations", str(i18n), "--builtin-langs", "en,es", "--src-dirs", "src/apps_local/pomodoro", "src/apps_local/ui"],
                       cwd=ROOT, check=True, stdout=log, env={**os.environ, "PYTHONUTF8": "1", "PYTHONIOENCODING": "utf-8"})
    suites = {
        "core": ["host-tests/pomodoro/test_core.cpp", *core],
        "history": ["host-tests/pomodoro/test_history.cpp", *core, *history],
        "screens": ["host-tests/pomodoro/test_screens.cpp", "src/apps_local/pomodoro/PomodoroCore.cpp",
                    "src/apps_local/pomodoro/PomodoroHistory.cpp", "src/apps_local/reading/ReadingStatsCore.cpp",
                    "src/apps_local/pomodoro/PomodoroScreens.cpp", "src/apps_local/pomodoro/PomodoroStatsScreens.cpp",
                    "src/apps_local/ui/PixelMenu.cpp", "src/apps_local/pomodoro/TimerSettingsUi.cpp",
                    str(i18n / "I18n.cpp"), str(i18n / "I18nStrings.cpp"), str(i18n / "LanguagePack.cpp"),
                    "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"],
    }
    for name, sources in suites.items():
        if args.suite and args.suite != name:
            continue
        executable = output / (name + (".cjs" if wasm else ""))
        ui = ["-I" + str(i18n), "-Ilib/EpdFont", "-Ifreeink-sdk/libs/ui/FreeInkUI/include",
              "-Ifreeink-sdk/libs/assets/Icons/include"] if name == "screens" else []
        pack_stubs = ["-Itest/language_pack/stubs"] if name == "screens" else []
        subprocess.run([compiler, *common, *pack_stubs, *include, *ui, *flags, *sources, "-o", str(executable)], cwd=ROOT, check=True)
        render = []
        if name == "screens" and args.render:
            args.render.mkdir(parents=True, exist_ok=True)
            render = ["--render", args.render.resolve().as_posix()]
        subprocess.run([*runtime, str(executable), *render], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
