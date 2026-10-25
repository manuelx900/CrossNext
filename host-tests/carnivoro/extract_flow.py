"""Exercise production navigation and note actions against a bounded host activity."""
from pathlib import Path
import sys

source = Path('src/apps_local/carnivoro/CarnivoroActivity.cpp').read_text()
start = source.index('void CarnivoroActivity::changed()')
end = source.index('void CarnivoroActivity::screen(', start)
Path(sys.argv[1]).write_text(source[start:end])
