"""Desktop-only usage ranking; never synthesizes a translation or rewrites a gloss."""
from collections import Counter
from functools import lru_cache
import hashlib
from importlib import metadata
from pathlib import Path
import re
import shutil


VERSION = "3.1.1"
SOURCE = "https://github.com/rspeer/wordfreq/tree/v3.1.1"
DATA_HASHES = {
    "en": "dffae8066b78dce0a6667cf5f58e567054f902674667090a7ac8a8a44628b05c",
    "es": "14f326b4f68d517f9b8b99c1e26ef56a508d2dc8d0ee7a9e6e8732ddab1aa65e",
    "fr": "6f16cd80b9b66c5698ed002becea83ae30d0584c3205f1cf01714ad562c00c8b",
    "de": "8ef04d3f4a28fb48d800d25fa80a96d90a5f80c8496c9a453f99f887c6ad3c12",
    "pt": "7d764586bca6262f554d5fa77ad8e6841ef42534776e70558065140853660ce2",
}
REGISTERS = {
    "historical": ("Historical use", "Uso antiguo", "Usage ancien", "Veralteter Gebrauch", "Uso antigo"),
    "regional": ("Regional use", "Uso regional", "Usage régional", "Regionaler Gebrauch", "Uso regional"),
    "informal": ("Informal use", "Uso coloquial", "Usage familier", "Umgangssprachlich", "Uso informal"),
    "rare": ("Uncommon use", "Uso poco frecuente", "Usage rare", "Seltener Gebrauch", "Uso pouco frequente"),
}
LANGUAGES = ("EN", "ES", "FR", "DE", "PT")


def register(context):
    # Usage markers are delimited metadata, not adjectives in an ordinary gloss.
    prefix = re.sub(r"^\s*\d+\.\s*", "", context).strip().casefold()
    if re.match(r"^sens ancien(?:\s+(?:de|du)\b|\s+d['’]|\s*:|$)", prefix):
        return "historical"
    if prefix.startswith("("):
        prefix = prefix[1:].split(")", 1)[0]
    else:
        prefix = prefix.split(":", 1)[0].split(".", 1)[0]
    tokens = [token.strip() for token in re.split(r"[,;]", prefix)]
    patterns = {
        "historical": (r"archaic|obsolete|dated|historical use|veraltet(?:e|er|es|en)?(?: bedeutung)?|"
                       r"veraltend(?:e|er|es|en)?(?: bedeutung)?|vieilli|désuet|anciennement|desusado|"
                       r"anticuado|obsoleto|antigo|arcaico|uso antiguo|usage ancien"),
        "regional": r"regional|dialectal|dialect|régional",
        "informal": r"informal|colloquial|slang|familier|coloquial|umgangssprachlich",
        "rare": r"rare|uncommon|raramente|raro|poco usado|selten",
    }
    for kind, pattern in patterns.items():
        if any(re.fullmatch(pattern, token) for token in tokens):
            return kind
    return ""


def usage_label(context, language):
    label = register(context)
    return REGISTERS[label][LANGUAGES.index(language)] if label else ""


class NaturalUsage:
    def __init__(self, frequency=None):
        self.removed = Counter()
        self.refined = 0
        self._frequency = frequency
        if frequency is None:
            try:
                import wordfreq
            except ImportError as error:
                raise RuntimeError("Install desktop resource requirements: python -m pip install -r tools_local/requirements-resources.txt") from error
            if metadata.version("wordfreq") != VERSION:
                raise ValueError("Dictionary usage ranking requires wordfreq==" + VERSION)
            self.package = Path(wordfreq.__file__).parent
            self.distribution = metadata.distribution("wordfreq")
            for language, expected in DATA_HASHES.items():
                path = self.package / "data" / ("large_" + language + ".msgpack.gz")
                if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
                    raise ValueError("Word frequency data hash mismatch: " + language)
            self._frequency = wordfreq.zipf_frequency

    @lru_cache(maxsize=131072)
    def frequency(self, term, language):
        return self._frequency(term, language.lower())

    def equivalents(self, terms, language, pos="", context=""):
        # Keep qualified/compound alternatives intact. Low corpus counts alone
        # cannot invalidate a regional term, technical name or fixed phrase.
        qualified = context.lstrip().startswith("(") or re.match(
            r"^(?:medicine|biology|botany|anatomy|mathematics|computing|law|finance|medicina|biología|botánica|"
            r"anatomía|informática|matemáticas|derecho|médecine|biologie|botanique|informatique|medizin|biologie|"
            r"botanik|informatik|mathematik|juristisch|biologia|botânica|informática)\b", context, re.I)
        ambiguous_symbol = any(term.isalpha() and len(term) <= 3 and term[0].isupper() for term in terms)
        if pos in {"proper", "symbol", "abbreviation", "numeral", "letter"} or register(context) or qualified or ambiguous_symbol:
            return terms
        positions = [n for n, term in enumerate(terms) if term.isalpha() and len(term) > 1]
        if len(positions) < 2:
            return terms
        scores = {n: self.frequency(terms[n], language) for n in positions}
        best = max(scores.values())
        removed = {n for n in positions if best >= 3.5 and scores[n] < 1.5 and best - scores[n] >= 2.0}
        for n in removed:
            self.removed[(language, terms[n])] += 1
        # A global count cannot distinguish singular/plural or the best sense
        # of a common word. Preserve the source order of surviving equivalents.
        result = tuple(term for n, term in enumerate(terms) if n not in removed)
        self.refined += result != terms
        return result

    def provenance(self):
        return {"source": SOURCE, "version": VERSION, "dataSha256": DATA_HASHES,
                "dataCoverage": "Language usage through about 2021; frequency is not a semantic correctness test.",
                "policy": {"candidateZipfBelow": 1.5, "commonEquivalentZipfAtLeast": 3.5,
                           "minimumZipfDifference": 2.0, "survivingOrder": "source"},
                "removedRedundantAlternatives": sum(self.removed.values()), "refinedGroups": self.refined}

    def notices(self, folder):
        # METADATA contains the upstream attribution, SUBTLEX credits and source terms.
        source = Path(self.distribution.locate_file("wordfreq-" + VERSION + ".dist-info/METADATA"))
        (folder / "ATTRIBUTION-wordfreq.txt").write_text(
            "wordfreq / Robyn Speer. Desktop-only usage ranking for this generated adaptation.\n"
            "Source: " + SOURCE + "\nData: CC BY-SA 4.0. Library: Apache-2.0.\n"
            "SUBTLEX word lists are freely available; credit Marc Brysbaert and Boris New.\n"
            "No raw frequency word list or runtime library is installed on the reader.\n\n" + source.read_text(encoding="utf-8"),
            encoding="utf-8")
        license_file = self.distribution.locate_file("wordfreq-" + VERSION + ".dist-info/LICENSE.txt")
        shutil.copy2(license_file, folder / "LICENSE-wordfreq.txt")
