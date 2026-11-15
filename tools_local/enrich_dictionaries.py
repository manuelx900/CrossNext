"""Merge pinned open lexical data into generated StarDict resource copies.

This is a desktop packaging step. The reader keeps its sampled, SD-backed index.
Only explicit target equivalents enter bilingual entries; aliases retain their
source lemma, and all source licenses travel with the generated dictionaries.
"""
from contextlib import contextmanager
import gzip
from html import escape
import hashlib
import json
from itertools import groupby
import re
from pathlib import Path
import shutil
import sqlite3
import struct
import tempfile
import urllib.request
import unicodedata
import zipfile
import zlib

from compact_dictionaries import Entry, GRAMMAR, Node, TRANSLATION, compact_html
import dictionary_fidelity as fidelity
from dictionary_usage import NaturalUsage

RELEASE = "https://github.com/DanielGregorini/koreader-dicts/releases/download/v1.2.0/"
ARCHIVES = (
    ("en-en", "English Simple", "142327769b0f4e2cd0e442a826bc6e561e5246a7796934ff23618c1c6984ab3c"),
    ("es-es", "Spanish Wiktionary", "d6856818b4982ac849c1b924c70b11bea0fb06f30da18a5127622195894a7c9b"),
    ("en-es", "WikDict EN-ES", "e5a21ee6701c9384477ea303f32b4636f6630aba27ff59bcb0a4ba51bcbf2109"),
    ("es-en", "WikDict ES-EN", "1dea828174133f36d3f76dfa93b541b0cb23965b851841924ae72be747d8e2ae"),
    ("en-pt", "WikDict EN-PT", "1b8f448db8c63c6859a5ee481839be7fdc91d48782e95d8b8beb812f9a5eda9b"),
    ("pt-en", "WikDict PT-EN", "6cf04b96b0c08eec044aac6e04df549ccf6d485e02ec757af4eadbfacddd27e7"),
    ("en-de", "WikDict EN-DE", "61ac5f8a481f1a5d067555d886f1e1f530a5bd7b6d28abec65f0ca44a0ffed8e"),
    ("de-en", "WikDict DE-EN", "4aed680cba6a9698af972b12d141405852a8ee390b9512d9b02bdcb7457dfdc0"),
    ("en-fr", "WikDict EN-FR", "23c4fadd44fb15cdaa4e00f378e5aeee75f99da3f0057c6764341777c42d58c8"),
    ("fr-en", "WikDict FR-EN", "9df76860848673638a910b49b5de745228c53d9cdc2c158f5bfd9c26d086e3f5"),
)
WIKDICT_PAIRS = {"en-fr", "fr-en"}
WIKDICT_DOWNLOAD = "https://download.wikdict.com/dictionaries/stardict/"
# Independently authored common-use equivalents precede the source alternatives.
PREFERRED_TRANSLATIONS = {
    "en-es": {
        "lastly": "<p><b>Adverbio</b></p><p>por último; finalmente</p>",
        "actually": "<p><b>Adverbio</b></p><p>en realidad; de hecho; realmente</p>",
    },
}
ABBREVIATIONS = {"n.": "noun", "v.": "verb", "adj.": "adjective", "adv.": "adverb",
                 "pron.": "pronoun", "prep.": "preposition", "conj.": "conjunction",
                 "interj.": "interjection", "num.": "numeral", "det.": "determiner",
                 "phrase": "expression", "phr.": "expression"}


def explicit_translation(source, language, headword="", source_language=""):
    entry = Entry()
    entry.feed(source)
    groups = []
    for row in entry.root.children:
        if not isinstance(row, Node) or row.tag != "div":
            continue
        equivalents = [child.text().strip() for child in row.children
                       if isinstance(child, Node) and child.tag in {"b", "strong"} and child.text().strip()]
        if not equivalents:
            continue
        pos = next((ABBREVIATIONS.get(child.text().strip(), "") for child in row.children
                    if isinstance(child, Node) and child.tag == "i"), "")
        if headword.lower() == "private" and source_language == "EN" and language == "ES" and pos == "noun":
            equivalents = [", ".join(term.strip() for term in value.split(",") if term.strip() != "cabo")
                           for value in equivalents]
            equivalents = [value for value in equivalents if value]
        if not equivalents:
            continue
        category = GRAMMAR[language].get(pos, TRANSLATION[language])
        group = (category, "; ".join(dict.fromkeys(equivalents)))
        if group not in groups:
            groups.append(group)
    if not groups:
        return ""
    output, previous = [], None
    for category, value in groups:
        if category != previous:
            output.append("<p><b>" + category + "</b></p>")
            previous = category
        output.append("<p>" + escape(value, quote=False) + "</p>")
    return "".join(output)


