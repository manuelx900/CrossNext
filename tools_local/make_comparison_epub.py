"""Build original, reproducible English-only EPUBs for the release."""
from pathlib import Path
from io import BytesIO
from xml.etree import ElementTree as ET
from html import escape
import argparse
import hashlib
import zipfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/downloads/CrossNext-EPUB-Comparison.epub"
TITLE = "CrossNext · EPUB Engine Comparison"
CSS = """body { margin: 0; } h1 { text-align: center; } p { text-indent: 1.2em; }
.intro { text-indent: 0; } blockquote { margin: 1em 8%; } img { max-width: 100%; height: auto; }
.center { text-align: center; text-indent: 0; } table { border-collapse: collapse; }
td, th { border: 1px solid; padding: .3em; } em { font-style: italic; } strong { font-weight: bold; }
"""


def picture(cover=False, jpeg=False, showcase=False):
    image = Image.new("RGB", (480, 720 if cover else 360), "white")
    draw = ImageDraw.Draw(image)
    if cover:
        draw.rectangle((16, 16, 463, 703), outline="black", width=4)
        draw.text((48, 80), "CROSSNEXT\n\n" + ("READER SHOWCASE" if showcase else "EPUB ENGINE COMPARISON") + "\n\nOriginal English test book", fill="black", font_size=20)
    for n in range(16):
        x = 24 + n * 27
        draw.rectangle((x, 240 if cover else 24, x + 23, 650 if cover else 155), fill=(n * 17,) * 3)
    y = 190 if not cover else 670
    if not cover:
        for n in range(8):
            draw.line((24, y + n * 19, 455, y + n * 19), fill="black", width=n % 4 + 1)
        draw.ellipse((200, 190, 360, 350), outline="black", width=3)
    output = BytesIO()
    image.save(output, "JPEG" if jpeg else "PNG", **({"quality": 95, "subsampling": 0} if jpeg else {}))
    return output.getvalue()


def chapter(title, body):
    return f'''<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en"><head><title>{escape(title)}</title>
<link rel="stylesheet" type="text/css" href="style.css"/></head><body><h1>{escape(title)}</h1>{body}</body></html>'''


