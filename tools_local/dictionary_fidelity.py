"""Meaning-aware formatting for the desktop dictionary resource builder."""
from dataclasses import dataclass
from html import escape, unescape
import re
import unicodedata

from compact_dictionaries import Entry, GRAMMAR, Node, TRANSLATION, compact_html, lexical_text
from dictionary_usage import register, usage_label


PROPER = dict(EN="Proper name", ES="Nombre propio", FR="Nom propre", DE="Eigenname", PT="Nome próprio")
CONTEXT = dict(EN="Context", ES="Contexto", FR="Contexte", DE="Kontext", PT="Contexto")
FORM = dict(EN="Verb form of", ES="Forma verbal de", FR="Forme verbale de", DE="Verbform von", PT="Forma verbal de")
SQLITE_HASHES = {
    "en-es": "d66fd6f8b7f3872f5fa17391846468ac18cda11a63b64961ebd36c7ba1069a79",
    "es-en": "e09d4da3699841ef6210058de837931a36a32ebfccfd2ab1b7baacf58313135c",
    "en-de": "f2154e82c4a34851922e0c0d275decea7b34ad0ec8fd087090c592b59112d0c2",
    "de-en": "7100de12c97e700f42414e7bf9c2b1801f812970c0bc62873927011acf29e6e8",
    "en-fr": "3908275d1c0fea866e681d5e065c25e4fd2c39a6d3d3dd23b48f351eb0e286a2",
    "fr-en": "888cc187b8e079de64e9ac2777157f3e86d3802c3b215c6813550db35a6ad196",
    "en-pt": "c37ed57201284cf187e17516fe9c6b4b6c1b9c166909a76e9c81cbdc19b961fd",
    "pt-en": "a7ebf4ebc21f26ee7e9539012afdfdac672af3e23b5a0fe973028c0818afe897",
}


def clean(value):
    value = lexical_text(value or "")
    value = re.sub(r"</?(?:br|div|p|li)(?:\s[^>]*)?/?>", " ", value, flags=re.I)
    value = re.sub(r"</?(?:b|i|em|strong|sup|sub|u|span|a|font)(?:\s[^>]*)?/?>", "", value, flags=re.I)
    return unicodedata.normalize("NFC", re.sub(r"\s+", " ", unescape(value))).strip()


def plain(value):
    entry = Entry()
    entry.feed(value)
    return clean(entry.root.text())


def part_of_speech(lexentry):
    """Read the source lexeme category, never infer proper names from capitals."""
    fields = (lexentry or "").split("__")
    label = fields[-2].replace("_", " ").casefold() if len(fields) > 2 else ""
    if any(term in label for term in ("proper noun", "sustantivo propio", "nompr", "prénom", "patronyme", "nom de famille", "eigenname")):
        return "proper"
    if label == "prepositional phrase":
        return "expression"
    prefixes = (
        (("noun", "nom", "substantiv", "sustantivo"), "noun"),
        (("adverb", "adv", "advérbio", "adverbio", "lokaladverb"), "adverb"),
        (("adjective", "adj", "adjetivo"), "adjective"),
        (("verb", "verbo"), "verb"),
        (("pronoun", "pronom", "pronombre", "personalpronomen", "indefinitpronomen", "demonstrativpronomen"), "pronoun"),
        (("preposition", "préposition", "preposición", "präposition"), "preposition"),
        (("interjection", "interjektion", "interjección", "interjeição", "onomatopée"), "interjection"),
        (("conjunction", "conjonction", "conjunción", "konjunktion", "subjunktion"), "conjunction"),
        (("numeral", "number", "numéral"), "numeral"),
        (("determiner", "determinante"), "determiner"),
        (("prefix", "prefijo", "préfix", "präfix"), "prefix"),
        (("suffix", "sufijo", "sufixo"), "suffix"),
        (("proverb", "sprichwort", "refrán"), "proverb"),
        (("abbreviation", "abkürzung", "abreviatura", "sigla"), "abbreviation"),
        (("phrase", "prepositional phrase", "expression", "locution", "locución", "redewendung", "wortverbindung"), "expression"),
    )
    return next((category for terms, category in prefixes if label.startswith(terms)), "")


