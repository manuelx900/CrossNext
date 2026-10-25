#!/usr/bin/env python3
"""Run PET's state and rendered-UI checks with native C++ or bundled Emscripten."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "qa-artifacts/pet-ui-2026-10-23")
    parser.add_argument("--art", action="store_true", help="Render supplied SD artwork instead of fallback icons")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    # resources/ is generated and may be absent in a fresh checkout.
    for options in ([], ["--home"]):
        subprocess.run([os.sys.executable, "tools_local/pet/build_resources.py", *options], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "tools_local/pet/build_habitat.py"], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "tools_local/pet/build_breathing.py"], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "tools_local/pet/build_farewell.py"], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "tools_local/pet/build_collection.py"], cwd=ROOT, check=True)
    wasm = os.name == "nt"
    compiler = str(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX", "c++")
    runtime = [shutil.which("node") or str(ROOT / ".venv/emsdk/node/24.19.0_64bit/node.exe")] if wasm else []
    common = ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-comment"]
    extension = ".cjs" if wasm else ""
    flags = ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sALLOW_MEMORY_GROWTH=1", "-sEXIT_RUNTIME=1", "-sSTACK_SIZE=1048576"] if wasm else []
    subprocess.run([os.sys.executable, "host-tests/pet/test_activity_scan.py", str(output / "activity")], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "host-tests/pet/test_selection.py", str(output / "selection")], cwd=ROOT, check=True)
    state = output / ("test-state" + extension)
    subprocess.run([compiler, *common, "host-tests/pet/test_state.cpp", "src/apps_local/pet/PetState.cpp",
                    *flags, "-o", str(state)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(state)], cwd=ROOT, check=True)
    provisions = output / ("test-provisions" + extension)
    subprocess.run([compiler, *common, "host-tests/pet/test_provisions.cpp", "src/apps_local/pet/PetState.cpp",
                    *flags, "-o", str(provisions)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(provisions)], cwd=ROOT, check=True)
    games = output / ("test-games" + extension)
    subprocess.run([compiler, *common, "-Ifreeink-sdk/libs/ui/FreeInkUI/include", "host-tests/pet/test_games.cpp", "src/apps_local/pet/PetState.cpp",
                    *flags, "-o", str(games)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(games)], cwd=ROOT, check=True)
    animation = output / ("test-animation" + extension)
    subprocess.run([compiler, *common, "host-tests/pet/test_animation.cpp", "src/apps_local/pet/PetState.cpp",
                    *flags, "-o", str(animation)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(animation)], cwd=ROOT, check=True)
    storage = output / ("test-storage" + extension)
    subprocess.run([compiler, *common, "-Ihost-tests/habits/stubs", "host-tests/pet/test_storage.cpp",
                    "src/apps_local/pet/PetStore.cpp", "src/apps_local/pet/PetState.cpp", "src/apps_local/pet/PetHistory.cpp",
                    *flags, "-o", str(storage)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(storage)], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "host-tests/pet/test_resources.py"], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "host-tests/pet/test_habitat.py"], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "host-tests/pet/test_breathing.py"], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "host-tests/pet/test_farewell.py"], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, "host-tests/pet/test_collection.py"], cwd=ROOT, check=True)
    packed = output / ("test-pack" + extension)
    subprocess.run([compiler, *common, "-Ihost-tests/sdart/stubs", "-Ifreeink-sdk/libs/ui/FreeInkUI/include",
                    "host-tests/pet/test_pack.cpp", "src/apps_local/ui/SdBitmapPack.cpp",
                    "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp", *flags, "-o", str(packed)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(packed)], cwd=ROOT, check=True)
    progression = output / ("test-progression" + extension)
    subprocess.run([compiler, *common, "host-tests/pet/test_progression.cpp", "src/apps_local/pet/PetState.cpp",
                    "src/apps_local/reading/ReadingStatsCore.cpp", *flags, "-o", str(progression)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(progression)], cwd=ROOT, check=True)
    sources = ["host-tests/pet/test_screens.cpp", "src/apps_local/pet/PetGameScreens.cpp", "src/apps_local/pet/PetNoticeScreen.cpp", "src/apps_local/pet/PetState.cpp",
               "src/apps_local/pet/PetScreens.cpp", "src/apps_local/pet/PetAlbumScreens.cpp", "src/apps_local/pet/PetReadingScreen.cpp", "src/apps_local/reading/ReadingStatsCore.cpp", "src/apps_local/pet/PetAffectionScreen.cpp", "src/apps_local/pet/PetNameScreen.cpp", "src/apps_local/pet/PetSetupScreens.cpp",
               "src/apps_local/pet/PetDetailScreens.cpp", "src/apps_local/pet/PetCareScreens.cpp", "src/apps_local/pet/PetLifeScreens.cpp", "src/apps_local/pet/PetFarewellScreen.cpp", "src/apps_local/pet/PetCollectionScreen.cpp", "src/apps_local/ui/PixelMenu.cpp", "lib/I18n/I18n.cpp",
               "lib/I18n/I18nStrings.cpp", "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"]
    includes = ["-Ifreeink-sdk/libs/ui/FreeInkUI/include", "-Ifreeink-sdk/libs/assets/Icons/include", "-Ilib/EpdFont", "-Ilib/I18n"]
    screens = output / ("test-screens" + extension)
    subprocess.run([compiler, *common, *includes, *sources, *flags, "-o", str(screens)],
                   cwd=ROOT, check=True)
    result = subprocess.run([*runtime, str(screens), str(output), *(["art"] if args.art else [])], cwd=ROOT, text=True, encoding="utf-8", capture_output=True)
    (output / "screens.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print((result.stdout + result.stderr).strip())
    try:
        from PIL import Image
        for path in output.glob("*.pbm"):
            with Image.open(path) as image:
                image.convert("RGB").save(path.with_suffix(".png"))
    except ImportError:
        print("Pillow unavailable: raw PBM previews were retained.")
    result.check_returncode()


if __name__ == "__main__":
    main()
