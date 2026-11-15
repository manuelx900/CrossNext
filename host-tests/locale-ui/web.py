"""Check translated note editing and HTML escaping with a local headless browser."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--scripts", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--browser", help="Existing Chrome/Chromium executable")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location("html_builder", ROOT / "scripts/build_html.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
source = ROOT / "src/network/html"
note = "Café <USER_NAME> & 你好"
original = "- [ ] " + note + "\n"

with sync_playwright() as playwright:
    browser = playwright.chromium.launch(headless=True, executable_path=args.browser)
    for code in ("en", "es", "de", "ar"):
        for width, height in ((390, 844), (844, 390)):
            page = browser.new_page(viewport={"width": width, "height": height})
            errors, saved = [], []
            page.on("pageerror", lambda error: errors.append(str(error)))

            def route(request):
                path = request.request.url.split("example.test", 1)[1]
                if path == "/ui.js":
                    request.fulfill(content_type="application/javascript; charset=utf-8",
                                    body=(args.scripts / (code + ".js")).read_text("utf8"))
                elif path == "/n/text":
                    if request.request.method == "PUT":
                        saved.append(request.request.post_data)
                    request.fulfill(content_type="text/plain; charset=utf-8", body=original,
                                    headers={"X-Note-Name": "USER_NAME", "X-Note-Kind": "list"})
                elif path == "/":
                    html = builder.resolve_includes((source / "NotesPage.html").read_text("utf8"), str(source))
                    request.fulfill(content_type="text/html; charset=utf-8", body=html)
                elif path == "/fonts":
                    html = builder.resolve_includes((source / "FontsPage.html").read_text("utf8"), str(source))
                    request.fulfill(content_type="text/html; charset=utf-8", body=html)
                elif path == "/api/fonts":
                    request.fulfill(content_type="application/json", body=json.dumps({"vectorFonts": True,
                        "families": [{"name": "USER_NAME", "sizes": [0], "files": [{"size": 1024}]}]}))
                elif path == "/api/fonts/upload":
                    saved.append(request.request.post_data)
                    request.fulfill(content_type="application/json", body='{"ok":true}')
                else:
                    request.fulfill(status=404, body="")

            page.route("**/*", route)
            page.goto("http://example.test/")
            page.wait_for_function("!document.getElementById('save').disabled")
            assert page.locator("html").get_attribute("lang") == code
            assert page.locator("html").get_attribute("dir") == ("rtl" if code == "ar" else "ltr")
            assert page.locator("textarea").first.input_value() == note
            assert page.locator("#name").inner_text() == "USER_NAME"
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
            # Translations inserted into generated HTML must survive quotes and markup.
            assert page.evaluate(r"""() => {
              const label = '\"<tag>&\'example';
              window.CrossNextUi['TEST'] = label;
              const host = document.createElement('div');
              host.innerHTML = `<button title="${uiHtml('TEST')}">x</button>`;
              return host.children.length === 1 && host.firstChild.getAttribute('title') === label &&
                     host.firstChild.attributes.length === 1;
            }""")
            assert page.evaluate("""() => {
              const source = 'First {one}, then {two}';
              CrossNextUi[source] = '{two} / {one}';
              if (uiFormat(source, {one:'USER_NAME', two:2}) !== '2 / USER_NAME') return false;
              CrossNextUi[source] = '{wrong}';
              return uiFormat(source, {one:'USER_NAME', two:2}) === 'First USER_NAME, then 2';
            }""")
            page.locator("#save").click()
            page.wait_for_function("!document.getElementById('save').disabled")
            assert saved == [original], saved
            assert not errors, errors
            page.screenshot(path=str(args.output / f"notes-{code}-{width}.png"), full_page=True)
            page.goto("http://example.test/fonts")
            page.wait_for_selector("#families h3")
            assert page.locator("#families h3").inner_text() == "USER_NAME"
            page.evaluate("""() => {
              const input = document.getElementById('fontFiles');
              Object.defineProperty(input, 'files', {value:[new File(['font'], 'USER_FONT-Regular.ttf')], configurable:true});
              input.dispatchEvent(new Event('change'));
            }""")
            assert page.locator("#pickedInfo").inner_text() == page.evaluate("""uiFormat(
              'Files selected: {count}. Font family: {family}.', {count:1, family:'USER_FONT'})""")
            page.evaluate("document.getElementById('uploadForm').dispatchEvent(new Event('submit', {cancelable:true}))")
            page.wait_for_function("document.getElementById('status').className === 'status-ok'")
            assert page.locator("#status").inner_text() == page.evaluate("""uiFormat(
              'Files uploaded: {count}. Font family: {family}.', {count:1, family:'USER_FONT'})""")
            assert len(saved) == 2 and "USER_FONT-Regular.ttf" in saved[1]
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
            assert not errors, errors
            page.screenshot(path=str(args.output / f"fonts-{code}-{width}.png"), full_page=True)
            page.close()
    browser.close()
print("16 browser pages passed: translated notes/fonts, named/reordered arguments, RTL, escaping and unchanged user data")
