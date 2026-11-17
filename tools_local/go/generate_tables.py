"""Regenerate or verify immutable Michi tables using its original C builders."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MICHI = ROOT / "src/apps_local/go/michi"
TABLES = MICHI / "MichiTables.inc"
UNITS = ["board", "board_util", "michi", "patterns", "params", "control", "MichiShim", "MichiBridge"]


def dump(out, compiler, runtime, reference):
    objects = []
    for unit in UNITS:
        obj = out / (unit + ("-reference" if reference else "-constant") + ".o")
        command = [compiler, "-std=c99", "-O2", "-w", "-I" + str(MICHI)]
        if reference:
            command += ["-DMICHI_GENERATE_TABLES=1"]
        subprocess.run([*command, "-c", str(MICHI / (unit + ".c")), "-o", str(obj)], check=True, cwd=ROOT)
        objects.append(str(obj))
    program = out / ("tables-reference" if reference else "tables-constant")
    if runtime:
        program = program.with_suffix(".cjs")
    elif os.name == "nt":
        program = program.with_suffix(".exe")
    subprocess.run([compiler, "-std=c99", "-O2", "-I" + str(MICHI),
                    str(ROOT / "tools_local/go/table_dump.c"), *objects, "-lm", "-o", str(program)],
                   check=True, cwd=ROOT)
    result = subprocess.run([*runtime, str(program)], check=True, capture_output=True, cwd=ROOT)
    return result.stdout.replace(b"\r\n", b"\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--emsdk", type=Path)
    args = parser.parse_args()
    compiler = os.environ.get("CC", "cc")
    runtime = []
    if args.emsdk:
        sdk = args.emsdk.resolve()
        compiler = str(sdk / "upstream/emscripten" / ("emcc.exe" if os.name == "nt" else "emcc"))
        node = sorted((sdk / "node").glob("*"))[-1] / ("node.exe" if os.name == "nt" else "bin/node")
        runtime = [str(node)]
    with tempfile.TemporaryDirectory(prefix="crossnext-go-tables-") as folder:
        out = Path(folder)
        reference = dump(out, compiler, runtime, True)
        if args.check:
            assert TABLES.read_bytes().replace(b"\r\n", b"\n") == reference, "Regenerate Michi tables"
        else:
            TABLES.write_bytes(reference)
        assert dump(out, compiler, runtime, False) == reference, "Frozen tables differ from C builders"
    print("Go tables: all 8192 pattern bytes and 1684 hashes match the original C builders")


if __name__ == "__main__":
    main()
