"""Build disposable simulator fixtures with the production profile codec."""
import argparse
import base64
from datetime import datetime, timezone, timedelta
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import yaml
from bestiary_catalog import load as load_bestiary
from simulator_languages import export_languages

ROOT = Path(__file__).resolve().parents[2]

def build(output):
    output.mkdir(parents=True, exist_ok=True)
    windows = os.name == 'nt'
    compiler = str((ROOT / '.venv/emsdk/upstream/emscripten/em++.exe').resolve()) if windows else os.environ.get('CXX', 'c++')
    flags = ['-sENVIRONMENT=node', '-sNODERAWFS=1', '-sEXIT_RUNTIME=1', '-sSTACK_SIZE=1048576'] if windows else []
    binary = output / ('seed.cjs' if windows else 'seed')
    subprocess.run([compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-Ihost-tests/study-suite/stubs',
                    'tools_local/quest/seed_simulator.cpp', 'src/apps_local/quest/QuestCore.cpp', 'src/apps_local/quest/QuestCampaign.cpp',
                    'src/apps_local/reading/ReadingStatsCore.cpp', *flags, '-o', str(binary)], cwd=ROOT, check=True)
    with tempfile.TemporaryDirectory(prefix='quest-fixtures-', dir=output) as temporary:
        generated = Path(temporary).resolve()
        if not generated.is_relative_to(output.resolve()):
            raise ValueError('Fixture staging must remain in the output directory')
        now = datetime.now(timezone(timedelta(hours=-3)))
        subprocess.run([*([shutil.which('node')] if windows else []), str(binary), str(generated), str(int(now.timestamp())), '-10800'], cwd=ROOT, check=True)
        source = ROOT / 'assets_local/quest'
        story = json.loads((source / 'campaign/story.json').read_text('utf8'))
        expansion = json.loads((source / 'expansion/catalog.json').read_text('utf8'))
        translations = yaml.safe_load((ROOT / 'lib/I18n/translations/spanish.yaml').read_text('utf8'))
        def current(key):
            for prefix in ("JOURNEY2_", "SOLO_", ""):
                if (revised := f"STR_QUEST_CAM_{prefix}{key}") in translations:
                    return translations[revised]
            raise KeyError(key)

        gear = []
        for group, first, count in [('body',0,100),('background',100,50),('pet',150,50)]:
            gear.extend({'id':first+n,'category':group,'name':translations[f'STR_QUEST_CATALOG_{group.upper()}_{n}']} for n in range(count))
        gear.extend({'id':200+d['id'],'category':'decoration','name':current(f"DECOR_NAME_{d['id']}")}
                    for d in story['decorations'])
        gear.extend({'id':d['item'],'category':'decoration','name':current(f"EXP_DECOR_{d['id']}")}
                    for d in expansion['decorations'] if d['item'] in expansion['activeDecorations'])
        gear.extend({'id':n,'category':group,'name':translations[key]} for n,group,key in [
            (317,'body','STR_QUEST_CAM_EXP_CROWN_BODY'),(318,'background','STR_QUEST_CAM_EXP_CROWN_BACKGROUND'),(319,'pet','STR_QUEST_CAM_EXP_CROWN_PET')])
        def encoded(path): return base64.b64encode(path.read_bytes()).decode('ascii')
        roster = [0,1,2,3,4,0,1,2,3,5]
        battles = []
        for chapter in story["chapters"]:
            n = chapter["id"]; family = 72 if n >= 118 else 71 if n == 110 else n // 10 * 6 + roster[n % 10]
            title = (translations["STR_QUEST_CAM_EXP_FINAL_KING"] if n == 119 else
                     translations["STR_QUEST_CAM_KING_SHADOW"] if n == 118 else
                     translations[f"STR_QUEST_CAM_ENEMY_{family // 6}_{family % 6}"])
            battles.append(dict(chapter=n,book=n//10,anchor=n*6+5,family=family,title=title,
                                episode=current(f"CHAPTER_{n}")))
        families = {battle['family']: battle['title'] for battle in battles if battle['chapter'] != 119}
        families[73] = translations["STR_QUEST_CAM_EXP_FINAL_KING"]
        assert len(families) == 74
        gear.extend({'id':321+n,'category':'body','name':translations['STR_QUEST_HUNTS_OUTFIT_NAME'] % families[n]}
                    for n in range(74))
        gear.extend({'id':395+n,'category':'body','name':translations[f'STR_QUEST_HUNTS_{key}_OUTFIT']}
                    for n,key in enumerate(('STORM','CRYSTAL','PHOENIX','INK')))
        gear.append({'id':399,'category':'decoration','name':translations['STR_QUEST_HUNTS_INK_RELIC']})
        bestiary=load_bestiary(source/'bestiary')
        gear.extend({'id':400+n,'category':'body','name':entry['outfit']['es']} for n,entry in enumerate(bestiary))
        catalog = dict(version=6,languages=export_languages(output),levels=sorted({int(p.stem.split('-')[2]) for p in generated.glob('shop-level-*.bin')}),bestiary=bestiary,battles=battles,generatedAt=now.isoformat(),journal=encoded(generated/'journal.bin'),
                       journals={p.stem.removesuffix('.history'):encoded(p) for p in generated.glob('*.history.bin')},
                       backgrounds=json.loads((source/'redesign/backgrounds.json').read_text('utf8'))['items'],
                       books=[{'id':b['id'],'title':current(f"BOOK_{b['id']}")} for b in story['books']],
                       roles=['combat'] * 720,
                       enemies=[{'book':e['book'],'step':e['step'],'title':e['title']['es']} for e in expansion['enemies']],
                       gear=gear, states={p.stem:encoded(p) for p in generated.glob('*.bin') if p.stem!='journal' and not p.stem.endswith('.history')})
        assert len([g for g in gear if g['category']=='decoration'])==61
        shutil.copy2(ROOT/'tools_local/quest/simulator.js', output/'simulator.js')
        (output/'catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,separators=(',',':')),'utf8')
        print(f"Simulator: {len(catalog['states'])} validated states;120 battles;73 guardian illustrations;60 active decorations")
        return output/'catalog.json'

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'.cache/quest-simulator')
    build(parser.parse_args().output.resolve())
