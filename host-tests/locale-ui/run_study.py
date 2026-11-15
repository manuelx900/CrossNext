"""Render Study with all four real interface faces; fail edited-copy regressions and retain unchanged diagnostics."""
import importlib.util,sys,os,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[2]
import argparse,json,re
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--strict',action='store_true',help='Also fail on unchanged legacy copy');args=parser.parse_args()
Q=args.output.resolve();Q.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('q',R/'host-tests/quest-locales/run.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
sdk=(R/'.venv/emsdk').resolve();cc=sdk/'upstream/emscripten'/('em++.exe' if os.name=='nt' else 'em++');node=sorted((sdk/'node').glob('*'))[-1]/('node.exe' if os.name=='nt' else 'bin/node')
sources=['host-tests/locale-ui/study.cpp','src/apps_local/study/StudyScreens.cpp','src/apps_local/flashcards/FlashScreens.cpp','src/apps_local/ui/PixelMenu.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp','lib/MiniBidi/BidiUtils.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
subprocess.run([sys.executable,str(R/'host-tests/quest-locales/prepare.py')],cwd=R,check=True)
subprocess.run([str(cc.with_name('emcc.exe' if os.name=='nt' else 'emcc')),'-O1','-c',str(R/'lib/MiniBidi/minibidi.c'),'-o',str(Q/'bidi.o')],cwd=R,check=True)
subprocess.run([str(cc),'-std=c++20','-O1','-Wno-comment','-sENVIRONMENT=node','-sNODERAWFS=1','-sALLOW_MEMORY_GROWTH=1','-sEXIT_RUNTIME=1','-sSTACK_SIZE=1048576',*['-I'+p for p in q.INCLUDES],*sources,str(Q/'bidi.o'),'-o',str(Q/'study.cjs')],cwd=R,check=True)
p=subprocess.run([str(node),str(Q/'study.cjs'),str(R),str(Q)],text=True,encoding='utf8',capture_output=True);(Q/'run.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr)
if p.returncode not in (0,1):sys.exit(p.returncode)

# The full builders also draw unchanged strings. Keep those diagnostics visible
# instead of presenting them as a passing whole-screen layout certification.
spec=importlib.util.spec_from_file_location('audit',R/'tools_local/i18n/audit_context.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
catalogues={p.stem:audit.packs.i18n.parse_yaml_file(str(p)) for p in (R/'lib/I18n/translations').glob('*.yaml')}
codes={name:values['_bcp47'] for name,values in catalogues.items()}
entries=json.loads((R/'tools_local/i18n/complete-regressions.json').read_text('utf8'))
fmt=re.compile(r'%[-+ #0]*(?:[0-9]+|\*)?(?:\.(?:[0-9]+|\*))?(?:hh|ll|[hljztL])?([diuoxXfFeEgGaAcsp%])')
def pattern(value):
    parts=[];end=0
    for match in fmt.finditer(value):
        parts.append(re.escape(value[end:match.start()]))
        parts.append('' if match[0]=='%.0s' else r'[0-9]+' if match[1] in 'diuoxX' else '.*' if match[1]=='s' else re.escape('%') if match[1]=='%' else '.+')
        end=match.end()
    return re.compile(''.join(parts)+re.escape(value[end:]),re.IGNORECASE)
patterns=[(codes[e['language']],e['key'],pattern(e['after'])) for e in entries]
all_issues=[json.loads(line) for line in (Q/'study-issues.jsonl').read_text('utf8').splitlines()]
changed={(e['language'],e['key']) for e in entries}
keys=set()
for source in ('src/apps_local/study/StudyScreens.cpp','src/apps_local/study/StudyActivity.cpp','src/apps_local/flashcards/FlashScreens.cpp','src/apps_local/ui/PixelMenu.cpp'):
    keys.update(re.findall(r'\bSTR_\w+', (R/source).read_text('utf8')))
unchanged=[]
for name,values in catalogues.items():
    for key in keys:
        value=values.get(key,catalogues['english'].get(key,''))
        if (name,key) not in changed and value:
            unchanged.append((codes[name],key,pattern(value)))
edited=[];legacy=[];unknown=[]
for issue in all_issues:
    keys=[key for code,key,regex in patterns if code==issue['language'] and regex.fullmatch(issue['text'])]
    inherited=[key for code,key,regex in unchanged if code==issue['language'] and regex.fullmatch(issue['text'])]
    (edited if keys else legacy if inherited else unknown).append(dict(issue,editedKeys=keys,unchangedKeys=inherited))
(Q/'edited-issues.json').write_text(json.dumps(edited,ensure_ascii=False,indent=2)+'\n','utf8')
(Q/'legacy-issues.json').write_text(json.dumps(legacy,ensure_ascii=False,indent=2)+'\n','utf8')
(Q/'unclassified-issues.json').write_text(json.dumps(unknown,ensure_ascii=False,indent=2)+'\n','utf8')
summary={'renderedFrames':int(re.search(r'Study: (\d+) frames',p.stdout)[1]),'languages':39,'interfaceFaces':4,'orientations':2,'editedCopyIssues':len(edited),'unchangedCopyDiagnostics':len(legacy),'unclassifiedIssues':len(unknown),'wholeScreenCertification':False}
(Q/'summary.json').write_text(json.dumps(summary,indent=2)+'\n','utf8');print(json.dumps(summary))
sys.exit(bool(edited or unknown or (args.strict and legacy)))

