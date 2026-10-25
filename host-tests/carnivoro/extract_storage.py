"""Compile the production storage methods against a fault-injected host HAL.

The method bodies are copied verbatim, rather than reimplementing the save
protocol in the test. No generated file is used by the firmware build.
"""
from pathlib import Path
import sys

source = Path('src/apps_local/carnivoro/CarnivoroActivity.cpp').read_text()
start = source.index('CarnivoroActivity::ReadResult CarnivoroActivity::read(')
end = source.index('void CarnivoroActivity::changed()', start)
Path(sys.argv[1]).write_text(source[start:end])
