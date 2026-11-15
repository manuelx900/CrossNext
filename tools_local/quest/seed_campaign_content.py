"""Generate translated campaign tables from story.json and paths.json without rewriting either."""
from pathlib import Path
import json
import importlib.util

_journey_spec = importlib.util.spec_from_file_location('quest_journey_content', Path(__file__).with_name('journey_content.py'))
_journey = importlib.util.module_from_spec(_journey_spec)
_journey_spec.loader.exec_module(_journey)
generate_journey = _journey.generate
_request_spec = importlib.util.spec_from_file_location("quest_request_content", Path(__file__).with_name("request_content.py"))
_request = importlib.util.module_from_spec(_request_spec)
_request_spec.loader.exec_module(_request)
_expansion_spec = importlib.util.spec_from_file_location("quest_expansion_content", Path(__file__).with_name("expansion_content.py"))
_expansion = importlib.util.module_from_spec(_expansion_spec)
_expansion_spec.loader.exec_module(_expansion)
_scene_spec = importlib.util.spec_from_file_location("quest_scene_content", Path(__file__).with_name("scene_content.py"))
_scene = importlib.util.module_from_spec(_scene_spec)
_scene_spec.loader.exec_module(_scene)

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets_local/quest/campaign"

UI = {
 "READER_PREVIOUS": ("Previous", "Anterior"), "READER_NEXT": ("Next", "Siguiente"),
 "READER_JUMP": ("Go to page", "Ir a la página"), "READER_RANGE": ("Page 1 to %d", "Página 1 a %d"),
 "READER_ERASE": ("Erase", "Borrar"), "READER_OPEN": ("Open", "Abrir"),
 "READER_CANCEL": ("Cancel", "Cancelar"),
 "DECORATIONS": ("Decor", "Adornos"), "PET": ("PET", "PET"),
 "DISCOVERY": ("A new discovery", "Un nuevo hallazgo"),
 "PLACE_REWARD": ("Place in my scene", "Colocar en mi escena"),
 "KEEP_REWARD": ("Keep in my equipment", "Guardar en mi equipo"),
 "REWARD_SAVED": ("This discovery is now part of your story.", "Este hallazgo ya forma parte de tu historia."),
 "NEXT_REWARD": ("Next discovery", "Próximo hallazgo"),
 "REWARD_SOURCE": ("Saga %s · Ch. %u", "Saga %s · Cap. %u"),
 "STORY_ONLY": ("Earn through the story", "Gánalo en la historia"),
 "DECOR_UPPER": ("Sky decoration · replaces the current one", "Adorno elevado · reemplaza el actual"),
 "DECOR_GROUND": ("Ground decoration · replaces the current one", "Adorno de suelo · reemplaza el actual"),
 "TITLE": ("Campaign", "Campaña"), "SETTINGS": ("Settings", "Ajustes"), "DATA": ("Reading data", "Datos de lectura"),
 "MY_CAMPAIGN": ("My campaign", "Mi campaña"), "BONUSES": ("Bonuses", "Bonificaciones"), "ENCOUNTER": ("Encounter", "Encuentro"),
 "PATH": ("Path", "Camino"), "SAGA": ("Saga", "Saga"), "SAGA_TITLE": ("The saga of Forgetting", "La saga del Olvido"),
 "REMAINING": ("%u min left", "%u min restantes"), "BOOK_OF": ("Book %s / XII", "Libro %s / XII"),
 "BOOK": ("Book %s", "Libro %s"), "CHAPTER": ("Chapter %u", "Capítulo %u"), "PERCENT": ("%u%%", "%u%%"),
 "FRACTION": ("%u.%u%%", "%u,%u%%"), "POWER": ("Less time", "Menos tiempo"), "READ_XP": ("Reading EXP", "EXP de lectura"),
 "MORE_COINS": ("More coins", "Más monedas"), "MINUS": ("-%u%%", "-%u%%"), "PLUS": ("+%u%%", "+%u%%"),
 "AFFINITY": ("Region affinity", "Afinidad regional"), "COMPARE": ("Combat: %u → %u min", "Combate: %u → %u min"),
 "LIMITS": ("Time: −25% · EXP: +30% · Coins: +20%. Recorded reading time is kept.", "Tiempo: −25% · EXP: +30% · Monedas: +20%. Se conserva el tiempo de lectura registrado."),
 "DAILY": ("A page of light", "Una página de luz"), "WEEKLY": ("The path lights up", "El camino se ilumina"),
 "MISSION_DAILY": ("Daily mission", "Misión diaria"), "MISSION_WEEKLY": ("Weekly mission", "Misión semanal"),
 "REQUEST_HINT": ("Campaign rematches", "Revanchas de Campaña"), "OPEN_REQUESTS": ("View hunts", "Ver cacerías"),
 "TODAY": ("TODAY", "HOY"), "WEEK": ("WEEK", "SEMANA"), "STORY": ("STORY", "HISTORIA"),
 "RECEIVE": ("Collect", "Recoger"), "MEMORY": ("Memory", "Recuerdo"), "IN_USE": ("In use", "En uso"), "AVAILABLE": ("Available", "Disponible"),
 "VICTORY": ("Victory", "Victoria"), "REWARD_READY": ("Reward ready", "Recompensa lista"), "NEW_PLACE": ("New background", "Nuevo fondo"),
 "STORY_READY": ("A story awaits", "Una historia te espera"), "OPEN_STORY": ("View story", "Ver historia"), "NEXT": ("Continue", "Seguir"),
 "BEGIN_INVITE": ("Begin your adventure", "Inicia tu aventura"), "BEGIN_HINT": ("Open Campaign to begin", "Abre Campaña para comenzar"),
 "BEGIN": ("Begin the journey", "Comenzar el viaje"), "INTRO_TITLE": ("Prologue", "Prólogo"), "INTRO_PAGE": ("%u / %u", "%u / %u"),
 "CHOOSE_SAGA": ("Choose your saga", "Elige tu saga"), "FOLLOW_PATH": ("Follow your path", "Sigue tu camino"),
 "NEXT_STEP": ("Discover your next step", "Descubre tu siguiente paso"), "DISCOVER": ("Discover the encounter", "Descubre el encuentro"),
 "OPEN_SAGA": ("View sagas", "Ver sagas"), "OPEN_PATH": ("View path", "Ver camino"),
 "SAGA_NUMBER": ("Saga %s", "Saga %s"), "START_SAGA": ("Start saga %s", "Iniciar saga %s"),
 "RESUME_SAGA": ("Continue saga %s", "Continuar saga %s"), "START_ENCOUNTER": ("Begin encounter", "Comenzar encuentro"),
 "BACK_SAGA": ("Back to sagas", "Volver a Saga"), "BACK_PATH": ("Return to path", "Volver al Camino"),
 "SAGA_LOCKED": ("Locked", "Bloqueada"),
 "RETURN_TITLE": ("Your reading makes a difference", "Tu lectura deja huella"),
 "RETURN_TIME": ("%llu min read", "%llu min leídos"), "RETURN_SECONDS": ("%llu s read", "%llu s leídos"),
 "RETURN_PROGRESS_SECONDS": ("%u s closer", "%u s de avance"),
 "RETURN_XP": ("+%llu EXP", "+%llu EXP"), "RETURN_PROGRESS": ("%u min closer", "%u min de avance"),
 "RETURN_BANKED": ("Reading saved for your next encounter", "Lectura guardada para tu próximo encuentro"),
 "RETURN_LOGGED": ("Reading recorded", "Lectura registrada"),
 "RETURN_PEACE": ("Your story keeps growing", "Tu historia sigue creciendo"),
 "RETURN_DEFEATED": ("Opponent defeated", "Enemigo vencido"),
 "CLEAR_PATH": ("A path recovered", "Un camino recuperado"), "AFTERMATH": ("The shadows retreat. A path opens through this chapter of the world.", "Las sombras retroceden. Se abre un camino por este capítulo del mundo."),
 "DONE": ("Completed", "Completado"), "IN_PROGRESS": ("In progress", "En curso"), "LOCKED": ("Complete the previous encounter.", "Completa el encuentro anterior."),
 "BOOK_LOCK": ("Opens after Book %s.", "Se abre tras el Libro %s."), "WORLD_SAFE": ("The world is safe", "El mundo está a salvo"),
 "COMPLETE": ("Campaign complete", "Campaña completa"), "END": ("End adventure", "Terminar aventura"), "END_CONFIRM": ("End this adventure?", "¿Terminar la aventura?"),
 "END_WARNING": ("This adventure will end. Start again resets your progress. General reading history is kept.", "Esta aventura terminará. Empezar de nuevo reinicia tu progreso. El historial general de lectura se conserva."),
 "KEEP": ("Keep adventuring", "Seguir aventura"), "CONFIRM_END": ("End", "Terminar"), "UNFINISHED": ("An unfinished journey", "Un viaje inconcluso"),
 "UNFINISHED_TEXT": ("The Demon King keeps his throne. You leave the road and return home; this adventure ends here.", "El Rey demonio conserva su trono. Dejas el camino y regresas a casa; esta aventura termina aquí."),
 "WORLD_HERO": ("Hero of the world", "Héroe del mundo"), "PEACE_TEXT": ("The world goes on without the king's shadow. Quiet days and books you have yet to open await you at home.", "El mundo sigue adelante sin la sombra del rey. En casa te esperan días tranquilos y libros que aún no has abierto."),
 "RESTART": ("Start again", "Empezar de nuevo"), "NEW_STORY": ("A new story", "Una nueva historia"), "CHOOSE_HERO": ("Choose your hero", "Elige tu héroe"),
 "YOUR_NAME": ("What is your hero's name?", "¿Cómo se llama tu héroe?"), "START": ("Begin", "Comenzar"), "TOTAL": ("Total", "Total"),
 "TODAY_LABEL": ("Today", "Hoy"), "WEEK_LABEL": ("This week", "Esta semana"), "HOURS": ("%llu h %u min", "%llu h %u min"),
 "MINUTES": ("%llu min", "%llu min"), "BOOKS": ("Books", "Libros"), "STREAK": ("Streak: %u days", "Racha: %u días"),
 "LONG_SAGA": ("A saga for years", "Una saga para años"), "STRUCTURE": ("12 books · 120 chapters", "12 libros · 120 capítulos"),
"DAILY_PACE": ("My daily reading", "Mi lectura diaria"),
"OPEN_BOOKS": ("Explore the books", "Explorar los libros"),
 "PROGRESS": ("%llu / %u", "%llu / %u"), "XP": ("%u EXP", "%u EXP"), "PAST": ("This encounter is already complete.", "Este encuentro ya está completado."),
}


