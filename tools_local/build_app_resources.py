"""Build the complete distributable artwork for the three microSD apps."""
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPS = ("reading-adventure", "pet", "intercept")


def module(name, path):
    directory = str((ROOT / path).parent)
    if directory not in sys.path:
        sys.path.insert(0, directory)
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def build(destination):
    module("quest_pack", "tools_local/quest/build_resource_pack.py").build(destination / "reading-adventure/graphics.bin")
    module("quest_story_pack", "tools_local/quest/build_story_art.py").build(destination / "reading-adventure/story.bin")
    module("quest_milestones", "tools_local/quest/build_milestone_art.py").build(destination / "reading-adventure")
    module("quest_expansion_pack", "tools_local/quest/build_expansion_art.py").build(destination / "reading-adventure")
    module("quest_bestiary_outfits", "tools_local/quest/build_bestiary_outfits.py").build(destination / "reading-adventure")
    module("quest_hunt_bounds", "tools_local/quest/build_hunt_bounds.py").build(destination / "reading-adventure", check=True)
    gray = module("pet_gray", "tools_local/pet/gray_assets.py")
    if gray.available():
        for name in ("graphics", "home", "habitat", "breathing", "farewell", "collection"):
            gray.build(name, destination / f"pet/{name}.bin")
    else:
        pet = module("pet_pack", "tools_local/pet/build_resources.py")
        source = ROOT / "assets_local/pet/png"
        pet.build(destination / "pet/graphics.bin", source)
        pet.build(destination / "pet/home.bin", source, home=True)
        for name in ("habitat", "breathing", "farewell", "collection"):
            module("pet_" + name, f"tools_local/pet/build_{name}.py").build(destination / f"pet/{name}.bin")
    module("intercept_pack", "tools_local/intercept/build_resource_pack.py").build(destination / "intercept/graphics.bin")