@dataclass(frozen=True)
class Sense:
    pos: str
    equivalents: tuple
    context: str = ""
    score: float = 0
    importance: float = 0


def restricted(context):
    return register(context) in {"historical", "rare"}


def render_translations(senses, language, usage=None):
    ordered = sorted(senses, key=lambda sense: (sense.pos == "proper", restricted(sense.context),
                                               -sense.importance, -sense.score, sense.context, sense.equivalents))
    # Repeat equivalents only when their usage/context really differs.
    seen, rows = set(), []
    for sense in ordered:
        terms = usage.equivalents(sense.equivalents, language, sense.pos, sense.context) if usage else sense.equivalents
        sense = Sense(sense.pos, terms, sense.context, sense.score, sense.importance)
        identity = (sense.pos, sense.equivalents, sense.context)
        if identity in seen:
            continue
        seen.add(identity)
        rows.append(sense)
    output, category = [], None
    for sense in rows:
        label = PROPER[language] if sense.pos == "proper" else GRAMMAR[language].get(sense.pos, TRANSLATION[language])
        if category != label:
            output.append("<p><b>" + label + "</b></p>")
            category = label
        label = usage_label(sense.context, language)
        output.append("<p>" + escape("; ".join(sense.equivalents), quote=False) +
                      (" <i>(" + label + ")</i>" if label else "") + "</p>")
        if sense.context:
            output.append("<p><i>" + CONTEXT[language] + ": " + escape(sense.context, quote=False) + "</i></p>")
    return "".join(output)


def translated_senses(rows):
    # Reverse-generated alternatives have no sense/category to disambiguate them.
    # Prefer qualified forward entries when present; retain reverse-only words.
    has_lexeme = any(row[0] for row in rows)
    result = []
    for lexentry, context, targets, score, importance in rows:
        if has_lexeme and not lexentry:
            continue
        equivalents = tuple(dict.fromkeys(clean(term) for term in targets.split(" | ") if clean(term)))
        if equivalents:
            pos = part_of_speech(lexentry)
            if re.match(r"^(?:deutscher |englischer |französischer )?(?:Nachname|Familienname|surname)\b", context or "", re.I):
                pos = "proper"
            result.append(Sense(pos, equivalents, clean(context), float(score or 0), float(importance or 0)))
    contextual = {(s.pos, s.equivalents) for s in result if s.context}
    result = [s for s in result if s.context or (s.pos, s.equivalents) not in contextual]
    return result


def monolingual(source, language):
    """Remove isolated phonetics and give imported inline categories real headings."""
    tree = Entry()
    tree.feed(source)
    root = tree.root
    content = [c for c in root.children if isinstance(c, Node)]
    if any(c.tag == "div" and any(isinstance(n, Node) and n.tag == "i" and
                                  re.fullmatch(r"(?:prop\. n|n|v|adj|adv|pron|prep|conj|interj|num|det)\.", n.text().strip())
                                  for n in c.children[:2]) for c in content):
        for c in content:
            if c.tag == "div" and re.fullmatch(r"(?:\[[^\]]+\]|/[^/]+/)(?:\s*,?\s*(?:\[[^\]]+\]|/[^/]+/))*", c.text().strip()):
                c.children.clear()
    # Feed sanitized markup back through the common compact renderer.
    def html(node):
        if isinstance(node, str):
            return escape(node, quote=False)
        attrs = "".join(" " + k + '="' + escape(v or "", quote=True) + '"' for k, v in node.attrs.items())
        inner = "".join(html(c) for c in node.children)
        return ("<" + node.tag + attrs + ">" + inner + ("</" + node.tag + ">" if node.tag not in Entry.VOID else "")) if node.tag else inner
    value = compact_html(html(root))
    categories = {"n.": "noun", "v.": "verb", "adj.": "adjective", "adv.": "adverb", "pron.": "pronoun",
                  "prep.": "preposition", "conj.": "conjunction", "interj.": "interjection", "num.": "numeral", "det.": "determiner"}
    def category(match):
        label = match[1].strip()
        heading = PROPER[language] if label == "prop. n." else GRAMMAR[language][categories[label]]
        return "<p><b>" + heading + "</b></p><p>"
    return re.sub(r"<p><i>(prop\. n\.|n\.|v\.|adj\.|adv\.|pron\.|prep\.|conj\.|interj\.|num\.|det\.)</i>\s*", category, value)


