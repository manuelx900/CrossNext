"""Validate the shipped Intercept graphics with the real firmware pack reader."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wasm", action="store_true")
    args = parser.parse_args()
    output = ROOT / "qa-artifacts/intercept-resources-tests"
    output.mkdir(parents=True, exist_ok=True)
    original = (ROOT / "resources/apps/intercept/graphics.bin").read_bytes()
    manifest = json.loads((ROOT / "resources/apps/intercept/manifest.json").read_text("utf-8"))
    assert hashlib.sha256(original).hexdigest() == manifest["sha256"]
    assert len(manifest["images"]) == manifest["sprites"] == 49
    for image in manifest["images"]:
        offset, width, height, size, crc = struct.unpack_from("<IHHII", original, 24 + 16 * image["id"])
        assert (width, height) == (image["width"], image["height"])
        assert hashlib.sha256(original[offset:offset+size]).hexdigest() == image["pixelSha256"]
    index = bytearray(original)
    index[24] ^= 1
    payload = bytearray(original)
    payload[24 + 16 * 49] ^= 1
    paths = [output / "missing.bin", output / "truncated.bin", output / "index.bin", output / "pixels.bin"]
    for path, data in zip(paths[1:], (original[:-1], index, payload)):
        path.write_bytes(data)
    compiler = str(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe") if args.wasm else os.environ.get("CXX", "c++")
    flags = ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1"] if args.wasm else []
    binary = output / ("pack.cjs" if args.wasm else "pack")
    command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
               "-Ihost-tests/sdart/stubs", "-Ifreeink-sdk/libs/ui/FreeInkUI/include",
               "host-tests/intercept/test_pack.cpp", "src/apps_local/ui/SdBitmapPack.cpp",
               "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp", *flags, "-o", str(binary)]
    subprocess.run(command, cwd=ROOT, check=True)
    runtime = ["node"] if args.wasm else []
    subprocess.run([*runtime, str(binary), *map(str, paths)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
