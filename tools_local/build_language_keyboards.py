"""Build bounded optional UI keyboard tables; all language data stays on microSD."""
from pathlib import Path
import argparse, json, struct, unicodedata

ROOT = Path(__file__).resolve().parents[1]
QWERTY = ['qwertyuiop', 'asdfghjkl', 'zxcvbnm']
LATIN_LANGUAGES = ('fr', 'az', 'bs', 'ca', 'ca-valencia', 'cs', 'da', 'nl', 'fi', 'hu',
                   'id', 'it', 'lt', 'nb', 'pl', 'pt-BR', 'pt-PT', 'ro', 'sk', 'sl', 'sv', 'tr', 'vi')

# Mobile letter rows. Optional accents are supplied by KeyboardAlternates.
MOBILE_ROWS = {
    'fr': ['azertyuiop', 'qsdfghjklm', 'wxcvbn'],
    'az': ['qüertyuiopöğ', 'asdfghjklıə', 'zxcvbnmçş'],
    'bs': ['qwertzuiop', 'asdfghjkl', 'yxcvbnm'],
    'ca': ['qwertyuiop', 'asdfghjklç', 'zxcvbnm'],
    'ca-valencia': ['qwertyuiop', 'asdfghjklç', 'zxcvbnm'],
    'cs': ['qwertzuiop', 'asdfghjkl', 'yxcvbnm'],
    'da': ['qwertyuiopå', 'asdfghjklæø', 'zxcvbnm'],
    'fi': ['qwertyuiopå', 'asdfghjklöä', 'zxcvbnm'],
    'hu': ['qwertzuiop', 'asdfghjkl', 'yxcvbnm'],
    'nb': ['qwertyuiopå', 'asdfghjkløæ', 'zxcvbnm'],
    'sk': ['qwertzuiop', 'asdfghjkl', 'yxcvbnm'],
    'sl': ['qwertzuiop', 'asdfghjkl', 'yxcvbnm'],
    'sv': ['qwertyuiopå', 'asdfghjklöä', 'zxcvbnm'],
    'tr': ['qwertyuıopğü', 'asdfghjklşi', 'zxcvbnmöç'],
}


def definitions():
    result = {}
    for tag in LATIN_LANGUAGES:
        rows = list(MOBILE_ROWS.get(tag, QWERTY))
        upper = [row.replace('i', 'İ').replace('ı', 'I').upper() if tag in ('az', 'tr')
                 else row.upper() for row in rows]
        result[tag] = {'rows': rows, 'shift': upper}
    result['bg'] = {'rows': ['явертъуиопч', 'асдфгхйклшщ', 'зьцжбнмю'],
                    'shift': ['ЯВЕРТЪУИОПЧ', 'АСДФГХЙКЛШЩ', 'ЗЬЦЖБНМЮ']}
    result['be'] = {'rows': ["йцукенгшўзх'", 'фывапролджэ', 'ячсмітьбю'],
                    'shift': ["ЙЦУКЕНГШЎЗХ'", 'ФЫВАПРОЛДЖЭ', 'ЯЧСМІТЬБЮ']}
    result['fa'] = {'rows': ['ضصثقفغعهخحجچ', 'شسیبلاتنمکگ', 'ظطزرذدپوژ'],
                    'shift': ['ضصثقفغعۀخحجچ', 'شسيبلآتنمكگ', 'ظطزرذدپؤژ']}
    result['th'] = {'rows': ['ๅ/-ภถุึคตจขช', 'ๆไำพะัีรนยบล', 'ฟหกดเ้่าสวงฃ', 'ผปแอิืทมใฝ'],
                    'shift': ['+๑๒๓๔ู฿๕๖๗๘๙', '๐"ฎฑธํ๊ณฯญฐ,', 'ฤฆฏโฌ็๋ษศซ.ฅ', '()ฉฮฺ์?ฒฬฦ']}
    # Twelve-key kana groups; holding opens the syllables and related forms.
    rows = ['あかさ', 'たなは', 'まやら', '\u3099わん']
    result['ja'] = {'rows': rows, 'shift': [''.join(chr(ord(c) + 0x60) if 'ぁ' <= c <= 'ゖ' else c
                                                for c in row) for row in rows]}
    for tag in ('zh-CN', 'zh-TW'):
        result[tag] = {'rows': QWERTY, 'shift': [row.upper() for row in QWERTY]}
    return result


def fnv(data):
    value = 2166136261
    for byte in data: value = ((value ^ byte) * 16777619) & 0xffffffff
    return value

def compile_layout(spec):
    payload = bytearray()
    for layer in ('rows','shift'):
        rows = spec[layer]
        if not 1 <= len(rows) <= 4: raise ValueError('Keyboard needs 1–4 character rows')
        for row in rows:
            if not 1 <= len(row) <= 12: raise ValueError('Keyboard row exceeds 12 keys')
            payload.append(len(row))
            for value in row:
                alternate=spec.get('alts',{}).get(value,'')
                for index,text in enumerate((value,alternate)):
                    text=unicodedata.normalize('NFC',text).encode('utf-8')
                    if not (0 if index else 1) <= len(text) <= 15: raise ValueError('Invalid key output')
                    payload.extend(bytes([len(text)]) + text + b'\0')
    if len(payload)>4096: raise ValueError('Keyboard resource exceeds budget')
    return struct.pack('<4sBBBBII',b'CPKB',1,1,len(spec['rows']),len(spec['shift']),len(payload),fnv(payload))+payload

def generate(destination=None):
    destination = destination or ROOT/'assets_local/language-keyboards'
    destination.mkdir(parents=True,exist_ok=True)
    result = definitions()
    (destination/'SOURCE.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    # LANGUAGE_CODES is the firmware's stable short-code namespace.
    tags = {json.loads(line.split(':',1)[1]):p for p in (ROOT/'lib/I18n/translations').glob('*.yaml')
            for line in p.read_text(encoding='utf-8').splitlines() if line.startswith('_bcp47:')}
    for tag, spec in result.items():
        path = tags[tag]
        code = next(json.loads(line.split(':',1)[1]) for line in path.read_text(encoding='utf-8').splitlines() if line.startswith('_language_code:'))
        (destination/(code+'.cpkbd')).write_bytes(compile_layout(spec))
    print(f'{len(result)} optional language keyboard layouts')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--destination',type=Path)
    generate(parser.parse_args().destination)
