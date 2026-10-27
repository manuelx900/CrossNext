"""Compact StarDict HTML in generated resource copies, preserving index order.

The licensed source dictionaries remain untouched. Monolingual definitions,
examples and lexical relations are kept. Bilingual entries show destination
equivalents and localized categories without source-language glosses. No generated
definitions, synonyms or examples are added.
"""
from html import escape
from html.parser import HTMLParser
from pathlib import Path
import gzip
import hashlib
import re
import shutil
import struct
import tempfile


# Localize the source part-of-speech labels to the destination language.
# Gender belongs to the source lemma and is not transferred to the translated word.
GRAMMAR_KEYS = ("noun adjective verb adverb pronoun preposition conjunction interjection article numeral "
                "determiner prefix suffix affix proverb phraseologicalunit expression postposition symbol particle "
                "abbreviation letter indefinitepronoun ordinaladjective interrogativepronoun personalpronoun "
                "possessiveadjective possessivepronoun pronominaladverb demonstrativepronoun").split()
GRAMMAR = {
    "EN": ("Noun|Adjective|Verb|Adverb|Pronoun|Preposition|Conjunction|Interjection|Article|Numeral|"
           "Determiner|Prefix|Suffix|Affix|Proverb|Phrase|Expression|Postposition|Symbol|Particle|"
           "Abbreviation|Letter|Indefinite pronoun|Ordinal adjective|Interrogative pronoun|Personal pronoun|"
           "Possessive adjective|Possessive pronoun|Pronominal adverb|Demonstrative pronoun"),
    "ES": ("Sustantivo|Adjetivo|Verbo|Adverbio|Pronombre|Preposición|Conjunción|Interjección|Artículo|Numeral|"
           "Determinante|Prefijo|Sufijo|Afijo|Proverbio|Locución|Expresión|Posposición|Símbolo|Partícula|"
           "Abreviatura|Letra|Pronombre indefinido|Adjetivo ordinal|Pronombre interrogativo|Pronombre personal|"
           "Adjetivo posesivo|Pronombre posesivo|Adverbio pronominal|Pronombre demostrativo"),
    "FR": ("Nom|Adjectif|Verbe|Adverbe|Pronom|Préposition|Conjonction|Interjection|Article|Numéral|"
           "Déterminant|Préfixe|Suffixe|Affixe|Proverbe|Locution|Expression|Postposition|Symbole|Particule|"
           "Abréviation|Lettre|Pronom indéfini|Adjectif ordinal|Pronom interrogatif|Pronom personnel|"
           "Adjectif possessif|Pronom possessif|Adverbe pronominal|Pronom démonstratif"),
    "DE": ("Substantiv|Adjektiv|Verb|Adverb|Pronomen|Präposition|Konjunktion|Interjektion|Artikel|Numerale|"
           "Determinativ|Präfix|Suffix|Affix|Sprichwort|Redewendung|Ausdruck|Postposition|Symbol|Partikel|"
           "Abkürzung|Buchstabe|Indefinitpronomen|Ordinaladjektiv|Interrogativpronomen|Personalpronomen|"
           "Possessivadjektiv|Possessivpronomen|Pronominaladverb|Demonstrativpronomen"),
    "PT": ("Substantivo|Adjetivo|Verbo|Advérbio|Pronome|Preposição|Conjunção|Interjeição|Artigo|Numeral|"
           "Determinante|Prefixo|Sufixo|Afixo|Provérbio|Locução|Expressão|Posposição|Símbolo|Partícula|"
           "Abreviatura|Letra|Pronome indefinido|Adjetivo ordinal|Pronome interrogativo|Pronome pessoal|"
           "Adjetivo possessivo|Pronome possessivo|Advérbio pronominal|Pronome demonstrativo"),
}
GRAMMAR = {language: dict(zip(GRAMMAR_KEYS, labels.split("|"), strict=True)) for language, labels in GRAMMAR.items()}
TRANSLATION = {"EN": "Translation", "ES": "Traducción", "FR": "Traduction", "DE": "Übersetzung", "PT": "Tradução"}


class Node:
    def __init__(self, tag="", attrs=()):
        self.tag, self.attrs, self.children = tag, dict(attrs), []

    def text(self):
        return "".join(child.text() if isinstance(child, Node) else child for child in self.children)


