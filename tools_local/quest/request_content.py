"""Generate bounded request tables from the authored bilingual catalogue."""
from pathlib import Path
import json

UI = {
 'HUNT_ACCEPT': ('Start hunt', 'Iniciar cacería'),
 'HUNT_CANCEL': ('Cancel', 'Cancelar'),
 'HUNT_BACK': ('Back to hunts', 'Volver a cacerías'),
 'HUNT_READ': ('Read your book to defeat it', 'Lee tu libro para vencerlo'),
 'HUNT_TARGET': ('Reading required', 'Lectura necesaria'),
 'HUNT_DEFEATED': ('Target defeated', 'Objetivo vencido'),
 'HUNT_PROGRESS': ('Hunt progress', 'Avance de cacería'),
 'HUNT_SEE_PROGRESS': ('See hunt progress', 'Ver avance de cacería'),
 'HUNT_LEGACY': ('Previous request', 'Encargo anterior'),
 'REQUEST_AVAILABLE': ('Available', 'Disponibles'),
 'REQUEST_COLLECTION': ('Keepsakes', 'Recuerdos'),
 'HUNT_AVAILABLE_HINT': ('Hunts are available.', 'Hay cacerías disponibles.'),
 'REQUEST_AVAILABLE_COUNT': ('%u requests available.', '%u encargos disponibles.'),
 'REQUEST_COMPLETE_COLLECTION': ('There are no more hunts available.', 'No hay más cacerías disponibles.'),
 'REQUEST_ACCEPT': ('Accept request', 'Aceptar encargo'),
 'REQUEST_COLLECT': ('Collect rewards', 'Recoger recompensas'),
 'REQUEST_ACTIVE': ('In progress', 'En curso'),
 'REQUEST_CANCEL': ('Cancel hunt', 'Cancelar cacería'),
 'REQUEST_SWITCH': ('Choose this request', 'Elegir este encargo'),
 'REQUEST_CANCEL_TITLE': ('Cancel this hunt?', '¿Cancelar esta cacería?'),
 'REQUEST_SWITCH_TITLE': ('Switch hunts?', '¿Cambiar de cacería?'),
 'REQUEST_LOSE_PROGRESS': ('You will lose this progress. You can start the hunt again later.', 'Perderás este avance. Podrás volver a iniciar la cacería.'),
 'REQUEST_SWITCH_WARNING': ('You will lose your current progress. The new hunt starts from zero.', 'Perderás el avance actual. La nueva cacería empieza desde cero.'),
 'REQUEST_KEEP_CURRENT': ('Keep current hunt', 'Conservar cacería'),
 'REQUEST_NEW_HUNT': ('New hunt', 'Nueva cacería'),
 'REQUEST_CONFIRM_SWITCH': ('Switch hunt', 'Cambiar cacería'),
 'REQUEST_ACCEPT_FIRST': ('Reading counts after accepting this request.', 'La lectura cuenta después de aceptar este encargo.'),
 'REQUEST_OTHER_ACTIVE': ('View current request', 'Ver encargo en curso'),
 'REQUEST_EMPTY': ('Defeat more guardians in Campaign to unlock hunts.', 'Vence guardianes en Campaña para abrir nuevas cacerías.'),
 'REQUEST_ALL_COLLECTED': ('More hunts unlock as you advance in Campaign.', 'Aparecerán más cacerías al avanzar en Campaña.'),
 'REQUEST_COLLECTION_EMPTY': ('Guardians defeated in hunts will appear here.', 'Aquí aparecerán los guardianes que venzas en Cacerías.'),
 'REQUEST_REWARD_OBJECT': ('Unique keepsake', 'Recuerdo único'),
 'REQUEST_MINUTES_SHORT': ('%u min', '%u min'),
 'REQUEST_OWNED': ('Collected', 'Conseguido'),
 'REQUEST_FROM': ('Given by %s', 'Entregado por %s'),
 'REQUEST_READING_LINK_LEGACY': ('Your reading helps preserve this memory.', 'Tu lectura ayuda a conservar este recuerdo.'),
 'REQUEST_RECEIVED': ('Your rewards', 'Tus recompensas'),
 'REQUEST_RECEIVED_TEXT': ('Thank you for helping. This keepsake is for you.', 'Gracias por tu ayuda. Este recuerdo es para ti.'),
 'ATLAS_EXPLAIN': ('Revisit the places you restored.', 'Visita los lugares recuperados.'),
 'ATLAS_REGION_EXPLAIN': ('Open a place to see the path you cleared.', 'Abre un lugar para ver el Camino que completaste.'),
 'ATLAS_CHAPTER_NUMBER': ('Chapter %u', 'Capítulo %u'),
 'ATLAS_LOCKED_NOTE': ('Continue in Campaign', 'Continúa en Campaña'),
}

