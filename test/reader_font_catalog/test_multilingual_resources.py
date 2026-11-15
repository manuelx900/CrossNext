"""Validate installed Chinese/Thai reader fonts, shaping and provenance."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import unittest

from fontTools.ttLib import TTFont
from PIL import ImageFont
import uharfbuzz as hb

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools_local"))
from build_multilingual_reader_fonts import FAMILIES, thai_pairs, verified_source
from shape_language_thai import cluster_codepoints

RES = ROOT / "assets_local/sd-resources"


class ReadingResources(unittest.TestCase):
    def test_hashes_coverage_and_weights(self):
        records = json.loads((RES / "font-sources.json").read_text(encoding="utf-8"))["files"]
        for family, _, name, *_ in FAMILIES:
            directory = RES / "fonts" / family
            with self.subTest(family=family):
                self.assertTrue((directory / "OFL.txt").is_file())
                for record in records:
                    if not record["path"].startswith("fonts/" + family + "/"):
                        continue
                    path = RES / record["path"]
                    self.assertEqual(path.stat().st_size, record["bytes"])
                    self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), record["sha256"])
                if family.endswith("Thai"):
                    continue
                path = directory / name
                font = TTFont(path, lazy=True)
                sample = "简体中文阅读书籍标点，。！？「图书馆」" if family.endswith("SC") else "繁體中文閱讀書籍標點，。！？「圖書館」"
                self.assertTrue(set(map(ord, sample)) <= set(font.getBestCmap()))
                self.assertGreater(len(font.getBestCmap()), 20000)
                for px in (25, 38, 63):
                    masks = []
                    for weight in (400, 700):
                        face = ImageFont.truetype(str(path), px)
                        face.set_variation_by_axes([weight])
                        masks.append(bytes(face.getmask(sample)))
                    self.assertNotEqual(*masks)
                font.close()

    def test_thai_cluster_chaining_and_source_metrics(self):
        clusters = cluster_codepoints([], input_alphabet=True)
        pairs = dict(thai_pairs(clusters))
        samples = ("เมื่อเช้านี้ฉันได้อ่านหนังสือที่บ้านแล้วจึงไปเที่ยวกับเพื่อน", "น้ำ กำลัง ปู่ ปี่ ผู้ ญู ฐุ กิ่ กี้ กิ๊ กิ๋ เกี๊ยะ")
        for weight, style in ((400, "Regular"), (700, "Bold")):
            font = TTFont(RES / "fonts/Noto Serif Thai" / f"NotoSerifThai-{style}.ttf")
            cmap = font.getBestCmap()
            self.assertTrue(set(map(ord, "".join(samples))) <= set(cmap))
            raw = font["CPLG"].data
            self.assertEqual(struct.unpack(">4sHH", raw[:8]), (b"CNLG", 1, len(clusters)))
            self.assertEqual(dict(struct.iter_unpack(">II", raw[8:])), pairs)
            self.assertEqual(font["OS/2"].usWeightClass, weight)
            self.assertNotIn("fvar", font)
            for text, cp in cluster_codepoints(samples).items():
                result = ord(text[0])
                for char in text[1:]:
                    result = pairs[(result << 16) | ord(char)]
                self.assertEqual(result, clusters[text])
            # Compare prepared advances with shaping the original variable
            # source, including Sara Am decomposition and stacked marks.
            _, folder, name, remote, digest, _ = FAMILIES[-1]
            source = verified_source(ROOT / "datos.local/noto-multilingual-2026-11-07/sources", folder, name, remote, digest)
            face = hb.Face(source.read_bytes())
            shaped_font = hb.Font(face)
            shaped_font.set_variations({"wght": weight, "wdth": 100})
            for text, cp in clusters.items():
                buffer = hb.Buffer(); buffer.add_str(text); buffer.guess_segment_properties(); buffer.language = "th"
                hb.shape(shaped_font, buffer)
                self.assertTrue(all(info.codepoint for info in buffer.glyph_infos), text)
                self.assertEqual(sum(p.x_advance for p in buffer.glyph_positions), font["hmtx"][cmap[cp]][0], text)
                self.assertGreater(font["glyf"][cmap[cp]].numberOfContours, 0, text)
            font.close()


if __name__ == "__main__":
    unittest.main()