class Entry(HTMLParser):
    VOID = {"br", "hr", "img", "wbr", "input", "meta", "link", "source"}

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = Node()
        self.stack = [self.root]

    def handle_starttag(self, tag, attrs):
        node = Node(tag, attrs)
        self.stack[-1].children.append(node)
        if tag not in self.VOID:
            self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag not in self.VOID:
            self.handle_endtag(tag)

    def handle_endtag(self, tag):
        for n in range(len(self.stack) - 1, 0, -1):
            if self.stack[n].tag == tag:
                del self.stack[n:]
                break

    def handle_data(self, data):
        self.stack[-1].children.append(data)


def pronunciation(node):
    # A semantic list containing a nested pronunciation stays intact.
    if node.tag != "li" or any(isinstance(c, Node) and c.tag in {"ul", "ol", "dl"} for c in node.children):
        return False
    def phonetic_markup(value):
        if not isinstance(value, Node):
            return False
        return (bool({"IPA", "enPR"} & set(value.attrs.get("class", "").split())) or
                "pronunciation" in value.attrs.get("title", "") or
                any(phonetic_markup(child) for child in value.children))
    label = node.text()
    return (phonetic_markup(node) and bool(re.search(r"\b(?:IPA|AFI|enPR|SAMPA)\s*\)?\s*:|pronunciación\b", label, re.I))) or bool(
        re.match(r"^\s*silabación\s*:", label))


def inflection_row(node):
    if node.tag != "p":
        return False
    children = [c for c in node.children if not isinstance(c, str) or c.strip()]
    if not children or not isinstance(children[0], Node) or children[0].tag not in {"b", "strong"}:
        return False
    fields = node.text().split("¦")
    return len(fields) > 1 and all(re.match(r"^\s*(?:plural|femenino|masculino|superlativo)\b[^:]*:", f, re.I)
                                   for f in fields[1:])


def translation_html(entry, language, headword="", source_language=None):
    """Read WikDict's explicit target divs; source-language glosses are not translations.

    Top-level sense groups stay separate, with alternative equivalents on one line.
    No monolingual definition is guessed from an ambiguous translated lemma.
    """
    groups, categories, source_categories = [], [], []

    def grammar(node):
        return (isinstance(node, Node) and node.tag == "font" and
                "grammar" in node.attrs.get("class", "").split())

    def targets(node):
        if not isinstance(node, Node):
            return []
        if node.tag == "div" and not any(isinstance(c, Node) for c in node.children):
            value = re.sub(r"\s+", " ", node.text()).strip()
            return [value] if value else []
        return [term for child in node.children for term in targets(child)]

    def flat_target(item):
        children = [c for c in item.children if not isinstance(c, str) or c.strip()]
        return (len(children) == 1 and isinstance(children[0], Node) and children[0].tag == "div" and
                not any(isinstance(c, Node) for c in children[0].children))

    def visit(node):
        if not isinstance(node, Node):
            return
        if grammar(node):
            key = node.text().split(",", 1)[0].strip().casefold()
            if key in {"male", "female", "neutral"}:
                return
            if key not in GRAMMAR[language]:
                raise ValueError(f"Unmapped WikDict grammar: {key!r}")
            source_categories.append(key)
            categories.append(GRAMMAR[language][key])
        elif node.tag == "ol":
            items = [child for child in node.children if isinstance(child, Node) and child.tag == "li"]
            if items and all(flat_target(child) for child in items):
                values = targets(node)
                if values:
                    groups.append(values)
                return
            for child in node.children:
                if isinstance(child, Node) and child.tag == "li":
                    values = targets(child)
                    if values:
                        groups.append(values)
                elif isinstance(child, Node):
                    raise ValueError("Unexpected WikDict list structure")
        elif node.tag == "div" and not any(isinstance(c, Node) for c in node.children):
            values = targets(node)
            if values:
                groups.append(values)
        else:
            for child in node.children:
                visit(child)

    visit(entry.root)
    # Private is the lowest army rank, not corporal (cabo). This narrow source
    # correction affects only the EN-ES noun, never the independent headword cabo.
    # https://dictionary.cambridge.org/dictionary/english-spanish/private
    # https://www.larousse.com/en/dictionaries/english-spanish/private/29210
    if source_language == "EN" and language == "ES" and headword.casefold() == "private" and source_categories == ["noun"]:
        groups = [[term for term in group if term != "cabo"] for group in groups]
        groups = [group for group in groups if group]
    if not groups:
        raise ValueError("WikDict entry has no explicit target translations")
    heading = " / ".join(dict.fromkeys(categories)) or TRANSLATION[language]
    rows = ["; ".join(escape(term, quote=False) for term in group) for group in groups]
    body = "".join(f"<p>{n + 1}. {row}</p>" if len(rows) > 1 else f"<p>{row}</p>"
                   for n, row in enumerate(rows))
    return f"<p><b>{heading}</b></p>" + body


