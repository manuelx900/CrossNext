"""Exercise production keyboard selection without loading optional layouts in bulk."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
out = root / "qa-artifacts/text-keyboard-settings"
out.mkdir(parents=True, exist_ok=True)
compiler = root / ".venv/emsdk/upstream/emscripten/em++.exe"
exe = out / "keyboard.cjs"
subprocess.run([str(compiler), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-comment",
    "-I.", "-Ihost-tests/keyboard-language/stubs", "-Ihost-tests/book-dictionaries/stubs",
    "-Ihost-tests/study-suite/stubs", "-Itest/sd_card_font/stubs", "-Ilib/I18n", "-Ifreeink-sdk/libs/ui/FreeInkUI/include",
    "-Ifreeink-sdk/libs/assets/Icons/include", "host-tests/text-keyboard-settings/test.cpp",
    "src/activities/util/KeyboardLayoutSet.cpp", "lib/I18n/I18nStrings.cpp",
    "-sENVIRONMENT=node", "-sEXIT_RUNTIME=1", "-o", str(exe)], cwd=root, check=True)
subprocess.run(["node", str(exe)], cwd=root, check=True)