def records(path, suffix):
    data = path.read_bytes()
    at = 0
    while at < len(data):
        end = data.find(b"\0", at)
        if end < 0 or end + 1 + suffix > len(data):
            raise ValueError(f"Truncated StarDict index: {path}")
        word = data[at:end]
        word.decode("utf-8")
        if not word:
            raise ValueError(f"Empty StarDict headword: {path}")
        yield word, data[end + 1:end + 1 + suffix]
        at = end + 1 + suffix


def proper_name_only(body):
    labels = re.findall(r"<(?:i|b)>\s*([^<]+)\s*</(?:i|b)>", body.decode("utf-8"))
    return bool(labels) and all(label.strip().casefold() in {"prop. n.", "proper noun", "proper name"} for label in labels)


def normalize_name(word):
    return unicodedata.normalize("NFC", word.decode("utf-8")).encode("utf-8")


def merge_body(existing, addition):
    """Keep distinct definitions while ignoring HTML-only differences."""
    if not existing:
        return addition
    if addition in existing:
        return existing
    def text(body):
        entry = Entry()
        entry.feed(body.decode("utf-8"))
        return re.sub(r"\s+", " ", entry.root.text()).strip()
    old, new = text(existing), text(addition)
    if old == new:
        return existing
    return existing + addition


def irregular_forms():
    source = Path(__file__).resolve().parents[1] / "src/util/EnglishInflections.h"
    return re.findall(r'\{"([a-z]+)", "([a-z]+)"\}', source.read_text(encoding="utf-8"))


def augment_irregular_forms(db, pair):
    if not pair.startswith("en-"):
        return
    language = pair[-2:].upper()
    forms = irregular_forms()
    # Snapshot lemma verb meanings before updating forms such as lie/lay/laid.
    verbs = {}
    for lemma in dict.fromkeys(lemma for _, lemma in forms):
        base = db.execute("SELECT body FROM entries WHERE key=?", (lemma.encode(),)).fetchone()
        if base:
            verbs[lemma] = fidelity.verb_meanings(base[0], language)
    for form, lemma in forms:
        direct = db.execute("SELECT body FROM entries WHERE key=?", (form.encode(),)).fetchone()
        verb = verbs.get(lemma)
        if not direct or not verb or verb in direct[0]:
            continue
        label = ("<p><b>" + fidelity.FORM[language] + " " + lemma + "</b></p>").encode("utf-8")
        value = (label + verb + direct[0] if form in {"shook", "risen"} or proper_name_only(direct[0])
                 else direct[0] + label + verb)
        db.execute("UPDATE entries SET body=? WHERE key=?", (value, form.encode()))
    db.commit()


@contextmanager
def raw_data(ifo, work, name):
    plain = ifo.with_suffix(".dict")
    if plain.exists():
        yield plain
        return
    target = work / name
    with gzip.open(str(plain) + ".dz", "rb") as source, target.open("wb") as destination:
        shutil.copyfileobj(source, destination)
    try:
        yield target
    finally:
        target.unlink()


