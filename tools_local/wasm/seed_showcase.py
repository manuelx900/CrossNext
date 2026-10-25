"""Populate only the generated browser card with documented synthetic examples."""
from pathlib import Path
import datetime as dt
import hashlib
import io
import json
import posixpath
import shutil
import struct
import xml.etree.ElementTree as ET
import zipfile
import zlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets_local/simulator"


def checked_assets(folder):
    for item in json.loads((folder / "manifest.json").read_text("utf-8")):
        path = folder / item["file"]
        if Path(item["file"]).name != item["file"] or hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
            raise ValueError("Showcase checksum mismatch: " + item["file"])
        yield item, path


def document_id(path):
    data = path.read_bytes()
    digest = hashlib.md5()
    for offset in [0] + [1024 << (2 * i) for i in range(11)]:
        digest.update(data[offset:offset + 1024])
    return digest.hexdigest()


def browser_cache_id(path):
    # libc++'s 32-bit std::hash<string> used by the WASM Epub cache.
    data = path.encode("utf-8")
    h, m, at = len(data), 0x5BD1E995, 0
    while at + 4 <= len(data):
        k = int.from_bytes(data[at:at+4], "little") * m & 0xFFFFFFFF
        k = ((k ^ (k >> 24)) * m) & 0xFFFFFFFF
        h = ((h * m) ^ k) & 0xFFFFFFFF
        at += 4
    tail = data[at:]
    if tail:
        h = ((h ^ int.from_bytes(tail, "little")) * m) & 0xFFFFFFFF
    h ^= h >> 13
    h = h * m & 0xFFFFFFFF
    return h ^ (h >> 15)


def put(data, offset, value, width):
    data[offset:offset+width] = int(value).to_bytes(width, "little")


def text(data, offset, width, value):
    raw = value.encode("utf-8")[:width-1]
    data[offset:offset+len(raw)] = raw


def checksum(data):
    value = 2166136261
    for byte in data:
        value = (value ^ byte) * 16777619 & 0xFFFFFFFF
    return value


