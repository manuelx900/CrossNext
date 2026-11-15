"""Production cover-byte prefetch and BMP pixel/SD-I/O equivalence checks."""
from pathlib import Path
import os
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / "qa-artifacts/library-speed-prefetch-2026-11-12/checks"
out.mkdir(parents=True, exist_ok=True)
wasm = os.name == "nt"
compiler = str(root / ".venv/emsdk/upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX", "c++")
flags = ["-sENVIRONMENT=node", "-sEXIT_RUNTIME=1", "-sALLOW_MEMORY_GROWTH=1"] if wasm else []
env = os.environ.copy()
if wasm:
    # Other worktrees share this SDK; their sanity checks may rebuild its default cache.
    env.setdefault("EM_CACHE", str(out / "emscripten-cache"))
binary = out / ("covers.cjs" if wasm else "covers")
includes = ["lib/Performance", "host-tests/library-prefetch/stubs", "host-tests/home-cache/stubs", "host-tests/reading-suite/stubs",
            "lib/Memory", "lib/GfxRenderer", "src", "freeink-sdk/libs/ui/FreeInkUI/include"]
sources = ["host-tests/library-prefetch/test.cpp", "src/apps_local/library/LibraryCoverPrefetch.cpp",
           "src/apps_local/library/PreparedCoverCache.cpp",
           "src/apps_local/home/CoverBitmapPainter.cpp", "lib/GfxRenderer/Bitmap.cpp", "lib/GfxRenderer/BitmapHelpers.cpp"]
subprocess.run([compiler, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror", "-Wno-comment", *flags,
                *["-I" + str(root / p) for p in includes], *sources, "-o", str(binary)], cwd=root, env=env, check=True)
runtime = [shutil.which("node") or "node"] if wasm else []
for arguments in ([], ["oom"]):
    subprocess.run([*runtime, str(binary), *arguments], cwd=root, check=True)