def write_dictzip(plain, destination):
    chunk = 58315
    size = plain.stat().st_size
    count = max(1, (size + chunk - 1) // chunk)
    if count > 8192:
        raise ValueError("Dictionary exceeds the reader's bounded chunk table")
    lengths = []
    extra_length = 10 + count * 2
    crc = 0
    compressor = zlib.compressobj(9, zlib.DEFLATED, -15)
    with plain.open("rb") as source, destination.open("w+b") as output:
        output.write(b"\x1f\x8b\x08\x04" + b"\0" * 5 + b"\xff" + struct.pack("<H", extra_length))
        output.write(b"\0" * extra_length)
        for _ in range(count):
            block = source.read(chunk)
            crc = zlib.crc32(block, crc)
            encoded = compressor.compress(block) + compressor.flush(zlib.Z_FULL_FLUSH)
            if len(encoded) > 65535:
                raise ValueError("dictzip chunk is too large")
            lengths.append(len(encoded))
            output.write(encoded)
        end = compressor.flush(zlib.Z_FINISH)
        output.write(end)
        lengths[-1] += len(end)
        if lengths[-1] > 65535:
            raise ValueError("Final dictzip chunk is too large")
        output.write(struct.pack("<II", crc, size))
        output.seek(12)
        output.write(b"RA" + struct.pack("<HHHH", 6 + count * 2, 1, chunk, count))
        output.write(struct.pack("<" + "H" * count, *lengths))


def export_entries(db, work, old_ifo, metadata, pair):
    source_language, target_language = (part.upper() for part in pair.split("-"))
    count = db.execute("SELECT count(*) FROM entries").fetchone()[0]
    searchable = db.execute("SELECT count(*) FROM (SELECT key FROM entries UNION SELECT folded FROM aliases)").fetchone()[0]
    with (work / "out.idx").open("wb") as idx, (work / "out.dict").open("wb") as data:
        for ordinal, (key, name, value) in enumerate(db.execute("SELECT key,name,body FROM entries ORDER BY key,name")):
            if len(value) > 65536 or len(name) > 255:
                raise ValueError(f"Entry exceeds the reader budget: {name!r}")
            if data.tell() + len(value) > 0xffffffff:
                raise ValueError("Dictionary exceeds the 32-bit index range")
            idx.write(name + b"\0" + struct.pack(">II", data.tell(), len(value)))
            data.write(value)
            db.execute("UPDATE entries SET ordinal=? WHERE key=?", (ordinal, key))
    aliases = 0
    with (work / "out.syn").open("wb") as syn:
        for name, ordinal in db.execute("SELECT a.name,e.ordinal FROM aliases a JOIN entries e ON a.key=e.key "
                                        "ORDER BY a.folded,a.name,e.ordinal"):
            if len(name) > 255:
                raise ValueError("Alias exceeds the reader budget")
            syn.write(name + b"\0" + struct.pack(">I", ordinal))
            aliases += 1
    db.close()
    write_dictzip(work / "out.dict", work / "out.dict.dz")
    fields = dict(line.split("=", 1) for line in metadata.splitlines() if "=" in line)
    fields.update(wordcount=str(count), synwordcount=str(aliases),
                  idxfilesize=str((work / "out.idx").stat().st_size), lang_from=source_language.lower(),
                  lang_to=target_language.lower(), compact_layout="crossnext-2", sametypesequence="h")
    for suffix in (".idx", ".syn", ".dict.dz"):
        (work / ("out" + suffix)).replace(old_ifo.with_suffix(suffix))
    plain = old_ifo.with_suffix(".dict")
    if plain.exists():
        plain.unlink()
    old_ifo.write_text("StarDict's dict ifo file\n" + "\n".join(key + "=" + value for key, value in fields.items()) + "\n",
                       encoding="utf-8", newline="\n")
    return count, aliases, searchable


def build_translations(folder, source, pair, database, usage=None):
    """Use scored, qualified senses; import inflection pointers, not noisy gloss merges."""
    old_ifo = next(folder.glob("*.ifo"))
    metadata = old_ifo.read_text(encoding="utf-8")
    target_language = pair[-2:].upper()
    corrections = json.loads(Path(__file__).with_name("dictionary_corrections.json").read_text(encoding="utf-8"))["entries"].get(pair, {})
    with tempfile.TemporaryDirectory(prefix=".translation-", dir=folder) as temporary:
        work = Path(temporary)
        db = sqlite3.connect(work / "entries.sqlite")
        db.executescript("PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;"
                         "CREATE TABLE entries (key BLOB PRIMARY KEY, name BLOB, body BLOB, ordinal INTEGER);"
                         "CREATE TABLE aliases (name BLOB, folded BLOB, key BLOB, PRIMARY KEY(name,key));"
                         "CREATE TABLE mapping (old INTEGER PRIMARY KEY, key BLOB);")
        original = sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
        columns = {row[1] for row in original.execute("PRAGMA table_info(translation)")}
        if not {"written_rep", "lexentry", "sense", "trans_list", "score", "importance"} <= columns:
            raise ValueError("Unsupported WikDict SQLite schema")
        rows = original.execute("SELECT written_rep,lexentry,sense,trans_list,score,importance FROM translation "
                                "WHERE trans_list IS NOT NULL ORDER BY CAST(lower(written_rep) AS BLOB),CAST(written_rep AS BLOB)")
        for _, group in groupby(rows, lambda row: row[0].encode("utf-8").lower()):
            group = list(group)
            names = [normalize_name(row[0].encode("utf-8")) for row in group]
            key = names[0].lower()
            name = next((name for name in names if name == key), names[0])
            senses = fidelity.translated_senses([row[1:] for row in group])
            if pair == "en-es" and key == b"private":
                senses = [fidelity.Sense(s.pos, tuple(v for v in s.equivalents if v != "cabo"), s.context, s.score, s.importance)
                          for s in senses]
                senses = [s for s in senses if s.equivalents]
            body = fidelity.render_translations(senses, target_language, usage).encode("utf-8")
            if not body:
                continue
            existing = db.execute("SELECT body FROM entries WHERE key=?", (key,)).fetchone()
            if existing:
                body = fidelity.merge_meanings(existing[0], body)
            db.execute("INSERT OR REPLACE INTO entries VALUES (?,?,?,NULL)", (key, name, body))
        original.close()
        for name, authored in corrections.items():
            senses = [fidelity.Sense(row["pos"], tuple(row["equivalents"]), row.get("context", ""), 0, 100 - n)
                      for n, row in enumerate(authored)]
            word = name.encode("utf-8")
            db.execute("INSERT OR REPLACE INTO entries VALUES (?,?,?,NULL)",
                       (word.lower(), word, fidelity.render_translations(senses, target_language).encode("utf-8")))
        for ifo in (old_ifo, source / (pair + ".ifo")):
            db.execute("DELETE FROM mapping")
            for ordinal, (word, _) in enumerate(records(ifo.with_suffix(".idx"), 8)):
                db.execute("INSERT INTO mapping SELECT ?,key FROM entries WHERE key=?", (ordinal, normalize_name(word).lower()))
            synonym = ifo.with_suffix(".syn")
            if synonym.exists():
                for name, ordinal in records(synonym, 4):
                    name = normalize_name(name)
                    db.execute("INSERT OR IGNORE INTO aliases SELECT ?,?,key FROM mapping WHERE old=?",
                               (name, name.lower(), struct.unpack(">I", ordinal)[0]))
        db.commit()
        augment_irregular_forms(db, pair)
        count, aliases, searchable = export_entries(db, work, old_ifo, metadata, pair)
    for name in ("LICENSE", "ATTRIBUTION"):
        shutil.copy2(source / name, folder / (name + "-expansion.txt"))
    return {"headwords": count, "aliases": aliases, "searchableForms": searchable,
            "addedHeadwords": None, "expandedHeadwords": None, "editorialEntries": sorted(corrections),
            "method": "Qualified WikDict SQLite senses; source inflection aliases; independently authored corrections."}


def merge(folder, source, pair, source_format="koreader", usage=None):
    if source_format not in {"koreader", "wikdict"}:
        raise ValueError(f"Unknown dictionary source format: {source_format}")
    old_ifo, new_ifo = next(folder.glob("*.ifo")), source / (pair + ".ifo")
    source_language, target_language = (part.upper() for part in pair.split("-"))
    metadata = old_ifo.read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix=".merge-", dir=folder) as temporary:
        work = Path(temporary)
        db = sqlite3.connect(work / "entries.sqlite")
        db.executescript("PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;"
                         "CREATE TABLE entries (key BLOB PRIMARY KEY, name BLOB, body BLOB, ordinal INTEGER);"
                         "CREATE TABLE mapping (source INTEGER, old INTEGER, key BLOB, PRIMARY KEY(source, old));"
                         "CREATE TABLE aliases (name BLOB, folded BLOB, key BLOB, PRIMARY KEY(name,key));")
        added, expanded = 0, set()
        for source_id, ifo in enumerate((old_ifo, new_ifo)):
            with raw_data(ifo, work, str(source_id) + ".dict") as path, path.open("rb") as data:
                total = path.stat().st_size
                for ordinal, (word, fields) in enumerate(records(ifo.with_suffix(".idx"), 8)):
                    offset, size = struct.unpack(">II", fields)
                    if offset + size > total:
                        raise ValueError(f"Entry outside dictionary: {ifo}")
                    word = normalize_name(word)
                    key = word.lower()
                    existing = db.execute("SELECT body FROM entries WHERE key=?", (key,)).fetchone()
                    data.seek(offset)
                    value = data.read(size)
                    if source_language == target_language:
                        value = fidelity.merge_meanings(b"", fidelity.monolingual(value.decode("utf-8"), target_language).encode("utf-8"))
                    elif source_id:
                        html = value.decode("utf-8")
                        if source_language == target_language:
                            value = compact_html(html)
                        elif source_format == "wikdict":
                            value = compact_html(html, target_language, word.decode("utf-8"), source_language)
                        else:
                            value = explicit_translation(html, target_language, word.decode("utf-8"), source_language)
                        value = value.encode("utf-8")
                    value = unicodedata.normalize("NFC", value.decode("utf-8")).encode("utf-8")
                    if value:
                        if not existing:
                            db.execute("INSERT INTO entries VALUES (?,?,?,NULL)", (key, word, value))
                            added += source_id
                        else:
                            merged = (fidelity.merge_meanings(existing[0], value) if source_language == target_language
                                      else merge_body(existing[0], value))
                            if merged != existing[0]:
                                db.execute("UPDATE entries SET body=?, name=CASE WHEN ?=? THEN ? ELSE name END WHERE key=?",
                                           (merged, word, key, word, key))
                                if source_id:
                                    expanded.add(key)
                    if value or existing:
                        db.execute("INSERT INTO mapping VALUES (?,?,?)", (source_id, ordinal, key))
            synonym = ifo.with_suffix(".syn")
            if synonym.exists():
                for name, number in records(synonym, 4):
                    name = normalize_name(name)
                    db.execute("INSERT OR IGNORE INTO aliases SELECT ?,?,key FROM mapping WHERE source=? AND old=?",
                               (name, name.lower(), source_id, struct.unpack(">I", number)[0]))
            db.commit()
        editorial = []
        for name, preferred in PREFERRED_TRANSLATIONS.get(pair, {}).items():
            key = name.encode("utf-8")
            row = db.execute("SELECT body FROM entries WHERE key=?", (key,)).fetchone()
            if row:
                prefix = preferred.encode("utf-8")
                if not row[0].startswith(prefix):
                    db.execute("UPDATE entries SET body=? WHERE key=?", (prefix + row[0], key))
                editorial.append(name)
        db.commit()
        if pair == "en-en":
            # Case-folded proper names must not hide a source's verb-form alias.
            # Keep the name sense after the genuine lemma, without changing rose etc.
            collisions = db.execute("SELECT DISTINCT e.key,e.body,a.key FROM entries e "
                                    "JOIN aliases a ON a.folded=e.key WHERE a.key<>e.key").fetchall()
            for key, body, target in collisions:
                if not proper_name_only(body):
                    continue
                lemma = db.execute("SELECT body FROM entries WHERE key=?", (target,)).fetchone()
                if lemma and not proper_name_only(lemma[0]):
                    db.execute("UPDATE entries SET body=? WHERE key=?", (lemma[0] + body, key))
            db.commit()
        augment_irregular_forms(db, pair)
        count, aliases, searchable = export_entries(db, work, old_ifo, metadata, pair)
    for name in ("LICENSE", "ATTRIBUTION"):
        shutil.copy2(source / name, folder / (name + "-expansion.txt"))
    return {"headwords": count, "aliases": aliases, "searchableForms": searchable, "addedHeadwords": added,
            "expandedHeadwords": len(expanded), "editorialEntries": editorial}