UI.update({'ORDER_CLEAR_PATH': ('Path cleared', 'Camino despejado'),
 'ORDER_READY': ('Ready to collect', 'Listo para recoger'),
 'ORDER_CONTINUE_TRIP': ('Continue journey', 'Continuar viaje'),
 'ORDER_MINUTES': ('Read for %u minutes', 'Leer %u minutos'),
 'ORDER_PAGES': ('Read %u pages', 'Leer %u páginas'),
 'ORDER_SESSIONS': ('Complete %u sessions', 'Completar %u sesiones'),
 'ORDER_BOOKS': ('Finish %u book', 'Terminar %u libro'),
 'ORDER_DAYS': ('Read on %u additional days', 'Leer otros %u días'),
 'ORDER_PROGRESS_MINUTES': ('%u / %u min', '%u / %u min'),
 'ORDER_PROGRESS_PAGES': ('%u / %u pages', '%u / %u pág.'),
 'ORDER_PROGRESS_SESSIONS': ('%u / %u sessions', '%u / %u sesiones'),
 'ORDER_PROGRESS_SHORT_SESSIONS': ('%u / %u sess.', '%u / %u ses.'),
 'ORDER_PROGRESS_BOOKS': ('%u / %u book', '%u / %u libro'),
 'ORDER_PROGRESS_DAYS': ('%u / %u days', '%u / %u días'),
 'ORDER_SHORT_MINUTES': ('%u min', '%u min'),
 'ORDER_SHORT_PAGES': ('%u pages', '%u pág.'),
 'ORDER_SHORT_SESSIONS': ('%u sessions', '%u sesiones'),
 'ORDER_SHORT_BOOKS': ('%u book', '%u libro'),
 'ORDER_SHORT_DAYS': ('%u days', '%u días'),
 'ORDER_AFTER_ACCEPT': ('Counts after you accept',
                        'Cuenta desde que lo aceptas'),
 'ORDER_BACK_REQUESTS': ('Back to requests', 'Volver a encargos'),
 'ORDER_REQUEST_DONE': ('Request completed', 'Encargo completado'),
 'ORDER_REQUEST_PROGRESS': ('Request progress', 'Avance del encargo'),
 'ORDER_SEE_REWARD': ('View reward', 'Ver recompensa'),
 'ORDER_ATLAS_EMPTY': ('Places restored in this saga will appear here.',
                       'Aquí aparecerán los lugares que recuperes en esta '
                       'saga.'),
 'ORDER_SHOP_NEXT_LEVEL': ('New items appear as your hero levels up.',
                           'Aparecerán más objetos cuando tu héroe suba de '
                           'nivel.')})

def generate(root, strings):
 data=json.loads((root/'assets_local/quest/campaign/requests.json').read_text('utf-8'))
 rows=data['requests'];assert data['version']==1 and len(rows)==300
 header='#pragma once\n// Generated by tools_local/quest/request_content.py.\n// clang-format off\n#include <I18n.h>\nnamespace questrequests {\nenum class Shape : uint8_t { Bottle, Key, Ribbon, Book, Letter, Quill, Leaf, Lantern, Gear, Medal, Star, Shell, Bell, Cup, Tool, Token, Compass, Needle, Clock, Map, Scale, Crystal, Mirror, Crown, Ink, Flag, Bowl, Spool, Bookmark, Antler };\nstruct Text { StrId title, summary, request, item, readingLink; Shape shape; };\ninline constexpr Text TEXTS[] = {\n'
 for n,row in enumerate(rows):
  assert row['id']==n and row['chapter']==n//25*10+n%25*10//25
  keys=[]
  assert row['goal'] in ('Minutes','Pages','Sessions','Books','Days')
  for field in ('title','summary','request','item','readingLink'):
   key=f'STR_QUEST_CAM_REQ_{n}_{field.upper()}'
   value=row[field]
   if field=='item' and 'huntItem' in row:
    key=f'STR_QUEST_CAM_HUNT_ITEM_{n}'
    value=row['huntItem']
   assert all(value[lang] and '%' not in value[lang] for lang in ('en','es'))
   if field=='summary':assert all(len(value[lang])<=70 for lang in ('en','es'))
   strings[key]=(value['en'],value['es']);keys.append('StrId::'+key)
  header+=' {'+','.join(keys)+','+'Shape::'+row['shape']+'},\n'
 header+='};\nstatic_assert(sizeof(TEXTS)/sizeof(TEXTS[0])==300);\n}  // namespace questrequests\n// clang-format on\n'
 for key,value in UI.items():strings['STR_QUEST_CAM_'+key]=value
 for field in ('title','item'):
  for lang in ('es','en'):
   assert len({(r.get('huntItem', r[field]) if field=='item' else r[field])[lang] for r in rows})==300
 (root/'src/apps_local/quest/QuestRequestData.h').write_text(header,'utf-8')
 goals='#pragma once\n// Generated by tools_local/quest/request_content.py.\n// clang-format off\n#include "QuestCore.h"\nnamespace quest {\ninline constexpr RequestGoal REQUEST_GOALS[] = {\n'
 goals+=',\n'.join('RequestGoal::'+row['goal'] for row in rows)
 goals+='\n};\nstatic_assert(sizeof(REQUEST_GOALS)/sizeof(REQUEST_GOALS[0])==kLegacyRequestCount);\n}\n// clang-format on\n'
 (root/'src/apps_local/quest/QuestRequestGoals.h').write_text(goals,'utf8')
