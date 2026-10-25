"""Install the downloaded learning corpus into the browser's disposable SD.

Sources and checksums live beside the archives in assets_local/simulator/learning-corpus.
No downloads or device SD writes occur during a normal simulator build.
"""
import hashlib
import json
from pathlib import Path
import tarfile
import zipfile


ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "assets_local/simulator/learning-corpus"
SD = ROOT / "tools_local/wasm/sdcard"


def write_if_changed(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def seed(sd):
    sd = Path(sd).resolve()
    if sd != SD.resolve():
        raise ValueError("The learning corpus is only for the browser simulator SD")
    manifest = json.loads((CORPUS / "manifest.json").read_text(encoding="utf-8"))
    for item in manifest["decks"] + manifest["dictionaries"]:
        source = CORPUS / item["archive"]
        if hashlib.sha256(source.read_bytes()).hexdigest() != item["sha256"]:
            raise ValueError("Learning corpus checksum mismatch: " + str(source))
    for item in manifest["decks"]:
        target = sd / "FLASHCARDS" / item["path"]
        source = CORPUS / item.get("prepared", item["archive"])
        if "prepared_sha256" in item and hashlib.sha256(source.read_bytes()).hexdigest() != item["prepared_sha256"]:
            raise ValueError("Prepared image deck checksum mismatch: " + str(source))
        write_if_changed(target, source.read_bytes())
    for item in manifest["dictionaries"]:
        folder = sd / "dictionaries" / item["folder"]
        source = CORPUS / item["archive"]
        if source.suffix == ".zip":
            with zipfile.ZipFile(source) as archive:
                members = [(Path(n).name, archive.read(n)) for n in archive.namelist()
                           if not n.endswith("/")]
        else:
            import zstandard
            with zstandard.ZstdDecompressor().stream_reader(source.open("rb")) as stream:
                with tarfile.open(fileobj=stream, mode="r|") as archive:
                    members = [(Path(m.name).name, archive.extractfile(m).read())
                               for m in archive if m.isfile() and "/res/" not in m.name]
        for name, data in members:
            if name.endswith((".ifo", ".idx", ".syn", ".dict", ".dict.dz", ".css")):
                write_if_changed(folder / name, data)
        ifo = next(folder.glob("*.ifo")).read_text(encoding="utf-8")
        item["entries"] = int(next(line.split("=", 1)[1] for line in ifo.splitlines()
                                   if line.startswith("wordcount=")))
    settings_path = sd / ".crosspoint/settings.json"
    settings = json.loads(settings_path.read_text(encoding="utf-8"))
    settings.update(dictionaryName="", translationDictionaryName="", dictionaryAuto=True, translationDictionaryAuto=True)
    write_if_changed(settings_path, json.dumps(settings, ensure_ascii=False).encode())
    write_if_changed(sd / "LEARNING-CORPUS.json",
                     json.dumps(manifest, ensure_ascii=False, indent=2).encode())
    print(f"Real learning corpus: {len(manifest["decks"])} downloaded decks; " + ", ".join(
        f"{item['folder']}: {item['entries']} entries" for item in manifest["dictionaries"]))


if __name__ == "__main__":
    seed(SD)
