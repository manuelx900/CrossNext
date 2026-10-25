#!/usr/bin/env python3
"""Exercise automatic dictionary ownership and validate the distributed resources."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def validate_resources():
    from fontTools.ttLib import TTFont

    resources = ROOT / "resources"
    sources = json.loads((resources / "SOURCES.json").read_text("utf-8"))["files"]
    for entry in sources:
        data = (resources / entry["path"]).read_bytes()
        assert hashlib.sha256(data).hexdigest() == entry["sha256"], entry["path"]
    covered = {entry["path"] for entry in sources}
    families = list((resources / "fonts").iterdir())
    assert len(families) == 8
    for family in families:
        assert "SIL OPEN FONT LICENSE Version 1.1" in (family / "OFL").read_text("utf-8")
        faces = list(family.glob("*.ttf"))
        assert len(faces) == 4
        roles = set()
        for path in faces:
            assert path.relative_to(resources).as_posix() in covered
            with TTFont(path) as font:
                roles.add((font["OS/2"].usWeightClass, bool(font["OS/2"].fsSelection & 1)))
                assert "fvar" not in font, path
                chars = font.getBestCmap()
                for ch in "English español français Deutsch português áéíóúñüàâçèêëîïôùûÿäößãõ":
                    assert ord(ch) in chars, (path, ch)
        regular_weight = 500 if family.name == "Bitter" else 400
        assert roles == {(regular_weight, False), (700, False), (regular_weight, True), (700, True)}, family
    dictionaries = list((resources / "dictionaries").iterdir())
    assert len(dictionaries) == 10
    total = 0
    for directory in dictionaries:
        assert len(directory.name.encode("utf-8")) < 32
        ifo, = directory.glob("*.ifo")
        metadata = dict(line.split("=", 1) for line in ifo.read_text("utf-8").splitlines() if "=" in line)
        assert metadata.get("idxoffsetbits", "32") == "32"
        index = ifo.with_suffix(".idx").read_bytes()
        assert len(index) == int(metadata["idxfilesize"])
        plain = ifo.with_suffix(".dict")
        data = plain.read_bytes() if plain.exists() else gzip.decompress(ifo.with_suffix(".dict.dz").read_bytes())
        position = count = 0
        while position < len(index):
            end = index.index(0, position)
            index[position:end].decode("utf-8")
            offset, size = struct.unpack_from(">II", index, end + 1)
            assert offset + size <= len(data), (directory, count)
            position = end + 9
            count += 1
        assert position == len(index) and count == int(metadata["wordcount"])
        synonyms = ifo.with_suffix(".syn")
        if synonyms.exists():
            raw = synonyms.read_bytes()
            position = synonym_count = 0
            while position < len(raw):
                end = raw.index(0, position)
                raw[position:end].decode("utf-8")
                target, = struct.unpack_from(">I", raw, end + 1)
                assert target < count, (directory, synonym_count)
                position = end + 5
                synonym_count += 1
            assert position == len(raw) and synonym_count == int(metadata.get("synwordcount", "0"))
        assert (directory / "ATTRIBUTION.txt").is_file()
        assert (directory / "LICENSE-CC-BY-SA-4.0.txt").is_file()
        for path in directory.iterdir():
            if path.suffix in (".ifo", ".idx", ".dict", ".dz", ".syn", ".css"):
                assert path.relative_to(resources).as_posix() in covered, path
        if list(directory.glob("*.css")):
            assert (directory / "LICENSE-GPL-3.0.txt").is_file()
        total += count
        print(f"PASS {directory.name}: {count:,} entries, index offsets and synonyms valid")
    print(f"PASS 8 font families / 32 static faces, five-language glyph coverage; {total:,} dictionary entries; source hashes/licenses")


def run(compiler):
    output = ROOT / "qa-artifacts/dictionary-defaults/test-defaults.exe"
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [str(compiler)]
    if compiler.name.lower().startswith("zig"):
        command += ["c++", "-Wno-nullability-completeness"]
    command += ["-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + str(ROOT / "host-tests/dictionary-defaults/stubs"), "-I" + str(ROOT / "src"),
                str(ROOT / "host-tests/dictionary-defaults/test_defaults.cpp"),
                str(ROOT / "src/apps_local/words/DictionaryDefaults.cpp"), "-o", str(output)]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(output)], cwd=ROOT, check=True)
    print("PASS EN/ES defaults, stable reboot, missing/partial resources, manual choices and None, legacy folder, other languages")
    validate_resources()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    run(parser.parse_args().compiler)
