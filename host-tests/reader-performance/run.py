"""Bounded pixel writes, operation timing and retained XML replay checks."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / ".pio/host-tests/reader-performance"
OUT.mkdir(parents=True, exist_ok=True)
wasm = os.name == "nt"
compiler = str(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX", "c++")
flags = ["-sENVIRONMENT=node", "-sEXIT_RUNTIME=1"] if wasm else []
env = os.environ.copy()
if wasm:
    env.setdefault("EM_CACHE", str(OUT / "emscripten-cache"))
def method(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]

reader = (ROOT / "src/activities/reader/EpubReaderActivity.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/activities/reader/EpubReaderActivity.h").read_text(encoding="utf-8")
section = (ROOT / "lib/Epub/Epub/Section.h").read_text(encoding="utf-8")
for name, source, signature in [
    ("turn", reader, "bool EpubReaderActivity::pageTurn("),
    ("settle", reader, "void EpubReaderActivity::settleOverlayRefresh("),
    ("presentation", header, "bool presentationComplete() const override"),
    ("overlay-tick", reader[reader.index("void EpubReaderActivity::handleOverlayInput()"):], "if (overlayPaintPending)"),
    ("offset", section, "bool buildReachedVisibleTextOffset(uint32_t offset) const"),
]:
    (OUT / (name + "-under-test.inc")).write_text(method(source, signature).replace(" const override", " const"), encoding="utf-8")

for name, sources, includes, definitions in [
    ("pixels", ["host-tests/reader-performance/pixel_cache.cpp"], ["host-tests/reader-performance/stubs"], []),
    ("timing", ["host-tests/reader-performance/timing.cpp", "lib/Performance/Performance.cpp"],
     ["host-tests/reader-performance/stubs", "lib/Performance"], ["-DCROSSPOINT_PERFORMANCE_TRACE", "-DSIMULATOR"]),
    ("reader", ["host-tests/reader-performance/reader.cpp", "lib/Performance/Performance.cpp"],
     [str(OUT), "host-tests/reader-performance/stubs", "lib/Performance"], ["-DCROSSPOINT_PERFORMANCE_TRACE", "-DSIMULATOR"]),
    ("events", ["host-tests/epub-event-cache/test.cpp", "lib/Epub/Epub/parsers/ChapterEventCache.cpp"],
     ["host-tests/epub-event-cache/stubs", "lib/Epub/Epub/parsers", "lib/Performance"], []),
]:
    exe = OUT / (name + (".cjs" if wasm else ""))
    command = [compiler, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
               *flags, *definitions, *["-I" + str(ROOT / p) for p in includes],
               *[str(ROOT / p) for p in sources], "-o", str(exe)]
    subprocess.run(command, cwd=ROOT, env=env, check=True)
    subprocess.run([shutil.which("node") or "node", str(exe)] if wasm else [str(exe)], cwd=ROOT, check=True)
