"""Give release copies CrossNext names without changing PlatformIO/OTA filenames."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_VERSION = "1.1.2"
OTA_ASSET = "CrossNext-X4PRO-OTA.bin"


def release_name(device):
    return f"CrossNext-X4PRO-Firmware-v{FIRMWARE_VERSION}" if device == "X4Pro" else f"CrossNext-{device}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", choices=("x4pro", "sticky", "all"), default="x4pro")
    selected = parser.parse_args().device
    targets = [("gh_release_x4pro", "X4Pro")] if selected == "x4pro" else [("gh_release_sticky", "Sticky")]
    if selected == "all": targets = [("gh_release_x4pro", "X4Pro"), ("gh_release_sticky", "Sticky")]
    output = ROOT / "dist/CrossNext"
    output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for environment, device in targets:
        candidates = [ROOT / parent / name for name in (environment, "x4pro" if device == "X4Pro" else "sticky")
                      for parent in ("dist/firmware", ".pio/quest-sd-build", ".pio/build")]
        candidates = [p for p in candidates if (p / "firmware.bin").is_file() and (p / "firmware.factory.bin").is_file()]
        if not candidates:
            raise FileNotFoundError(f"No complete application/factory images for {device}; build this target through scripts_local/check.sh first")
        source = max(candidates, key=lambda p: (p / "firmware.bin").stat().st_mtime_ns)
        for original, suffix in (("firmware.bin", ""), ("firmware.factory.bin", "-factory")):
            path = source / original
            if not path.is_file():
                raise FileNotFoundError(path)
            target = output / f"{release_name(device)}{suffix}.bin"
            shutil.copy2(path, target)
            manifest.append(dict(file=target.name, bytes=target.stat().st_size,
                                 sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
        if device == "X4Pro":
            ota = output / OTA_ASSET
            shutil.copy2(source / "firmware.bin", ota)
            manifest.append(dict(file=ota.name, bytes=ota.stat().st_size,
                                 sha256=hashlib.sha256(ota.read_bytes()).hexdigest()))
    (output/"SHA256SUMS.txt").write_text("".join(f"{item['sha256']}  {item['file']}\n" for item in manifest), encoding="utf-8")
    (output/"manifest.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")
    (output/"README.txt").write_text("CrossNext firmware\n\nChoose the device that matches your reader.\nThe -factory images contain the bootloader and partition table (flash at 0x0).\nThe other .bin files contain the application (OTA or flash at 0x10000).\nDo not use another model's image.\n", encoding="utf-8")
    for _, device in targets:
        entries = [item for item in manifest if item["file"].startswith(release_name(device)) or (device == "X4Pro" and item["file"] == OTA_ASSET)]
        notes = (
            f"{release_name(device)}\n\n"
            "EN: Back up your microSD. Use only this device model's images.\n"
            "The application .bin is for a compatible update/OTA or offset 0x10000.\n"
            "The -factory.bin contains bootloader and partitions; flash at 0x0.\n"
            "Verify SHA256SUMS.txt. Device testing is required before release.\n"
            "Online updates use manuelx900/CrossNext GitHub Releases.\n"
            "Upload CrossNext-X4PRO-OTA.bin as a raw release asset for device updates.\n\n"
            "ES: Guarda una copia de tu microSD. Usa solo las imagenes de este modelo.\n"
            "El .bin de aplicacion sirve para actualizacion/OTA compatible o 0x10000.\n"
            "El -factory.bin incluye arranque y particiones; se instala en 0x0.\n"
            "Comprueba SHA256SUMS.txt y prueba el dispositivo antes de publicar.\n"
            "La actualizacion en linea usa GitHub Releases de manuelx900/CrossNext.\n"
            "Adjunta CrossNext-X4PRO-OTA.bin directamente al Release para habilitar OTA.\n\n"
            "Resources and source code are distributed in separate ZIP files.\n"
            "See README.es.md / docs/install.md in the source archive for details.\n"
        )
        archive_path = ROOT / "dist" / f"{release_name(device)}.zip"
        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for item in entries:
                archive.write(output / item["file"], item["file"])
            archive.writestr("README.txt", notes)
            archive.writestr("SHA256SUMS.txt", "".join(f"{item['sha256']}  {item['file']}\n" for item in entries))
            archive.writestr("manifest.json", json.dumps(entries, indent=2) + "\n")
            archive.write(ROOT / "LICENSE", "LICENSE")
            archive.write(ROOT / "THIRD-PARTY-NOTICES.md", "THIRD-PARTY-NOTICES.md")
            archive.write(ROOT / "third_party/stb/LICENSE", "licenses/stb_image.txt")
            # Keep full notices beside binary distributions, including embedded fonts/icons.
            for directory in ("freeink-sdk", "lib", "src/apps_local", "assets_local", "third_party", "licenses"):
                for notice in sorted((ROOT / directory).rglob("*")):
                    if not notice.is_file() or {".git", "node_modules", "__pycache__"} & set(notice.parts):
                        continue
                    name = notice.name.lower()
                    if name.startswith(("license", "licence", "copying", "copyright", "ofl", "notice")) or name in {"upstream-readme.md", "readme_en_us.txt"}:
                        archive.write(notice, "licenses/" + notice.relative_to(ROOT).as_posix())
        with zipfile.ZipFile(archive_path) as archive:
            if archive.testzip() is not None:
                raise ValueError(f"Corrupt archive: {archive_path}")
            for item in entries:
                if hashlib.sha256(archive.read(item["file"])).hexdigest() != item["sha256"]:
                    raise ValueError(f"Image differs in archive: {item['file']}")
        print(archive_path)
    print(output)


if __name__ == "__main__":
    main()
