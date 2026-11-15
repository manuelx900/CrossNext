"""Generate bounded progression metadata and translated names from the art catalogue."""
import json

UI={
 'CONTINUE_APP': ('Keep reading', 'Seguir leyendo'),
 'VIEW_GIFTS': ('View my rewards', 'Ver mis recompensas'),
 'VICTORY_TITLE': ('The world remembers again', 'El mundo vuelve a recordar'),
 'VICTORY_TEXT': ('You defeated the King of Oblivion. Keep reading, hunting guardians and using your equipment.', 'Has vencido al Rey del Olvido. Puedes seguir leyendo, cazar guardianes y usar tu equipo.'),
 'VICTORY_COLLECTION': ('King of Oblivion collection', 'Colección del Rey del Olvido'),
 'CLOSED_TEXT': ('This journey has ended. You can keep reading, hunting guardians and using your equipment.', 'Este viaje ha terminado. Puedes seguir leyendo, cazar guardianes y usar tu equipo.'),
 'RESTART': ('Start a new campaign', 'Empezar otra campaña'),
 'RESTART_WARNING': ('Starting another campaign will erase this profile\'s progress and equipment.', 'Empezar otra campaña borrará el progreso y el equipo de este perfil.'),
 'DECORATION': ('Decoration', 'Adorno'),
 'DETAIL_TIME': ('Time', 'Tiempo'),
 'DETAIL_XP': ('Reading EXP', 'EXP al leer'),
 'DETAIL_COINS': ('Coins', 'Monedas'),
 'DETAIL_SOURCE': ('How to obtain it', 'Cómo se obtiene'),
 'DETAIL_PRICE': ('%u coins · Level %u', '%u monedas · Nivel %u'),
 'DETAIL_COMPARE': ('Time for this encounter', 'Tiempo de este encuentro'),
 'DETAIL_DURATION': ('%u -> %u min', '%u -> %u min'),
 'DETAIL_UNCHANGED': ('Your current time stays the same', 'Tu tiempo actual no cambia'),
 'DETAIL_LATER': ('Reading EXP. Victory coins.', 'EXP al leer. Monedas al vencer.'),
 'DETAIL_READING': ('Reading benefits', 'Bonificaciones de lectura'),
 'DETAIL_DECOR': ('One decoration at a time', 'Un solo adorno a la vez'),
 'DETAIL_OWNED': ('In your equipment', 'En tu equipo'),
 'FINAL_KING': ('King of Forgetting', 'Rey del Olvido'),
 'REPLAY_LOCATION': ('Saga %s · Chapter %u', 'Saga %s · Capítulo %u'),
 'REPLAY_EMPTY_TITLE': ('Story to recover', 'Relato por recuperar'),
 'REPLAY_EMPTY': ('You have not recovered any passages from this step yet.', 'Todavía no has recuperado ningún pasaje de este paso.'),
 'ALSO_KEEPSAKE': ('Also: %s', 'También: %s'),
 'FROM_REQUEST': ('Earned through a character request', 'Se obtiene con un encargo'),
 'FROM_CHAPTER': ('Earned in chapter %u of saga %s', 'Se obtiene en el capítulo %u de la saga %s'),
 'FROM_VICTORY': ('Earned by completing the campaign', 'Se obtiene al completar la campaña'),
 'CROWN_BODY': ('King of Oblivion armor', 'Armadura del Rey del Olvido'),
 'CROWN_BACKGROUND': ('The recovered throne', 'El trono recuperado'),
 'CROWN_PET': ('Crown dragon', 'Dragón de la corona'),
 'CLOSE_WARNING': ('You can keep reading and completing requests after closing the campaign. Starting another campaign will reset this profile.', 'Al cerrar la campaña podrás seguir leyendo y haciendo encargos. Empezar otra campaña borrará este perfil.'),
}

