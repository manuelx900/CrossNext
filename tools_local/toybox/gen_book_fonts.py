"""Generate the 10pt book-list cuts from the bundled Noto Sans sources."""
import contextlib
import ctypes
import io
from pathlib import Path
import re
import runpy
import sys
import tempfile

import freetype
from fontTools import subset

root = Path(__file__).resolve().parents[2]
TEXT_STROKE_64 = 8  # One eighth pixel, applied before the one-bit rasterization.


def convert(font, name, stroke_64):
    original_load = freetype.Face.load_glyph
    original_argv = sys.argv

    def load_with_stroke(face, index, flags=freetype.FT_LOAD_RENDER):
        original_load(face, index, flags & ~freetype.FT_LOAD_RENDER)
        slot = face.glyph
        if slot.format == freetype.FT_GLYPH_FORMAT_OUTLINE:
            error = freetype.FT_Outline_EmboldenXY(ctypes.byref(slot.outline._FT_Outline), stroke_64, stroke_64)
            if error:
                raise freetype.FT_Exception(error)
        if flags & freetype.FT_LOAD_RENDER:
            slot.render(freetype.FT_RENDER_MODE_NORMAL)

    output = io.StringIO()
    try:
        # Keep the shared converter's metrics/kerning path; adjust outlines only.
        if stroke_64:
            freetype.Face.load_glyph = load_with_stroke
        sys.argv = ["fontconvert.py", name, "10", str(font), "--force-autohint"]
        with contextlib.redirect_stdout(output):
            runpy.run_path(str(root / "lib/EpdFont/scripts/fontconvert.py"), run_name="__main__")
        return output.getvalue()
    finally:
        freetype.Face.load_glyph = original_load
        sys.argv = original_argv


with tempfile.TemporaryDirectory() as temporary:
    for weight, name, stroke_64 in (("Regular", "book_text_10", TEXT_STROKE_64), ("Bold", "book_bold_10", 0)):
        font = Path(temporary) / f"book-{weight}.ttf"
        subset.main([
            str(root / f"lib/EpdFont/builtinFonts/source/NotoSans/NotoSans-{weight}.ttf"),
            "--unicodes=U+0020-007E,U+00A0-00FF,U+2013-2014,U+2018-2019,U+2026",
            "--output-file=" + str(font),
        ])
        source = re.sub(r" \* Command used:.*",
                        " * Source: tools_local/toybox/gen_book_fonts.py (Noto Sans, SIL OFL)",
                        convert(font, name, stroke_64))
        source = "\n".join(line for line in source.splitlines() if not line.endswith("extracted")) + "\n"
        (root / f"src/apps_local/ui/fonts/{name}.h").write_text(source, encoding="utf-8")