def compact_html(source, target_language=None, headword="", source_language=None):
    entry = Entry()
    entry.feed(source)
    entry.close()
    if target_language is not None:
        return translation_html(entry, target_language, headword, source_language)
    def render(node, marker="", semantic=False):
        if isinstance(node, str):
            return escape(re.sub(r"\s+", " ", node), quote=False)
        tag = node.tag
        semantic = semantic or tag in {"ol", "dl"}
        if tag in {"script", "style", "audio", "video"} or (not semantic and (inflection_row(node) or pronunciation(node))):
            return ""
        children = node.children
        if tag == "div" and re.match(r"^\s*/", node.text()):
            # WikDict's initial phonetics precede the first explicit line break.
            first_break = next((n for n, c in enumerate(children) if isinstance(c, Node) and c.tag == "br"), None)
            if first_break is not None and any(isinstance(c, Node) and c.tag == "font" and
                                               c.attrs.get("color") == "gray" for c in children[:first_break]):
                children = children[first_break + 1:]
        if tag == "section":
            heading = next((c for c in children if isinstance(c, Node)), None)
            def has_senses(value):
                return isinstance(value, Node) and (value.tag in {"ol", "dt"} or
                                                   any(has_senses(c) for c in value.children))
            if heading and heading.tag in {"h2", "h3", "h4"} and heading.text().strip().casefold() in {
                    "etymology", "etimología", "pronunciation", "pronunciación", "references", "referencias",
                    "external links", "enlaces externos"} and not has_senses(node):
                return ""
            if heading and heading.tag in {"h2", "h3", "h4"} and heading.text().strip().casefold() in {
                    "synonyms", "synonym", "sinónimos", "sinónimo", "antonyms", "antonym", "antónimos", "antónimo"}:
                groups = [c for c in children if isinstance(c, Node) and c.tag == "ul"]
                terms = [render(term, semantic=semantic).strip().replace("\n", " ") for group in groups for term in group.children
                         if isinstance(term, Node) and term.tag == "li"]
                rest = "".join(render(c, semantic=semantic) for c in children if c is not heading and c not in groups)
                if terms:
                    return "\n<b>" + escape(heading.text().strip()) + ":</b> " + "; ".join(terms) + "\n" + rest
        if tag in {"ol", "ul"}:
            rows = []
            number = 0
            for child in children:
                if isinstance(child, Node) and child.tag == "li":
                    if tag == "ul" and not semantic and pronunciation(child):
                        continue
                    number += 1
                    prefix = f"{number}. " if tag == "ol" else ""
                    rows.append(render(child, prefix, semantic))
                else:
                    rows.append(render(child, semantic=semantic))
            return "\n" + "".join(rows) + "\n"
        if tag == "dl" and any(isinstance(c, Node) and c.tag == "dt" for c in children):
            rows, term = [], ""
            for child in children:
                if isinstance(child, Node) and child.tag == "dt":
                    if term:
                        rows.append(term + "\n")
                    term = "".join(render(c, semantic=semantic) for c in child.children).strip() + ". "
                elif isinstance(child, Node) and child.tag == "dd":
                    rows.append(term + "".join(render(c, semantic=semantic) for c in child.children) + "\n")
                    term = ""
                else:
                    rows.append(render(child, semantic=semantic))
            return "\n" + "".join(rows) + term + "\n"
        value = "".join(render(c, semantic=semantic) for c in children)
        if tag in {"b", "strong"} or tag == "font" and "grammar" in node.attrs.get("class", "").split():
            return "\n".join(f"<b>{line}</b>" if line.strip() else "" for line in value.split("\n"))
        if tag in {"i", "em"}:
            return "\n".join(f"<i>{line}</i>" if line.strip() else "" for line in value.split("\n"))
        if tag in {"sub", "sup"}:
            return " [" + value.strip() + "]"
        if tag.startswith("h") and tag in {"h1", "h2", "h3", "h4", "h5", "h6"}:
            return f"\n<b>{value.strip()}</b>\n"
        if tag in {"br", "hr"}:
            return "\n"
        if tag == "li":
            return "\n" + marker + value.strip() + "\n"
        if tag in {"div", "p", "dd", "dt", "blockquote"}:
            return "\n" + value.strip() + "\n"
        return value

    lines = [line.strip() for line in render(entry.root).splitlines() if line.strip()]
    result = "".join(f"<p>{line}</p>" for line in lines)
    # Metadata-only entries still retain their source text, escaped for XHTML.
    return result if result else "<p>" + escape(re.sub(r"\s+", " ", entry.root.text()).strip(), quote=False) + "</p>"