def make(destination=DEFAULT, showcase=False):
    book_title = "CrossNext 2.0 · Reader Showcase" if showcase else TITLE
    uid = "urn:crossnext:reader-showcase:2" if showcase else "urn:crossnext:epub-comparison:2"
    instructions = """<p class="intro">Explore the CrossNext 2.0 EPUB reader with this original English-only book. No font is embedded.</p>
<p>Choose Settings &gt; Reading &gt; EPUB engine: New or Classic (1.4), then close and reopen this book. Keep the same font, size, line spacing, indentation, margins, alignment, orientation and book-style setting for both engines.</p>
<p>The new engine reuses parsed chapter events, exact word measurements and prepared layout variants when valid. The desktop Book Preparer can do pagination, image processing and cover preparation before the book reaches the reader. A new layout can still need preparation, and the e-ink panel retains its physical refresh time.</p>
<p>Compare the same numbered paragraph rather than a page number. Time first opening separately from later openings. On the long chapter, change font, size, indentation and line spacing. Inspect the PNG and JPEG chapters for fine lines, gray levels and unchanged proportions.</p>
<p>Use another copy under a different filename for a cold test. Never delete the whole .crosspoint folder: it also contains your saved progress and app data. This book demonstrates content to compare; it does not certify a speed advantage or identical support for every EPUB style.</p>"""
    english = "At the river library, Ada opened a notebook and watched the light move across the shelves. Every page held a small discovery: a map, a remembered journey, or a question for tomorrow. She read at her own pace, returning to the sentences that deserved a second look."
    chapters = [
        ("1 · Meet the reader", instructions),
        ("2 · A long chapter", "".join(f'<p><strong>[{n:03d}]</strong> {english}</p>' for n in range(1, 241))),
        ("3 · Typography and spacing", '<p>Regular text, <strong>bold text</strong>, <em>italic text</em>, <strong><em>bold italic text</em></strong>.</p><p>Try the Font and Layout tabs: font family, size, weight, indentation, line spacing, paragraph spacing and alignment. Compare the same marked passage after each change.</p><blockquote><p>A quiet room and a good book leave space for attention.</p></blockquote><h2>A second heading</h2><p>Punctuation: ( ) [ ] — … “ ” ‘ ’. Reading rewards patience, curiosity and careful attention.</p>'),
        ("4 · PNG detail", '<p class="intro">Sixteen gray steps, thin lines and a circle. Inspect the same details and proportions with each engine.</p><p class="center"><img src="patterns.png" alt="Sixteen gray steps, lines and a circle"/></p><p>Display retains grayscale; the optional Black and white image mode uses a monochrome dot pattern. Keep the same mode for a fair engine comparison.</p>'),
        ("5 · JPEG detail", '<p class="intro">This JPEG is saved at quality 95 without chroma subsampling. Both engines read this identical image file.</p><p class="center"><img src="patterns.jpg" alt="JPEG gray steps and line pattern"/></p><p>Compare fine lines, image aspect ratio and gray levels before changing any image settings.</p>'),
        ("6 · Mixed content and navigation", '<p>Before the image. ' + english + '</p><img src="patterns.png" alt="Gray pattern"/><p>After the image. ' + english + '</p><ul><li>Open Contents to jump between chapters.</li><li>Use More for search, bookmarks, quotes and location.</li><li>Change the opening gesture in Settings, Controls, Open book settings.</li></ul><p><a href="ch7.xhtml#notes">Jump to the notes in the last chapter</a>.</p>'),
        ("7 · Your comparison notes", '<h2 id="notes">Record what you observe</h2><p>Write down the engine, font, size, layout, first-open time, repeat-open time and setting-change time. Check visible text and image quality alongside timing. Prepared pages only cover the engine and layout combinations you chose.</p><p>Changing fonts or styles does not erase reading progress. This original book complements testing with your own EPUBs; it is not a benchmark result.</p><p>Text and generated pictures are dedicated to the public domain under CC0 1.0: https://creativecommons.org/publicdomain/zero/1.0/ . CrossNext names identify the software being tested.</p>'),
    ]
    if showcase:
        chapters[1] = ("2 · The river library", "".join(f'<p><strong>[{n:03d}]</strong> {english}</p>' for n in range(1, 61)))
    files = {"style.css": CSS.encode(), "cover.png": picture(True, showcase=showcase), "patterns.png": picture(), "patterns.jpg": picture(jpeg=True)}
    for index, (title, body) in enumerate(chapters, 1):
        files[f"ch{index}.xhtml"] = chapter(title, body).encode()
    nav = "".join(f'<navPoint id="n{i}" playOrder="{i}"><navLabel><text>{escape(title)}</text></navLabel><content src="ch{i}.xhtml"/></navPoint>' for i, (title, _) in enumerate(chapters, 1))
    files["toc.ncx"] = f'''<?xml version="1.0" encoding="utf-8"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head><meta name="dtb:uid" content="{uid}"/><meta name="dtb:depth" content="1"/><meta name="dtb:totalPageCount" content="0"/><meta name="dtb:maxPageNumber" content="0"/></head><docTitle><text>{escape(book_title)}</text></docTitle><navMap>{nav}</navMap></ncx>'''.encode()
    manifest = ''.join(f'<item id="c{i}" href="ch{i}.xhtml" media-type="application/xhtml+xml"/>' for i in range(1, 8))
    spine = ''.join(f'<itemref idref="c{i}"/>' for i in range(1, 8))
    files["content.opf"] = f'''<?xml version="1.0" encoding="utf-8"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>{escape(book_title)}</dc:title><dc:creator>CrossNext contributors</dc:creator><dc:language>en</dc:language><dc:identifier id="uid">{uid}</dc:identifier><dc:rights>CC0 1.0</dc:rights><meta name="cover" content="cover"/></metadata><manifest>{manifest}<item id="css" href="style.css" media-type="text/css"/><item id="cover" href="cover.png" media-type="image/png"/><item id="png" href="patterns.png" media-type="image/png"/><item id="jpg" href="patterns.jpg" media-type="image/jpeg"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/></manifest><spine toc="ncx">{spine}</spine></package>'''.encode()
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w") as archive:
        def put(name, value, compression=zipfile.ZIP_DEFLATED):
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = compression
            archive.writestr(info, value)
        put("mimetype", b"application/epub+zip", zipfile.ZIP_STORED)
        put("META-INF/container.xml", b'<?xml version="1.0"?><container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        for name, content in files.items():
            put("OEBPS/" + name, content)
    validate(destination, book_title, uid)
    print(f"{destination}: {destination.stat().st_size} bytes; SHA256 {hashlib.sha256(destination.read_bytes()).hexdigest()}")


def validate(path, expected_title=None, expected_uid=None):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert archive.namelist()[0] == "mimetype"
        assert archive.getinfo("mimetype").compress_type == zipfile.ZIP_STORED
        assert archive.read("mimetype") == b"application/epub+zip"
        for name in archive.namelist():
            if name.endswith((".xml", ".opf", ".ncx", ".xhtml")):
                ET.fromstring(archive.read(name))
        opf = ET.fromstring(archive.read("OEBPS/content.opf"))
        metadata = {"dc": "http://purl.org/dc/elements/1.1/"}
        ncx = ET.fromstring(archive.read("OEBPS/toc.ncx"))
        if expected_title:
            assert opf.findtext('.//dc:title', namespaces=metadata) == expected_title
            assert ncx.findtext('{http://www.daisy.org/z3986/2005/ncx/}docTitle/{http://www.daisy.org/z3986/2005/ncx/}text') == expected_title
        if expected_uid:
            assert opf.findtext('.//dc:identifier', namespaces=metadata) == expected_uid
        assert [x.text for x in opf.findall('.//dc:language', metadata)] == ['en']
        ns = {"o": "http://www.idpf.org/2007/opf"}
        items = {e.attrib["id"]: e.attrib["href"] for e in opf.findall("o:manifest/o:item", ns)}
        assert all("OEBPS/" + href in archive.namelist() for href in items.values())
        assert all(e.attrib["idref"] in items for e in opf.findall("o:spine/o:itemref", ns))
        for name in ("cover.png", "patterns.png", "patterns.jpg"):
            Image.open(BytesIO(archive.read("OEBPS/" + name))).verify()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, nargs="?", default=DEFAULT)
    parser.add_argument("--showcase", action="store_true")
    args = parser.parse_args()
    make(args.output, args.showcase)
