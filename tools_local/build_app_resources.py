"""Build the complete distributable artwork for the three microSD apps."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPS = ("reading-adventure", "pet", "intercept")


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def build(destination):
    module("quest_pack", "tools_local/quest/build_resource_pack.py").build(destination / "reading-adventure/graphics.bin")
    pet = module("pet_pack", "tools_local/pet/build_resources.py")
    source = ROOT / "assets_local/pet/png"
    pet.build(destination / "pet/graphics.bin", source)
    pet.build(destination / "pet/home.bin", source, home=True)
    for name in ("habitat", "breathing", "farewell", "collection"):
        module("pet_" + name, f"tools_local/pet/build_{name}.py").build(destination / f"pet/{name}.bin")
    module("intercept_pack", "tools_local/intercept/build_resource_pack.py").build(destination / "intercept/graphics.bin")
