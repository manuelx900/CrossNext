#!/usr/bin/env python3
"""Compile community CSV or repository YAML translations into microSD language packs."""
import argparse
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import unicodedata
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("crossnext_i18n", ROOT / "scripts/gen_i18n.py")
i18n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(i18n)
MAGIC = b"CPLANG01"
HEADER = struct.Struct("<8sHHIII24s64s")
ENTRY = struct.Struct("<QII")
MAX_TEXT = 1024 * 1024
MAX_ENTRIES = 8192
MAX_STRING_BYTES = 8192
MAX_FORMAT_FIELD = 128


def format_signature(text):
    """Match printf argument types/order, while allowing translated widths and flags."""
    result = []
    pos = 0
    while pos < len(text):
        if text[pos] != "%":
            pos += 1
            continue
        pos += 1
        if pos < len(text) and text[pos] == "%":
            pos += 1
            continue
        start = pos
        while pos < len(text) and text[pos] in "-+ #0":
            pos += 1
        if pos < len(text) and text[pos] == "*":
            result.append("int")
            pos += 1
        else:
            width = 0
            while pos < len(text) and "0" <= text[pos] <= "9":
                width = width * 10 + ord(text[pos]) - ord("0")
                if width > MAX_FORMAT_FIELD:
                    raise ValueError("Printf width exceeds 128")
                pos += 1
        if pos < len(text) and text[pos] == ".":
            pos += 1
            if pos < len(text) and text[pos] == "*":
                result.append("int")
                pos += 1
            else:
                precision = 0
                while pos < len(text) and "0" <= text[pos] <= "9":
                    precision = precision * 10 + ord(text[pos]) - ord("0")
                    if precision > MAX_FORMAT_FIELD:
                        raise ValueError("Printf precision exceeds 128")
                    pos += 1
        length = ""
        if text[pos:pos + 2] in ("hh", "ll"):
            length = text[pos:pos + 2]
            pos += 2
        elif pos < len(text) and text[pos] in "hljztL":
            length = text[pos]
            pos += 1
        if pos >= len(text) or text[pos] not in "diouxXfFeEgGaAcsp":
            raise ValueError(f"Invalid printf marker near {text[max(0, start - 1):pos + 1]!r}; use %% for a literal percent")
        conversion = text[pos]
        pos += 1
        kind = ("int" if conversion in "di" else "unsigned" if conversion in "ouxX" else
                "float" if conversion in "fFeEgGaA" else conversion)
        result.append(length + ":" + kind)
    return result


def reference():
    return {key: value for key, value in i18n.parse_yaml_file(str(ROOT / "lib/I18n/translations/english.yaml")).items()
            if key.startswith("STR_")}


def plain_percent_count(text):
    """Literal-percent labels are not printf formats; never admit a directive."""
    count = 0
    for match in re.finditer("%", text):
        tail = text[match.start():]
        try:
            if format_signature(tail):
                return None
        except ValueError:
            pass
        following = tail[1:2]
        if following and (following.isascii() and (following.isalnum() or following in "%*$")):
            return None
        count += 1
    return count


def normalized(text):
    text = unicodedata.normalize("NFC", text)
    if "\0" in text or any(ord(c) < 32 and c not in "\n\r\t" for c in text):
        raise ValueError("Translation contains a control character")
    text.encode("utf-8", errors="strict")
    return text