def cover_and_progress(card, item, ordinal):
    path = card / item["path"].lstrip("/")
    cache = card / ".crosspoint" / f"epub_{browser_cache_id(item['path'])}"
    cache.mkdir(exist_ok=True)
    with zipfile.ZipFile(path) as archive:
        container = ET.fromstring(archive.read("META-INF/container.xml"))
        opf_name = next(e.attrib["full-path"] for e in container.iter() if e.tag.endswith("rootfile"))
        opf = ET.fromstring(archive.read(opf_name))
        spine = next(e for e in opf if e.tag.endswith("spine"))
        count = len(spine)
        (cache / "progress.bin").write_bytes(struct.pack("<HHH", min(count-1, 1 + ordinal % max(1, count//2)), 2, 12))
        cover_id = next((e.attrib.get("content") for e in opf.iter() if e.tag.endswith("meta") and e.attrib.get("name") == "cover"), None)
        image = next((e.attrib["href"] for e in opf.iter() if e.tag.endswith("item") and
                      (e.attrib.get("id") == cover_id or "cover-image" in e.attrib.get("properties", ""))), None)
        if image:
            name = posixpath.normpath(posixpath.join(posixpath.dirname(opf_name), image))
            picture = Image.open(io.BytesIO(archive.read(name))).convert("L")
            picture.thumbnail((480, 720))
            destination = card / ".crosspoint/showcase-covers" / (path.stem + ".bmp")
            destination.parent.mkdir(exist_ok=True)
            picture.convert("1").save(destination)
            item["coverBmpPath"] = "/" + destination.relative_to(card).as_posix()


def reading_history(card, books, today):
    history = bytearray(b"CRS1\x80\x01" + bytes(10))
    running = {}
    for ago in range(119, -1, -1):
        if ago % 13 == 7:
            continue
        day = today - dt.timedelta(days=ago)
        for session in range(2 if ago % 3 == 0 else 1):
            index = (ago + session * 3) % len(books)
            book = books[index]
            uid = document_id(card / book["path"].lstrip("/"))
            b = running.setdefault(uid, dict(seconds=0, sessions=0, pages=0, first=0, finished=0, days=set(), buckets=[0]*4))
            seconds = 1100 + (ago * 137 + session * 557) % 3100
            pages = 9 + ago % 23
            hour = 9 if session == 0 else 20
            epoch = int(dt.datetime.combine(day, dt.time(hour, 15)).timestamp())
            localday = (day - dt.date(1970, 1, 1)).days
            b["seconds"] += seconds; b["sessions"] += 1; b["pages"] += pages
            b["first"] = b["first"] or epoch; b["days"].add(localday)
            b["buckets"][1 if session == 0 else 3] += seconds
            first = index >= len(books)-3 and not b["finished"] and ago < 20
            if first:
                b["finished"] = epoch + seconds
            data = bytearray(384); data[:4] = b"RSE1"
            put(data, 4, seconds, 4); put(data, 8, pages, 4); data[12] = first
            put(data, 14, 10000 if b["finished"] else min(9500, 1500 + b["pages"]*12), 2)
            for offset, value in [(16,epoch),(24,b["seconds"]),(40,b["first"]),(48,epoch+seconds),(56,b["finished"])]:
                put(data, offset, value, 8)
            for offset, value in [(32,b["sessions"]),(36,b["pages"]),(64,len(b["days"])),(68,localday),(330,localday),(334,hour*3600+900)]:
                put(data, offset, value, 4)
            for n, value in enumerate(b["buckets"]):
                put(data, 72+n*8, value, 8)
            text(data,104,33,uid); text(data,137,128,book["title"]); text(data,265,64,book["author"])
            data[329] = bool(b["finished"]); put(data,380,checksum(data[:380]),4)
            history += data
    directory = card / ".crosspoint/reading-history"
    directory.mkdir(exist_ok=True)
    # Discard disposable indexes copied from the older example journal.
    for path in directory.glob("*.idx"):
        path.unlink()
    (directory / "sessions.bin").write_bytes(history)
    return (len(history)-16)//384


def seed(card):
    original = Path(card)
    card = original.resolve()
    expected = (ROOT / ".pio/build/wasm/resource-card").resolve()
    if original.is_symlink() or not expected.is_relative_to(ROOT.resolve()):
        raise ValueError("Showcase card cannot be redirected outside the project")
    marker = card / "CROSSNEXT-SIMULATOR-CARD.json"
    if card != expected or not marker.is_file() or json.loads(marker.read_text("utf-8")).get("format") != 1:
        raise ValueError("Showcase data is restricted to the generated browser card")
    data = card / ".crosspoint"
    keep = {"English Simple", "Spanish Wiktionary", "WikDict EN-ES", "WikDict ES-EN"}
    discarded = [p for p in (card / "dictionaries").iterdir() if p.is_dir() and p.name not in keep]
    discarded.append(card / "FLASHCARDS/Pruebas_extremas")
    for path in discarded:
        if path.exists():
            if path.is_symlink() or not path.resolve().is_relative_to(card):
                raise ValueError("Unsafe generated showcase cleanup target")
            shutil.rmtree(path)
    (card / "FLASHCARDS/Prueba directa 100 tarjetas.apkg").unlink(missing_ok=True)
    today = dt.date.today()
    recent = json.loads((data / "recent.json").read_text("utf-8"))["books"]
    added = []
    sources = []
    for item, path in checked_assets(ASSETS / "showcase-books"):
        shutil.copy2(path, card / "books" / path.name)
        added.append(dict(path="/books/"+path.name, title=item["title"], author=item["author"], coverBmpPath=""))
        sources.append(f"- {item['title']}: {item['source']} ({item['rights']})")
    books = added + [b for b in recent if b["path"] not in {a["path"] for a in added}]
    for n, item in enumerate(books):
        cover_and_progress(card, item, n)
    (data / "recent.json").write_text(json.dumps({"books": books[:10]}, ensure_ascii=False), encoding="utf-8")
    records = reading_history(card, books, today)
    for item, path in checked_assets(ASSETS / "showcase-progress"):
        shutil.copy2(path, data / item["file"])
    # Eight habits fill more than one page, with 60 days of synthetic check-ins.
    anchor = (today-dt.date(1970,1,1)).days
    habits = bytearray(1208); habits[:4] = b"HBT1"; put(habits,4,1,4); put(habits,8,anchor,4); habits[12] = 8
    for n, name in enumerate(("Leer", "Caminar", "Beber agua", "Repasar palabras", "Escribir", "Dormir bien", "Ejercicio", "Practicar ajedrez")):
        offset=16+n*99; text(habits,offset,33,name); put(habits,offset+33,anchor-60,4)
        for ago in range(60):
            if (ago+n)%7 not in (1,5):
                slot=399-ago; habits[offset+49+slot//8] |= 1 << (slot%8)
    put(habits,1204,zlib.crc32(habits[:1204]),4); (data / "habits-a.bin").write_bytes(habits)
    notes = card / "notes"
    for n in range(1,19):
        (notes/f"Lista {n:02} - Lectura y proyectos.md").write_text("\n".join(f"- [{'x' if i%3==0 else ' '}] Actividad {i+1}: "+("Leer un capítulo" if i%2 else "Anotar una idea") for i in range(12))+"\n",encoding="utf-8")
    (notes/".crossplay-free-tasks.md").write_text("\n".join(f"- [{'x' if n%4==0 else ' '}] "+name for n,name in enumerate(["Leer Frankenstein", "Revisar notas", "Caminar", "Repasar palabras", "Regar plantas", "Resolver un problema", "Organizar biblioteca", "Preparar mochila"]*3))+"\n",encoding="utf-8")
    documents = card / "Documentos"; documents.mkdir(exist_ok=True)
    for n in range(1,25):
        (documents/f"Cuaderno {n:02}.txt").write_text(f"Cuaderno de demostración {n}\n\nLecturas, ideas y proyectos.\n"*6,encoding="utf-8")
    for name in ("wifi.json", "koreader.json", "credentials.json"):
        (data / name).unlink(missing_ok=True)
    settings_path=data/"settings.json"; settings=json.loads(settings_path.read_text("utf-8"))
    settings.update(language="ES", libraryView=1)
    settings_path.write_text(json.dumps(settings,ensure_ascii=False),encoding="utf-8")
    (card/"SHOWCASE-SOURCES.md").write_text("# CrossNext browser demonstration\n\nAll reading/app/game progress is synthetic. Only the generated browser card is populated; device SD cards are never changed.\n\n"+"\n".join(sources)+"\n",encoding="utf-8")
    inventory=dict(books=len(books),recentBooks=min(10,len(books)),readingRecords=records,historyDays=120,
                   habits=8,taskLists=24,documents=24,progressFormats=8,seedDate=today.isoformat())
    (card/"SHOWCASE.json").write_text(json.dumps(inventory,indent=2)+"\n",encoding="utf-8")
    return inventory