def enrich(staging, cache):
    cache.mkdir(parents=True, exist_ok=True)
    usage = NaturalUsage()
    output = []
    for pair, name, expected in ARCHIVES:
        usage.removed.clear()
        usage.refined = 0
        official = pair in WIKDICT_PAIRS
        archive_name = ("wikdict-" if official else "") + pair + ".zip"
        url = (WIKDICT_DOWNLOAD if official else RELEASE) + archive_name
        archive = cache / archive_name
        if not archive.exists():
            temporary = archive.with_suffix(".part")
            with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as data:
                shutil.copyfileobj(response, data)
            temporary.replace(archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Source archive hash mismatch: {pair}")
        with tempfile.TemporaryDirectory(prefix=".dictionary-", dir=cache) as temporary:
            root = Path(temporary)
            with zipfile.ZipFile(archive) as zipped:
                for member in zipped.infolist():
                    target = (root / member.filename).resolve()
                    if not target.is_relative_to(root.resolve()):
                        raise ValueError("Dictionary archive contains a parent path")
                zipped.extractall(root)
            source = root / ("wikdict-" + pair if official else pair)
            if official:
                metadata = (source / "stardict.ifo").read_text(encoding="utf-8")
                for path in list(source.glob("stardict.*")):
                    path.rename(source / (pair + path.name[len("stardict"):]))
                license_path = Path(__file__).resolve().parents[1] / "assets_local/sd-resources/licenses/CC-BY-SA-4.0.txt"
                shutil.copy2(license_path, source / "LICENSE")
                (source / "ATTRIBUTION").write_text(
                    "Wiktionary contributors; WikDict / Karl Bartel; DBnary; FreeDict.\n"
                    "Source: " + url + "\nSHA256: " + expected + "\n"
                    "License: CC BY-SA 4.0. Original source metadata follows.\n\n" + metadata,
                    encoding="utf-8")
            if pair[:2] == pair[-2:]:
                result = merge(staging / name, source, pair, usage=usage)
            else:
                database = cache / (pair + ".sqlite3")
                data_url = "https://download.wikdict.com/dictionaries/sqlite/2/" + database.name
                if not database.exists():
                    with urllib.request.urlopen(data_url, timeout=120) as response, database.with_suffix(".part").open("wb") as data:
                        shutil.copyfileobj(response, data)
                    database.with_suffix(".part").replace(database)
                if hashlib.sha256(database.read_bytes()).hexdigest() != fidelity.SQLITE_HASHES[pair]:
                    raise ValueError("Translation database hash mismatch: " + pair)
                result = build_translations(staging / name, source, pair, database, usage=usage)
                license_path = Path(__file__).resolve().parents[1] / "assets_local/sd-resources/licenses/CC-BY-SA-4.0.txt"
                shutil.copy2(license_path, staging / name / "LICENSE-WikDict.txt")
                (staging / name / "ATTRIBUTION-WikDict.txt").write_text(
                    "Wiktionary contributors; WikDict / Karl Bartel; DBnary.\nSource: " + data_url + "\nSHA256: " +
                    fidelity.SQLITE_HASHES[pair] + "\nLicense: CC BY-SA 4.0.\n"
                    "Senses, usage contexts, categories and relative importance from the source database.\n"
                    "CrossNext adaptation: sense ranking, deduplication and specified editorial corrections.\n",
                    encoding="utf-8")
                result.update(translationSource=data_url, translationSha256=fidelity.SQLITE_HASHES[pair])
        result.update(folder=name, url=url, sha256=expected, license="CC-BY-SA-4.0")
        usage.notices(staging / name)
        result["usageRanking"] = usage.provenance()
        output.append(result)
        print("Dictionary expansion:", name, result["headwords"], "headwords,", result["aliases"], "aliases", flush=True)
    return output