def compile_pack(translations, code, name, *, skip_invalid=False, included_keys=None):
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9-]{0,22}", code):
        raise ValueError("Language code must contain 1-23 ASCII letters, digits or hyphens")
    name = normalized(name.strip())
    if not name or len(name.encode("utf-8")) >= 64 or any(ord(c) < 32 for c in name):
        raise ValueError("Language name must fit in 63 UTF-8 bytes and contain no controls")
    source = reference()
    records = []
    warnings = []
    for key, text in translations.items():
        if key not in source:
            if skip_invalid:
                warnings.append(f"{key}: obsolete key omitted")
                continue
            raise ValueError(f"Unknown string key: {key}")
        if not text:
            continue
        try:
            text = normalized(text)
            if len(text.encode("utf-8")) > MAX_STRING_BYTES:
                raise ValueError("Translation exceeds 8192 UTF-8 bytes")
            if re.findall(r"https?://", source[key]) != re.findall(r"https?://", text):
                raise ValueError("URL schemes must remain unchanged")
            # Existing plain labels sometimes contain a literal, unescaped percent.
            # Those must preserve the percent text exactly, never become a format.
            try:
                original = format_signature(source[key])
            except ValueError:
                original = plain_percent_count(source[key])
                if original is None or plain_percent_count(text) != original:
                    raise ValueError("Literal percent label changed into a printf format")
            else:
                if format_signature(text) != original:
                    raise ValueError("Printf arguments differ from English")
            if key.startswith("STR_WEB_MSG_"):
                markers = lambda value: sorted(re.findall(r"\{[a-z]+\}", value))
                if markers(text) != markers(source[key]):
                    raise ValueError("Named web arguments differ from English")
        except ValueError as error:
            if not skip_invalid:
                raise ValueError(f"{key}: {error}") from error
            warnings.append(f"{key}: {error}")
            continue
        if text != source[key] and (included_keys is None or key in included_keys):
            records.append((i18n.language_key_hash(key), text.encode("utf-8") + b"\0"))
    records.sort()
    if len(records) > MAX_ENTRIES or len({key for key, _ in records}) != len(records):
        raise ValueError("Too many entries or a key hash collision")
    table, data = bytearray(), bytearray()
    for key_hash, text in records:
        table.extend(ENTRY.pack(key_hash, len(data), len(text) - 1))
        data.extend(text)
    if len(data) > MAX_TEXT:
        raise ValueError("Translation exceeds 1024 KiB")
    payload = table + data
    header = HEADER.pack(MAGIC, 1, 0, len(records), len(data), zlib.crc32(payload),
                         code.encode("ascii"), name.encode("utf-8"))
    return header + payload, warnings


