"""Generate fresh and multi-day virtual-SD fixtures using the production PET rules."""
from pathlib import Path
import base64
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "qa-artifacts/pet-refinement-2026-10-25"
FIXTURES = OUT / "fixtures"
FIXTURES.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env.update(PYTHONUTF8="1", EMCC_CORES="2", PYTHON_CPU_COUNT="2", TZ="America/Punta_Arenas")
# The bundled compiler keeps these checks independent of a native firmware toolchain.
compiler = ROOT / ".venv/emsdk/upstream/emscripten/em++.exe"
node = Path("C:/Program Files/nodejs/node.exe")
cmd = [str(compiler), "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
       "host-tests/pet/test_progression.cpp", "src/apps_local/pet/PetState.cpp",
       "src/apps_local/reading/ReadingStatsCore.cpp", "-sENVIRONMENT=node", "-sNODERAWFS=1",
       "-sEXIT_RUNTIME=1", "-o", str(OUT / "progression.cjs")]
with (OUT / "progression-build.log").open("w", encoding="utf-8") as log:
    subprocess.run(cmd, cwd=ROOT, env=env, check=True, stdout=log, stderr=subprocess.STDOUT)
with (OUT / "progression.log").open("w", encoding="utf-8") as log:
    subprocess.run([str(node), str(OUT / "progression.cjs"), str(FIXTURES)], cwd=ROOT,
                   env=env, check=True, stdout=log, stderr=subprocess.STDOUT)
names = ["fresh"] + [f"day{day}-{minutes}" for minutes in (15, 30, 60) for day in (1, 7, 14)]
cases = {name: {key: base64.b64encode((FIXTURES / f"{name}-{key}.bin").read_bytes()).decode("ascii")
                for key in ("state", "journal")} for name in names}
(ROOT / "site/qa-pet-progress.json").write_text(json.dumps(cases, separators=(",", ":")), "utf-8")
print((OUT / "progression.log").read_text("utf-8"))
print("Generated10 independent presets without inherited reading, stock or furniture.")
