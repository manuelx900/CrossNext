"""Validate QA language resources against the actual translation sources."""
from pathlib import Path
import importlib.util
import json
import struct
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools_local/quest"))
from simulator_languages import export_languages

spec = importlib.util.spec_from_file_location("packs", ROOT / "tools_local/language_packs.py")
packs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packs)
reference = packs.reference()
active = packs.i18n.find_used_string_keys([str(ROOT / "src"), str(ROOT / "lib")])
with tempfile.TemporaryDirectory(prefix="quest-languages-") as folder:
    output = Path(folder)
    locales = export_languages(output)
    assert len(locales) == 39
    assert len({item["code"] for item in locales}) == 39
    assert sum(not item["fictional"] for item in locales) == 38
    sources = {}
    for path in (ROOT / "lib/I18n/translations").glob("*.yaml"):
        values = packs.i18n.parse_yaml_file(str(path))
        sources[values["_bcp47"]] = values
    for item in locales:
        values = sources[item["code"]]
        assert item["setting"] == values["_language_code"]
        assert item["name"] == values["_language_name"]
        if item["setting"] in ("EN", "ES"):
            assert item["pack"] is None
        else:
            blob = (output / "languages" / item["pack"]).read_bytes()
            magic, version, flags, count, length, crc, code, name = packs.HEADER.unpack_from(blob)
            assert magic == packs.MAGIC and version == 1 and flags == 0
            assert zlib.crc32(blob[packs.HEADER.size:]) == crc
            assert code.rstrip(b"\0").decode() == item["code"]
            assert name.rstrip(b"\0").decode() == item["name"]
            assert len(blob) == packs.HEADER.size + count * packs.ENTRY.size + length
            index = {}
            data = packs.HEADER.size + count * packs.ENTRY.size
            for n in range(count):
                key, offset, size = packs.ENTRY.unpack_from(blob, packs.HEADER.size + n * packs.ENTRY.size)
                assert offset + size < length and blob[data + offset + size] == 0
                index[key] = blob[data + offset:data + offset + size].decode("utf8")
            for key, value in values.items():
                if key.startswith("STR_QUEST_") and key in active and key in reference and not item["fictional"]:
                    assert index.get(packs.i18n.language_key_hash(key), reference[key]) == value, (item["code"], key)
        region = "_SC" if item["code"] == "zh-CN" else "_TC" if item["code"] == "zh-TW" else ""
        assert item["files"][:3] == [f"fonts/Interface{region}_{n}.cpfont" for n in (8, 10, 12)]
        for relative in item["files"]:
            resource = output / "languages" / relative
            assert resource.is_file() and resource.stat().st_size > 0
        if item["code"] in ("zh-CN", "zh-TW"):
            assert any(path.endswith(".bin") for path in item["files"])
    assert (output / "languages/fonts/licenses/NotoSans.txt").is_file()
print("39 locale exports passed: source values, pack hashes/CRC, settings, regional fonts and input resources.")
