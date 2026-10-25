"""Source identities and art direction for the complete-outfit catalog."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "assets_local/quest/catalog_v4"
OUTFITS = """Apprentice|Aprendiz|plain white lace-neck tunic, dark trousers and simple boots
Linen traveler|Viajero de lino|light rolled-sleeve linen tunic with broad dark belt
Field scholar|Estudiante de campo|short waistcoat, plain shirt, loose trousers and small journal pouch
Book courier|Mensajero|short buttoned courier coat and a diagonal plain satchel strap
Herbalist|Herbolario|simple apron over tunic, small leaf clasp and rolled sleeves
Bookbinder|Artesano del libro|plain work apron and protective wrist cuffs, no tools held
Forest scout|Explorador|dark sleeveless vest, white shirt and light baggy trousers
Ink scribe|Escriba|short scholar robe with ink-drop brooch and dark sash
Wayfarer|Caminante|simple shoulder mantle, travel tunic and sturdy boots
Novice guard|Guardia novel|small smooth breastplate and plain rounded shoulder guards
Library ranger|Guarda de biblioteca|fitted ranger tunic and short squared cape, book clasp
River sailor|Marinero|striped undershirt under simple cropped sailor coat
Sky courier|Correo del cielo|short aviator jacket and round goggles resting on the head
Copper artificer|Artificiero|simple segmented leather apron and one large gear buckle
Cloaked pilgrim|Peregrino|full plain travel cloak with the face visible, sturdy robe
Sword student|Aprendiz de espada|original martial training wrap top and wide dark waist sash, no weapon
Spirit trainee|Aprendiz espiritual|white short-sleeved cross-wrap tunic over dark undershirt
Storm runner|Corredor del viento|asymmetric short jacket, scarf and close-fitting trousers
Star cadet|Cadete estelar|original smooth padded space academy suit with simple circular chest patch
Archive knight|Caballero del archivo|simple plate breastplate with an open-book clasp and short cape
Oz wanderer|Viajero de Oz|original 1900-book inspired checked travel over-tunic and silver shoes, no film likeness
Straw scholar|Sabio de paja|patchwork scholarly coat, straw tassels at cuffs, friendly human face
Tin sentinel|Centinela de hojalata|rounded tin-like armor with a simple heart badge, original literary interpretation
Lionhearted reader|Corazon de leon|short mane-like fur collar and a lion clasp on a plain coat
Emerald sage|Sabio esmeralda|original geometric gem-trimmed robe and small faceted crown
Wonderland guest|Huesped del pais raro|original pinafore-style travel overcoat with diamond pocket shapes
Mirror envoy|Enviado del espejo|mirrored split-panel coat, contrasting cuffs and chess-square brooch
Clockwork reader|Lector mecanico|original clock-face waistcoat and chunky mechanical shoulder accents
Nautilus navigator|Navegante Nautilus|original Jules Verne inspired naval frock coat, sea collar and brass-like round buttons
Silver road warden|Guardian del camino|long roadwarden coat, silver triangular hem plates and white scarf
Gothic scholar|Estudioso gotico|dark high-collar scholarly coat with white cravat and simple pointed cuffs
Alchemy keeper|Guardian alquimista|layered alchemist robe with one large flask-shaped clasp
Prometheus creation|Criatura de Prometeo|original Shelley-book inspired oversized patched coat, human rounded head, no neck bolts or flat head
Night noble|Noble nocturno|original 1897-literary vampire formal coat, white cravat and sweeping cape
Moon hunter|Cazador lunar|high-collar travel coat and broad-brim simple hat, geometric moon clasp
Portrait keeper|Guardian del retrato|ornate yet chunky Victorian frock coat and a simple framed brooch
Quixote knight|Caballero de la Mancha|original literary makeshift armor and shallow basin-like helmet, no weapon
Sea captain|Capitan del mar|double-breasted long sea coat and compact bicorne hat
Dream cartographer|Cartografo de suenos|long coat with simple star-map panels and round shoulder clasps
Glass alchemist|Alquimista del cristal|angular crystal collar and layered white lab-robe with dark waist sash
Wind ronin|Ronin del viento|original wandering swordsman layered wrap robe and short cape, no franchise symbols
Sun adept|Adepto del sol|original martial arts uniform with a broad sash and plain circular sun crest
Shadow courier|Correo de sombras|original stealth tunic with compact neck scarf and plain wrist wraps
Spirit blade scholar|Sabio de la espada|original formal cross-wrap robe, geometric spirit crest, no famous uniform
Astral voyager|Viajero astral|original smooth science-fiction explorer coat with round chest light
Mecha squire|Escudero mecanico|original chunky miniature mechanical armor, simple shoulder plates and white visor-free collar
Grimoire captain|Capitan del grimorio|original long magic-academy captain coat with three bold book clasps
Thunder sentinel|Centinela del trueno|angular shoulder armor and a newly designed zigzag breastplate motif
Frost warden|Guardian de escarcha|thick pale coat, broad fur collar and faceted ice belt buckle
Flame knight|Caballero de la llama|plate armor with broad flame-shaped shoulder silhouettes and dark undersuit
Silver paladin|Paladin de plata|polished white plate armor, layered skirt plates and a long plain cloak
Rune scholar|Erudito de runas|heavy angular scholarly robe with sparse invented geometric rune blocks
Royal archivist|Archivero real|long formal royal coat, broad cuffs and a simple small crown
Forest champion|Campeon del bosque|leaf-shaped shoulder plates and layered short cloak
Sea guardian|Guardian del oceano|shell-like rounded shoulder armor, wave-shaped hem and pearl clasp
Desert sovereign|Soberano del desierto|layered desert robe, wide cloth headband and crescent shoulder accents
Mountain warden|Guardian de montanas|large squared stone-like shoulders and a thick dark cloak
Crystal knight|Caballero de cristal|large clean faceted crystal breastplate and shard shoulder guards
Storm marshal|Mariscal de tormentas|long military-fantasy coat with bold lightning shoulder shapes
Royal magus|Mago real|broad high-collar robe, geometric mantle and compact crown
Dragon knight|Caballero del dragon|original dragon-scale shaped large armor plates and small horned crown
Phoenix scholar|Sabio del fenix|layered feather-shaped robe panels and bird-shaped shoulder silhouette
Moon hierophant|Hierofante lunar|ceremonial pale robe and crescent crown, broad black sash
Solar sentinel|Centinela solar|radiating sun-shaped shoulder plates and round sun chest motif
Void walker|Caminante del vacio|dark angular layered cloak with stark white edges and empty-circle clasp
Rune sovereign|Soberano de runas|ceremonial armor with four large invented square glyphs
Celestial envoy|Enviado celestial|long white coat with small symmetrical feather mantle
Chronicle guardian|Guardian de cronicas|massive book-like layered armor plates and plain crown
Astral admiral|Almirante astral|broad sweeping admiral coat, star collar and epaulets
Arcane regent|Regente arcano|large tiered wizard mantle and a symmetrical faceted crown
Phoenix sovereign|Soberano del fenix|large symmetric flame-feather mantle and long layered royal robe
Titan keeper|Guardian de titanes|massive rounded guardian armor and broad squared shoulder silhouette
Dragon sovereign|Soberano draconico|ornate horned shoulder armor with small integrated dragon-wing cloak
Moon oracle|Oraculo lunar|flowing crescent-trimmed robe, a broad moon crown and layered white sash
Sun emperor|Emperador solar|imperial plate armor with bold sunburst collar and long cape
Storm monarch|Monarca de tormentas|huge angular lightning mantle and layered royal cloak
Crystal archon|Arconte de cristal|large geometric crystal armor with a tall faceted crown
Spirit monarch|Monarca espiritual|white layered ceremonial robes and black hovering-looking shoulder ornaments attached to costume
World tree keeper|Guardian del arbol|large branching leaf crown and tree-bark shaped chest armor
Starforged champion|Campeon estelar|bold stellar plate armor with compact integrated star-wing mantle
Dawn seraph|Serafin del alba|white ceremonial armor and two small integrated feather wings
Celestial knight|Caballero celestial|majestic symmetric pale armor, wing-shaped shoulders and compact halo crown
Lunar seraph|Serafin lunar|crescent armor, long white robe and two integrated moon-feather wings
Solar seraph|Serafin solar|sunburst breastplate, feather mantle and integrated radiating crown
Astral archon|Arconte astral|large star-shaped collar and ceremonial segmented armor
Heavenly regent|Regente del cielo|majestic royal robe, feather shoulders and high symmetrical crown
Star paladin|Paladin de estrellas|white faceted plate armor, broad star cloak and bright geometric crown
Sky sovereign|Soberano del cielo|towering layered mantle, wing-shaped robe panels and open-face crown
Eclipse guardian|Guardian del eclipse|half-dark half-white ceremonial plate armor and circular eclipse collar
Cosmic librarian|Erudito cosmico|grand layered book-like robes and enormous star-shaped shoulder mantle
Divine herald|Heraldo divino|ceremonial white armor, integrated feather wings and a radiant crown
Empyrean knight|Caballero empireo|maximum celestial plate armor with a broad winged mantle and triple crown
Eternal oracle|Oraculo eterno|grand layered white robes with symmetric celestial shoulder arches
Astral deity|Deidad astral|divine geometric armor, integrated small star wings and tall radiant crown
Moon deity|Deidad lunar|majestic moon-shaped white armor and crescent wing mantle
Sun deity|Deidad solar|grand sunburst armor with long layered white cape and radiant crown
Creation keeper|Guardian de la creacion|divine robes with tree and star geometry, huge layered collar
Infinite sovereign|Soberano infinito|ornate white cosmic plate armor with broad infinity-loop shoulder silhouette
Celestial god|Dios celestial|ultimate celestial royal armor, symmetric integrated angel wings and grand halo crown
God of chronicles|Dios de las cronicas|final supreme reading deity: majestic geometric white armor, broad layered wing mantle and grand star-and-book crown"""

PETS = """Hamster|Hamster|35
Bird|Pajaro|42
Turtle|Tortuga|48
Cat|Gato|68
Rabbit|Conejo|62
Dog|Perro|98
Fox|Zorro|90
Owl|Buho|82
Squirrel|Ardilla|60
Hedgehog|Erizo|45
Wolf|Lobo|158
Raven|Cuervo|90
Red panda|Panda rojo|108
Deer|Ciervo|190
Snow leopard|Leopardo de las nieves|185
Bear|Oso|270
Lion|Leon|220
Tiger|Tigre|235
Eagle|Aguila|130
Polar bear|Oso polar|290
Friendly slime|Slime amistoso|58
Mushroom sprite|Seta viviente|70
Spirit rabbit|Conejo espiritual|85
Spirit fox|Zorro espiritual|120
Moon cat|Gato lunar|95
Jackalope|Conejo cornudo|112
Shadow wolf|Lobo de sombras|210
Mandrake|Mandragora|88
Rune owl|Buho de runas|140
Spark drake|Draco de chispa|200
Griffin|Grifo|380
Hippogriff|Hipogrifo|420
Unicorn|Unicornio|340
Pegasus|Pegaso|380
Kirin|Kirin|370
Manticore|Manticora|450
Basilisk|Basilisco|480
Stone golem|Golem de piedra|540
Chimera|Quimera|560
Phoenix|Fenix|460
Wyvern|Guiverno|640
Mountain dragon|Dragon de montana|800
Eastern dragon|Dragon oriental|940
Hydra|Hidra|900
Leviathan|Leviatan|1140
Ancient titan|Titan ancestral|1050
Star wyrm|Sierpe estelar|1180
Ancient dragon|Dragon ancestral|1300
Cosmic stag|Ciervo cosmico|900
Celestial dragon|Dragon celestial|1450"""

AURAS = """Quiet spark|Chispa tenue|two small diamond pixels and one tiny four point spark
Reading dots|Puntos de lectura|four tiny floating dots around the outside
Little stars|Estrellitas|four small four-point stars
Soft breeze|Brisa suave|two short curved pixel wind strokes
Falling leaves|Hojas al viento|four small leaves around the sides
Moon glimmer|Brillo lunar|one small crescent and two little stars
Fireflies|Luciernagas|six tiny diamond firefly sparks
Dew ring|Rocio|four little outlined water drops
Ink motes|Motas de tinta|five floating simple square ink motes
Gentle halo|Halo tenue|one thin broken oval ring and two sparkles
Star circle|Circulo estelar|six medium four-point stars in an oval
Forest breeze|Brisa del bosque|six leaves and two little wind swirls
Moonlight|Luz de luna|crescent moon and six small sparkles
Ember ring|Anillo de brasas|six little outlined flame motes
Water ripple|Ondas de agua|two thin concentric broken water ovals
Wind spiral|Espiral de viento|two sweeping pixel wind arcs
Crystal glimmer|Destello de cristal|six small diamond crystal shards
Rune sparks|Chispas de runas|four newly invented simple square glyphs and stars
Snow halo|Halo de nieve|six tiny snowflake symbols around an oval
Dawn rays|Rayos del alba|eight medium short radiating rays
Arcane circle|Circulo arcano|one bold geometric rune ring with four large star points
Frost ring|Anillo de escarcha|an angular ice ring with six large diamond crystals
Flame spiral|Espiral de llamas|two larger outlined flame arcs around the outside
Lunar orbit|Orbita lunar|one crescent above and two orbit arcs with stars
Thunder field|Campo de trueno|six angular lightning bolts surrounding an empty center
Petal storm|Tormenta de petalos|eight large falling stylized petals and a light ring
Crystal orbit|Orbita de cristal|eight faceted crystals around a double thin orbit
Spirit circle|Circulo espiritual|three wisp flames and a broken rune ring
Solar corona|Corona solar|a bold sun ray ring with a clear empty center
Archive seal|Sello del archivo|a large circular invented glyph seal and book-like square marks
Phoenix halo|Halo del fenix|two large symmetric upward flame-feather arcs
Dragon sigil|Sello draconico|bold invented dragon-wing geometry around a circular seal
Celestial ring|Anillo celestial|two bright outlined rings with six star crystals
Moon sanctuary|Santuario lunar|large crescent, layered orbital arches and stars
Sun sanctuary|Santuario solar|large sunburst corona and multiple orbit fragments
Crystal sanctuary|Santuario de cristal|large crystal clusters around a triple ring
Storm crown|Corona de tormentas|many bold lightning arcs around a broad empty center
Spirit wings|Alas espirituales|two broad symmetric ethereal feather wing shapes behind an empty center
Starfall|Lluvia de estrellas|large falling stars and two long comet trails
World tree halo|Halo del arbol|large leafy branching arcs framing an empty center
Astral gate|Portal astral|two ornate concentric invented rune circles and large stars
Eclipse gate|Portal del eclipse|a broad black and white eclipse ring and radiating sharp rays
Empyrean wings|Alas empireas|two enormous layered feather wings and a geometric halo
Cosmic orbit|Orbita cosmica|three sweeping planet-like orbital rings, stars and crescents
Creation seal|Sello de la creacion|large complex invented circular creation geometry with leaves and stars
Infinity field|Campo infinito|two large intersecting infinity loops and bright star points
Divine corona|Corona divina|huge triple sunburst halo with bold layered rays and wing fragments
Celestial throne|Trono celestial|majestic symmetric wing arches and three ornate geometric rings
Eternal firmament|Firmamento eterno|enormous layered celestial arches, starbursts, crescent moons and falling stars
Godly radiance|Resplandor divino|the final maximal divine aura: enormous symmetric feather wings, triple rune halos, broad starburst rays and orbiting crystal stars"""


def main():
    outfits = []
    for n, line in enumerate(OUTFITS.splitlines()):
        en, es, art = line.split("|")
        outfits.append({"id": n, "en": en, "es": es, "art": art, "level": n + 1, "tier": n // 10})
    pets = []
    for n, line in enumerate(PETS.splitlines()):
        en, es, height = line.split("|")
        pets.append({"id": n, "en": en, "es": es, "height": int(height), "level": 1 + round(n * 99 / 49)})
    auras = []
    for n, line in enumerate(AURAS.splitlines()):
        en, es, art = line.split("|")
        auras.append({"id": n, "en": en, "es": es, "art": art, "level": 1 + round(n * 99 / 49), "scale": 110 + n * 3})
    assert len(outfits) == 100 and len(pets) == len(auras) == 50
    DEST.mkdir(parents=True, exist_ok=True)
    data = {"version": 4, "outfits": outfits, "pets": pets, "auras": auras,
            "literarySources": ["https://www.gutenberg.org/ebooks/55", "https://www.gutenberg.org/ebooks/84"],
            "artDirection": "Original monochrome chibi pixel art; modern archetypes have no franchise names, logos or copied costumes. Literary interpretations refer to the original texts, not screen adaptations."}
    (DEST / "catalog.json").write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("100 outfits x 2 genders, 100 garment icons, 50 pets, 50 auras")


if __name__ == "__main__":
    main()
