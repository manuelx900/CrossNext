"""Regressions for lost senses, usage restrictions and case-folded proper names."""
import gzip
import json
from pathlib import Path
import sqlite3
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools_local"))
import dictionary_fidelity as fidelity
import enrich_dictionaries as dictionaries
from test_enrichment import fixture


class Fidelity(unittest.TestCase):
    def test_qualified_obsolete_translation_follows_common_sense(self):
        rows = [("deu/Gift__Substantiv__2", "veraltet: Geschenk", "gift | present", 100, 4),
                ("deu/Gift__Substantiv__1", "gesundheitsschädliche Substanz", "poison | toxin", 218, 2)]
        text = fidelity.render_translations(fidelity.translated_senses(rows), "EN")
        self.assertLess(text.index("poison"), text.index("gift; present"))
        self.assertIn("veraltet: Geschenk", text)

    def test_case_does_not_turn_german_common_noun_into_name(self):
        self.assertEqual(fidelity.part_of_speech("deu/Hund__Substantiv__1"), "noun")
        self.assertEqual(fidelity.part_of_speech("eng/Pan__Proper_noun__1"), "proper")
        rows = [("deu/Hund__Substantiv__1", "deutscher Nachname, Familienname", "Hund", 100, 5),
                ("deu/Hund__Substantiv__1", "Haustier", "dog | hound", 230, 4)]
        text = fidelity.render_translations(fidelity.translated_senses(rows), "EN")
        self.assertLess(text.index("dog"), text.index("Proper name"))

    def test_reverse_generated_unqualified_alternative_does_not_pollute_forward_entry(self):
        rows = [(None, None, "actually", 2, 9),
                ("spa/actualmente__adverbio_de_tiempo__1", None, "at present", 1, 1)]
        text = fidelity.render_translations(fidelity.translated_senses(rows), "EN")
        self.assertNotIn("actually", text)
        self.assertIn("at present", text)

    def test_reverse_only_word_still_searchable(self):
        text = fidelity.render_translations(fidelity.translated_senses([(None, None, "folder", 4, 0)]), "EN")
        self.assertIn("folder", text)

    def test_distinct_contexts_survive_same_equivalent(self):
        rows = [("eng/private__Adjective__1", "not public", "privado", 120, 1),
                ("eng/private__Adjective__1", "not employed by government", "privado", 110, 1),
                ("eng/private__Adjective__1", None, "privado", 100, 1)]
        text = fidelity.render_translations(fidelity.translated_senses(rows), "ES")
        self.assertIn("not public", text)
        self.assertIn("not employed by government", text)
        self.assertEqual(text.count("<p>privado</p>"), 2)

    def test_correction_numbers_and_lastly_are_unambiguous(self):
        data = json.loads(Path(fidelity.__file__).with_name("dictionary_corrections.json").read_text(encoding="utf-8"))["entries"]
        for pair, target in (("en-es", "mil millones"), ("en-fr", "milliard"), ("en-de", "Milliarde"), ("en-pt", "bilhão")):
            with self.subTest(pair=pair):
                rows = data[pair]["billion"]
                self.assertIn(target, rows[0]["equivalents"][0])
                self.assertIn("10^9", rows[0]["context"])
                self.assertIn("10^12", rows[1]["context"])
                self.assertTrue(data[pair]["lastly"])
        self.assertEqual(data["en-es"]["lastly"][0]["equivalents"], ["por último", "finalmente"])

    def test_phonetics_are_removed_only_when_separate_prelude(self):
        source = '<div style="color:gray">/ʃʊk/</div><div><i>n.</i> A bundle.</div>'
        text = fidelity.monolingual(source, "EN")
        self.assertNotIn("ʃʊk", text)
        self.assertIn("A bundle.", text)
        self.assertIn("Noun", text)
        self.assertIn("/a/", fidelity.monolingual('<p>The symbol /a/ represents a vowel.</p>', "EN"))

    def test_duplicate_meaning_keeps_examples_but_not_duplicate_numbering(self):
        old = b'<p><b>Verb</b></p><p>1. To read a text.</p><p><i>Read this.</i></p>'
        new = b'<p><b>Verb</b></p><p>4. To read a text.</p><p><i>Read that.</i></p><p>5. To interpret.</p>'
        text = fidelity.merge_meanings(old, new).decode()
        self.assertEqual(text.count("To read a text."), 1)
        self.assertIn("Read this.", text)
        self.assertIn("Read that.", text)
        self.assertIn("To interpret.", text)

    def test_qualifier_differences_do_not_merge_distinct_meanings(self):
        text = fidelity.merge_meanings(b'<p>(archaic) A gift.</p>', b'<p>A gift.</p>').decode()
        self.assertIn("(archaic) A gift.", text)
        self.assertEqual(text.count("gift"), 2)

    def test_decimal_and_numbered_citation_are_stable(self):
        original = '<p>23.1.5. Cintas y huinchas aislantes.</p><p>3.14 is an approximation.</p>'.encode()
        once = fidelity.merge_meanings(b"", original)
        twice = fidelity.merge_meanings(once, original)
        self.assertIn(b"23.1.5.", twice)
        self.assertIn(b"3.14", twice)
        self.assertEqual(once, twice)

    def test_same_wording_under_distinct_categories_is_not_discarded(self):
        result = fidelity.merge_meanings(b'<p><b>Noun</b></p><p>A test.</p>',
                                         b'<p><b>Verb</b></p><p>A test.</p>')
        self.assertEqual(result.count(b"A test."), 2)

    def test_inline_translation_markup_is_readable_without_losing_symbols(self):
        self.assertEqual(fidelity.clean('(Deine)<sup>†</sup> Durchlaucht'), '(Deine)† Durchlaucht')
        self.assertEqual(fidelity.clean('ech<u>i</u>dna'), 'echidna')
        self.assertEqual(fidelity.clean('x < 5; <token>; A &amp; B'), 'x < 5; <token>; A & B')

    def test_compound_category_is_expression(self):
        self.assertEqual(fidelity.part_of_speech("eng/at_home__Prepositional_phrase__1"), "expression")
        self.assertEqual(fidelity.part_of_speech("eng/at__Preposition__1"), "preposition")

    def test_exact_irregular_collisions_keep_verb_but_not_lemma_noun(self):
        db = sqlite3.connect(":memory:")
        db.execute("CREATE TABLE entries(key BLOB PRIMARY KEY,body BLOB)")
        for form, lemma in (("found", "find"), ("saw", "see"), ("left", "leave"), ("rose", "rise"), ("spoke", "speak")):
            db.execute("INSERT INTO entries VALUES(?,?)", (form.encode(), b'<p>Direct meaning.</p>'))
            db.execute("INSERT INTO entries VALUES(?,?)", (lemma.encode(),
                b'<p><b>Sustantivo</b></p><p>A noun.</p><p><b>Verbo</b></p><p>Verb meaning.</p>'))
        dictionaries.augment_irregular_forms(db, "en-es")
        for form in ("found", "saw", "left", "rose", "spoke"):
            body, = db.execute("SELECT body FROM entries WHERE key=?", (form.encode(),)).fetchone()
            self.assertIn(b"Direct meaning.", body)
            self.assertIn(b"Verb meaning.", body)
            self.assertNotIn(b"A noun.", body)
        db.close()

    def test_common_definition_precedes_acronym_and_surname(self):
        text = fidelity.merge_meanings('<p><b>Sustantivo</b></p><p>Sigla de una red.</p>'.encode(),
                                       '<p><b>Sustantivo</b></p><p>Alimento de harina y agua.</p>'.encode()).decode()
        self.assertLess(text.index("Alimento"), text.index("Sigla"))
        text = fidelity.merge_meanings('<p><b>Sigla</b></p><p>1 Informática. Una red.</p>'.encode(),
                                       '<p><b>Sustantivo masculino</b></p><p>1 Alimentos. Harina y agua.</p>'.encode()).decode()
        self.assertLess(text.index("Harina"), text.index("Sigla"))

    def test_sqlite_export_does_not_reuse_noisy_bodies_and_remaps_forms(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            old = fixture(root / "old", "test", [("advice", "<p>incorrect advice</p>"), ("shake", "<p>bad</p>")])
            source = fixture(root / "source", "en-es", [("shake", "<p>bad noisy duplicate</p>")], [("shook", 0)])
            database = root / "source.sqlite3"
            db = sqlite3.connect(database)
            db.execute("CREATE TABLE translation(written_rep,lexentry,sense,trans_list,score,importance)")
            db.executemany("INSERT INTO translation VALUES(?,?,?,?,?,?)", [
                ("advice", "eng/advice__Noun__1", "guidance", "consejo | asesoramiento", 177, 2),
                ("shake", "eng/shake__Verb__1", "move repeatedly", "sacudir | agitar", 200, 2)])
            db.commit()
            db.close()
            dictionaries.build_translations(old.parent, source.parent, "en-es", database)
            values = gzip.decompress(old.with_suffix(".dict.dz").read_bytes()).decode()
            self.assertIn("consejo", values)
            self.assertIn("guidance", values)
            self.assertNotIn("incorrect", values)
            self.assertNotIn("noisy", values)
            aliases = dict(dictionaries.records(old.with_suffix(".syn"), 4))
            entries = list(dictionaries.records(old.with_suffix(".idx"), 8))
            self.assertEqual(entries[struct.unpack(">I", aliases[b"shook"])[0]][0], b"shake")
            self.assertIn(b"lastly", dict(entries))


if __name__ == "__main__":
    unittest.main()
