"""Run per-book dictionary persistence tests with Emscripten/Node."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[2]
output = root / "datos.local/language-wordle-polish/book-dictionaries.cjs"
output.parent.mkdir(parents=True, exist_ok=True)
emsdk = root / ".venv/emsdk/upstream/emscripten"
environment = os.environ.copy()
environment["PATH"] = str(emsdk) + os.pathsep + environment.get("PATH", "")
compiler = emsdk / ("em++.exe" if os.name == "nt" else "em++")
subprocess.run([str(compiler), "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror", "-Wno-comment",
                "-Ihost-tests/book-dictionaries/stubs", "-Ihost-tests/study-suite/stubs",
                "-Itest/sd_card_font/stubs", "host-tests/book-dictionaries/test.cpp",
                "src/util/BookDictionarySettings.cpp", "-sENVIRONMENT=node", "-sEXIT_RUNTIME=1",
                "-o", str(output)], cwd=root, env=environment, check=True)
subprocess.run(["node", str(output)], cwd=root, check=True)