def build():
    data = json.loads((SOURCE / "story.json").read_text(encoding="utf-8"))
    books, chapters, enemies, minutes = (data[key] for key in ("books", "chapters", "enemies", "baseMinutes"))
    assert data["version"] == 1 and len(books) == len(enemies) == 12 and len(chapters) == 120
    assert [b["id"] for b in books] == list(range(12))
    assert [c["id"] for c in chapters] == list(range(120))
    assert all(c["book"] == c["id"] // 10 for c in chapters)
    assert all(len(e[lang]) == 6 for e in enemies for lang in ("en", "es"))
    assert len(minutes) == 720 and sum(minutes) == 72000 and all(type(n) is int and 0 < n <= 1440 for n in minutes)
    strings = {"STR_QUEST_CAM_"+key:value for key,value in UI.items()}
    UI.update({
      "KING_SHADOW": ("Shadow of the King", "Sombra del rey"),
      "LONG_SAGA": ("Your adventure", "Tu aventura"),
      "PACING_NOTE": ("Read at your pace, without a deadline.", "Lee a tu ritmo, sin plazo."),
      "NEXT_READING": ("Next reading: %u min", "Próxima lectura: %u min"),
      "BATTLE_NEXT": ("Next path", "Siguiente camino"),
      "PATH_FIGHT": ("Fight", "Combatir"),
      "BATTLE_ENTER": ("Enter encounter", "Entrar al encuentro"),
      "BATTLE_CHOOSE_PATH": ("Choose an enemy in Path", "Elige un enemigo en Camino"),
      "BATTLE_HINT": ("Read your book to defeat it.", "Lee tu libro para vencerlo."),
      "BATTLE_MINUTE_PROGRESS": ("%llu / %llu min read", "%llu / %llu min leídos"),
      "BATTLE_CONTINUE": ("Continue battle", "Continuar combate"),
      "BATTLE_VICTORY": ("Victory", "Victoria"),
      "BATTLE_GIFTS": ("Saved in your equipment", "Guardado en tu equipo"),
      "BATTLE_START": ("Start the adventure", "Comenzar la aventura"),
      "BATTLE_FINISH": ("See ending", "Ver final"),
      "BATTLE_SESSION_MINUTES": ("Session: %llu min", "Sesión: %llu min"),
      "BATTLE_SESSION_SECONDS": ("Session: %llu sec", "Sesión: %llu s"),
      "BATTLE_READING": ("%llu min read", "%llu min leídos"),
      "BATTLE_BANK": ("Your surplus reading weakens the next enemy.", "La lectura sobrante debilita al siguiente enemigo."),
    })
    strings.update({"STR_QUEST_CAM_"+key:value for key,value in UI.items()})
    generate_journey(data, ROOT, strings)
    brief = _scene.load(ROOT)
    _scene.generate(ROOT, strings, brief)
    _request.generate(ROOT, strings)
    _expansion.generate(ROOT, strings)
    collection_spec = importlib.util.spec_from_file_location("quest_collection_content", Path(__file__).with_name("collection_content.py"))
    collection = importlib.util.module_from_spec(collection_spec)
    collection_spec.loader.exec_module(collection)
    strings.update({"STR_QUEST_COLLECTION_"+key: pair for key, pair in collection.UI.items()})
    assert len(data['decorations']) == 16
    # Keep this edition distinct from translations of the previous plot in installed language packs.
    narrative = "STR_QUEST_CAM_JOURNEY2_"

    def decor_key(kind, index):
        return f"STR_QUEST_CAM_{'SOLO_' if index == 1 else ''}DECOR_{kind.upper()}_{index}"

    forest_lantern = {
        "name": {"en": "Forest lantern", "es": "Farol del bosque"},
        "text": {"en": "A lantern to light your reading corner.", "es": "Un farol para iluminar tu rincón de lectura."},
    }
    for n,item in enumerate(data['decorations']):
        assert item['id'] == n and item['place'] in (0,1)
        for kind in ('name','text'):
            value = (forest_lantern if n == 1 else item)[kind]
            strings[decor_key(kind, n)]=(value['en'],value['es'])
    for n,page in enumerate(brief["prologue"]):
        strings[f"{narrative}PROLOGUE_TITLE_{n}"]=(page['title']['en'],page['title']['es'])
        strings[f"{narrative}PROLOGUE_TEXT_{n}"]=(page['text']['en'],page['text']['es'])
    for n, saga in enumerate(brief["sagas"]):
        strings[f"{narrative}BOOK_{n}"]=(saga['title']['en'],saga['title']['es'])
    strings[narrative+"EPILOGUE"]=(brief["epilogue"]["en"],brief["epilogue"]["es"])
    strings[narrative+"EPILOGUE_TITLE"]=("Epilogue", "Epílogo")
    strings["STR_QUEST_CAM_SAGA_VICTORY"]=("Saga %s completed!", "¡Saga %s completada!")
    strings["STR_QUEST_CAM_SAGA_START"]=("Start the saga", "Empezar la saga")
    strings["STR_QUEST_CAM_SAGA_NEXT"]=("Next saga", "Siguiente saga")
    strings["STR_QUEST_CAM_SAGA_REWARD_SOURCE"]=("Earned by completing Saga %s", "Se obtiene al completar Saga %s")
    for key, pair in {
        "RANK_BASIC": ("Basic", "Básico"),
        "RANK_ELITE": ("Elite", "Élite"),
        "RANK_LEGENDARY": ("Legendary", "Legendario"),
        "RANK_SECRET": ("Secret", "Secreto"),
        "STORM_NAME": ("Storm Owl", "Búho de la tormenta"),
        "CRYSTAL_NAME": ("Crystal Stag", "Ciervo de cristal"),
        "PHOENIX_NAME": ("Ashwing Phoenix", "Fénix de ceniza"),
        "INK_NAME": ("Dragon of Eternal Ink", "Dragón de tinta eterna"),
        "STORM_OUTFIT": ("Thunder Mantle", "Manto del trueno"),
        "CRYSTAL_OUTFIT": ("Crystal Antler Armor", "Armadura de cristal"),
        "PHOENIX_OUTFIT": ("Ashwing Cloak", "Capa del fénix"),
        "INK_OUTFIT": ("Eternal Ink Armor", "Armadura de tinta eterna"),
        "INK_RELIC": ("Eternal Ink Brooch", "Broche de tinta eterna"),
        "OUTFIT": ("Outfit", "Atuendo"),
        "OUTFIT_NAME": ("Outfit · %s", "Atuendo · %s"),
        "REWARD_SOURCE": ("Earned in Hunts", "Se obtiene en Cacerías"),
        "REWARD_ACTION": ("Earn it in Hunts", "Gánalo en Cacerías"),
        "HOURS": ("%u h", "%u h"),
        "HOURS_PROGRESS": ("%u h %02u min / %u h", "%u h %02u min / %u h"),
        "SHORT_HOURS_PROGRESS": ("%u h %02u min", "%u h %02u min"),
        "PAUSED": ("Paused", "En pausa"),
        "PAUSE": ("Pause", "Pausar"),
        "RESUME": ("Resume hunt", "Retomar cacería"),
        "PAUSE_TITLE": ("Pause hunt", "Pausar cacería"),
        "PAUSE_WARNING": ("Your progress is saved. You can resume this hunt later.", "Tu avance se guarda. Puedes retomar esta cacería después."),
        "RESUME_WARNING": ("Current progress will be lost. The Secret hunt keeps its saved progress.", "Perderás el avance actual. Conservarás el de la cacería secreta."),
    }.items():
        strings[f"STR_QUEST_HUNTS_{key}"]=pair
    strings[narrative+"SHADOW_REWARD"]=("Emblem of the Shadow", "Insignia de la Sombra")
    strings[narrative+"KING_REWARD"]=("Emblem of the King", "Insignia del Rey")
    strings[narrative+"RECAP_PROLOGUE"]=(
        "Reading weakens the monsters sent to destroy books.",
        "Leer debilita a los monstruos que el rey envió a destruir los libros.",
    )
    for c in chapters:
        title = brief["paths"][c["id"]]["title"]
        strings[f"{narrative}CHAPTER_{c['id']}"]=(title['en'],title['es'])
        assert len(c['encounters']) == 6
        for encounter in c['encounters']:
            assert encounter['kind'] in ('combat','explore','rescue','restore')
    # Keep the names used in the new story for the existing boss art.
    bosses = {2: ("Captain of the Mist", "Capitán de la Niebla"), 3: ("Beast from the Depths", "Bestia de las Profundidades"),
              8: ("Stone Colossus", "Coloso de Piedra"), 9: ("Commander without a Story", "Comandante sin Historia")}
    for book, (en, es) in bosses.items():
        enemies[book]["en"][-1], enemies[book]["es"][-1] = en, es
    for book,enemies_at in enumerate(enemies):
        for n,(en,es) in enumerate(zip(enemies_at['en'],enemies_at['es'])):strings[f"STR_QUEST_CAM_ENEMY_{book}_{n}"]=(en,es)
    for language,column in (("english",0),("spanish",1)):
        path=ROOT/f"lib/I18n/translations/{language}.yaml"
        source=path.read_text(encoding="utf-8")
        source="\n".join(line for line in source.splitlines() if not line.startswith(('STR_QUEST_CAM_', 'STR_QUEST_HUNTS_', 'STR_QUEST_COLLECTION_'))).rstrip()+"\n"
        aliases={
            'STR_QUEST_COMPANIONS': ('PET','PET'),
            'STR_QUEST_INFO_UNLOCK': ('Discover outfits, backgrounds, PET and decorations.','Descubre atuendos, fondos, PET y adornos.'),
            'STR_QUEST_INFO_REWARDS': ('Each chapter brings a discovery. Earn unique equipment through the story and spend your coins in the shop. Equip one decoration in your home scene.','Cada capítulo trae un hallazgo. Gana equipo único en la historia y usa tus monedas en la tienda. Coloca un adorno en tu escena.')}
        source='\n'.join(key+': '+json.dumps(aliases[key][column],ensure_ascii=False) if (key:=line.partition(':')[0]) in aliases else line for line in source.splitlines())+'\n'
        source+="\n"+"\n".join(key+": "+json.dumps(pair[column],ensure_ascii=False) for key,pair in strings.items())+"\n"
        path.write_text(source,encoding="utf-8")
    header='#pragma once\n#include <I18n.h>\nnamespace queststory {\n'
    arrays = (
        ("BOOKS", [f"{narrative}BOOK_{n}" for n in range(12)]),
        ("CHAPTERS", [f"{narrative}CHAPTER_{n}" for n in range(120)]),
        ("PROLOGUE_TITLES", [f"{narrative}PROLOGUE_TITLE_{n}" for n in range(4)]),
        ("PROLOGUE_TEXTS", [f"{narrative}PROLOGUE_TEXT_{n}" for n in range(4)]),
        ("DECOR_NAMES", [decor_key("name", n) for n in range(16)]),
        ("DECOR_TEXTS", [decor_key("text", n) for n in range(16)]),
    )
    for array, keys in arrays:
        header+=f'inline constexpr StrId {array}[] = {{\n'
        for key in keys:header+=f'  StrId::{key},\n'
        header+='};\n'
    header+='inline constexpr StrId ENEMIES[][6] = {\n'
    for b in range(12):header+='  {'+', '.join(f'StrId::STR_QUEST_CAM_ENEMY_{b}_{n}' for n in range(6))+'},\n'
    header+='};\n'
    header+='inline constexpr uint8_t COVERS[] = {'+', '.join(str(b['cover']) for b in books)+'};\n'
    header+='}  // namespace queststory\n'
    (ROOT/'src/apps_local/quest/QuestCampaignStory.h').write_text(header,encoding='utf-8')
    kinds={'combat':0,'explore':1,'rescue':2,'restore':3}
    kind_header='#pragma once\n#include <cstdint>\n// Generated by tools_local/quest/seed_campaign_content.py.\n// clang-format off\nnamespace questjourney {\ninline constexpr uint8_t ENCOUNTER_KINDS[] = {\n'
    kind_header+=',\n'.join(','.join(str(kinds[e['kind']]) for e in c['encounters']) for c in chapters)
    kind_header+='\n};\nstatic_assert(sizeof(ENCOUNTER_KINDS)==720);\n}\n// clang-format on\n'
    (ROOT/'src/apps_local/quest/QuestEncounterKinds.h').write_text(kind_header,'utf8')
    pacing = '#pragma once\n#include <cstdint>\nnamespace quest {\n'
    pacing += 'inline constexpr uint8_t SAGA_MILESTONES[] = {' + ', '.join(map(str, _scene.MILESTONES)) + '};\n'
    pacing += 'inline constexpr uint16_t CAMPAIGN_MINUTES[] = {\n'
    for at in range(0, len(minutes), 24): pacing += '    ' + ', '.join(map(str, minutes[at:at+24])) + ',\n'
    pacing += '};\n}  // namespace quest\n'
    (ROOT/'src/apps_local/quest/QuestCampaignPacing.h').write_text(pacing,encoding='utf-8')
    print(f'Campaign content: {len(chapters)} authored bilingual chapters, 12 books, 72 encounter names, {len(strings)} translation keys')


if __name__ == "__main__":
    build()
