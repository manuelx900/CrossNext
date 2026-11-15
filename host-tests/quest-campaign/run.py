"""Run pure campaign progression and storage checks on Windows or a native host."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / ".cache/reading-campaign"


def run():
    OUT.mkdir(parents=True, exist_ok=True)
    from test_content import check
    check(ROOT, OUT)
    from test_continuity import check as check_continuity
    check_continuity(ROOT)
    from test_bestiary_outfits import check as check_outfits
    check_outfits(ROOT)
    compiler = str(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe") if os.name == "nt" else os.environ.get("CXX", "c++")
    flags = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-comment", "-Ihost-tests/study-suite/stubs"]
    if os.name == "nt":
        flags += ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sALLOW_MEMORY_GROWTH=1", "-sEXIT_RUNTIME=1", "-sSTACK_SIZE=1048576"]
    output = OUT / ("campaign-tests.cjs" if os.name == "nt" else "campaign-tests")
    sources = ["host-tests/quest-campaign/test.cpp", "src/apps_local/quest/QuestCore.cpp", "src/apps_local/quest/QuestCampaign.cpp",
               "src/apps_local/quest/QuestStore.cpp", "src/apps_local/reading/ReadingStatsCore.cpp"]
    subprocess.run([compiler, *flags, *sources, "-o", str(output)], cwd=ROOT, check=True)
    subprocess.run([*([shutil.which("node")] if os.name == "nt" else []), str(output)], cwd=ROOT, check=True)
    request_output = OUT / ("request-tests.cjs" if os.name == "nt" else "request-tests")
    subprocess.run([compiler, *flags, "host-tests/study-suite/test_requests.cpp", *sources[1:], "-o", str(request_output)], cwd=ROOT, check=True)
    subprocess.run([*([shutil.which("node")] if os.name == "nt" else []), str(request_output)], cwd=ROOT, check=True)
    expansion_output = OUT / ("expansion-tests.cjs" if os.name == "nt" else "expansion-tests")
    subprocess.run([compiler, *flags, "host-tests/study-suite/test_expansion.cpp", *sources[1:], "-o", str(expansion_output)], cwd=ROOT, check=True)
    subprocess.run([*([shutil.which("node")] if os.name == "nt" else []), str(expansion_output)], cwd=ROOT, check=True)
    polish_output = OUT / ("polish-tests.cjs" if os.name == "nt" else "polish-tests")
    subprocess.run([compiler, *flags, "host-tests/quest-campaign/test_polish.cpp", *sources[1:], "-o", str(polish_output)], cwd=ROOT, check=True)
    subprocess.run([*([shutil.which("node")] if os.name == "nt" else []), str(polish_output)], cwd=ROOT, check=True)

    order_output = OUT / ("order-tests.cjs" if os.name == "nt" else "order-tests")
    subprocess.run([compiler, *flags, "host-tests/quest-campaign/test_order.cpp", *sources[1:], "-o", str(order_output)], cwd=ROOT, check=True)
    subprocess.run([*([shutil.which("node")] if os.name == "nt" else []), str(order_output)], cwd=ROOT, check=True)

    simulator_output = OUT / ("simulator-reading-tests.cjs" if os.name == "nt" else "simulator-reading-tests")
    subprocess.run([compiler, *flags, "host-tests/quest-campaign/test_simulator_reading.cpp", "src/apps_local/reading/ReadingStatsCore.cpp", "-o", str(simulator_output)], cwd=ROOT, check=True)
    subprocess.run([*([shutil.which("node")] if os.name == "nt" else []), str(simulator_output)], cwd=ROOT, check=True)
    node = shutil.which("node")
    continuity_output = OUT / ("continuity-tests.cjs" if os.name == "nt" else "continuity-tests")
    subprocess.run([compiler, *flags, "host-tests/quest-campaign/test_continuity.cpp", *sources[1:], "-o", str(continuity_output)], cwd=ROOT, check=True)
    subprocess.run([*([shutil.which("node")] if os.name == "nt" else []), str(continuity_output)], cwd=ROOT, check=True)
    bestiary_output = OUT / ("bestiary-tests.cjs" if os.name == "nt" else "bestiary-tests")
    subprocess.run([compiler, *flags, "host-tests/quest-campaign/test_bestiary.cpp", *sources[1:], "-o", str(bestiary_output)], cwd=ROOT, check=True)
    subprocess.run([*([shutil.which("node")] if os.name == "nt" else []), str(bestiary_output)], cwd=ROOT, check=True)
    for test in ("balance", "progression", "milestones", "victory_economy", "repeat_hunts"):
        target = OUT / (f"{test}-tests.cjs" if os.name == "nt" else f"{test}-tests")
        subprocess.run([compiler, *flags, f"host-tests/quest-campaign/test_{test}.cpp", *sources[1:], "-o", str(target)], cwd=ROOT, check=True)
        subprocess.run([*([node] if os.name == "nt" else []), str(target)], cwd=ROOT, check=True)
    if node:
        subprocess.run([node, "host-tests/quest-campaign/test_simulator_bridge.cjs"], cwd=ROOT, check=True)
    else:
        print("Simulator reading bridge: not run (Node.js unavailable)")


if __name__ == "__main__":
    run()
