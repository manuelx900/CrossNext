#!/usr/bin/env python3
"""Canonical entry point for Reading Adventure's CNART003 grayscale resources."""
import argparse
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def build(output):
    # The complete SD builder imports this entry point by filename.
    sys.path.insert(0, str(Path(__file__).parent))
    try:
        spec = importlib.util.spec_from_file_location("quest_gray_art", Path(__file__).with_name("build_gray_art.py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    finally:
        sys.path.pop(0)
    return module.build(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "resources/apps/reading-adventure-v2/graphics.bin")
    build(parser.parse_args().output)
