import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools_local'))
import build_wordle_languages as banks


class WordleBankTest(unittest.TestCase):
    def test_source_cache_does_not_hide_full_vocabulary(self):
        with tempfile.TemporaryDirectory() as folder:
            cache = Path(folder)
            (cache / 'en.txt').write_text('old 1\n', encoding='utf-8')
            with patch.object(banks.urllib.request, 'urlopen', return_value=io.BytesIO(b'water 10\n')) as download:
                words, credit = banks.source('en', cache)
            self.assertEqual(words, ['water'])
            self.assertTrue(download.call_args.args[0].endswith('/en_full.txt'))
            self.assertEqual(credit['sourceSha256'], hashlib.sha256(b'water 10\n').hexdigest())

    def test_binary_hash_is_distinct_from_source_hash(self):
        with tempfile.TemporaryDirectory() as folder, \
                patch.object(banks, 'source', return_value=(['calor', 'árbol', 'niño'], {'sourceSha256': 'source'})), \
                patch.object(banks, 'MIN_ANSWERS', 0):
            report, words = banks.build_one(('es', 'ES'), Path(folder), Path(folder))
            self.assertEqual(report['sourceSha256'], 'source')
            self.assertEqual(report['sha256'], hashlib.sha256((Path(folder) / 'ES.cpwb').read_bytes()).hexdigest())
            self.assertIn('arbol', words)
            self.assertIn('niño', words)
            self.assertNotIn('árbol', words)

    def test_packaged_banks_match_their_manifest(self):
        root = ROOT / 'assets_local/wordle-languages'
        manifest = json.loads((root / 'MANIFEST.json').read_text(encoding='utf-8'))
        self.assertEqual(len(manifest), 38)
        for entry in manifest:
            with self.subTest(language=entry['tag']):
                path = root / (entry['code'] + '.cpwb')
                with path.open('rb') as stream:
                    self.assertEqual(hashlib.file_digest(stream, 'sha256').hexdigest(), entry['sha256'])
                self.assertEqual(path.stat().st_size, entry['bytes'])
                self.assertGreaterEqual(entry['totalAnswers'], 2000)
                self.assertGreater(entry['totalGuesses'], entry['totalAnswers'])
                self.assertTrue(all(n <= banks.MAX_GUESSES_PER_LENGTH for n in entry['counts']))

    def test_pinned_kana_source_and_notices_work_offline(self):
        with tempfile.TemporaryDirectory() as folder, \
                patch.object(banks.urllib.request, 'urlopen', side_effect=AssertionError('Network not needed')):
            words, credits = banks.additional_guesses('ja', Path(folder))
        self.assertTrue({'さくら', 'すこし', 'こころ'} <= set(words))
        self.assertEqual(credits[0]['sha256'], banks.JMDICT_SHA)
        for name in ('JMDICT-LICENSE.txt', 'JMDICT-DOCUMENTATION.txt'):
            source = ROOT / 'tools_local/wordle_sources' / name
            self.assertGreater(source.stat().st_size, 1000)
            self.assertEqual(source.read_bytes(), (ROOT / 'assets_local/wordle-languages' / name).read_bytes())


if __name__ == '__main__':
    unittest.main()
