"""Exercise the real QA controls with a captured emulator boot, using Playwright."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import threading
import time
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
CATALOG = json.loads((SITE / "qa-reading-adventure/catalog.json").read_text("utf8"))
assert CATALOG["version"] == 6

class QuietHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if getattr(self.server, "delay_fonts", False) and "/languages/fonts/" in self.path:
            time.sleep(2)
        try:
            super().do_GET()
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *_):
        pass

server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(SITE)))
threading.Thread(target=server.serve_forever, daemon=True).start()
url = f"http://127.0.0.1:{server.server_port}/qa-reading-adventure/"
BRIDGE = """window.Crossplay={async boot(mount,options){
 window.qaBoot={settings:JSON.parse(options.files['/fs_/.crosspoint/settings.json']),
 files:Object.fromEntries(Object.entries(options.files).map(([path,data])=>[path,data.length])),env:options.env};
 options.onFirstFrame();return {canvas:{focus(){}},scancodes:{up:1,down:2},bindButton(){},swipeBack(){}};
}};"""
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--channel", help="Use an installed Playwright browser channel, e.g. chrome")
parser.add_argument("--browser", help="Explicit Chromium executable path")
args = parser.parse_args()
try:
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(channel=args.channel, executable_path=args.browser)
        page = browser.new_page()
        page.route("**/assets/emulator.js", lambda route: route.fulfill(content_type="text/javascript", body=BRIDGE))
        for item in CATALOG["languages"]:
            page.goto(url + "?stage=custom&encounter=65&moment=2&panel=encounter&language=" + item["code"])
            page.wait_for_function("window.qaBoot !== undefined")
            boot = page.evaluate("window.qaBoot")
            assert boot["settings"]["language"] == item["setting"]
            assert boot["settings"]["languagePack"] == (item["pack"] or "")
            expected = set(item["files"] + ([item["pack"]] if item["pack"] else []))
            actual = {path.removeprefix("/fs_/languages/") for path in boot["files"] if path.startswith("/fs_/languages/")}
            assert actual == expected, (item["code"], actual, expected)
            for path in actual:
                assert boot["files"]["/fs_/languages/" + path] == (SITE / "qa-reading-adventure/languages" / path).stat().st_size
            assert page.locator("#language option").count() == 39
        page.goto(url + "?stage=custom&encounter=65&moment=2&language=zh-CN")
        page.wait_for_function("window.qaBoot !== undefined")
        page.select_option("#language", "ja")
        page.wait_for_url("**language=ja**")
        page.wait_for_function("window.qaBoot !== undefined")
        assert "encounter=65" in page.url and "moment=2" in page.url
        page.select_option("#stage", "collection")
        page.wait_for_url("**stage=collection**")
        page.wait_for_function("window.qaBoot !== undefined")
        assert "language=ja" in page.url
        assert page.evaluate("window.qaBoot.settings.language") == "JA"
        # A locale switch during slow downloads must preserve the current encounter.
        server.delay_fonts = True
        page.goto(url + "?stage=custom&encounter=65&moment=2&language=es", wait_until="domcontentloaded")
        page.wait_for_function("!document.querySelector('#language').disabled")
        page.select_option("#language", "ja")
        page.wait_for_url("**language=ja**")
        assert "encounter=65" in page.url and "moment=2" in page.url
        assert "background=&" not in page.url and "level=" not in page.url
        server.delay_fonts = False
        page.goto(url + "?stage=custom&encounter=65&moment=2&language=ja")
        page.wait_for_function("window.qaBoot !== undefined")
        page.goto(url + "?language=not-a-locale&stage=new")
        page.wait_for_function("window.qaBoot !== undefined")
        assert page.locator("#language").input_value() == "es"
        assert page.evaluate("window.qaBoot.settings.languagePack") == ""
        page.route("**/languages/ar.cplang", lambda route: route.fulfill(status=404, body="missing"))
        page.goto(url + "?language=ar&stage=new")
        page.wait_for_function("document.querySelector('#status').textContent.includes('ar.cplang')")
        assert not page.evaluate("Boolean(window.qaBoot)")
        browser.close()
finally:
    server.shutdown()
    server.server_close()
print("QA language selector passed: all 39 choices, real resources, URL persistence, invalid locale and missing-resource handling.")
