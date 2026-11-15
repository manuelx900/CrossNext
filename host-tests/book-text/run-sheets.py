"""Render the shared settings/reader panel and verify its actual touch regions."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / "qa-artifacts/reader-ui-fixes/sheets"
out.mkdir(parents=True, exist_ok=True)
compiler = root / ".venv/emsdk/upstream/emscripten/em++.exe"
includes = ["test/language_pack/stubs", "host-tests/reading-suite/stubs", "lib/I18n", "lib/EpdFont",
            "freeink-sdk/libs/ui/FreeInkUI/include", "freeink-sdk/libs/assets/Icons/include"]
sources = ["test/reader_font_sheet/ReaderFontSheetTest.cpp", "src/apps_local/ui/PixelMenu.cpp",
           "lib/I18n/I18n.cpp", "lib/I18n/I18nStrings.cpp", "lib/I18n/LanguagePack.cpp",
           "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"]
exe = out / "test.cjs"
subprocess.run([str(compiler), "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror", "-Wno-comment",
                *["-I" + p for p in includes],
                *sources, "-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1", "-sSTACK_SIZE=1048576",
                "-sALLOW_MEMORY_GROWTH=1", "-o", str(exe)], cwd=root, check=True)
subprocess.run(["node", str(exe), out.as_posix()], cwd=root, check=True)