def read_csv(path):
    result = {}
    with path.open(encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames or not {"key", "translation"}.issubset(reader.fieldnames):
            raise ValueError("CSV needs key and translation columns")
        for row in reader:
            key = row["key"]
            if key in result:
                raise ValueError(f"Duplicate key: {key}")
            if key:
                result[key] = row["translation"] or ""
    return result


def write_template(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["key", "english", "translation"])
        writer.writerows((key, value, "") for key, value in reference().items())


GUIDE = """CROSSNEXT - IDIOMAS / LANGUAGES

ESPAÑOL
Copia la carpeta languages en la raíz de la microSD, junto a books y fonts.
Abre Ajustes > Sistema > Idioma y selecciona un idioma. Confirma el reinicio.
Cada fila muestra su estado a la derecha: Integrado, microSD, En uso o No disponible.
English y Español permanecen integrados y funcionan sin estos archivos.
Solo se carga el paquete seleccionado en PSRAM. Las consultas de traducción no leen la tarjeta.
Conserva languages/fonts: contiene las fuentes de apoyo para los caracteres de los idiomas.
Si el paquete falta, está dañado o no cabe en PSRAM, se usa English. Puedes elegir otro idioma.
Los paquetes cubren los textos de la interfaz y los juegos. Combinan traducciones del proyecto
con traducción asistida y necesitan revisión por hablantes nativos. Los nombres de libros,
las definiciones de diccionarios y los textos introducidos por el usuario no se traducen.

PARA TRADUCIR
1. Copia template.csv y completa solamente la columna translation. No cambies las claves.
2. Conserva los marcadores %s, %d, %lu, etc., en el mismo orden. Vacío = respaldo inglés.
3. Desde el repositorio, ejecuta:
   python tools_local/language_packs.py build mi-idioma.csv --code fr --name Français --output fr.cplang
4. Copia fr.cplang a /languages/ y reinicia para instalarlo o reemplazarlo.
Usa un nombre de archivo ASCII (letras, números, guion, punto y guion bajo), máximo 63 caracteres.
Los paquetes se identifican por claves estables y admiten traducciones parciales.
Una clave nueva necesita su traducción; mientras tanto utiliza el texto inglés.
El ZIP incluye teclados, candidatos pinyin, vocabularios de Wordle y fuentes en languages.
Wordle incluye al menos 2000 respuestas por idioma y listas más amplias para aceptar jugadas.
Las respuestas se eligen por frecuencia de uso; también necesitan revisión nativa.
Instala estos vocabularios junto con el firmware de este paquete: usan el formato CPWB v2.
Español e inglés conservan un vocabulario integrado de respaldo si falta la microSD.
Instala también sus bancos EN.cpwb y ES.cpwb para ampliar las jugadas admitidas, incluso
en partidas anteriores. En español, las vocales con tilde y la ü se juegan sin acento;
la ñ sigue siendo distinta. El banco japonés admite lecturas en kana.
Los bancos ampliados se consultan desde la tarjeta, sin cargarlos completos en RAM.
El paquete completo necesita aproximadamente 800 MB en la microSD. Puedes conservar
solo los bancos .cpwb de los idiomas que uses. Ninguna lista finita cubre todas las palabras.
No instala diccionarios de lectura. La fuente seleccionada debe
incluir sus caracteres; las escrituras complejas también necesitan soporte del renderizador.

ENGLISH
Copy languages to the microSD root beside books and fonts. Open Settings > System > Language,
select a language and confirm the restart. English and Spanish remain built in.
Each row shows its status on the right: Built in, microSD, In use or Unavailable.
Only the selected pack is loaded into PSRAM. Translation lookups do not read the microSD.
Keep languages/fonts: it provides supporting glyphs for the interface languages.
Missing, damaged or oversized packs fall back to English. Missing translations also use English.
Packs cover interface and game text. They combine project translations with assisted
translation and still need native-speaker review. Book metadata, dictionary definitions
and user-entered text are not translated. The Ook joke pack remains partial.
Translate the translation column of template.csv, then run the build command above.
Keep printf arguments in their original order. Numeric width/precision is limited to 128.
Empty cells use English.
Wordle includes at least 2000 frequency-ranked answers per language and larger allowed-guess
lists. Answers still need native-speaker review. Install these CPWB v2 banks with the firmware
supplied with this bundle. Spanish and English retain built-in fallback vocabularies.
Install EN.cpwb and ES.cpwb as well to expand accepted guesses in existing games.
Spanish vowel accents and ü are folded for play; ñ remains distinct. Japanese accepts kana
readings. Banks are searched on the card without loading the whole vocabulary into RAM.
The complete bundle needs about 800 MB on the microSD. Keep only the .cpwb banks for the
languages you use if space is limited. No finite vocabulary covers every valid word.
Install or replace the resulting .cplang in /languages/ and restart the reader.
Install the matching firmware supplied with this bundle before using these resources.
The complete narrative packs can exceed the older 512/640 KiB text limits and need
firmware with the 1024 KiB limit. Older firmware rejects them and falls back to English.
Packs add text only: glyphs, shaping, keyboards and dictionaries are separate resources.
Limits: 1024 KiB text per pack, 8192 bytes per string, 8192 entries, 1024 installed packs listed.
To return to a built-in language, choose English or Español and confirm the restart.
Translations inherit the repository's license; preserve attribution when sharing changes.
"""


def bundle(destination, archive=None):
    destination.mkdir(parents=True, exist_ok=True)
    report = []
    active = i18n.find_used_string_keys([str(ROOT / "src"), str(ROOT / "lib")])
    for path in sorted((ROOT / "lib/I18n/translations").glob("*.yaml")):
        values = i18n.parse_yaml_file(str(path))
        if values["_language_code"] in ("EN", "ES"):
            continue
        code = values["_bcp47"]
        strings = {k: v for k, v in values.items() if k.startswith("STR_")}
        if code != "ook" and set(strings) != set(reference()):
            raise ValueError(f"Incomplete language source: {path.name}")
        blob, warnings = compile_pack(strings, code, values["_language_name"], skip_invalid=code == "ook",
                                      included_keys=active)
        target = destination / (code + ".cplang")
        target.write_bytes(blob)
        report.append({"file": target.name, "code": code, "name": values["_language_name"],
                       "sourceKeys": sum(k.startswith("STR_") for k in values),
                       "totalKeys": len(reference()),
                       "entries": struct.unpack_from("<I", blob, 12)[0], "bytes": len(blob),
                       "sha256": hashlib.sha256(blob).hexdigest(), "omittedInvalid": warnings})
    editable = destination / "translation-sources"
    editable.mkdir(exist_ok=True)
    for source in sorted((ROOT / "lib/I18n/translations").glob("*.yaml")):
        shutil.copy2(source, editable / source.name)
    write_template(destination / "template.csv")
    (destination / "README.txt").write_text(GUIDE, encoding="utf-8")
    (destination / "LICENSE.txt").write_bytes((ROOT / "LICENSE").read_bytes())
    keyboard_source = ROOT / "assets_local/language-keyboards"
    if keyboard_source.is_dir():
        for source in sorted(keyboard_source.iterdir()):
            if not source.is_file() or source.suffix not in (".cpkbd", ".bin", ".txt"):
                continue
            target = destination / "keyboards" / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    wordle_source = ROOT / 'assets_local/wordle-languages'
    if wordle_source.is_dir():
        for source in sorted(wordle_source.iterdir()):
            if source.suffix not in ('.cpwb', '.txt', '.json') or source.name == 'GLYPHS.txt': continue
            target = destination / 'wordle' / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    font_source = ROOT / "assets_local/language-fonts"
    font_files = []
    if font_source.is_dir():
        for source in sorted(font_source.rglob("*")):
            if not source.is_file() or source.name == "BASE_REPERTOIRE.txt":
                continue
            relative = Path("fonts") / source.relative_to(font_source)
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            font_files.append({"file": relative.as_posix(), "bytes": target.stat().st_size,
                               "sha256": hashlib.sha256(target.read_bytes()).hexdigest()})
    input_files = [{"file": p.relative_to(destination).as_posix(), "bytes": p.stat().st_size,
                    "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                   for folder in ('keyboards', 'wordle') for p in sorted((destination / folder).glob('*')) if p.is_file()]
    (destination / "MANIFEST.json").write_text(json.dumps({"formatVersion": 1, "packs": report, "fonts": font_files, "input": input_files,
                                                        "editableSources": [{"file": p.relative_to(destination).as_posix(), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                                                                            for p in sorted(editable.glob("*.yaml"))]},
                                                        ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if archive:
        archive.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as handle:
            for path in sorted(destination.rglob("*")):
                if path.is_file():
                    info = zipfile.ZipInfo("languages/" + path.relative_to(destination).as_posix(),
                                           date_time=(2026, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.external_attr = 0o100644 << 16
                    handle.writestr(info, path.read_bytes(), compresslevel=9)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    template = commands.add_parser("template")
    template.add_argument("output", type=Path)
    build = commands.add_parser("build")
    build.add_argument("input", type=Path)
    build.add_argument("--code", required=True)
    build.add_argument("--name", required=True)
    build.add_argument("--output", required=True, type=Path)
    resources = commands.add_parser("bundle")
    resources.add_argument("output", type=Path)
    resources.add_argument("--zip", type=Path)
    args = parser.parse_args()
    if args.command == "template":
        write_template(args.output)
    elif args.command == "build":
        translations = read_csv(args.input)
        active = i18n.find_used_string_keys([str(ROOT / "src"), str(ROOT / "lib")])
        blob, _ = compile_pack(translations, args.code, args.name, included_keys=active)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(blob)
        print(f"{args.output}: {len(blob)} bytes")
    else:
        report = bundle(args.output, args.zip)
        print(f"{len(report)} packs; {sum(p['bytes'] for p in report)} bytes on microSD")


if __name__ == "__main__":
    main()
