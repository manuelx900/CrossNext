"""Natural equivalents without rewriting senses, examples or qualified usages."""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools_local"))
from dictionary_usage import NaturalUsage, register, usage_label
import dictionary_fidelity as fidelity


class Usage(unittest.TestCase):
    def setUp(self):
        self.usage = NaturalUsage(lambda term, lang: {
            "trovar": 1.48, "hallar": 3.92, "encontrar": 5.22,
            "condir": 0, "fundar": 3.74, "instaurar": 3.25,
            "rare": 1, "common": 5, "technical": 1.5,
        }.get(term, 3))

    def test_common_equivalents_first_and_redundant_rare_alternatives_removed(self):
        self.assertEqual(self.usage.equivalents(("trovar", "encontrar", "hallar"), "ES"),
                         ("encontrar", "hallar"))
        self.assertEqual(self.usage.equivalents(("condir", "fundar", "instaurar"), "ES"),
                         ("fundar", "instaurar"))

    def test_sole_rare_translation_is_not_lost(self):
        self.assertEqual(self.usage.equivalents(("rare",), "EN"), ("rare",))
        self.assertEqual(self.usage.equivalents(("rare", "technical"), "EN"), ("rare", "technical"))

    def test_frequency_cannot_promote_plural_or_different_common_sense(self):
        usage = NaturalUsage(lambda term, lang: {"woman": 5, "women": 6, "house": 5, "home": 6}[term])
        self.assertEqual(usage.equivalents(("woman", "women"), "EN"), ("woman", "women"))
        self.assertEqual(usage.equivalents(("house", "home"), "EN"), ("house", "home"))

    def test_compounds_and_qualified_terms_keep_their_slots(self):
        self.assertEqual(self.usage.equivalents(("rare", "fixed phrase", "common", "rare (regional)"), "EN"),
                         ("fixed phrase", "common", "rare (regional)"))

    def test_technical_and_explicit_registers_are_protected(self):
        for context in ("(medicine) a cell", "biology: a cell", "regional: a village",
                        "archaic: a message", "Sens ancien de bibliothèque", "(US, slang) a greeting"):
            with self.subTest(context=context):
                self.assertEqual(self.usage.equivalents(("rare", "common"), "EN", "noun", context),
                                 ("rare", "common"))

    def test_names_symbols_and_numbers_are_protected(self):
        for pos in ("proper", "symbol", "abbreviation", "numeral", "letter"):
            self.assertEqual(self.usage.equivalents(("rare", "common"), "EN", pos), ("rare", "common"))
        usage = NaturalUsage(lambda term, lang: 7.27 if term == "In" else 1)
        self.assertEqual(usage.equivalents(("indium", "In"), "EN", "noun"), ("indium", "In"))

    def test_uncommon_reflexive_verb_is_not_confused_with_archaism(self):
        usage = NaturalUsage(lambda term, lang: {"cambiar": 5, "estropearse": 2.06}[term])
        self.assertEqual(usage.equivalents(("cambiar", "estropearse"), "ES", "verb"), ("cambiar", "estropearse"))

    def test_equal_scores_preserve_order_and_case(self):
        self.assertEqual(self.usage.equivalents(("CO", "Co", "co"), "EN"), ("CO", "Co", "co"))

    def test_usage_metadata_not_ordinary_text_controls_register(self):
        self.assertEqual(register("Sens ancien de bibliothèque"), "historical")
        self.assertEqual(register("(countable, archaic) a gift"), "historical")
        self.assertEqual(register("someone who studies obsolete devices"), "")
        self.assertEqual(register("The old part of a city"), "")
        self.assertEqual(register("regional unit"), "")
        self.assertEqual(register("rare genetic disorder"), "")
        self.assertEqual(register("antigo palácio papal e atual residência"), "")
        for context in ("veraltete Bedeutung: Vertrag", "veraltend: der Abschluss", "veraltende Bedeutung: Ende"):
            self.assertEqual(register(context), "historical")
        self.assertEqual(usage_label("informal: a greeting", "ES"), "Uso coloquial")

    def test_historical_sense_labeled_after_current_meaning(self):
        senses = [fidelity.Sense("noun", ("library",), "Sens ancien de bibliothèque", 200, 10),
                  fidelity.Sense("noun", ("bookshop", "bookstore"), "Magasin de livres", 100, 1)]
        text = fidelity.render_translations(senses, "EN", self.usage)
        self.assertLess(text.index("bookshop"), text.index("library"))
        self.assertIn("Historical use", text)
        self.assertIn("Sens ancien de bibliothèque", text)

    def test_german_historical_variant_follows_current_meaning(self):
        rows = [("deu/Akkord__Substantiv__1", "veraltete Bedeutung: Vertrag", "accord | chord", 100, .75),
                ("deu/Akkord__Substantiv__1", "Musik: harmonisches Erklingen mehrerer Töne", "chord", 210, .58)]
        text = fidelity.render_translations(fidelity.translated_senses(rows), "EN", self.usage)
        self.assertTrue(text.startswith("<p><b>Noun</b></p><p>chord</p>"))
        self.assertIn("accord; chord <i>(Historical use)</i>", text)

    def test_frequency_never_merges_different_senses(self):
        text = fidelity.render_translations([
            fidelity.Sense("noun", ("rare",), "a technical meaning"),
            fidelity.Sense("noun", ("common",), "an ordinary meaning")], "EN", self.usage)
        self.assertIn("rare", text)
        self.assertIn("common", text)
        self.assertIn("a technical meaning", text)

    def test_case_distinctions_do_not_merge_meanings(self):
        text = fidelity.merge_meanings(b'<p>CO is carbon monoxide.</p>', b'<p>Co is cobalt.</p>')
        self.assertIn(b"CO is", text)
        self.assertIn(b"Co is", text)

    def test_editorial_core_words_have_natural_equivalents(self):
        data = json.loads(Path(fidelity.__file__).with_name("dictionary_corrections.json").read_text(encoding="utf-8"))["entries"]
        checks = {
            "en-es": {"find": ("encontrar", "trovar"), "found": ("fundar", "condir"),
                      "lastly": ("por último", "postremamente"), "actually": ("en realidad", "actualmente"),
                      "realize": ("darse cuenta", "apiolarse"), "because": ("porque", "; ca;"),
                      "however": ("sin embargo", "así que asado"), "library": ("biblioteca", "librería"),
                      "often": ("a menudo", "acostumbrar"), "attend": ("asistir", "ocupar"),
                      "suddenly": ("de repente", "subitáneamente")},
            "es-en": {"asistir": ("attend", ""), "sensible": ("sensitive", ""), "librería": ("bookshop", "")},
            "fr-en": {"librairie": ("bookshop", ""), "sensible": ("sensitive", ""),
                      "éventuellement": ("possibly", "eventually")},
            "de-en": {"eventuell": ("possibly", "eventually")},
            "pt-en": {"eventualmente": ("possibly", "eventual"), "sensível": ("sensitive", "")},
        }
        for pair, words in checks.items():
            for word, (expected, unwanted) in words.items():
                with self.subTest(pair=pair, word=word):
                    values = [term for row in data[pair][word] for term in row["equivalents"]]
                    self.assertIn(expected, values)
                    if unwanted:
                        self.assertNotIn(unwanted, values)


if __name__ == "__main__":
    unittest.main()
