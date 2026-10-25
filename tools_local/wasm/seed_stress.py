"""Install synthetic stress decks only on the browser simulator's disposable SD."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "assets_local/simulator/stress-corpus"
SD = ROOT / "tools_local/wasm/sdcard"

def seed(sd=SD):
    sd = Path(sd).resolve()
    if sd != SD.resolve():
        raise ValueError("Stress decks are restricted to the browser simulator SD")
    manifest = json.loads((CORPUS / "manifest.json").read_text(encoding="utf-8"))
    for item in manifest["decks"] + manifest["integrity"]:
        source = CORPUS / item["file"]
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != item["sha256"]:
            raise ValueError("Stress corpus checksum mismatch: " + str(source))
        target = sd / "FLASHCARDS/Pruebas_extremas" / item["file"]
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
    # One extra copy at the root tests files alongside folders; both copies
    # intentionally have the same card IDs to exercise separate deck caches.
    source = CORPUS / "Escala/00100_tarjetas.apkg"
    target = sd / "FLASHCARDS/Prueba directa 100 tarjetas.apkg"
    target.write_bytes(source.read_bytes())
    target = sd / "FLASHCARDS/Pruebas_extremas/Submazos/Nivel 2/Nombre de mazo largo con espacios y acentos para comprobar el ajuste de texto.apkg"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(source.read_bytes())
    (sd / "FLASHCARDS-STRESS.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print("Installed 8 stress decks, 4 integrity cases and 2 hierarchy copies; existing decks preserved.")

if __name__ == "__main__":
    seed()
