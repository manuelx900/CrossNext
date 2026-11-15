"""Native cover reuse, view residency, and storage-revision regression checks."""
from pathlib import Path
import os, shutil, subprocess
root = Path(__file__).resolve().parents[2]
out = root / "qa-artifacts/library-cover-fast-open/checks"
out.mkdir(parents=True, exist_ok=True)
wasm = os.name == "nt"
compiler = str(root / ".venv/emsdk/upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX", "c++")
env = os.environ.copy()
if wasm: env["EM_CACHE"] = str(root / "qa-artifacts/library-native-covers-2026-11-13/checks/emscripten-cache")
flags = ["-sENVIRONMENT=node", "-sEXIT_RUNTIME=1", "-sALLOW_MEMORY_GROWTH=1"] if wasm else []
binary = out / ("scenes.cjs" if wasm else "scenes")
includes = ["lib/Performance", "host-tests/cover-scene/stubs", "host-tests/epub-event-cache/stubs", "freeink-sdk/libs/ui/FreeInkUI/include"]
subprocess.run([compiler, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror", *flags, *["-I" + str(root / p) for p in includes], str(root / "host-tests/cover-scene/test.cpp"), str(root / "src/apps_local/home/CoverSceneCache.cpp"), "-o", str(binary)], cwd=root, env=env, check=True)
for args in [[], ["oom"]]:
 subprocess.run(([shutil.which("node") or "node"] if wasm else []) + [str(binary), *args], cwd=root, check=True)
subprocess.run([shutil.which("python") or "python", "host-tests/library-native-covers/run.py"], cwd=root, check=True)
subprocess.run([shutil.which("python") or "python", "host-tests/library-prefetch/run.py"], cwd=root, check=True)
