"""Verify the new resource layout, save compatibility and rendered UI on Windows or a native host."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / ".cache/quest-redesign"


def run():
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / "host-tests/quest-campaign/test_art.py")], cwd=ROOT, check=True)
    compiler = str(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe") if os.name == "nt" else os.environ.get("CXX", "c++")
    flags = ["-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-comment"]
    runtime = [shutil.which("node")] if os.name == "nt" else []
    if os.name == "nt":
        flags += ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sALLOW_MEMORY_GROWTH=1", "-sEXIT_RUNTIME=1", "-sSTACK_SIZE=1048576"]
    def check(name, sources, includes):
        output = OUT / (name + (".cjs" if os.name == "nt" else ""))
        subprocess.run([compiler, *flags, *["-I" + i for i in includes], *sources, "-o", str(output)], cwd=ROOT, check=True)
        subprocess.run([*runtime, str(output), *(["--quest"] if name == "study-ui" else [])], cwd=ROOT, check=True)
    common = ["src/apps_local/quest/QuestCore.cpp", "src/apps_local/quest/QuestCampaign.cpp", "src/apps_local/reading/ReadingStatsCore.cpp"]
    check("quest-state", ["host-tests/quest-redesign/test_state.cpp", "src/apps_local/quest/QuestStore.cpp", *common],
          ["host-tests/study-suite/stubs"])
    bidi = OUT / "minibidi.o"
    subprocess.run([compiler, "-x", "c", "-c", "lib/MiniBidi/minibidi.c", "-o", str(bidi)], cwd=ROOT, check=True)
    ui = ["freeink-sdk/libs/ui/FreeInkUI/include", "freeink-sdk/libs/assets/Icons/include", "lib/EpdFont", "lib/I18n", "lib/Utf8", "lib/MiniBidi", "lib/Memory", "host-tests/sdart/stubs", "src/apps_local/quest"]
    check("quest-scene", ["host-tests/quest-redesign/test_scene.cpp", "src/apps_local/ui/SdBitmapPack.cpp", *common,
                           "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"], ui)
    check("quest-expansion-scene", ["host-tests/quest-redesign/test_expansion_scene.cpp", "src/apps_local/ui/SdBitmapPack.cpp", *common,
                                     "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"], ui)
    # Built-in translation tables are the UI fixture; installed language packs are out of scope here.
    stub = OUT / "language-pack-fixture.cpp"
    stub.write_text('#include "LanguagePack.h"\nnamespace languagepack { const char* Pack::get(size_t) const { return nullptr; } }\n', encoding="utf-8")
    check("study-ui", ["host-tests/study-suite/test_screens.cpp", "src/apps_local/tools/ToolsScreens.cpp",
          "src/apps_local/quest/QuestScreens.cpp", "src/apps_local/quest/QuestJourneyScreens.cpp", *common, "src/apps_local/intercept/InterceptScreens.cpp",
          "src/apps_local/intercept/InterceptCore.cpp", "src/apps_local/intercept/InterceptData.cpp",
          "src/apps_local/ui/PixelMenu.cpp", "src/apps_local/ui/SdBitmapPack.cpp", "lib/I18n/I18n.cpp",
          "lib/I18n/I18nStrings.cpp", "lib/Utf8/Utf8.cpp", "lib/MiniBidi/BidiUtils.cpp", str(bidi), str(stub), "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"], ui)
    path = ROOT / "resources/apps/reading-adventure/graphics.bin"
    data = path.read_bytes()
    magic, count, size, checksum, index_crc = struct.unpack_from("<8sIIII", data)
    assert magic == b"CNART003" and count == 1103 and size == len(data)
    assert zlib.crc32(data[24:]) == checksum and zlib.crc32(data[24:24+count*16]) == index_crc
    records = [struct.unpack_from("<IHHII", data, 24+n*16) for n in range(count)]
    for offset, width, height, length, crc in records:
        assert 0 < width <= 280 and 0 < height <= 512 and length == ((width*2+7)//8)*height
        assert zlib.crc32(data[offset:offset+length]) == crc
    for n in range(200):
        ink, mask = records[n], records[200+n]
        assert ink[1:3] == mask[1:3] and 128 <= mask[1] <= 200 and mask[2] == 208
        offset, width, height, length, _ = mask
        raw = data[offset:offset+length]
        stride = (width*2+7)//8
        occupied = [y for y in range(height) if any(raw[y*stride:(y+1)*stride])]
        assert occupied[-1] - occupied[0] + 1 >= 203, f"Hero {n} shrank to fit a wide outfit"
    assert all(records[n][1:3] == (240, 320) for n in range(1029,1065))
    for n in range(50):
        assert records[600+n][1:3] == records[650+n][1:3] == (240,320)
        assert records[901+n][1:3] == (112,144)
        offset, _, _, length, _ = records[901+n]
        raw = data[offset:offset+length]
        assert sum((byte >> shift) & 3 == 3 for byte in raw for shift in (0,2,4,6)) >= 100
    assert len(set(data[records[n][0]:records[n][0]+records[n][3]] for n in range(200))) == 200
    assert len(set(data[records[400+n][0]:records[400+n][0]+records[400+n][3]] for n in range(50))) == 50
    assert len(set(data[records[901+n][0]:records[901+n][0]+records[901+n][3]] for n in range(50))) == 50
    assert len(set(data[records[951+n][0]:records[951+n][0]+records[951+n][3]] for n in range(39))) == 39
    for n in range(39):
        ink, mask = records[951+n], records[990+n]
        assert ink[1:3] == mask[1:3] == (248, 280)
        a, b = data[ink[0]:ink[0]+ink[3]], data[mask[0]:mask[0]+mask[3]]
        assert all((black >> shift & 3) == 0 or (solid >> shift & 3) > 0
                   for black, solid in zip(a, b) for shift in (0, 2, 4, 6))
    for n in range(3):
        ink, mask = records[1065+n], records[1068+n]
        assert ink[1:3] == mask[1:3] == (192,192)
        a,b=data[ink[0]:ink[0]+ink[3]],data[mask[0]:mask[0]+mask[3]]
        assert any(a) and all((v >> shift & 3)==0 or (m >> shift & 3)==3
                             for v,m in zip(a,b) for shift in (0,2,4,6))
    assert len(set(data[records[n][0]:records[n][0]+records[n][3]] for n in range(1065,1068))) == 3
    for n in range(16):
        ink,mask=records[1071+n],records[1087+n]
        assert ink[1:3] == mask[1:3] == (112,144)
        a,b=data[ink[0]:ink[0]+ink[3]],data[mask[0]:mask[0]+mask[3]]
        assert any(a) and all((v>>shift&3)==0 or (m>>shift&3)==3
                             for v,m in zip(a,b) for shift in (0,2,4,6))
    assert len(set(data[records[n][0]:records[n][0]+records[n][3]] for n in range(1071,1087)))==16
    assert len(data) <= 8*1024*1024
    story = (path.parent / "story.bin").read_bytes()
    magic, count, size, checksum, index_crc = struct.unpack_from("<8sIIII", story)
    assert magic == b"CNART003" and count == 240 and size == len(story) == 4611864
    assert zlib.crc32(story[24:]) == checksum and zlib.crc32(story[24:24+count*16]) == index_crc
    pairs = []
    for n in range(count):
        offset, width, height, length, crc = struct.unpack_from("<IHHII", story, 24+n*16)
        assert (width,height,length) == (240,320,19200)
        tile=story[offset:offset+length]
        assert len(tile)==length and zlib.crc32(tile)==crc
        pairs.append(tile)
    assert len({pairs[n]+pairs[n+1] for n in range(0,240,2)})==120
    print("Path story: 120 distinct full scenes, 240 bounded tiles, all checksums passed")
    backgrounds = json.loads((ROOT / "assets_local/quest/redesign/backgrounds.json").read_text("utf-8"))["items"]
    assert len(backgrounds) == 50 and backgrounds[0]["level"] == 1 and backgrounds[-1]["level"] == 100
    print("Quest resources: 1103 record checksums, 200 equal-height heroes, six prologue illustrations, twelve saga scenes, 50 readable background emblems and unique companions passed")


if __name__ == "__main__":
    run()
