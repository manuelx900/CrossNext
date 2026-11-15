"""Exercise the production text preference store, including interrupted writes."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[2]
output = root / "qa-artifacts/reader-ui-fixes/book-text.cjs"
output.parent.mkdir(parents=True, exist_ok=True)
compiler = root / ".venv/emsdk/upstream/emscripten/em++.exe"
subprocess.run([str(compiler), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-comment",
                "-Ihost-tests/book-text/stubs", "-Ihost-tests/book-dictionaries/stubs",
                "-Ihost-tests/study-suite/stubs", "-Itest/sd_card_font/stubs",
                "-I.pio/libdeps/x4pro/ArduinoJson/src",
                "host-tests/book-text/test.cpp", "src/apps_local/readertext/BookTextPreferences.cpp",
                "-sENVIRONMENT=node", "-sEXIT_RUNTIME=1", "-o", str(output)], cwd=root, check=True)
subprocess.run(["node", str(output)], cwd=root, check=True)
