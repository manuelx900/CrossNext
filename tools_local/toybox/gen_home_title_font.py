"""Generate the carousel's 18pt cut from the bundled Jersey 25 (SIL OFL).

Requires fonttools[woff] and freetype-py. Existing font cuts are left intact.
Source and license: site/assets/fonts/jersey25.woff2 and OFL-Jersey25.txt.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory() as temporary:
        source = Path(temporary) / "jersey25.ttf"
        font = TTFont(ROOT / "site/assets/fonts/jersey25.woff2")
        font.flavor = None
        font.save(source)
        options = subset.Options()
        selected = subset.Subsetter(options=options)
        selected.populate(unicodes=[*range(0x20, 0x7F), *range(0xA0, 0x100),
                                   0x2013, 0x2014, 0x2018, 0x2019, 0x2026])
        selected.subset(font)
        font.save(source)
        result = subprocess.run([sys.executable, str(ROOT / "lib/EpdFont/scripts/fontconvert.py"),
                                 "toybox_18", "18", str(source)], check=True,
                                capture_output=True, encoding="utf-8")
        output = "\n".join(line for line in result.stdout.splitlines() if not line.endswith("extracted")) + "\n"
        output = re.sub(r" \* Command used:.*", " * Source: tools_local/toybox/gen_home_title_font.py (Jersey 25, SIL OFL)", output)
        (ROOT / "src/apps_local/ui/fonts/toybox_18.h").write_text(output, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
