"""Disposable EPUB card for the desktop shell's real-engine tests."""
from pathlib import Path
import json, sys, zipfile

card = Path(sys.argv[1])
(card / 'books').mkdir(parents=True, exist_ok=True)
(card / '.crosspoint/reading').mkdir(parents=True, exist_ok=True)
(card / '.crosspoint/settings.json').write_text(json.dumps({'epubEngine': 0, 'fontSize': 12, 'readerFontStorageVersion': 1}), encoding='utf-8')
(card / '.crosspoint/reading/stats.json').write_text('{"minutes":123}', encoding='utf-8')
with zipfile.ZipFile(card / 'books/Un capítulo — 日本語.epub', 'w', zipfile.ZIP_DEFLATED) as book:
    book.writestr('mimetype', 'application/epub+zip')
    book.writestr('META-INF/container.xml', '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
    book.writestr('content.opf', '<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">test</dc:identifier><dc:title>Un capítulo</dc:title><dc:creator>CrossNext</dc:creator><dc:language>es</dc:language></metadata><manifest><item id="body" href="body.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="body"/></spine></package>')
    book.writestr('body.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Un capítulo</title></head><body><h1>Un capítulo</h1>' + '<p>El árbol y la lluvia acompañan esta lectura. Una biblioteca preparada conserva los libros y el progreso.</p>' * 1800 + '</body></html>')
