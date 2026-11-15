import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import unicodedata
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("packs", ROOT / "tools_local/language_packs.py")
packs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packs)


class LanguagePackCompilerTest(unittest.TestCase):
    def test_full_line_context_comments_do_not_change_quoted_copy(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "source.yaml"
            path.write_text('# File-transfer context\n  # A second comment\nSTR_TITLE: "# literal text"\n', encoding="utf8")
            self.assertEqual(packs.i18n.parse_yaml_file(str(path)), {"STR_TITLE": "# literal text"})

    def test_sparse_pack_crc_stable_hash_and_nfc(self):
        blob, warnings = packs.compile_pack({"STR_SETTINGS_TITLE": "Re\u0301glages"}, "fr", "Franc\u0327ais")
        header = packs.HEADER.unpack_from(blob)
        self.assertEqual(header[:5], (packs.MAGIC, 1, 0, 1, len("Réglages".encode()) + 1))
        self.assertEqual(header[5], zlib.crc32(blob[112:]))
        self.assertEqual(packs.ENTRY.unpack_from(blob, 112)[0], packs.i18n.language_key_hash("STR_SETTINGS_TITLE"))
        self.assertTrue(blob.endswith("Réglages\0".encode()))
        self.assertFalse(warnings)
        self.assertEqual(blob, packs.compile_pack({"STR_SETTINGS_TITLE": "Réglages"}, "fr", "Français")[0])

    def test_invalid_formats_rejected(self):
        for text in ("%n", "%s", "%1$lu", "%*lu", "%", "%٠٦lu", "%129lu", "%999999999lu", "%.129lu"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                packs.compile_pack({"STR_BT_PAIRING_CODE": text}, "fr", "Français")
        with self.assertRaises(ValueError):
            packs.compile_pack({"STR_PAGE_OVERALL_FORMAT": "Page %d, %.100000000f%% overall"}, "fr", "Français")

    def test_template_csv_quotes_and_multiline(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "template.csv"
            packs.write_template(path)
            self.assertEqual(set(packs.read_csv(path)), set(packs.reference()))
            self.assertTrue(all(not value for value in packs.read_csv(path).values()))

    def test_unknown_keys_and_control_characters(self):
        for values in ({"STR_UNKNOWN": "A"}, {"STR_SETTINGS_TITLE": "A\0B"}):
            with self.assertRaises(ValueError):
                packs.compile_pack(values, "fr", "Français")

    def test_filtered_pack_still_validates_inactive_input(self):
        blob, _ = packs.compile_pack({"STR_SETTINGS_TITLE": "Réglages"}, "fr", "Français", included_keys=set())
        self.assertEqual(packs.HEADER.unpack_from(blob)[3], 0)
        with self.assertRaises(ValueError):
            packs.compile_pack({"STR_BT_PAIRING_CODE": "%n"}, "fr", "Français", included_keys=set())

    def test_web_templates_preserve_names_and_allow_reordered_arguments(self):
        key = "STR_WEB_MSG_FONT_SELECTION"
        packs.compile_pack({key: "Famille : {family}. Fichiers : {count}."}, "fr", "Français")
        for text in ("Famille : {family}.", "{family} {number}", "{family} {count} {count}"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                packs.compile_pack({key: text}, "fr", "Français")

    def test_invalid_metadata_and_size(self):
        for code, name in (("../x", "X"), ("fr", ""), ("fr", "A" * 64), ("x" * 24, "X")):
            with self.assertRaises(ValueError):
                packs.compile_pack({}, code, name)
        with self.assertRaises(ValueError):
            packs.compile_pack({"STR_SETTINGS_TITLE": "A" * packs.MAX_TEXT}, "fr", "Français")

    def test_preserves_format_arguments(self):
        self.assertEqual(packs.format_signature("%06lu %.*s %%"), packs.format_signature("%lu : %.*s %%"))

    def test_translated_literal_percent_label(self):
        blob, warnings = packs.compile_pack({"STR_GO_TO_PERCENT": "Ir al %"}, "es-test", "Español")
        self.assertTrue(blob.endswith("Ir al %\0".encode()))
        self.assertFalse(warnings)
        for text in ("Ir al %s", "Ir al %n", "Ir al % %d"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                packs.compile_pack({"STR_GO_TO_PERCENT": text}, "es-test", "Español")

    def test_url_prefix_keeps_a_valid_scheme(self):
        blob, warnings = packs.compile_pack({"STR_OR_HTTP_PREFIX": "または http://"}, "ja", "日本語")
        self.assertIn(b"http://", blob)
        self.assertFalse(warnings)
        for text in ("または http: //", "または https://", "または"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                packs.compile_pack({"STR_OR_HTTP_PREFIX": text}, "ja", "日本語")

    def test_campaign_percentages_are_text_not_printf_arguments(self):
        source = packs.reference()['STR_QUEST_CAM_LIMITS']
        self.assertEqual(packs.plain_percent_count(source), 3)
        text = 'Время: −25% · Опыт: +30% · Монеты: +20%. Время чтения сохраняется.'
        blob, warnings = packs.compile_pack({'STR_QUEST_CAM_LIMITS': text}, 'ru', 'Русский')
        self.assertTrue(blob.endswith((text + '\0').encode()))
        self.assertFalse(warnings)

    def test_bundle_contains_only_sd_packs_and_template(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = Path(folder) / "packs.zip"
            report = packs.bundle(Path(folder) / "languages", archive)
            folder = Path(folder) / "languages"
            self.assertEqual(len(report), 37)
            self.assertTrue({"ja", "th", "zh-CN", "zh-TW", "id"}.issubset({item["code"] for item in report}))
            self.assertTrue(all(item["entries"] > 0 for item in report))
            self.assertFalse((Path(folder) / "en.cplang").exists())
            self.assertTrue((Path(folder) / "template.csv").exists())
            for item in report:
                blob = (Path(folder) / item["file"]).read_bytes()
                header = packs.HEADER.unpack_from(blob)
                self.assertEqual(header[5], zlib.crc32(blob[112:]))
                if item["code"] != "ook":
                    self.assertEqual(item["sourceKeys"], len(packs.reference()))
                    self.assertFalse(item["omittedInvalid"])
            with zipfile.ZipFile(archive) as handle:
                self.assertIn("languages/fonts/Interface_10.cpfont", handle.namelist())
                self.assertIn("languages/fonts/licenses/NotoSansJP.txt", handle.namelist())
                self.assertIn("languages/fonts/Interface_SC_10.cpfont", handle.namelist())
                self.assertIn("languages/fonts/Interface_TC_10.cpfont", handle.namelist())
                self.assertIn("languages/fonts/licenses/NotoSansThai.txt", handle.namelist())

    def test_complete_language_sources_preserve_formats_and_unicode(self):
        reference = packs.reference()
        for path in (ROOT / "lib/I18n/translations").glob("*.yaml"):
            values = packs.i18n.parse_yaml_file(str(path))
            if values["_bcp47"] in ("en", "es", "ook"):
                continue
            strings = {k: v for k, v in values.items() if k.startswith("STR_")}
            with self.subTest(language=path.name):
                self.assertEqual(set(strings), set(reference))
                self.assertTrue(all(v or not reference[k] for k, v in strings.items()))
                self.assertTrue(all(unicodedata.is_normalized("NFC", v) for v in strings.values()))
                # Validate inactive source copy too, without treating the source catalogue as one runtime pack.
                keys = list(strings)
                for start in range(0, len(keys), 1024):
                    packs.compile_pack({key: strings[key] for key in keys[start:start + 1024]},
                                       values["_bcp47"], values["_language_name"])


if __name__ == "__main__":
    unittest.main()
