"""Original StarDict examples for the browser simulator's disposable SD only."""
import json
from pathlib import Path
import struct


def seed_dictionaries(sd, vocabulary, languages):
    sd = Path(sd).resolve()
    assert sd == Path(__file__).resolve().parents[2] / 'tools_local/wasm/sdcard'
    english = [
        ('curious', 'Wanting to know or learn something.'),
        ('wonder', 'To ask yourself about something you do not know.'),
        ('whisper', 'To speak very quietly.'), ('journey', 'A trip from one place to another.'),
        ('narrow', 'Having little space from one side to the other.'), ('brave', 'Ready to face something frightening.'),
        ('gentle', 'Calm, kind or soft.'), ('bright', 'Giving or reflecting much light.'),
        ('wander', 'To walk without a fixed destination.'), ('shelter', 'A place that protects you from danger or weather.'),
        ('distant', 'Far away.'), ('remarkable', 'Unusual enough to be noticed.'),
        ('alice', 'The name of the girl in this demonstration book.'), ('rabbit', 'A small animal with long ears.'),
        ('book', 'A collection of pages to read.'), ('door', 'A movable entrance to a room.'),
    ]
    translations = [(word, meaning) for word, meaning, _ in languages] + [('alice', 'Alicia'), ('rabbit', 'conejo'), ('book', 'libro'), ('door', 'puerta')]
    for name, rows in [('Demo ES definitions', [(a, b) for a, b, _ in vocabulary]), ('Demo EN definitions', english), ('Demo EN-ES', translations)]:
        folder = sd / 'dictionaries' / name
        folder.mkdir(parents=True, exist_ok=True)
        index, definitions = bytearray(), bytearray()
        for word, meaning in sorted(rows, key=lambda row: row[0].encode('utf-8')):
            value = meaning.encode('utf-8')
            index += word.encode('utf-8') + b'\0' + struct.pack('>II', len(definitions), len(value))
            definitions += value
        (folder / 'demo.idx').write_bytes(index)
        (folder / 'demo.dict').write_bytes(definitions)
        (folder / 'demo.ifo').write_text("StarDict's dict ifo file\nversion=2.4.2\nbookname=" + name + '\nwordcount=' + str(len(rows)) + '\nidxfilesize=' + str(len(index)) + '\nsametypesequence=m\n', encoding='utf-8')
    path = sd / '.crosspoint/settings.json'
    settings = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
    settings['dictionaryName'] = 'Demo ES definitions'
    settings['translationDictionaryName'] = 'Demo EN-ES'
    path.write_text(json.dumps(settings, ensure_ascii=False), encoding='utf-8')
