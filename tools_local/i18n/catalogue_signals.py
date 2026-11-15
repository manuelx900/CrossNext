"""Inventory signals for contextual/native editorial review; signals are not confirmed errors."""
import argparse
import collections
import importlib.util
import json
from pathlib import Path
import re
import unicodedata

R=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
Q=args.output.resolve();Q.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('audit',R/'tools_local/i18n/audit_context.py')
a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
catalogues={p.stem:a.packs.i18n.parse_yaml_file(str(p)) for p in sorted((R/'lib/I18n/translations').glob('*.yaml'))}
english={k:v for k,v in catalogues['english'].items() if k.startswith('STR_')}
refs=a.source_references()
words=lambda s: re.findall(r"[A-Za-z]+(?:'[A-Za-z]+)?",s.lower())
protected={'crossnext','crossplay','crosspoint','wordle','the intercept','freeink','noto sans','noto serif','libreoffice','google chrome','microsoft edge','wi fi','sd card','crossnext web','pet','arduino','little fs','open ai'}
names=('Hooper','Harris','Liora','Aren','Ada','Iri','Taren','Mara','Nadir','Orin','Sera','Aster')
latin_languages={'azerbaijani','bosnian','catalan','czech','danish','dutch','finnish','french','german','hungarian','indonesia','italian','lithuanian','norwegian','polish','portuguese-BR','portuguese-PT','romanian','slovak','slovenian','swedish','turkish','valencian','vietnamese'}
findings=[]

def add(lang,key,kind,detail):
    findings.append(dict(language=lang,key=key,kind=kind,detail=detail,english=english[key],translation=catalogues[lang][key],active=key in refs,references=refs.get(key,[])))

def numbers(s):
    # Ignore printf fields; allow native decimal separators/digit shapes.
    s=re.sub(r'%[-+ #0]*[\d*]*(?:\.[\d*]+)?(?:hh|ll|[hljztL])?[diouxXfFeEgGaAcsp%]','',s)
    s=''.join(str(unicodedata.digit(c)) if c.isdecimal() else c for c in s)
    return collections.Counter(re.findall(r'(?<!\w)[+-]?\d+(?:[.,]\d+)?(?:\s*%)?',s))

for lang,values in catalogues.items():
    if lang in ('english','spanish','orangutan'):continue
    for key,source in english.items():
        target=values[key]
        # File paths, product names and protocol identifiers intentionally retain their spelling.
        source_for_words=re.sub(r'(?:https?://)?[A-Za-z0-9_./-]+\.[A-Za-z0-9_./-]+|/[^\s]+','',source)
        target_for_words=re.sub(r'(?:https?://)?[A-Za-z0-9_./-]+\.[A-Za-z0-9_./-]+|/[^\s]+','',target)
        w1,w2=words(source_for_words),words(target_for_words)
        if len(w1)>=2 and re.search(r'[A-Za-z]{3}',source_for_words) and source==target and not source.lower().strip(' !:.,') in protected:
            # Separate content titles / technical labels from ordinary prose for review.
            add(lang,key,'english-copy',source)
        elif len(w1)>=3:
            overlaps=[]
            for n in (5,4,3):
                spans={' '.join(w2[i:i+n]) for i in range(len(w2)-n+1)}
                overlaps=[s for i in range(len(w1)-n+1) if (s:=' '.join(w1[i:i+n])) in spans and len(s)>=14 and s not in protected]
                if overlaps:break
            if overlaps:add(lang,key,'english-fragment',list(dict.fromkeys(overlaps)))
        if lang in latin_languages and key.startswith(('STR_INTERCEPT_','STR_MYST_','STR_QUEST_')):
            for name in names:
                # Inflected Latin names retain the identifying stem; very short
                # names require a word boundary to avoid e.g. Hungarian suffixes.
                if re.search(r'\b'+name+r'\b',source) and not re.search(r'\b'+name,target,re.I):
                    add(lang,key,'name-consistency',name)
        if any(token in source for token in ('CrossNext','CrossPoint','CrossPlay')):
            for token in ('CrossNext','CrossPoint','CrossPlay'):
                if token in source and any(other in target for other in ('CrossNext','CrossPoint','CrossPlay') if other!=token):
                    add(lang,key,'brand-mismatch',token)
        # Concrete paths and file extensions form part of the instruction.
        identifiers=re.findall(r'(?<!\w)(?:/\.[A-Za-z][\w.-]*(?:/[A-Za-z][\w./-]*)?|/[A-Z][A-Z0-9_/-]*)',source)
        for ident in identifiers:
            if ident not in target:add(lang,key,'path-changed',ident)
        if re.search(r'\d',source):
            old,new=numbers(source),numbers(target)
            if old!=new:add(lang,key,'numeric-difference',dict(source=old,translation=new))

(Q/'inventory-findings.json').write_text(json.dumps(findings,ensure_ascii=False,indent=2)+'\n','utf8')
summary=dict(catalogues=len(catalogues),sourceKeys=len(english),sourceValues=sum(sum(k.startswith('STR_') for k in v) for v in catalogues.values()),activeKeys=len(refs),candidates=len(findings),linguisticCertification=False,notice='Candidates require context and native editorial review. Shared terminology, inflection, units and numeral formatting can be valid.',byKind=dict(collections.Counter(f['kind'] for f in findings)),byLanguage=dict(collections.Counter(f['language'] for f in findings)))
(Q/'inventory-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n','utf8')
print(json.dumps(summary,ensure_ascii=False))
