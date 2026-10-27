#!/usr/bin/env python3
"""Assemble a microSD-ready resource tree and deterministic distributable ZIP."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DICTIONARIES = ROOT / "tools_local/wasm/sdcard/dictionaries"
ADDITIONS = ROOT / "assets_local/sd-resources"


def copy_tree(source, destination):
    for path in sorted(source.rglob("*")):
        if path.is_file() and "__pycache__" not in path.parts:
            target = destination / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)


def assemble(output, dictionaries, staging):
    staging.mkdir(parents=True)
    spec = importlib.util.spec_from_file_location("app_resources", ROOT / "tools_local/build_app_resources.py")
    apps = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(apps)
    apps.build(staging / "apps")
    copy_tree(ROOT / "assets_local/reader-fonts/sd-card/fonts", staging / "fonts")
    copy_tree(ADDITIONS / "fonts", staging / "fonts")
    if not dictionaries.is_dir():
        raise FileNotFoundError(f"Dictionary source is missing: {dictionaries}")
    # Only distributable StarDict files, never simulator settings or saved progress.
    allowed = (".ifo", ".idx", ".dict", ".dict.dz", ".syn", ".css", ".txt", ".md")
    (staging / "dictionaries").mkdir(exist_ok=True)
    sources = [dictionaries]
    if dictionaries.resolve() == DEFAULT_DICTIONARIES.resolve():
        sources.append(ADDITIONS / "dictionaries")
    for source in sources:
        for path in sorted(source.rglob("*")):
            if path.is_file() and path.name.endswith(allowed):
                relative = path.relative_to(source)
                folder = "Spanish Wiktionary" if relative.parts[0] == "Espanol Wikcionario" else relative.parts[0]
                target = staging / "dictionaries" / folder / Path(*relative.parts[1:])
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    copy_tree(ADDITIONS / "licenses", staging / "licenses")
    shutil.copy2(ADDITIONS / "THIRD-PARTY-NOTICES.txt", staging / "THIRD-PARTY-NOTICES.txt")
    shutil.copy2(ADDITIONS / "CUSTOMIZATION.txt", staging / "CUSTOMIZATION.txt")
    catalog = {"retrieved": "2026-10-20", "files": []}
    for name in ("font-sources.json", "dictionary-sources.json", "existing-dictionary-sources.json"):
        catalog["files"].extend(json.loads((ADDITIONS / name).read_text(encoding="utf-8"))["files"])
    catalog["files"] = [entry for entry in catalog["files"] if (staging / entry["path"]).is_file()]
    for entry in catalog["files"]:
        if hashlib.sha256((staging / entry["path"]).read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"Resource differs from its provenance record: {entry['path']}")
    spec = importlib.util.spec_from_file_location("compact_dictionaries", ROOT / "tools_local/compact_dictionaries.py")
    compact = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(compact)
    changes = []
    for directory in sorted((staging / "dictionaries").iterdir()):
        if directory.is_dir():
            changes.extend(compact.compact_dictionary(directory))
    by_input = {change["input"].relative_to(staging).as_posix(): change for change in changes}
    for entry in catalog["files"]:
        change = by_input.get(entry["path"])
        if not change:
            continue
        entry["sourceSha256"] = change["sourceSha256"]
        entry["path"] = change["output"].relative_to(staging).as_posix()
        entry["bytes"] = change["output"].stat().st_size
        entry["sha256"] = hashlib.sha256(change["output"].read_bytes()).hexdigest()
        entry["modified"] = True
        entry["modifications"] = entry.get("modifications", "") + (
            " Compact reading layout: web styles, pronunciation, syllabification, repeated headword/inflection rows "
            "and etymology metadata omitted; "
            "Bilingual target equivalents retained in sense groups; source-language glosses omitted, "
            "category labels localized to the destination language, source grammatical gender omitted. "
            "Dictionary order and synonym pointers retained." if change["targetLanguage"] else
            " Compact reading layout: web styles, pronunciation, syllabification, repeated headword/inflection rows "
            "and etymology metadata omitted; "
            "definitions, examples, symbols and lexical relations retained. Dictionary order and synonym pointers retained.")
        if entry["path"].startswith("dictionaries/WikDict EN-ES/"):
            entry["modifications"] += " EN-ES private (noun): incorrect cabo equivalent removed; soldado raso retained."
    (staging / "SOURCES.json").write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    for directory in (staging / "dictionaries").iterdir():
        if not directory.is_dir():
            continue
        attribution = ADDITIONS / "attribution" / (directory.name + ".txt")
        if attribution.is_file():
            shutil.copy2(attribution, directory / "ATTRIBUTION.txt")
            shutil.copy2(ADDITIONS / "licenses/CC-BY-SA-4.0.txt", directory / "LICENSE-CC-BY-SA-4.0.txt")
            if list(directory.glob("*.css")):
                shutil.copy2(ADDITIONS / "licenses/GPL-3.0.txt", directory / "LICENSE-GPL-3.0.txt")
    dictionary_description = (
        "Includes English and Spanish definitions, plus English <-> Spanish, French, German\n"
        "and Portuguese translations (10 StarDict dictionaries in total).\n"
        if dictionaries.resolve() == DEFAULT_DICTIONARIES.resolve()
        else "Includes a custom StarDict dictionary collection selected by the package builder.\n")
    (staging / "README.txt").write_text(
        "CROSSNEXT - MICROSD RESOURCES\n\n"
        "Extract this ZIP and copy apps, fonts and dictionaries to the ROOT of your microSD.\n"
        "Merge with existing folders. Do not copy the resources parent folder or format the card.\n"
        "Power off before removing the card; restart the reader after installing resources.\n\n"
        "Includes PET, The Intercept and Reading Adventure artwork, plus 8 font families:\n"
        "Bitter, Lexend Deca, Literata,\n"
        "Atkinson Hyperlegible, Merriweather, EB Garamond, PT Serif and Inter.\n"
        + dictionary_description +
        "EN/ES interface languages automatically choose installed definition/translation defaults.\n"
        "Dictionary entries use compact numbered meanings, synonyms and examples when supplied by the source.\n"
        "The eight bundled WikDict translators show destination-language equivalents and categories, grouped by sense.\n"
        "Their source-language explanations and gender metadata are omitted. Custom dictionaries retain their language.\n"
        "No meanings or examples are generated. Web styles and pronunciation/etymology metadata are omitted.\n"
        "Manual dictionary selections, including None, take precedence.\n"
        "App resources: apps/reading-adventure, apps/pet and apps/intercept.\n"
        "Install compatible firmware for your device separately. This ZIP contains no firmware.\n"
        "These apps need their complete resource folders to work. If resources are unavailable,\n"
        "replace the indicated app folder with the complete folder from this ZIP.\n"
        "Saved progress stays in .crosspoint. This package contains no saves and never replaces it.\n"
        "Keep your existing books, settings, fonts and dictionaries.\n\n"
        "Keep THIRD-PARTY-NOTICES.txt, SOURCES.json, licenses and the per-folder notices.\n"
        "Fonts: SIL OFL 1.1. Dictionary data: CC BY-SA 4.0. Optional xxyzz CSS: GPL 3 or later.\n"
        "See CUSTOMIZATION.txt to customize artwork or add fonts and dictionaries.\n",
        encoding="utf-8")
    files = [p for p in sorted(staging.rglob("*")) if p.is_file() and p.name != "MANIFEST.json"]
    if any(".crosspoint" in path.relative_to(staging).parts for path in files):
        raise ValueError("Saved state must never be included in a resource package")
    manifest = {"formatVersion": 1, "apps": {name: "included" for name in apps.APPS},
                "files": [{"path": p.relative_to(staging).as_posix(), "bytes": p.stat().st_size,
                           "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]}
    (staging / "MANIFEST.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    files.append(staging / "MANIFEST.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(files):
            info = zipfile.ZipInfo(path.relative_to(staging).as_posix(), date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes(), compresslevel=9)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError("ZIP integrity check failed")



def build(output, dictionaries, staging_target=None):
    target = staging_target or ROOT / "resources"
    workspace = ROOT.resolve()
    if target.is_symlink() or not target.resolve().is_relative_to(workspace) or target.resolve() == workspace:
        raise ValueError("Resource staging must stay inside the project workspace")
    if target.exists():
        # Only replace a complete, unmodified output owned by this generator.
        manifest_path = target / "MANIFEST.json"
        if not manifest_path.is_file():
            raise ValueError("Existing resource staging has no generation manifest; preserve it manually")
        previous = json.loads(manifest_path.read_text(encoding="utf-8"))
        expected = {item["path"]: item for item in previous["files"]}
        actual = {p.relative_to(target).as_posix(): p for p in target.rglob("*") if p.is_file()}
        if set(actual) != set(expected) | {"MANIFEST.json"}:
            raise ValueError("Resource staging contains personal or untracked files; move them before rebuilding")
        for name, entry in expected.items():
            if actual[name].is_symlink() or hashlib.sha256(actual[name].read_bytes()).hexdigest() != entry["sha256"]:
                raise ValueError(f"Resource staging was edited; preserve the changed file before rebuilding: {name}")
    scratch_root = ROOT / "dist"
    scratch_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".resource-build-", dir=scratch_root) as directory:
        scratch = Path(directory).resolve()
        # TemporaryDirectory cleans up recursively, including the previous generated tree.
        if scratch.parent != scratch_root.resolve():
            raise ValueError("Unexpected resource build directory")
        staging = scratch / "resources"
        archive = scratch / "CrossNext-Resources.zip"
        assemble(archive, dictionaries, staging)
        output.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            target.rename(scratch / "previous")
        target.parent.mkdir(parents=True, exist_ok=True)
        try:
            staging.rename(target)
        except OSError:
            if (scratch / "previous").exists():
                (scratch / "previous").rename(target)
            raise
        shutil.copy2(archive, output)
    print(f"Resources ZIP: {output} ({output.stat().st_size:,} bytes)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist/CrossNext-Resources.zip")
    parser.add_argument("--dictionaries", type=Path, default=DEFAULT_DICTIONARIES)
    args = parser.parse_args()
    build(args.output, args.dictionaries)
