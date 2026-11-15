"""Regression checks for preserving guesses, answers and source boundaries."""
import gzip
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools_local'))
import expand_wordle_languages as expansion


class WordleExpansionTest(unittest.TestCase):
    def test_lexical_forms_exclude_wrong_language_names_metadata_and_typos(self):
        entry = {'lang_code': 'en', 'pos': 'noun', 'word': 'house',
                 'forms': [{'form': 'houses', 'tags': ['plural']},
                           {'form': 'bad', 'tags': ['misspelling']},
                           {'form': 'noun', 'tags': ['table-tags']}]}
        self.assertEqual(list(expansion.lexical_forms(entry, 'en')), ['house', 'houses'])
        self.assertEqual(list(expansion.lexical_forms(entry, 'de')), [])
        self.assertEqual(list(expansion.lexical_forms({**entry, 'pos': 'name'}, 'en')), [])
        self.assertEqual(list(expansion.lexical_forms({**entry, 'senses': [{'tags': ['misspelling']}]}, 'en')), [])

    def test_normalization_keeps_distinct_letters_and_rejects_unwriteable(self):
        snapshot = expansion.read_json(expansion.SOURCES / 'wordle-repertoire.json')
        def allowed(word, tag, code):
            return expansion.eligible(word, tag, expansion.tokens_from_snapshot(code, tag, snapshot),
                                      set(snapshot['glyphs']), set(snapshot['thaiClusters']))
        self.assertEqual(allowed('ÁRBOL', 'es', 'ES')[0], 'arbol')
        self.assertEqual(allowed('NIÑO', 'es', 'ES')[0], 'niño')
        self.assertEqual(allowed('IŞIK', 'tr', 'TR')[0], 'ışık')
        self.assertEqual(allowed('İNCİ', 'tr', 'TR')[0], 'inci')
        self.assertEqual(allowed('па́лец', 'be', 'BE')[0], 'палец')
        self.assertEqual(allowed('لُغَة', 'ar', 'AR')[0], 'لغة')
        self.assertIsNotNone(allowed('ґанок', 'uk', 'UK'))
        self.assertIsNotNone(allowed('か\u3099く', 'ja', 'JA'))
        for word in ('a b', 'abc123', 'abcdefg', '😀😀', '\x00ab', 'ab\n'):
            self.assertIsNone(allowed(word, 'en', 'EN'))
        self.assertIsNone(allowed('abcd', 'ar', 'AR'))
        self.assertIsNone(allowed('xyz', 'zh-CN', 'ZH_CN'))
        self.assertIsNone(allowed('ก่่', 'th', 'TH'))

    def make_bank(self, folder):
        groups = [['aa', 'ab'], ['cat'], ['tree'], ['house'], ['houses']]
        answers = [group[:1] for group in groups]
        body = b''.join(expansion.encode(w, list(w)) for bank in (groups, answers) for group in bank for w in group)
        counts = list(map(len, groups)); answer_counts = list(map(len, answers))
        source = folder / 'old.cpwb'
        source.write_bytes(expansion.header(counts, answer_counts, expansion.fnv(body)) + body)
        return source, {'code': 'EN', 'counts': counts, 'answers': answer_counts, 'sha256': expansion.sha(source)}

    def test_merge_preserves_every_old_record_and_exact_answer_bytes(self):
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name); old, entry = self.make_bank(folder)
            extra = {n: set() for n in range(2, 7)}
            extra[2] = {'az', 'aa'}; extra[3] = {'bat', 'dog'}
            target = folder / 'new.cpwb'
            report, _ = expansion.merge_bank(old, target, entry, extra)
            before, after = old.read_bytes(), target.read_bytes()
            self.assertEqual(report['addedGuesses'], [1, 2, 0, 0, 0])
            self.assertEqual(after[64 + 108 * sum(report['counts']):], before[64 + 108 * sum(entry['counts']):])
            self.assertEqual(struct.unpack_from('<I', after, 48)[0], expansion.fnv(after[64:]))
            offset_old = offset_new = 64
            for old_count, new_count in zip(entry['counts'], report['counts']):
                originals = {before[i:i + 108] for i in range(offset_old, offset_old + old_count * 108, 108)}
                merged = [after[i:i + 108] for i in range(offset_new, offset_new + new_count * 108, 108)]
                self.assertTrue(originals.issubset(merged))
                self.assertEqual(merged, sorted(merged))
                offset_old += old_count * 108; offset_new += new_count * 108

    def test_runtime_bound_and_wrong_baseline_fail_without_published_target(self):
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name); old, entry = self.make_bank(folder)
            extra = {n: set() for n in range(2, 7)}; extra[2] = {'az'}
            target = folder / 'new.cpwb'
            with patch.object(expansion, 'MAX_GUESSES_PER_LENGTH', 2):
                with self.assertRaisesRegex(ValueError, 'runtime bound'):
                    expansion.merge_bank(old, target, entry, extra)
            self.assertFalse(target.exists())
            with self.assertRaisesRegex(ValueError, 'Baseline layout'):
                expansion.merge_bank(old, target, {**entry, 'answers': [0] * 5}, extra)

    def test_pinned_sources_reject_silent_upstream_changes(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / 'source'
            path.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'Pinned input changed'):
                expansion.require_digest(path, '0' * 64)

    def test_new_case_pairs_preserve_the_existing_case_map(self):
        with tempfile.TemporaryDirectory() as name:
            target = Path(name) / 'WordleCase.h'
            expansion.write_case_header(set('ōậặệộ'), target)
            text = target.read_text()
            self.assertIn('{0x14C, 0x14D}', text)
            self.assertIn('{0x1EAC, 0x1EAD}', text)
            previous = (ROOT / 'src/apps_local/wordle/WordleCase.h').read_text()
            for mapping in expansion.re.findall(r'\{0x[0-9A-F]+, 0x[0-9A-F]+\}', previous):
                self.assertIn(mapping, text)


if __name__ == '__main__':
    unittest.main()
