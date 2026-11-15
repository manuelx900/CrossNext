"""Validate generated indexes, alias remapping and random-access dictzip data."""
import gzip
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools_local"))
import enrich_dictionaries as dictionaries


def fixture(folder, stem, entries, aliases=()):
    folder.mkdir(parents=True, exist_ok=True)
    base = folder / stem
    data, idx, syn = bytearray(), bytearray(), bytearray()
    for name, body in entries:
        encoded = body.encode()
        idx += name.encode() + b"\0" + struct.pack(">II", len(data), len(encoded))
        data += encoded
    for name, ordinal in aliases:
        syn += name.encode() + b"\0" + struct.pack(">I", ordinal)
    base.with_suffix(".dict").write_bytes(data)
    base.with_suffix(".idx").write_bytes(idx)
    base.with_suffix(".syn").write_bytes(syn)
    base.with_suffix(".ifo").write_text("StarDict's dict ifo file\nversion=2.4.2\nbookname=Test\n"
                                       "sametypesequence=h\nwordcount=2\n", encoding="utf-8")
    for name in ("LICENSE", "ATTRIBUTION"):
        (folder / name).write_text("Test fixture", encoding="utf-8")
    return base


class Enrichment(unittest.TestCase):
    def test_definition_refinement_keeps_symbols_and_technical_synonyms(self):
        from dictionary_usage import NaturalUsage
        usage = NaturalUsage(lambda term, lang: 7 if term.casefold() in {"in", "cura"} else 1)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            old = fixture(root / "old", "dictionary", [
                ("atomic number 49", "<p><b>indium, In</b> — a metallic element.</p>"),
                ("aguacate", "<p>(Persea americana) Un árbol.</p><p><b>Sinónimos:</b> aguacatero; palto; cura</p>")])
            source = fixture(root / "source", "en-en", [])
            dictionaries.merge(old.parent, source.parent, "en-en", usage=usage)
            value = gzip.decompress(old.with_suffix(".dict.dz").read_bytes()).decode()
            self.assertIn("indium, In", value)
            self.assertIn("aguacatero; palto; cura", value)

    def test_random_access_chunks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            # Repeated data crosses chunk boundaries; pseudo-random data tests incompressible blocks.
            data = bytes(range(256)) * 1200 + bytes((n * 137 + n // 41) % 256 for n in range(170000))
            raw, zipped = root / "data", root / "data.dz"
            raw.write_bytes(data)
            dictionaries.write_dictzip(raw, zipped)
            encoded = zipped.read_bytes()
            self.assertEqual(gzip.decompress(encoded), data)
            extra, = struct.unpack_from("<H", encoded, 10)
            version, chunk, count = struct.unpack_from("<HHH", encoded, 16)
            self.assertEqual(version, 1)
            sizes = struct.unpack_from("<" + "H" * count, encoded, 22)
            offset = 12 + extra
            for number, size in enumerate(sizes):
                decoded = zlib.decompressobj(-15).decompress(encoded[offset:offset + size])
                self.assertEqual(decoded, data[number * chunk:(number + 1) * chunk])
                offset += size
            self.assertEqual(offset, len(encoded) - 8)

    def test_merge_preserves_baseline_and_remaps_aliases(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            old = fixture(root / "old", "dictionary", [("rise", "<p>ascender</p>"), ("zebra", "<p>cebra</p>")],
                          [("risen", 0)])
            source = fixture(root / "source", "en-es", [
                ("apple", "<div><i>n.</i> <b>manzana</b><span>English source explanation</span></div>"),
                ("rise", "<div><i>v.</i><b>subir</b></div>")], [("apples", 0), ("risen", 1), ("rose", 1)])
            result = dictionaries.merge(old.parent, source.parent, "en-es")
            self.assertEqual(result["headwords"], 3)
            self.assertEqual(result["searchableForms"], 6)
            entries = list(dictionaries.records(old.with_suffix(".idx"), 8))
            self.assertEqual([name for name, _ in entries], [b"apple", b"rise", b"zebra"])
            data = gzip.decompress(old.with_suffix(".dict.dz").read_bytes())
            bodies = []
            for _, fields in entries:
                offset, length = struct.unpack(">II", fields)
                bodies.append(data[offset:offset + length].decode())
            self.assertIn("manzana", bodies[0])
            self.assertNotIn("English source explanation", bodies[0])
            self.assertIn("ascender", bodies[1])
            self.assertIn("subir", bodies[1])
            self.assertEqual(result["expandedHeadwords"], 1)
            aliases = {name: struct.unpack(">I", fields)[0] for name, fields in dictionaries.records(old.with_suffix(".syn"), 4)}
            self.assertEqual(aliases, {b"apples": 0, b"risen": 1, b"rose": 1})
            self.assertTrue((old.parent / "LICENSE-expansion.txt").exists())

    def test_index_rejects_truncation(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "bad.idx"
            path.write_bytes(b"word\0\0\0")
            with self.assertRaises(ValueError):
                list(dictionaries.records(path, 8))

    def test_translation_requires_explicit_target(self):
        self.assertEqual(dictionaries.explicit_translation("<div>English explanation only</div>", "ES"), "")
        value = dictionaries.explicit_translation("<div><i>v.</i><b>temblar</b><span>to shake</span></div>", "ES")
        self.assertIn("Verbo", value)
        self.assertNotIn("to shake", value)

    def test_noun_and_verb_translations_keep_their_categories(self):
        value = dictionaries.explicit_translation(
            "<div><i>n.</i><b>libro</b><span>a written work</span></div>"
            "<div><i>v.</i><b>reservar</b><span>arrange in advance</span></div>", "ES")
        self.assertIn("<b>Sustantivo</b></p><p>libro", value)
        self.assertIn("<b>Verbo</b></p><p>reservar", value)
        self.assertNotIn("arrange in advance", value)

    def test_official_translations_keep_senses_categories_and_aliases(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            old = fixture(root / "old", "dictionary", [("book", "<p>A source equivalent.</p>")])
            source = fixture(root / "source", "en-fr", [
                ("book", '<div><div><font class="grammar">noun</font></div>'
                         '<ol><li>source explanation<div>livre</div></li></ol></div>'),
                ("book", '<div><div><font class="grammar">verb</font></div><div>réserver</div></div>')],
                             [("books", 0)])
            result = dictionaries.merge(old.parent, source.parent, "en-fr", "wikdict")
            body = gzip.decompress(old.with_suffix(".dict.dz").read_bytes()).decode()
            self.assertIn("<b>Nom</b>", body)
            self.assertIn("<b>Verbe</b>", body)
            self.assertIn("livre", body)
            self.assertIn("réserver", body)
            self.assertNotIn("source explanation", body)
            self.assertEqual(result["headwords"], 1)
            self.assertEqual(result["expandedHeadwords"], 1)
            self.assertEqual(list(dictionaries.records(old.with_suffix(".syn"), 4))[0][0], b"books")

    def test_overlapping_monolingual_senses_are_not_discarded(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            old = fixture(root / "old", "dictionary", [("bank", "<p>A financial institution.</p>")])
            source = fixture(root / "source", "en-en", [("bank", "<p>The ground beside a river.</p>")])
            dictionaries.merge(old.parent, source.parent, "en-en")
            result = gzip.decompress(old.with_suffix(".dict.dz").read_bytes()).decode()
            self.assertIn("financial institution", result)
            self.assertIn("beside a river", result)

    def test_duplicate_markup_does_not_repeat_definition(self):
        self.assertEqual(dictionaries.merge_body(b"<p>A tree.</p>", b"<div>A tree.</div>"), b"<p>A tree.</p>")
        self.assertEqual(dictionaries.merge_body(b"<p>A tree.</p>", b"<p>A tree.</p>"), b"<p>A tree.</p>")

    def test_headwords_and_aliases_compose_accents(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            old = fixture(root / "old", "dictionary", [("árbol", "<p>Planta.</p>")])
            source = fixture(root / "source", "es-es", [("a\u0301rbol", "<p>Vegetal.</p>")], [("a\u0301rboles", 0)])
            result = dictionaries.merge(old.parent, source.parent, "es-es")
            self.assertEqual(result["headwords"], 1)
            names = list(dictionaries.records(old.with_suffix(".syn"), 4))
            self.assertEqual(names[0][0], "árboles".encode())

    def test_common_equivalents_precede_source_alternatives(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            old = fixture(root / "old", "dictionary", [("lastly", "<p>postremamente</p>")])
            source = fixture(root / "source", "en-es", [("lastly", "<div><i>adv.</i><b>finalmente</b></div>")])
            result = dictionaries.merge(old.parent, source.parent, "en-es")
            body = gzip.decompress(old.with_suffix(".dict.dz").read_bytes()).decode()
            self.assertLess(body.index("por último"), body.index("postremamente"))
            self.assertIn("finalmente", body)
            self.assertEqual(result["editorialEntries"], ["lastly"])

    def test_wiki_links_keep_visible_equivalent(self):
        value = dictionaries.explicit_translation("<div><i>v.</i><b>[[abanar|abanar-se]]</b></div>", "PT")
        self.assertIn("abanar-se", value)
        self.assertNotIn("[[", value)
        self.assertEqual(dictionaries.compact_html("<p>[[house|home]]</p>"), "<p>home</p>")
        self.assertIn("[[(2S)-amino]", dictionaries.compact_html("<p>[[(2S)-amino]</p>"))

    def test_inflection_precedes_proper_name_but_not_common_word(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            old = fixture(root / "old", "dictionary", [("rose", "<p><i>n.</i> A flower.</p>")])
            source = fixture(root / "source", "en-en", [
                ("rise", "<p><i>v.</i> To move upwards.</p>"),
                ("shake", "<p><i>v.</i> To move back and forth.</p>"),
                ("Shook", "<p><i>prop. n.</i> A surname.</p>")], [("shook", 1), ("rose", 0)])
            dictionaries.merge(old.parent, source.parent, "en-en")
            data = gzip.decompress(old.with_suffix(".dict.dz").read_bytes())
            values = {}
            for word, fields in dictionaries.records(old.with_suffix(".idx"), 8):
                start, size = struct.unpack(">II", fields)
                values[word.lower()] = data[start:start + size].decode()
            self.assertLess(values[b"shook"].index("back and forth"), values[b"shook"].index("surname"))
            self.assertTrue(values[b"rose"].startswith("<p><b>Noun</b></p><p>A flower.</p>"))
            self.assertIn("upwards", values[b"rose"])


if __name__ == "__main__":
    unittest.main()
