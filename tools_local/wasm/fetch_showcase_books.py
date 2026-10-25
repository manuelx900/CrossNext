"""Explicit, one-time download of public-domain books for the browser demo."""
from pathlib import Path
import hashlib
import json
import urllib.request
import os
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "assets_local/simulator/showcase-books"
BOOKS = [
    (84, "Frankenstein", "Mary Shelley"),
    (1661, "The Adventures of Sherlock Holmes", "Arthur Conan Doyle"),
    (2701, "Moby Dick", "Herman Melville"),
    (1342, "Pride and Prejudice", "Jane Austen"),
    (345, "Dracula", "Bram Stoker"),
    (35, "The Time Machine", "H. G. Wells"),
    (2000, "Don Quijote", "Miguel de Cervantes"),
]


def main():
    DEST.mkdir(parents=True, exist_ok=True)
    items = []
    for number, title, author in BOOKS:
        path = DEST / f"{title}.epub"
        edition = "noimages" if number == 1342 else "images"
        url = f"https://www.gutenberg.org/ebooks/{number}.epub.{edition}"
        if not path.exists():
            temporary = path.with_suffix(".download")
            if os.name == "nt":
                subprocess.run(["curl.exe", "--fail", "--location", "--max-time", "120", url,
                                "--output", str(temporary)], check=True)
            else:
                request = urllib.request.Request(url, headers={"User-Agent": "CrossNext-demo/1.0"})
                with urllib.request.urlopen(request, timeout=90) as response:
                    temporary.write_bytes(response.read())
            with zipfile.ZipFile(temporary) as archive:
                assert archive.read("mimetype") == b"application/epub+zip"
                assert archive.testzip() is None
            temporary.replace(path)
        items.append(dict(file=path.name, title=title, author=author,
                          source=f"https://www.gutenberg.org/ebooks/{number}", download=url,
                          rights="Public domain in the USA; Project Gutenberg notices retained inside EPUB",
                          sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        print(title, path.stat().st_size, flush=True)
    (DEST / "manifest.json").write_text(json.dumps(items, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
