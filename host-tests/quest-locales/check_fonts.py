"""Check regular and bold microSD fonts against catalogues and input resources."""
from pathlib import Path
import importlib.util,struct,json,hashlib
r=Path(__file__).resolve().parents[2]
s=importlib.util.spec_from_file_location('g',r/'scripts/gen_i18n.py');g=importlib.util.module_from_spec(s);s.loader.exec_module(g)
used=set();bylang={}
controls={0x200c,0x200d,0x200e,0x200f,0x2028,0x2029,0x202a,0x202b,0x202c,0x2066,0x2067,0x2068,0x2069,0xfeff}
for p in (r/'lib/I18n/translations').glob('*.yaml'):
 d=g.parse_yaml_file(str(p));c={ord(x) for k,v in d.items() if not k.startswith('STR_WEB_') for x in v if ord(x)>=32}-controls;used|=c;bylang[d['_bcp47']]=c
# Keyboard layers, long-press alternatives, Pinyin candidates and game alphabets.
keyboard=json.loads((r/'assets_local/language-keyboards/SOURCE.json').read_text('utf8'))
keyboard_chars=set()
for spec in keyboard.values():
 for layer in ('rows','shift'):
  keyboard_chars.update(ord(c) for row in spec[layer] for c in row if ord(c)>=32)
 keyboard_chars.update(ord(c) for text in spec.get('alts',{}).values() for c in text if ord(c)>=32)
pinyin=json.loads((r/'assets_local/language-keyboards/PINYIN.json').read_text('utf8'))
keyboard_chars.update(ord(c) for entries in pinyin.values() for text in entries.values() for c in text if ord(c)>=32)
keyboard_chars.update(ord(c) for c in (r/'assets_local/wordle-languages/GLYPHS.txt').read_text('utf8') if ord(c)>=32)
used|=keyboard_chars-controls
expected={f'Interface{region}_{point}.cpfont' for region in ('','_SC','_TC') for point in (8,10,12)}
actual={p.name for p in (r/'assets_local/language-fonts').glob('*.cpfont')}
assert expected<=actual, 'Missing font files: '+str(expected-actual)
(r/'.cache/locale-ui').mkdir(parents=True,exist_ok=True)
report=[]
for p in sorted((r/'assets_local/language-fonts').glob('*.cpfont')):
 b=p.read_bytes();magic,version,flags,count,fingerprint=struct.unpack_from('<8sHHB19s',b)
 row={'file':p.name,'version':version,'sha256':hashlib.sha256(b).hexdigest(),'styles':[]}
 for i in range(count):
  t=struct.unpack_from('<B3xIIBhhHHBBBIH2x',b,32+i*32);supported=set()
  for n in range(t[1]):
   first,last,offset=struct.unpack_from('<III',b,t[11]+n*12);supported.update(range(first,last+1))
  missing=used-supported
  row['styles'].append({'style':t[0],'characters':len(supported),'missing':[f'U+{x:04X}' for x in sorted(missing)],'languages':{code:len(c-supported) for code,c in bylang.items() if c-supported}})
 report.append(row)
(r/'.cache/locale-ui/font-coverage.json').write_text(json.dumps({'used':len(used),'fonts':report},ensure_ascii=False,indent=2),'utf8')
print(json.dumps({'used':len(used),'fonts':[{'file':x['file'],'styles':x['styles']} for x in report]},ensure_ascii=False))

assert all(not st['missing'] for row in report for st in row['styles']), 'Missing language glyphs'
assert all({st['style'] for st in row['styles']} >= {0,1} for row in report), 'Regular or bold face missing'
