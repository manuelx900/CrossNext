"""Resource conversion preserves lexical content, order and StarDict pointers."""
from pathlib import Path
import gzip
import importlib.util
import struct
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("compact", ROOT / "tools_local/compact_dictionaries.py")
compact = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compact)


class Resources(unittest.TestCase):
    def translation(self, source, language="ES"):
        result = compact.compact_html(source, language)
        tree = ET.fromstring("<entry>" + result + "</entry>")
        return result, "".join(tree.itertext())

    def test_private_translation_has_target_language_only(self):
        result, text = self.translation('<div>/<font color="gray">private IPA</font>/<br>'
            '<div><font class="grammar">adjective</font></div><ol>'
            '<li>belonging to an individual</li><li>not accessible by the public</li>'
            '</ol><div>privado</div></div>')
        self.assertEqual(result, '<p><b>Adjetivo</b></p><p>privado</p>')
        self.assertNotIn('accessible', text)
        self.assertNotIn('IPA', text)

    def test_translation_senses_and_alternatives_stay_grouped(self):
        result, text = self.translation('<div><div><font class="grammar">noun</font></div>'
            '<ol><li>musical script<div>libreto</div></li>'
            '<li>bound sheets<ol><li><div>libro</div></li><li><div>libraco</div></li></ol></li>'
            '<li>company accounts<div>libros</div></li></ol></div>')
        self.assertEqual(result, '<p><b>Sustantivo</b></p><p>1. libreto</p><p>2. libro; libraco</p><p>3. libros</p>')
        self.assertNotIn('bound sheets', text)

    def test_shared_gloss_target_list_is_one_group(self):
        result, _ = self.translation('<div><div><font class="grammar">noun</font></div>'
            '<ol><li>collection of sheets of paper</li><li>major division of a published work</li></ol>'
            '<ol><li><div>livre</div></li><li><div>bouquin</div></li></ol></div>', 'FR')
        self.assertEqual(result, '<p><b>Nom</b></p><p>livre; bouquin</p>')

    def test_glossless_nested_senses_stay_separate(self):
        result, _ = self.translation('<div><div><font class="grammar">noun</font></div>'
            '<ol><li><ol><li><div>scale</div></li><li><div>range</div></li></ol></li>'
            '<li><div>doe</div></li></ol></div>', 'EN')
        self.assertEqual(result, '<p><b>Noun</b></p><p>1. scale; range</p><p>2. doe</p>')

    def test_reverse_translation_does_not_show_spanish_explanation(self):
        result, _ = self.translation('<div><div><font class="grammar">noun</font></div>'
            'Conjunto de hojas de papel<div>book</div></div>', 'EN')
        self.assertEqual(result, '<p><b>Noun</b></p><p>book</p>')

    def test_private_army_rank_correction_is_narrow(self):
        source = ('<div><div><font class="grammar">noun</font></div>soldier'
                  '<ol><li><div>soldado raso</div></li><li><div>cabo</div></li></ol></div>')
        self.assertEqual(compact.compact_html(source, 'ES', 'PRIVATE', 'EN'),
                         '<p><b>Sustantivo</b></p><p>soldado raso</p>')
        self.assertIn('cabo', compact.compact_html(source, 'ES', 'corporal', 'EN'))
        self.assertIn('cabo', compact.compact_html(source, 'ES', 'private', 'PT'))

    def test_destination_labels_for_every_bundled_language(self):
        for language, label in [('ES','Sustantivo'),('EN','Noun'),('DE','Substantiv'),('FR','Nom'),('PT','Substantivo')]:
            result, _ = self.translation('<div><div><font class="grammar">noun, female</font></div>'
                '<div>target &amp; equivalent</div></div>', language)
            self.assertEqual(result, f'<p><b>{label}</b></p><p>target &amp; equivalent</p>')
        for table in compact.GRAMMAR.values():
            self.assertEqual(len(table), len(compact.GRAMMAR_KEYS))
            self.assertTrue(all(table.values()))

    def test_translation_preserves_target_case_symbols_and_punctuation(self):
        result, text = self.translation('<div><div><font class="grammar">symbol</font></div>'
            '<ol><li><div>🜨</div></li><li><div>♁</div></li><li><div>École / A &amp; B</div></li></ol></div>','FR')
        self.assertIn('🜨', text)
        self.assertIn('♁', text)
        self.assertIn('École / A & B', text)

    def test_translation_fails_safely_on_unrecognized_source(self):
        with self.assertRaises(ValueError):
            self.translation('<p>Unstructured English gloss with no targets</p>')
        with self.assertRaises(ValueError):
            self.translation('<div><font class="grammar">unknown grammar</font><div>target</div></div>')

    def test_all_eight_dictionary_folders_use_their_target_language(self):
        for pair in ('EN-ES','ES-EN','EN-FR','FR-EN','EN-DE','DE-EN','EN-PT','PT-EN'):
            with self.subTest(pair=pair), tempfile.TemporaryDirectory() as work:
                folder = Path(work) / ('WikDict ' + pair)
                folder.mkdir()
                raw = ('<div><div><font class="grammar">noun</font></div>'
                       'source explanation<div>target &amp; equivalent</div></div>').encode()
                index = b'word\0' + struct.pack('>II', 0, len(raw))
                (folder/'test.idx').write_bytes(index)
                (folder/'test.dict.dz').write_bytes(gzip.compress(raw))
                (folder/'test.ifo').write_text('sametypesequence=h\nwordcount=1\nidxfilesize=13\n')
                changes = compact.compact_dictionary(folder)
                expected = compact.GRAMMAR[pair[-2:]]['noun']
                payload = (folder/'test.dict').read_bytes()
                self.assertEqual(payload.decode(), f'<p><b>{expected}</b></p><p>target &amp; equivalent</p>')
                self.assertEqual(struct.unpack('>II', (folder/'test.idx').read_bytes()[-8:]), (0,len(payload)))
                self.assertTrue(all(item['targetLanguage'] == pair[-2:] for item in changes))

    def check_html(self, source, expected, absent=()):
        result = compact.compact_html(source)
        ET.fromstring("<entry>" + result + "</entry>")
        text = "".join(ET.fromstring("<entry>" + result + "</entry>").itertext())
        for value in expected:
            self.assertIn(value, text)
        for value in absent:
            self.assertNotIn(value, result)
        self.assertEqual(compact.compact_html(result), result)
        return result

    def test_definitions_and_inline_spacing(self):
        self.check_html(
            '<h4>Noun</h4><ol><li>The <a>Earth</a>. <ul><li><b>Synonyms:</b> <a>🜨</a> '
            '<i>and</i> <a>♁</a></li></ul><dl><dd><i>Our <b>world</b> needs care.</i></dd></dl></li>'
            '<li>The universe.</li></ol>',
            ['Noun', '1. The Earth.', 'Synonyms: 🜨 and ♁', 'Our world needs care.', '2. The universe.'],
            ['<a', '<dl', '<ul'])

    def test_inline_pronunciation_does_not_remove_sense(self):
        self.check_html('<h4>Proper noun</h4><ol><li>A religious name.</li>'
                        '<li>Jesus (<span class="IPA">/he.sus/</span>) is a boy\'s name.</li></ol>',
                        ['1. A religious name.', "2. Jesus (/he.sus/) is a boy's name."])

    def test_symbol_does_not_invent_an_earth_synonym(self):
        self.check_html('<ol><li>A chemical element.<ul><li><b>Synonyms:</b> Sb and <a>♁</a></li></ul></li></ol>',
                        ['Synonyms: Sb and ♁'], ['Earth'])

    def test_pronunciation_removed_but_ipa_meaning_retained(self):
        self.check_html('<h4>Noun</h4><ul><li>IPA: <span class="IPA">/bier/</span></li></ul>'
                        '<ol><li>An IPA is a type of beer.</li></ol>',
                        ['1. An IPA is a type of beer.'], ['/bier/'])

    def test_spanish_senses_and_synonyms(self):
        self.check_html('<section lang="es"><h4>Sustantivo</h4><ul><li><b>silabación</b>: li-bro</li></ul>'
                        '<dl><dt>1</dt><dd>Hojas unidas.<ul><li><b>Sinónimos:</b> tomo, volumen.</li></ul></dd>'
                        '<dt>2 Zoología</dt><dd>Cavidad del estómago.</dd></dl>'
                        '<section><h4>Etimología</h4><p>Latin metadata</p></section></section>',
                        ['1. Hojas unidas.', 'Sinónimos: tomo, volumen.', '2 Zoología. Cavidad del estómago.'],
                        ['Latin metadata', 'li-bro', 'Etimología'])

    def test_imaginacion_starts_with_real_definition(self):
        result = self.check_html('<h4>Sustantivo femenino</h4><ul>'
            '<li><b>seseante</b> (<a title="Alfabeto Fonético Internacional">AFI</a>): '
            '<span class="IPA">[i.ma.xi.naˈsjon]</span></li>'
            '<li><b>no seseante</b> (<a title="Alfabeto Fonético Internacional">AFI</a>): '
            '<span class="IPA">[i.ma.xi.naˈθjon]</span></li>'
            '<li><b>silabación</b>: i-ma-gi-na-ción</li></ul>'
            '<p><b>imaginación</b> ¦ plural: imaginaciones</p>'
            '<dl><dt>1</dt><dd>Facultad de representarse mentalmente objetos no presentes.'
            '<ul><li><b>Ejemplo:</b> Usó su imaginación.</li></ul></dd></dl>'
            '<dl><dt>2</dt><dd>Capacidad de crear ideas nuevas y originales.</dd></dl>',
            ['Sustantivo femenino','1. Facultad de representarse','Ejemplo: Usó su imaginación.',
             '2. Capacidad de crear ideas nuevas'], ['AFI','seseante','plural:','silabación','ˈsjon','ˈθjon'])
        self.assertTrue(result.startswith('<p><b>Sustantivo femenino</b></p><p>1. Facultad'))

    def test_inline_afi_in_meaning_is_retained(self):
        self.check_html('<ol><li>El AFI representa sonidos como '
                        '<span class="IPA">[a]</span>.</li></ol>', ['1. El AFI representa sonidos como [a].'])
        self.check_html('<dl><dt>1</dt><dd>Notación fonética.<ul><li>AFI: '
                        '<span class="IPA">[a]</span></li></ul></dd></dl>',
                        ['1. Notación fonética.', 'AFI: [a]'])

    def test_inflection_filter_keeps_ordinary_definition(self):
        self.check_html('<p><b>plural</b> significa más de uno.</p><p>A ¦ B.</p>',
                        ['plural significa más de uno.', 'A ¦ B.'])

    def test_qualified_english_phonetic_preludes_are_removed(self):
        for prefix in ('* ', '(UK): ', '(noun): (UK) (US) ', '(US) ((varieties without the cot-caught merger)) (UK) '):
            with self.subTest(prefix=prefix):
                self.check_html('<h4>Noun</h4><ul><li>' + prefix + 'IPA: '
                    '<span class="IPA">/əˈrɪθmətɪk/</span></li></ul><ol><li>A genuine meaning.</li></ol>',
                    ['Noun', '1. A genuine meaning.'], ['IPA:', 'əˈrɪθmətɪk', 'cot-caught'])

    def test_translation_and_nested_lists(self):
        self.check_html('<div>/<font color="gray">/bu:k/</font>/<br><div><font class="grammar">noun</font></div>'
                        '<ol><li>collection of paper<ol><li><div>libro</div></li><li><div>libracho</div></li></ol>'
                        '</li><li>record of accounts<div>libros</div></li></ol></div>',
                        ['noun', 'collection of paper', 'libro', 'libracho', 'record of accounts', 'libros'], ['/bu:k/'])

    def test_styles_entities_unknown_markup(self):
        self.check_html('<style>bad{}</style><script>bad()</script><p><b>A &amp; B</b> &lt; C.</p>'
                        '<p>Keep <unknown>rare meaning</unknown> and &#241;.</p><i>Example<br>continues</i>',
                        ['A & B < C.', 'rare meaning', 'ñ', 'Example', 'continues'], ['bad()'])

    def test_original_when_only_metadata(self):
        source = '<ul><li>IPA: <span class="IPA">/a/</span></li></ul>'
        self.check_html(source, ['IPA: /a/'])

    def test_shared_etymology_container_keeps_meaning(self):
        self.check_html('<section><h4>Etimología</h4><p>Origin.</p><h4>Adjetivo</h4>'
                        '<dl><dt>1</dt><dd>A genuine meaning.</dd></dl></section>', ['1. A genuine meaning.'])

    def test_relations_heading_stays_with_terms(self):
        value = self.check_html('<section><h4>Synonyms</h4><ul><li>anybody</li><li>someone</li></ul></section>',
                                ['Synonyms: anybody; someone'])
        self.assertEqual(value.count('<p>'), 1)

    def test_index_duplicates_synonyms_and_offsets(self):
        for compressed in [False, True]:
            with self.subTest(compressed=compressed), tempfile.TemporaryDirectory() as work:
                folder = Path(work)
                payload = bytearray()
                index = bytearray()
                for word, value in [('book', '<h4>Noun</h4><p>A printed work.</p>'),
                                    ('book', '<h4>Verb</h4><p>To reserve.</p>'),
                                    ('world', '<p>The Earth.</p>')]:
                    raw = value.encode()
                    payload.extend(b'padding')
                    index.extend(word.encode() + b'\0' + struct.pack('>II', len(payload), len(raw)))
                    payload.extend(raw)
                (folder / 'test.idx').write_bytes(index)
                data = folder / ('test.dict.dz' if compressed else 'test.dict')
                data.write_bytes(gzip.compress(payload) if compressed else payload)
                synonyms = b'reserve\0' + struct.pack('>I', 1)
                (folder / 'test.syn').write_bytes(synonyms)
                (folder / 'test.ifo').write_text("StarDict's dict ifo file\nversion=2.4.2\n"
                                               f"sametypesequence=h\nwordcount=3\nidxfilesize={len(index)}\n")
                changes = compact.compact_dictionary(folder)
                self.assertEqual(len(changes), 3)
                self.assertEqual((folder / 'test.syn').read_bytes(), synonyms)
                data = (folder / 'test.dict').read_bytes()
                index = (folder / 'test.idx').read_bytes()
                at, words, values = 0, [], []
                while at < len(index):
                    end = index.index(0, at)
                    offset, size = struct.unpack_from('>II', index, end + 1)
                    words.append(index[at:end].decode())
                    values.append(data[offset:offset + size].decode())
                    at = end + 9
                self.assertEqual(words, ['book', 'book', 'world'])
                for value, expected in zip(values, ['A printed work.', 'To reserve.', 'The Earth.']):
                    self.assertIn(expected, value)

    def test_rejects_bad_index(self):
        with tempfile.TemporaryDirectory() as work:
            folder = Path(work)
            (folder / 'test.ifo').write_text('sametypesequence=h\nwordcount=1\n')
            (folder / 'test.idx').write_bytes(b'word\0' + struct.pack('>II', 50, 10))
            (folder / 'test.dict').write_bytes(b'short')
            with self.assertRaises(ValueError):
                compact.compact_dictionary(folder)
            self.assertEqual((folder / 'test.dict').read_bytes(), b'short')


if __name__ == '__main__':
    unittest.main()
