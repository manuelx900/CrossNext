"""Render native WinForms layouts; never opens or writes a microSD."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "dist" / (sys.argv[1] if len(sys.argv) > 1 else "CrossNext-Book-Preparer-Test.exe")
CASES = [
    ("100-small-en", 96, "1280x720", False),
    ("100-desktop-es", 96, "1920x1040", True),
    ("125-laptop-en", 120, "1920x1040", False),
    ("150-laptop-es", 144, "1920x1040", True),
    ("200-laptop-en", 192, "1920x1040", False),
    ("200-small-es", 192, "1366x728", True),
    ("200-desktop-es", 192, "2560x1440", True),
    ("200-4k-en", 192, "3840x2080", False),
    ("250-desktop-en", 240, "2560x1440", False),
]

results = []
for name, dpi, work, spanish in CASES:
    output = ROOT / "build/dpi-layout" / name
    output.mkdir(parents=True, exist_ok=True)
    for filename in ("result.json", "failure.txt", "trace.txt"):
        (output / filename).unlink(missing_ok=True)
    args = [str(EXE), "--layout-output", str(output), "--layout-dpi", str(dpi), "--layout-work", work]
    if spanish:
        args.append("--es")
    completed = subprocess.run(args, timeout=60)
    assert completed.returncode == 0, (name, completed.returncode)
    failure = output / "failure.txt"
    assert not failure.exists(), (name, failure.read_text(encoding="utf-8") if failure.exists() else "")
    result = json.loads((output / "result.json").read_text(encoding="utf-8"))
    assert abs(result["fontPixels"] - result["baseFontPixels"] * dpi / 96) < 0.1, result
    results.append({"case": name, **result})
    print(name, "PASS", f'{result["width"]}x{result["height"]}', flush=True)
(ROOT / "build/dpi-layout/results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
