#!/usr/bin/env python3
"""Check lossless sprite migration, damaged packs and safe ZIP installation."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[2]


def run(compiler, wasm=False):
    output = ROOT / "qa-artifacts/quest-sd-resources/tests"
    output.mkdir(parents=True, exist_ok=True)
    binary = output / ("test-pack.cjs" if wasm else "test-pack.exe")
    command = [str(compiler)]
    if compiler.name.lower().startswith("zig"):
        command += ["c++", "-Wno-nullability-completeness"]
    command += ["-std=c++17", "-Wall", "-Wextra", "-Werror"]
    command += ["-I" + str(ROOT / p) for p in (
        "host-tests/sdart/stubs", "freeink-sdk/libs/ui/FreeInkUI/include", "src/apps_local/quest")]
    command += [str(ROOT / p) for p in ("host-tests/sdart/test_pack.cpp", "src/apps_local/ui/SdBitmapPack.cpp",
                                       "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp")]
    if wasm: command += ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1", "-sALLOW_MEMORY_GROWTH=1"]
    command += ["-o", str(binary)]
    subprocess.run(command, cwd=ROOT, check=True)
    pack = ROOT / "resources/apps/reading-adventure/graphics.bin"
    original = pack.read_bytes()
    cases = {"magic": b"BADMAGIC" + original[8:], "truncated": original[:-1],
             "checksum": original[:-1] + bytes([original[-1] ^ 1]),
             "trailing": original + b"extra"}
    for label, offset, value, fmt in (("zero-width", 28, 0, "<H"),
                                      ("large-width", 28, 65535, "<H"),
                                      ("bad-offset", 24, 0xFFFFFFFF, "<I"),
                                      ("bad-length", 32, 0xFFFFFFFF, "<I")):
        data = bytearray(original)
        struct.pack_into(fmt, data, offset, value)
        struct.pack_into("<I", data, 16, zlib.crc32(data[24:]))
        if original[:8] == b"CNART002":
            struct.pack_into("<I", data, 20, zlib.crc32(data[24:24+900*16]))
        cases[label] = data
    paths = []
    for name, data in cases.items():
        path = output / (name + ".bin")
        path.write_bytes(data)
        paths.append(str(path))
    paths.append(str(output / "missing.bin"))
    subprocess.run([*(["node"] if wasm else []), str(binary), str(pack), *paths], cwd=ROOT, check=True)
    archive = ROOT / "dist/CrossNext-Resources.zip"
    with zipfile.ZipFile(archive) as zipped:
        names = zipped.namelist()
        assert not any(name.startswith(("/", "resources/")) or ".." in Path(name).parts or
                       ".crosspoint" in Path(name).parts for name in names)
        manifest = json.loads(zipped.read("MANIFEST.json"))
        assert manifest["apps"] == {name: "included" for name in ("reading-adventure", "pet", "intercept")}
        assert {name.split("/")[1] for name in names if name.startswith("apps/")} == {"reading-adventure", "pet", "intercept"}
        assert {path.name for path in (ROOT / "resources/apps").iterdir()} == {"reading-adventure", "pet", "intercept"}
        for entry in manifest["files"]:
            data = zipped.read(entry["path"])
            assert len(data) == entry["bytes"] and hashlib.sha256(data).hexdigest() == entry["sha256"]
        card = output / "card"
        save = card / ".crosspoint/reading-quest-v5.dat"
        save.parent.mkdir(parents=True, exist_ok=True)
        save.write_bytes(b"Existing user progress: do not replace")
        before = save.read_bytes()
        zipped.extractall(card)
        assert save.read_bytes() == before
        assert (card / "apps/reading-adventure/graphics.bin").read_bytes() == original
        assert len(list((ROOT / "resources/fonts").rglob("*.ttf"))) == 32
        assert len(list((ROOT / "resources/fonts").glob("*/OFL"))) == 8
        for folder in ("English Simple", "Spanish Wiktionary", "WikDict EN-ES", "WikDict ES-EN",
                       "WikDict EN-FR", "WikDict FR-EN", "WikDict EN-DE", "WikDict DE-EN",
                       "WikDict EN-PT", "WikDict PT-EN"):
            assert len(list((card / "dictionaries" / folder).glob("*.ifo"))) == 1
            assert (card / "dictionaries" / folder / "ATTRIBUTION.txt").is_file()
            assert (card / "dictionaries" / folder / "LICENSE-CC-BY-SA-4.0.txt").is_file()
    print("PASS: ZIP hashes and root layout, 32 font faces in 8 families, 10 dictionaries and licenses, existing progress retained")

    spec = importlib.util.spec_from_file_location("resource_builder", ROOT / "tools_local/build_resources.py")
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    for suffix in ("dict", "dict.dz"):
        dictionaries = output / ("source-" + suffix) / "Example"
        dictionaries.mkdir(parents=True, exist_ok=True)
        (dictionaries / "example.ifo").write_text("StarDict's dict ifo file\nversion=3.0.0\n")
        (dictionaries / "example.idx").write_bytes(b"updated index " + suffix.encode())
        (dictionaries / ("example." + suffix)).write_bytes(b"test dictionary " + suffix.encode())
        builder.build(output / "rebuild.zip", dictionaries.parent, output / "resources")
    with zipfile.ZipFile(output / "rebuild.zip") as rebuilt:
        assert "dictionaries/Example/example.dict.dz" in rebuilt.namelist()
        assert "dictionaries/Example/example.dict" not in rebuilt.namelist()
    assert not (output / "resources/dictionaries/Example/example.dict").exists()
    print("PASS: rebuilding with changed dictionary inputs removes obsolete files from staging and ZIP")

    empty = output / "source-empty"
    empty.mkdir(exist_ok=True)
    builder.build(output / "empty-dictionaries.zip", empty, output / "resources")
    with zipfile.ZipFile(output / "empty-dictionaries.zip") as minimal:
        assert not any(name.startswith("dictionaries/") for name in minimal.namelist())
        assert "apps/reading-adventure/graphics.bin" in minimal.namelist()
        assert "fonts/Inter/Inter-Regular.ttf" in minimal.namelist()
    print("PASS: an empty custom dictionary collection still builds artwork and fonts")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=Path(shutil.which("c++") or shutil.which("g++") or "c++"))
    parser.add_argument("--wasm", action="store_true")
    args = parser.parse_args()
    run(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe" if args.wasm else args.compiler, args.wasm)