def compact_dictionary(folder):
    """Rewrite a generated folder; return old-path/new-path/hash mappings."""
    folder = Path(folder)
    pair = re.fullmatch(r"WikDict (EN|ES|FR|DE|PT)-(EN|ES|FR|DE|PT)", folder.name)
    target_language = pair[2] if pair else None
    ifos = list(folder.glob("*.ifo"))
    if len(ifos) != 1:
        raise ValueError(f"Expected one dictionary: {folder}")
    ifo = ifos[0]
    metadata = ifo.read_text(encoding="utf-8")
    if "sametypesequence=h" not in metadata.splitlines():
        return []
    if "idxoffsetbits=64" in metadata.splitlines():
        raise ValueError(f"64-bit index is unsupported: {ifo}")
    index = ifo.with_suffix(".idx")
    data = ifo.with_suffix(".dict")
    source_data = data if data.exists() else Path(str(data) + ".dz")
    inputs = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in (ifo, index, source_data)}
    count, old_bytes, new_bytes = 0, 0, 0
    with tempfile.TemporaryDirectory(prefix=".compact-", dir=folder) as work:
        work = Path(work)
        raw = work / "source.dict"
        if source_data.name.endswith(".dz"):
            with gzip.open(source_data, "rb") as source, raw.open("wb") as output:
                shutil.copyfileobj(source, output)
        else:
            raw = source_data
        raw_size = raw.stat().st_size
        with index.open("rb") as idx, raw.open("rb") as source, (work / "out.idx").open("wb") as new_idx, \
                (work / "out.dict").open("wb") as new_data:
            while True:
                word = bytearray()
                while True:
                    char = idx.read(1)
                    if not char:
                        if word:
                            raise ValueError(f"Unterminated word in {index}")
                        break
                    word.extend(char)
                    if char == b"\0":
                        break
                if not word:
                    break
                fields = idx.read(8)
                if len(fields) != 8:
                    raise ValueError(f"Truncated index: {index}")
                offset, size = struct.unpack(">II", fields)
                if offset > raw_size or size > raw_size - offset:
                    raise ValueError(f"Entry outside data file: {index}")
                source.seek(offset)
                original = source.read(size)
                value = compact_html(original.decode("utf-8"), target_language,
                                     word[:-1].decode("utf-8"), pair[1] if pair else None).encode("utf-8")
                new_idx.write(word + struct.pack(">II", new_data.tell(), len(value)))
                new_data.write(value)
                count += 1
                old_bytes += size
                new_bytes += len(value)
        declared = re.search(r"^wordcount=(\d+)$", metadata, re.M)
        if not declared or int(declared[1]) != count:
            raise ValueError(f"Word count mismatch: {ifo}")
        metadata = re.sub(r"^idxfilesize=\d+$", f"idxfilesize={(work / 'out.idx').stat().st_size}", metadata, flags=re.M)
        (work / "out.idx").replace(index)
        (work / "out.dict").replace(data)
        if source_data != data:
            source_data.unlink()
        ifo.write_text(metadata, encoding="utf-8", newline="\n")
    return [{"input": old, "output": data if old == source_data else old, "sourceSha256": sha,
             "targetLanguage": target_language,
             "entries": count, "inputBytes": old_bytes, "outputBytes": new_bytes} for old, sha in inputs.items()]
