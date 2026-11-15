"""Build and install the QA catalogue using the shared production-codec exporter."""
from pathlib import Path
import os
import shutil
from export_simulator import build as export

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / ".cache/quest-redesign/scenarios"

def build():
    catalog = export(OUT)
    target = ROOT / "site/qa-reading-adventure/catalog.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(OUT / "languages", target.parent / "languages", dirs_exist_ok=True)
    controls = target.with_name("simulator.js")
    controls_pending = controls.with_suffix(".pending.js")
    controls_pending.write_bytes((OUT / "simulator.js").read_bytes())
    os.replace(controls_pending, controls)
    pending = target.with_name("catalog.pending.json")
    pending.write_bytes(catalog.read_bytes())
    os.replace(pending, target)
    print("Installed QA catalogue and matching simulator controls")
    return target

if __name__ == "__main__":
    build()