def merge_meanings(existing, addition):
    """Deduplicate whole meanings, retaining their examples and explicit qualifiers."""
    groups = []
    for body in (existing, addition):
        parser = Entry()
        parser.feed(body.decode("utf-8"))
        heading, current = "", None
        for node in parser.root.children:
            if not isinstance(node, Node) or not node.text().strip():
                continue
            text = clean(node.text())
            if node.tag == "p" and len(node.children) == 1 and isinstance(node.children[0], Node) and node.children[0].tag == "b":
                heading = text
                current = None
                continue
            # compact_html() produces only p/b/i markup, so retain the original inline tags.
            def inline(n):
                if isinstance(n, str):
                    return escape(n, quote=False)
                value = "".join(inline(c) for c in n.children)
                return "<" + n.tag + ">" + value + "</" + n.tag + ">" if n.tag in {"b", "i"} else value
            value = "".join(inline(c) for c in node.children)
            numbered = re.match(r"^\d+(?:[a-z])?(?:\.|\s)", text)
            # Quotes, examples, synonyms and explicit usage lines belong to the preceding meaning.
            continuation = bool(current and (re.match(r"^(?:[\"“«]|Ejemplo\b|Example\b|Sinónimos?\b|Synonyms?\b|Antonyms?\b|Antónimos?\b)", text, re.I)
                                           or len(node.children) == 1 and isinstance(node.children[0], Node) and node.children[0].tag == "i"))
            if continuation and not numbered:
                if value not in current[2]:
                    current[2].append(value)
                continue
            value = re.sub(r"^\d+(?:[a-z])?\.\s+(?!\d)", "", value)
            category = heading.casefold()
            for base in ("sustantivo", "adjetivo", "verbo", "adverbio", "noun", "verb", "adjective", "adverb"):
                if re.fullmatch(base + r"(?: (?:masculino|femenino|transitivo|intransitivo|y))*", category):
                    category = base
                    break
            signature = (category, plain(value).rstrip("."))
            previous = next((group for group in groups if group[3] == signature), None)
            if previous:
                current = previous
            else:
                current = [heading, value, [], signature]
                groups.append(current)
    def name_group(group):
        return any(term in group[0].casefold() for term in ("proper", "propio", "nombre propio", "sigla")) or bool(
            re.match(r"^(?:A |An |Un |Una )?(?:surname\b|apellido\b|sigla\b|acrónimo\b|abbreviation\b|initialism\b)", plain(group[1]), re.I))
    groups.sort(key=lambda group: (name_group(group), restricted(plain(group[1]))))
    output, previous = [], None
    for heading, value, extras, _ in groups:
        if heading and heading != previous:
            output.append("<p><b>" + escape(heading) + "</b></p>")
            previous = heading
        output.append("<p>" + value + "</p>")
        output.extend("<p>" + extra + "</p>" for extra in extras)
    return "".join(output).encode("utf-8")


def verb_meanings(body, language):
    """A verb form must not inherit the noun meanings of its lemma."""
    parser = Entry()
    parser.feed(body.decode("utf-8"))
    categories = {label for labels in GRAMMAR.values() for label in labels.values()} | set(PROPER.values())
    active, output = False, []
    for node in parser.root.children:
        if not isinstance(node, Node) or node.tag != "p":
            continue
        if len(node.children) == 1 and isinstance(node.children[0], Node) and node.children[0].tag == "b":
            label = node.text().strip()
            if label in categories:
                active = label == GRAMMAR[language]["verb"]
        if active:
            def markup(n):
                if isinstance(n, str):
                    return escape(n, quote=False)
                text = "".join(markup(c) for c in n.children)
                return "<" + n.tag + ">" + text + "</" + n.tag + ">" if n.tag in {"p", "b", "i"} else text
            output.append(markup(node))
    return "".join(output).encode("utf-8")
