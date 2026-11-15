import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
import struct

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools_local'))
from shape_language_thai import add_clusters, cluster_codepoints
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont


class ThaiInterfaceFontTest(unittest.TestCase):
    def test_all_catalogue_clusters_survive_serialized_font(self):
        import language_packs
        texts = []
        for path in (ROOT / 'lib/I18n/translations').glob('*.yaml'):
            texts.extend(str(v) for k, v in language_packs.i18n.parse_yaml_file(str(path)).items()
                         if k.startswith('STR_'))
        # The font builder assigns shared cluster IDs after including the Wordle bank.
        bank = ROOT / 'assets_local/wordle-languages/TH.cpwb'
        if bank.exists():
            data = bank.read_bytes()
            header, stride = (64, 108) if int.from_bytes(data[4:6], 'little') == 2 else (32, 104)
            texts.extend(data[at:at + 97].split(b'\0', 1)[0].decode('utf-8')
                         for at in range(header, len(data), stride))
        clusters = cluster_codepoints(texts, input_alphabet=(ROOT / 'assets_local/language-keyboards/SOURCE.json').exists())
        fonts = list((ROOT / 'assets_local/language-fonts').glob('*.cpfont'))
        if not fonts:
            self.skipTest('Build the language fonts before this binary integration check')
        for path in fonts:
            data = path.read_bytes()
            version = struct.unpack_from('<H', data, 8)[0]
            for style in range(data[12]):
                toc = 32 + 32 * style
                intervals, glyphs = struct.unpack_from('<II', data, toc + 4)
                kern_l, kern_r = struct.unpack_from('<HH', data, toc + 17)
                count = struct.unpack_from('<H', data, toc + 28)[0] if version == 5 else data[toc + 23]
                offset = struct.unpack_from('<I', data, toc + 24)[0]
                offset += intervals * 12 + glyphs * 16 + (kern_l + kern_r) * 3
                offset += data[toc + 21] * data[toc + 22]
                pairs = dict(struct.unpack_from('<II', data, offset + i * 8) for i in range(count))
                for text, result in clusters.items():
                    cp = ord(text[0])
                    for char in text[1:]:
                        cp = pairs.get(cp << 16 | ord(char), 0)
                    self.assertEqual(cp, result, (path.name, style, text))

    def test_cluster_prefixes_allow_existing_pair_lookup(self):
        clusters = cluster_codepoints(['ที่ เริ่ม ตั้งค่า เพิ่ม น้ำ'])
        self.assertIn('ที', clusters)
        self.assertIn('ที่', clusters)
        self.assertIn('น้', clusters)
        self.assertIn('น้ำ', clusters)
        self.assertEqual(len(clusters.values()), len(set(clusters.values())))
        self.assertEqual(clusters, cluster_codepoints(['น้ำ เพิ่ม ตั้งค่า เริ่ม ที่']))

    def test_shaped_cluster_is_encoded_as_chained_ligatures(self):
        source = ROOT / 'datos.local/languages-complete/font-source/NotoSansThai.ttf'
        if not source.exists():
            self.skipTest('Download the pinned language font sources before this integration check')
        clusters = cluster_codepoints(['ที่ น้ำ เริ่ม ตั้งค่า เพิ่ม'])
        font = TTFont(source)
        axes = {axis.axisTag: axis.defaultValue for axis in font['fvar'].axes}
        axes['wght'] = 400
        font = instantiateVariableFont(font, axes, inplace=True)
        add_clusters(font, source, 400, clusters)
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'thai.ttf'
            font.save(path)
            spec = importlib.util.spec_from_file_location('converter', ROOT / 'lib/EpdFont/scripts/fontconvert_sdcard.py')
            converter = importlib.util.module_from_spec(spec)
            sys.path.insert(0, str(ROOT / 'lib/EpdFont/scripts'))
            spec.loader.exec_module(converter)
            cps = set(font.getBestCmap())
            pairs = dict(converter.extract_ligatures_fonttools(str(path), cps))
            for text, result in clusters.items():
                cp = ord(text[0])
                for char in text[1:]:
                    cp = pairs.get(cp << 16 | ord(char), 0)
                self.assertEqual(cp, result, text)
            # The stacked vowel+tone needs more vertical ink than the vowel alone.
            vowel = font['glyf'][font.getBestCmap()[clusters['ที']]]
            stacked = font['glyf'][font.getBestCmap()[clusters['ที่']]]
            self.assertGreater(stacked.yMax, vowel.yMax)
        font.close()


if __name__ == '__main__':
    unittest.main()
