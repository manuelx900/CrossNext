"""Run the Settings UI suite with a native compiler or the local Emscripten SDK."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
EMSCRIPTEN = ROOT / ".venv/emsdk/upstream/emscripten/em++.exe"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX"))
    parser.add_argument("--build-dir", type=Path)
    options, test_args = parser.parse_known_args()
    compiler = options.compiler or shutil.which("c++")
    if not compiler and EMSCRIPTEN.is_file():
        compiler = str(EMSCRIPTEN)
    if not compiler:
        parser.error("No C++ compiler found; set CXX or pass --compiler.")
    emscripten = Path(compiler).name.startswith(("em++", "emcc"))
    includes = ["test/language_pack/stubs", "host-tests/reading-suite/stubs", "lib/EpdFont", "lib/I18n",
                "freeink-sdk/libs/ui/FreeInkUI/include", "freeink-sdk/libs/assets/Icons/include"]
    sources = ["host-tests/settings-redesign/test_screens.cpp", "src/apps_local/ui/PixelMenu.cpp",
               "src/apps_local/ShelfScreen.cpp", "lib/I18n/I18n.cpp", "lib/I18n/I18nStrings.cpp",
               "lib/I18n/LanguagePack.cpp", "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"]
    with tempfile.TemporaryDirectory(prefix="settings-redesign-") as temporary:
        build = options.build_dir.resolve() if options.build_dir else Path(temporary)
        build.mkdir(parents=True, exist_ok=True)
        binary = build / ("tests.cjs" if emscripten else "tests.exe" if os.name == "nt" else "tests")
        command = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-comment",
                   "-Wno-missing-field-initializers", *["-I" + path for path in includes], *sources]
        if emscripten:
            command += ["-O1", "-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1",
                        "-sSTACK_SIZE=1048576", "-sALLOW_MEMORY_GROWTH=1"]
        subprocess.run([*command, "-o", str(binary)], cwd=ROOT, check=True)
        run = ["node", str(binary)] if emscripten else [str(binary)]
        return subprocess.run([*run, *test_args], cwd=ROOT).returncode


if __name__ == "__main__":
    raise SystemExit(main())
