import json, re, struct, pathlib, sys, importlib.util
root=pathlib.Path(__file__).resolve().parents[2]
out=root/'.cache/locale-ui/fixtures';out.mkdir(parents=True,exist_ok=True)
header=(root/'lib/I18n/I18nKeys.h').read_text(encoding='utf8')
keys=re.findall(r'^  (STR_\w+),',header,re.M)
langs=re.findall(r'^  (\w+)\s*=\s*(\d+),\s*//\s*(\S+)',header,re.M)
spec=importlib.util.spec_from_file_location('gen_i18n',root/'scripts/gen_i18n.py')
gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
files={}
for p in (root/'lib/I18n/translations').glob('*.yaml'):
 d=gen.parse_yaml_file(str(p));files[d['_bcp47']]=d
en=files['en']
with (out/'locales.inc').open('w',encoding='utf8') as f:
 f.write('static constexpr const char* localeCodes[] = {'+','.join(json.dumps(x[2]) for x in langs)+'};\n')
for enum,idx,code in langs:
 d=files[code];(out/(str(idx)+'.json')).write_text(json.dumps({'code':code,'texts':[str(d.get(k,en[k])) for k in keys]},ensure_ascii=False),encoding='utf8')
 # One NUL-terminated string per ID; the host reads this independently of firmware packing.
 (out/(str(idx)+'.bin')).write_bytes(b''.join(str(d.get(k,en[k])).encode('utf8')+b'\0' for k in keys))
print(len(langs),'locales;',len(keys),'string IDs')
