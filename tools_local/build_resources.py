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

import enrich_dictionaries
import language_packs

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DICTIONARIES = ROOT / "tools_local/wasm/sdcard/dictionaries"
ADDITIONS = ROOT / "assets_local/sd-resources"


def copy_tree(source, destination):
    for path in sorted(source.rglob("*")):
        if path.is_file() and "__pycache__" not in path.parts:
            target = destination / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)


def dictionary_notices(staging, expansions):
    by_folder = {item["folder"]: item for item in expansions}
    for directory in (staging / "dictionaries").iterdir():
        if not directory.is_dir():
            continue
        attribution = ADDITIONS / "attribution" / (directory.name + ".txt")
        if not attribution.is_file():
            continue
        # Historical notices describe the original input, not the generated files.
        shutil.copy2(attribution, directory / "ATTRIBUTION-original.txt")
        shutil.copy2(ADDITIONS / "licenses/CC-BY-SA-4.0.txt", directory / "LICENSE-CC-BY-SA-4.0.txt")
        if list(directory.glob("*.css")):
            shutil.copy2(ADDITIONS / "licenses/GPL-3.0.txt", directory / "LICENSE-GPL-3.0.txt")
        result = by_folder.get(directory.name)
        notice = (
            directory.name + " - CrossNext generated adaptation\n\n"
            "Original input attribution: ATTRIBUTION-original.txt. Its byte-preservation statements\n"
            "describe the input archive, not this adaptation. Keep all original copyright/license notices.\n"
            "Source data: Wiktionary contributors, with providers and source links in the original notices.\n"
            "Original input metadata: SOURCE-METADATA.txt.\n"
            "Web formatting, pronunciation and etymology metadata omitted; meanings and examples compacted.\n"
            "Bilingual entries retain equivalents, localized categories and source usage contexts;\n"
            "common senses precede proper names and explicitly obsolete uses.\n")
        if directory.name == "WikDict EN-ES":
            notice += "EN-ES private (noun): incorrect cabo equivalent removed; soldado raso retained.\n"
        if result:
            notice += (
                "Additional source: " + result["url"] + "\nSHA256: " + result["sha256"] + "\n"
                "See ATTRIBUTION-expansion.txt and LICENSE-expansion.txt for its original notices and terms.\n"
                "Accents normalized to NFC; inflection aliases remapped to the rebuilt index.\n")
            if result.get("translationSource"):
                notice += ("Translation database: " + result["translationSource"] + "\nSHA256: " +
                           result["translationSha256"] + "\nSee ATTRIBUTION-WikDict.txt and LICENSE-WikDict.txt.\n")
            if result["editorialEntries"]:
                notice += "Independently authored corrections: " + ", ".join(result["editorialEntries"]) + ".\n"
            if result.get("usageRanking"):
                notice += (
                    "Redundant very-low-frequency single-word alternatives omitted only alongside a common\n"
                    "equivalent of the same sense. Ordinary equivalent order, sole translations, compounds,\n"
                    "qualified usages, names and examples preserved. Historical usages explicitly labeled.\n"
                    "Desktop usage data: wordfreq 3.1.1 (coverage through about 2021), not a semantic validator.\n"
                    "See ATTRIBUTION-wordfreq.txt and LICENSE-wordfreq.txt for credits, sources and terms.\n")
        notice += (
            "Indexes, synonym pointers and compressed data rebuilt. Adapted lexical data: CC BY-SA 4.0;\n"
            "other source material retains the terms in its notices. This does not relicense the firmware.\n"
            "SOURCES.json records source hashes and transformations; MANIFEST.json hashes this output.\n"
            "No provider endorsement is implied.\n")
        (directory / "ATTRIBUTION.txt").write_text(notice, encoding="utf-8", newline="\n")
        ifo = next(directory.glob("*.ifo"))
        metadata = ifo.read_text(encoding="utf-8")
        description = (
            "description=CrossNext compact adaptation of open lexical data. Distinct source meanings combined; "
            "formatting compacted and indexes rebuilt. Bilingual equivalents grouped by meaning and refined for everyday use. "
            "See ATTRIBUTION.txt, ATTRIBUTION-original.txt and any expansion notices for providers, "
            "original source metadata, licenses and editorial changes. Adapted lexical data: CC BY-SA 4.0.")
        lines = [line for line in metadata.splitlines() if not line.startswith("description=")]
        ifo.write_text("\n".join(lines + [description]) + "\n", encoding="utf-8", newline="\n")
    notice_path = staging / "THIRD-PARTY-NOTICES.txt"
    notice = notice_path.read_text(encoding="utf-8").replace(
        "The dictionary data and .ifo metadata are preserved without textual changes.",
        "Dictionary outputs are adapted: formatting compacted, overlapping source meanings merged,\n"
        "Unicode accents normalized and indexes rebuilt. Selected independently authored corrections\n"
        "are identified in each affected folder's ATTRIBUTION.txt. Original source notices are\n"
        "preserved separately; their unchanged-byte claims apply only to the original inputs.")
    notice = notice.replace("Retain per-dictionary ATTRIBUTION.txt, original .ifo and full license notices.",
                            "Retain per-dictionary ATTRIBUTION*.txt, source metadata and full license notices.")
    notice_path.write_text(notice, encoding="utf-8", newline="\n")


