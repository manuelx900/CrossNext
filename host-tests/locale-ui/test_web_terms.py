"""Keep static web labels connected to the firmware's English source catalogue."""
import importlib.util
import ast
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("i18n", ROOT / "scripts/gen_i18n.py")
i18n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(i18n)


class WebTerms(unittest.TestCase):
    def test_sources_and_static_calls(self):
        english = i18n.parse_yaml_file(str(ROOT / "lib/I18n/translations/english.yaml"))
        header = (ROOT / "src/apps_local/ui/WebUiTerms.h").read_text("utf8")
        entries = re.findall(r'\{((?:"(?:[^"\\]|\\.)*"\s*)+),\s*StrId::(STR_\w+)\}', header)
        terms = {"".join(json.loads(part) for part in re.findall(r'"(?:[^"\\]|\\.)*"', text)): key
                 for text, key in entries}
        self.assertEqual(len(terms), len(entries))
        for text, key in terms.items():
            with self.subTest(key=key):
                self.assertEqual(english[key], text)
        for path in (ROOT / "src/network/html").glob("*.html"):
            source = path.read_text("utf8")
            for quoted in re.findall(r'''ui(?:Text|Html|Format)\(\s*("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')''', source):
                with self.subTest(page=path.name, text=quoted):
                    self.assertIn(ast.literal_eval(quoted), terms)
            self.assertNotIn('${uiText(', source)
            marked = MarkedText()
            marked.feed(source)
            for text in marked.texts:
                with self.subTest(page=path.name, marked=text):
                    if any(character.isalpha() for character in text) and text != "📦 85% JPEG":
                        self.assertIn(text, terms)


class MarkedText(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stack = []
        self.texts = []

    def handle_starttag(self, tag, attrs):
        if tag not in {"input", "br", "hr", "meta", "link", "img", "source"}:
            self.stack.append((tag, "data-ui" in dict(attrs)))

    def handle_endtag(self, tag):
        for index in range(len(self.stack) - 1, -1, -1):
            if self.stack[index][0] == tag:
                self.stack = self.stack[:index]
                break

    def handle_data(self, text):
        if self.stack and self.stack[-1][1] and text.strip():
            self.texts.append(text.strip())


if __name__ == "__main__":
    unittest.main()
