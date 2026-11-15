"""Check independently specified everyday equivalents in the actual generated pack."""
import argparse
import gzip
import json
from pathlib import Path
import re
import struct
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools_local"))
from compact_dictionaries import Entry, Node
from enrich_dictionaries import ARCHIVES, records


def equivalent_text(body):
    tree = Entry()
    tree.feed(body)
    return " ".join(node.text() for node in tree.root.children if isinstance(node, Node)
                    and not (len(node.children) == 1 and isinstance(node.children[0], Node)
                             and node.children[0].tag == "i"))


def check(root):
    checks = json.loads(Path(__file__).with_name("natural-equivalents.json").read_text(encoding="utf-8"))
    failures, count = [], 0
    for pair, folder, _ in ARCHIVES:
        if pair not in checks:
            continue
        ifo = next((root / "dictionaries" / folder).glob("*.ifo"))
        wanted = {word.encode("utf-8").lower(): (word, expected) for word, expected in checks[pair].items()}
        with tempfile.TemporaryFile() as data, gzip.open(ifo.with_suffix(".dict.dz"), "rb") as source:
            while chunk := source.read(1024 * 1024):
                data.write(chunk)
            for name, location in records(ifo.with_suffix(".idx"), 8):
                if name.lower() not in wanted:
                    continue
                word, expected = wanted.pop(name.lower())
                offset, size = struct.unpack(">II", location)
                data.seek(offset)
                text = equivalent_text(data.read(size).decode("utf-8"))
                if not re.search(r"(?<!\w)" + re.escape(expected) + r"(?!\w)", text, re.I):
                    failures.append((pair, word, expected, text))
                count += 1
        failures.extend((pair, word, expected, "MISSING HEADWORD") for word, expected in wanted.values())
    for failure in failures:
        print(json.dumps(failure, ensure_ascii=False))
    if failures:
        raise AssertionError(f"{len(failures)} sample failures")
    print(f"{count} everyday equivalent checks passed in eight generated translators.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resources", type=Path)
    check(parser.parse_args().resources)
