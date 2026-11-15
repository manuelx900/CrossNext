"""Prepare optional common-character Mandarin input tables from Unicode Unihan."""
from pathlib import Path
import hashlib, json, re, struct, unicodedata, urllib.request, zipfile
from opencc import OpenCC

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://www.unicode.org/Public/17.0.0/ucd/Unihan.zip'
SHA = 'f7a48b2b545acfaa77b2d607ae28747404ce02baefee16396c5d2d7a8ef34b5e'

def checksum(data):
    value = 2166136261
    for byte in data: value = ((value ^ byte) * 16777619) & 0xffffffff
    return value

def pinyin(text):
    text = text.lower().replace('ü','v').replace('ǖ','v').replace('ǘ','v').replace('ǚ','v').replace('ǜ','v')
    text = ''.join(c for c in unicodedata.normalize('NFD',text) if not unicodedata.combining(c))
    return text if text.isascii() and text.isalpha() and 0 < len(text) <= 6 else None

def generate(cache=None):
    cache = Path(cache or ROOT/'datos.local/language-ui-input/Unihan-17.0.0.zip')
    cache.parent.mkdir(parents=True,exist_ok=True)
    if not cache.exists(): cache.write_bytes(urllib.request.urlopen(URL,timeout=40).read())
    if hashlib.sha256(cache.read_bytes()).hexdigest()!=SHA: raise ValueError('Unihan checksum mismatch')
    out = ROOT/'assets_local/language-keyboards';out.mkdir(parents=True,exist_ok=True)
    readings, frequency = {}, {}
    with zipfile.ZipFile(cache) as archive:
        for line in archive.read('Unihan_Readings.txt').decode().splitlines():
            if not line.startswith('U+'): continue
            cp, field, value = line.split('\t',2);cp=int(cp[2:],16)
            if not 0x3400 <= cp <= 0x9fff: continue
            if field=='kMandarin': readings[cp] = value.split()
            if field=='kHanyuPinlu':
                frequency[cp] = sum(map(int,re.findall(r'\((\d+)\)',value)))
    # The frequency-attested characters plus characters already present in the
    # UI form a useful bounded name-entry set, without loading the full Unihan
    # database or a word-prediction engine on the device.
    selected=set(frequency)
    for file in (ROOT/'lib/I18n/translations').glob('chinese-*.yaml'):
        selected.update(ord(c) for c in file.read_text(encoding='utf-8') if 0x3400<=ord(c)<=0x9fff)
    maps={}
    for suffix, converter in [('sc',OpenCC('t2s')),('tc',OpenCC('s2twp'))]:
        groups={}
        for cp in selected:
            for spelling in readings.get(cp,[]):
                key=pinyin(spelling)
                value=converter.convert(chr(cp))
                if not key or len(value)!=1 or not 0x3400<=ord(value)<=0x9fff: continue
                candidates=groups.setdefault(key,{})
                candidates[ord(value)]=max(candidates.get(ord(value),0),frequency.get(cp,0))
        keys=sorted(groups)
        if len(keys)>1024: raise ValueError('Pinyin index exceeds device bound')
        index=bytearray(); payload=bytearray(); base=16+len(keys)*16
        for key in keys:
            candidates=sorted(groups[key],key=lambda cp:(-groups[key][cp],cp))
            if len(candidates)>2048: raise ValueError('Pinyin candidate bound exceeded')
            index.extend(struct.pack('<8sIHH',key.encode(),base+len(payload),len(candidates),0))
            payload.extend(b''.join(struct.pack('<I',cp) for cp in candidates))
        data=index+payload
        binary=struct.pack('<4sHHII',b'CPPY',1,len(keys),16+len(data),checksum(data))+data
        if len(binary)>256*1024: raise ValueError('Pinyin resource exceeds device bound')
        (out/f'pinyin-{suffix}.bin').write_bytes(binary)
        maps[suffix]={key:''.join(chr(cp) for cp in sorted(groups[key],key=lambda cp:(-groups[key][cp],cp))) for key in keys}
    (out/'PINYIN.json').write_text(json.dumps(maps,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    license_path=out/'UNICODE-LICENSE.txt'
    if not license_path.exists(): license_path.write_bytes(urllib.request.urlopen('https://www.unicode.org/license.txt',timeout=30).read())
    (out/'PINYIN-SOURCE.txt').write_text(f'Unicode Unihan 17.0.0\n{URL}\nSHA-256: {SHA}\nUnicode License v3; see UNICODE-LICENSE.txt.\nDerived common-character pinyin table; no word prediction.\n',encoding='utf-8')
    print({suffix:len(set(''.join(groups.values()))) for suffix,groups in maps.items()})

if __name__=='__main__': generate()
