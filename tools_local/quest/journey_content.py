"""Generate atlas labels and location metadata from story.json."""

UI = {
    'ATLAS': ('Atlas', 'Atlas'),
    'ATLAS_TITLE': ('The world you remember', 'El mundo que recuerdas'),
    'ATLAS_PROGRESS': ('%u / 120 memories', '%u / 120 recuerdos'),
    'ATLAS_REGION': ('%u / 10 recovered', '%u / 10 recuperados'),
    'ATLAS_VISIT': ('Visit this memory', 'Visitar este recuerdo'),
    'ATLAS_RESTORED': ('Recovered', 'Recuperado'),
    'ATLAS_NEW': ('A memory in your atlas', 'Un recuerdo en tu atlas'),
    'ATLAS_HIDDEN': ('Still to discover', 'Por descubrir'),
    'ATLAS_EMPTY': ('The atlas will remember the places and lives you help restore. Begin in Campaign.', 'El atlas recordará los lugares y las vidas que ayudes a recuperar. Empieza en Campaña.'),
    'ATLAS_ROADS': ('Your reading opens roads and brings the world back.', 'Tu lectura abre caminos y devuelve la vida al mundo.'),
    'REQUESTS': ('Hunts', 'Cacerías'),
    'REQUEST_OF': ('A request from %s', 'Un encargo de %s'),
    'REQUEST_DAILY': ('For today', 'Para hoy'),
    'REQUEST_WEEK': ('A little at a time', 'Un poco cada día'),
    'REQUEST_STORY': ('The current account', 'El relato actual'),
    'REQUEST_OPEN': ('Read request', 'Ver encargo'),
    'REQUEST_THANKS': ('A few words for you', 'Unas palabras para ti'),
    'REQUEST_WAIT': ('Read the chapter ending first', 'Lee primero el desenlace'),
    'REQUEST_GIFT': ('A keepsake from this chapter', 'Un recuerdo de este capítulo'),
    'REQUEST_FULFILLED': ('This request is fulfilled. Its thanks are waiting for you.', 'Este encargo está cumplido. Puedes recoger su agradecimiento.'),
    'REQUEST_STEPS': ('%u / 6 steps', '%u / 6 pasos'),
    'REQUEST_KEPT': ('Already in your equipment', 'Ya está en tu equipo'),
    'REQUEST_START': ('Defeat your first guardian to unlock hunts.', 'Derrota al primer guardián para abrir las cacerías.'),
    'RECAP': ('Where you left off', 'Donde dejaste la historia'),
    'SOLO_RECAP_PROLOGUE': ('You found an atlas and learned that reading weakens the ink wolf.', 'Encontraste un atlas y descubriste que leer debilita al lobo de tinta.'),
    'RECAP_NEXT': ('Your next step', 'Tu siguiente paso'),
    'RECAP_READ': ('Reread the account', 'Releer el relato'),
    'RECAP_RESUME': ('Continue the journey', 'Retomar el viaje'),
    'JOURNEY_COMBAT': ('Combat', 'Combate'),
    'JOURNEY_EXPLORE': ('Exploration', 'Exploración'),
    'JOURNEY_RESCUE': ('Rescue a memory', 'Liberar un recuerdo'),
    'JOURNEY_RESTORE': ('Restoration', 'Restauración'),
    'JOURNEY_COMPLETE': ('Step completed', 'Paso completado'),
    'JOURNEY_BEGIN': ('Begin this step', 'Empezar este paso'),
    'JOURNEY_DEFEATED': ('Guardian defeated', 'Guardián vencido'),
    'JOURNEY_FOUND': ('Route discovered', 'Ruta descubierta'),
    'JOURNEY_FREED': ('Memory freed', 'Recuerdo liberado'),
    'JOURNEY_REPAIRED': ('Words restored', 'Palabras recuperadas'),
    'JOURNEY_ONGOING': ('Your reading is helping this account return.', 'Tu lectura ayuda a devolver este relato.'),
}

LANDMARKS = ['forest','town','bridge','library','tower','harbor','mountain','well','clock','palace','garden','gate','house','fortress','lighthouse']

def generate(data, root, strings):
    journey=data['journey']
    assert journey['version']==1
    assert journey['kinds']==['combat','explore','rescue','restore','explore','combat']
    regions=journey['regions']
    assert [r['book'] for r in regions]==list(range(12))
    for key,value in UI.items(): strings['STR_QUEST_CAM_'+key]=value
    arrays={}
    def column(array, field):
        values=[]
        for n,r in enumerate(regions):
            key=f'STR_QUEST_CAM_JOURNEY_{field.upper()}_{n}'
            strings[key]=(r[field]['en'],r[field]['es'])
            values.append(key)
        arrays[array]=values
    for array,field in [('PATRONS','patron'),('DAILY_TITLES','dailyTitle'),('DAILY_REQUESTS','dailyRequest'),('WEEKLY_TITLES','weeklyTitle'),('WEEKLY_REQUESTS','weeklyRequest'),('THANKS','thanks'),('GIFT_FROM','giftFrom')]:
        column(array,field)
    assert all(c['landmark'] in LANDMARKS for c in data['chapters'])
    header='#pragma once\n// clang-format off\n#include <I18n.h>\n#include "QuestJourney.h"\nnamespace questjourney {\n'
    for array,values in arrays.items():
        header+=f'inline constexpr StrId {array}[] = {{'+','.join('StrId::'+k for k in values)+'};\n'
    header+='inline constexpr StrId TITLES[][6] = {\n'
    for b,r in enumerate(regions):
        assert len(r['titles'])==6
        keys=[]
        for n,title in enumerate(r['titles']):
            key=f'STR_QUEST_CAM_JOURNEY_TITLE_{b}_{n}'
            strings[key]=(title['en'],title['es']);keys.append('StrId::'+key)
        header+='  {'+','.join(keys)+'},\n'
    header+='};\ninline constexpr Landmark LANDMARKS[] = {'+','.join('Landmark::'+c['landmark'].title() for c in data['chapters'])+'};\n}\n'
    (root/'src/apps_local/quest/QuestJourneyData.h').write_text(header,'utf-8')
