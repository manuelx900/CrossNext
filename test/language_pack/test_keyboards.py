from pathlib import Path
import importlib.util
import json
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('keyboards', ROOT/'tools_local/build_language_keyboards.py')
keyboards = importlib.util.module_from_spec(spec)
spec.loader.exec_module(keyboards)

class LanguageKeyboardResourcesTest(unittest.TestCase):
    def test_every_real_language_has_a_builtin_or_resource_layout(self):
        builtin = {'en','es','fr','de','ru','uk','be','kk','he','ar'}
        external = keyboards.definitions()
        tags = set()
        for path in (ROOT/'lib/I18n/translations').glob('*.yaml'):
            tags.add(next(json.loads(line.split(':',1)[1]) for line in path.read_text(encoding='utf-8').splitlines()
                          if line.startswith('_bcp47:')))
        self.assertEqual(tags-{'ook'},builtin | set(external))

    def test_layouts_are_bounded_complete_utf8_and_deterministic(self):
        for tag, layout in keyboards.definitions().items():
            with self.subTest(language=tag):
                binary = keyboards.compile_layout(layout)
                magic, version, flags, rows, shift, size, digest = struct.unpack_from('<4sBBBBII',binary)
                self.assertEqual((magic,version,flags),(b'CPKB',1,1))
                self.assertEqual(size,len(binary)-16)
                self.assertEqual(digest,keyboards.fnv(binary[16:]))
                cursor=16
                for count in (rows,shift):
                    self.assertLessEqual(count+2,6)
                    total=16  # ten digits and six controls
                    for _ in range(count):
                        keys=binary[cursor];cursor+=1;total+=keys
                        self.assertLessEqual(keys,12)
                        for _ in range(keys):
                            length=binary[cursor];cursor+=1
                            value=binary[cursor:cursor+length].decode('utf-8')
                            self.assertTrue(value)
                            self.assertEqual(binary[cursor+length],0)
                            cursor+=length+1
                            length=binary[cursor];cursor+=1
                            binary[cursor:cursor+length].decode('utf-8')
                            self.assertEqual(binary[cursor+length],0)
                            cursor+=length+1
                    self.assertLessEqual(total,80)
                self.assertEqual(cursor,len(binary))
                self.assertEqual(binary,keyboards.compile_layout(layout))

    def test_native_alphabets_are_not_english_placeholders(self):
        expected={'th':'กขฃคฅฆงจฉชซฌญฎฏฐฑฒณดตถทธนบปผฝพฟภมยรลวศษสหฬอฮ','ja':'あかさたなはまやらわん',
                  'bg':'абвгдежзийклмнопрстуфхцчшщъьюя','fa':'پچژگ','tr':'çğıöşü','id':'abcdefghijklmnopqrstuvwxyz'}
        for tag, needed in expected.items():
            text=''.join(keyboards.definitions()[tag]['rows']+keyboards.definitions()[tag]['shift'])
            self.assertTrue(set(needed)<=set(text),(tag,set(needed)-set(text)))

    def test_pinyin_offsets_and_common_candidates(self):
        folder=ROOT/'assets_local/language-keyboards'
        for variant, expected in [('sc','你'),('tc','你')]:
            binary=(folder/f'pinyin-{variant}.bin').read_bytes()
            magic,version,count,size,digest=struct.unpack_from('<4sHHII',binary)
            self.assertEqual((magic,version,size),(b'CPPY',1,len(binary)))
            self.assertEqual(digest,keyboards.fnv(binary[16:]))
            self.assertLessEqual(count,1024)
            previous='';groups={}
            for i in range(count):
                key,offset,n,reserved=struct.unpack_from('<8sIHH',binary,16+i*16)
                key=key.rstrip(b'\0').decode('ascii')
                self.assertGreater(key,previous);previous=key
                self.assertGreaterEqual(offset,16+count*16)
                self.assertLessEqual(offset+n*4,len(binary))
                cps=struct.unpack_from('<'+'I'*n,binary,offset)
                self.assertTrue(all(0x3400<=cp<=0x9fff for cp in cps))
                groups[key]=''.join(map(chr,cps))
            self.assertIn(expected,groups['ni'])
            self.assertIn('好',groups['hao'])
            self.assertIn('中',groups['zhong'])

if __name__=='__main__': unittest.main()