def assemble(output, dictionaries, staging, dictionary_cache=None):
    staging.mkdir(parents=True)
    spec = importlib.util.spec_from_file_location("app_resources", ROOT / "tools_local/build_app_resources.py")
    apps = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(apps)
    apps.build(staging / "apps")
    language_packs.bundle(staging / "languages")
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
            ifo = next(directory.glob("*.ifo"))
            shutil.copy2(ifo, directory / "SOURCE-METADATA.txt")
            if dictionaries.resolve() != DEFAULT_DICTIONARIES.resolve():
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
    expansions = []
    if dictionaries.resolve() == DEFAULT_DICTIONARIES.resolve():
        expansions = enrich_dictionaries.enrich(staging / "dictionaries", dictionary_cache or ROOT / "datos.local/dictionary-sources")
    catalog["dictionaryExpansions"] = expansions
    if expansions:
        with (staging / "THIRD-PARTY-NOTICES.txt").open("a", encoding="utf-8") as notices:
            notices.write("\nDictionary usage refinement: wordfreq 3.1.1 / Robyn Speer; Apache-2.0 library, "
                          "CC BY-SA 4.0 data. Desktop preparation only. Each dictionary includes "
                          "ATTRIBUTION-wordfreq.txt with original data-source credits and LICENSE-wordfreq.txt.\n")
    dictionary_notices(staging, expansions)
    for entry in catalog["files"]:
        path = staging / entry["path"]
        if not path.exists() and path.suffix == ".dict" and Path(str(path) + ".dz").is_file():
            entry["sourcePath"] = entry["path"]
            entry["path"] += ".dz"
    for result in expansions:
        folder = staging / "dictionaries" / result["folder"]
        for path in sorted(folder.iterdir()):
            if not path.is_file():
                continue
            relative = path.relative_to(staging).as_posix()
            entry = next((entry for entry in catalog["files"] if entry["path"] == relative), None)
            if entry is None:
                entry = {"path": relative}
                catalog["files"].append(entry)
            entry.update(bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                         expansionSource=result["url"], expansionSourceSha256=result["sha256"],
                         expansionLicense=result["license"], modified=True)
    # Refresh hashes after generated metadata and notices have reached their final form.
    for entry in catalog["files"]:
        path = staging / entry["path"]
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != entry["sha256"]:
            entry.setdefault("sourceSha256", entry["sha256"])
            entry.update(bytes=path.stat().st_size, sha256=digest, modified=True)
    (staging / "SOURCES.json").write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    dictionary_description = (
        "Includes English and Spanish definitions, plus English <-> Spanish, French, German\n"
        "and Portuguese translations (10 StarDict dictionaries in total).\n"
        if dictionaries.resolve() == DEFAULT_DICTIONARIES.resolve()
        else "Includes a custom StarDict dictionary collection selected by the package builder.\n")
    (staging / "README.txt").write_text(
        "CROSSNEXT - MICROSD RESOURCES\n\n"
        "Extract this ZIP and copy apps, fonts, languages and dictionaries to the ROOT of your microSD.\n"
        "Select microSD UI languages in Settings > System > Language; see languages/README.txt.\n"
        "Back up custom fonts and dictionaries before updating. Leave your images folder unchanged. Replace the old bundled resource\n"
        "folders with these complete copies; do not merge old and new packs. Keep books and /.crosspoint/\n"
        "reading/app data. Restore custom resources separately without restoring old bundled packs.\n"
        "Do not copy the resources parent folder or format the card.\n"
        "Power off before removing the card; restart the reader after installing resources.\n\n"
        "Includes PET, The Intercept and Reading Adventure artwork, plus 14 font families:\n"
        "Bitter, Lexend Deca, Literata,\n"
        "Atkinson Hyperlegible, Merriweather, EB Garamond, PT Serif, Inter, Noto Serif, Ubuntu,\n"
        "Noto Serif JP, Noto Serif SC, Noto Serif TC and Noto Serif Thai.\n"
        "The JP/SC/TC/Thai reading faces stream from microSD on PSRAM devices.\n"
        "Thai uses prepared regular/bold files and requires the matching CrossNext firmware.\n"
        "Install the complete fonts/Noto Serif folder for the default reading font.\n"
        "Its compact interface version remains built into the firmware.\n"
        "Ubuntu reading sizes use fonts/Ubuntu; the resident interface fallback is retained.\n"
        + dictionary_description +
        "EN/ES interface languages automatically choose installed definition/translation defaults.\n"
        "Dictionary entries use compact numbered meanings, synonyms and examples when supplied by the source.\n"
        "The eight bundled WikDict translators show destination-language equivalents and categories, grouped by sense.\n"
        "Monolingual source meanings are combined with duplicate meanings removed; examples are retained.\n"
        "Bilingual entries use qualified WikDict database senses, ranked by usage labels and source importance.\n"
        "Contexts remain in the source language to distinguish meanings accurately; equivalents use the target language.\n"
        "Selected independently authored corrections replace known faulty translations, listed in each folder.\n"
        "Usage refinement removes redundant very-low-frequency alternatives within a sense; it preserves\n"
        "ordinary equivalent order, sole translations, compounds, qualified usages, names and examples.\n"
        "Historical usages are labeled. Desktop-only wordfreq credits and source terms accompany each dictionary.\n"
        "This improves the conversion and known errors; it does not certify every source entry as error-free.\n"
        "Web styles and pronunciation/etymology metadata are omitted.\n"
        "Manual dictionary selections, including None, take precedence.\n"
        "App resources: apps/reading-adventure-v2, apps/pet-v2 and apps/intercept.\n"
        "Install compatible firmware for your device separately. This ZIP contains no firmware.\n"
        "These apps need their complete resource folders to work. If resources are unavailable,\n"
        "replace the indicated app folder with the complete folder from this ZIP.\n"
        "Saved progress stays in .crosspoint. This package contains no saves and never replaces it.\n"
        "Keep your existing books, settings, fonts and dictionaries.\n\n"
        "Keep THIRD-PARTY-NOTICES.txt, SOURCES.json, licenses and the per-folder notices.\n"
        "Fonts: SIL OFL 1.1; Ubuntu: Ubuntu Font Licence 1.0. Dictionary source terms: CC BY-SA 4.0 and other terms in each folder's notices.\n"
        "Optional xxyzz CSS: GPL 3 or later.\n"
        "See CUSTOMIZATION.txt to customize artwork or add fonts and dictionaries.\n",
        encoding="utf-8")
    files = [p for p in sorted(staging.rglob("*")) if p.is_file() and p != staging / "MANIFEST.json"]
    if any(path.relative_to(staging).parts[0] == "images" for path in files):
        raise ValueError("The images folder is user-owned and must not be distributed")
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



def build(output, dictionaries, staging_target=None, dictionary_cache=None):
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
        assemble(archive, dictionaries, staging, dictionary_cache)
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
    parser.add_argument("--dictionary-cache", type=Path, default=ROOT / "datos.local/dictionary-sources")
    args = parser.parse_args()
    build(args.output, args.dictionaries, dictionary_cache=args.dictionary_cache)