def generate(root,strings):
    source=json.loads((root/'assets_local/quest/expansion/catalog.json').read_text('utf8'))
    decorations,scenes,enemies=source['decorations'],source['scenes'],source['enemies']
    assert source['version']==1 and len(decorations)==101 and len(scenes)==120 and len(enemies)==72
    assert [d['id'] for d in decorations]==list(range(101))
    assert [d['item'] for d in decorations]==list(range(216,317))
    assert all(len({d.get('soloName', d['name'])[lang] for d in decorations}) == 101 for lang in ('en', 'es'))
    active=source['activeDecorations']
    assert len(active)==60 and len(set(active))==60 and set(range(200,216))<=set(active) and 316 in active
    assert all(200<=item<=316 for item in active)
    chapter_rewards=[320]*120;request_rewards=[320]*300
    rows=[]
    name_keys=[]
    for d in decorations:
        assert d['place'] in (0,1) and d['anchor'] in (0,1)
        assert all(0<=d[field]<=8 for field in ('power','experience','fortune'))
        if d['source']=='Chapter':
            assert 0<=d['sourceId']<120 and chapter_rewards[d['sourceId']]==320
            if d['item'] in active:chapter_rewards[d['sourceId']]=d['item']
        elif d['source']=='Request':
            assert 0<=d['sourceId']<300 and request_rewards[d['sourceId']]==320
            if d['item'] in active:request_rewards[d['sourceId']]=d['item']
        else:assert d['source']=='Victory' and d['item']==316
        rows.append('{'+','.join(map(str,(d['place'],d['anchor'],d['power'],d['experience'],d['fortune'])))+',Source::'+d['source']+','+str(d['sourceId'])+'}')
        label = d.get('soloName', d['name'])
        key = f'STR_QUEST_CAM_{"SOLO_" if "soloName" in d else ""}EXP_DECOR_{d["id"]}'
        strings[key] = label['en'], label['es']
        name_keys.append(key)
    assert sum(v!=320 for v in chapter_rewards)+sum(v!=320 for v in request_rewards)==43
    scene_map=[-1]*120;beats=[]
    for n,scene in enumerate(scenes):
        assert scene['id']==n and 0<=scene['chapter']<120 and scene_map[scene['chapter']]==-1 and scene['beat'] in (1,2,3)
        scene_map[scene['chapter']]=n;beats.append(scene['beat'])
    assert all(n>=0 for n in scene_map)
    request_scenes=[-1]*300
    for scene in source['requestScenes']:
        assert 0<=scene['request']<300 and request_scenes[scene['request']]==-1
        assert scene['index']==len(scenes)+sum(n>=0 for n in request_scenes)
        request_scenes[scene['request']]=scene['index']
    enemy_map=[]
    for n,enemy in enumerate(enemies):
        assert (enemy['book'],enemy['step'])==divmod(n,6)
        assert ('legacy' in enemy)!=('sprite' in enemy)
        enemy_map.append(-1-enemy['legacy'] if 'legacy' in enemy else enemy['sprite'])
    assert sorted(v for v in enemy_map if v>=0)==list(range(60))
    policy='#pragma once\n#include <cstdint>\n// Generated by tools_local/quest/expansion_content.py.\n// clang-format off\nnamespace questexp {\nenum class Source : uint8_t { Chapter, Request, Victory };\nstruct Decoration { uint8_t place,anchor,power,experience,fortune; Source source; uint16_t sourceId; };\ninline constexpr uint16_t EMPTY=320;\ninline constexpr Decoration DECORATIONS[]={\n'+',\n'.join(rows)+'\n};\n'
    policy+='inline constexpr uint16_t MOMENT_SCENES='+str(len(scenes)+len(source['requestScenes']))+';\n'
    for name,kind,values in [('ACTIVE_DECORATIONS','uint16_t',active),('CHAPTER_REWARDS','uint16_t',chapter_rewards),('REQUEST_REWARDS','uint16_t',request_rewards),('SCENES','int16_t',scene_map),('SCENE_BEATS','uint8_t',beats),('REQUEST_SCENES','int16_t',request_scenes),('ENEMIES','int16_t',enemy_map)]:
        policy+='inline constexpr '+kind+' '+name+'[]={'+','.join(map(str,values))+'};\n'
    policy+='}\n// clang-format on\n'
    (root/'src/apps_local/quest/QuestExpansionPolicy.h').write_text(policy,'utf8')
    text='#pragma once\n#include <I18n.h>\n// Generated by tools_local/quest/expansion_content.py.\n// clang-format off\nnamespace questexp {\ninline constexpr StrId DECOR_NAMES[]={'+','.join(f'StrId::{key}' for key in name_keys)+'};\n}\n// clang-format on\n'
    (root/'src/apps_local/quest/QuestExpansionText.h').write_text(text,'utf8')
    for key,values in UI.items():strings['STR_QUEST_CAM_EXP_'+key]=values
    return source
