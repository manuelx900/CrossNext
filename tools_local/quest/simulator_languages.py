"""Export the firmware language packs and input/font resources used by Quest QA."""
import importlib.util
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[2]


def export_languages(output):
    spec = importlib.util.spec_from_file_location("quest_language_packs", ROOT / "tools_local/language_packs.py")
    packs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packs)
    reference = packs.reference()
    active = packs.i18n.find_used_string_keys([str(ROOT / "src"), str(ROOT / "lib")])
    destination = output / "languages"
    destination.mkdir(parents=True, exist_ok=True)
    resources = set()
    languages = []
    for path in sorted((ROOT / "lib/I18n/translations").glob("*.yaml")):
        values = packs.i18n.parse_yaml_file(str(path))
        code, setting = values["_bcp47"], values["_language_code"]
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9-]{0,22}", code):
            raise ValueError(f"Invalid locale: {path}")
        pack = None
        if setting not in ("EN", "ES"):
            pack = code + ".cplang"
            strings = {k: v for k, v in values.items() if k in reference}
            blob, warnings = packs.compile_pack(strings, code, values["_language_name"], skip_invalid=code == "ook", included_keys=active)
            if warnings and code != "ook":
                raise ValueError(f"Invalid language pack: {code}")
            (destination / pack).write_bytes(blob)
        region = "_SC" if setting == "ZH_CN" else "_TC" if setting == "ZH_TW" else ""
        files = [f"fonts/Interface{region}_{points}.cpfont" for points in (8, 10, 12)]
        keyboard = f"{setting}.cpkbd"
        if (ROOT / "assets_local/language-keyboards" / keyboard).is_file():
            files.append("keyboards/" + keyboard)
        if setting in ("ZH_CN", "ZH_TW"):
            files.append("keyboards/pinyin-sc.bin" if setting == "ZH_CN" else "keyboards/pinyin-tc.bin")
        resources.update(files)
        languages.append(dict(code=code, setting=setting, name=values["_language_name"],
                              pack=pack, files=files, fictional=code == "ook"))
    if len(languages) != len({item["code"] for item in languages}):
        raise ValueError("Duplicate simulator locale")
    for relative in sorted(resources):
        source_folder = "language-fonts" if relative.startswith("fonts/") else "language-keyboards"
        source = ROOT / "assets_local" / source_folder / Path(relative).name
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    for name in ("LICENSE.txt", "MANIFEST.json"):
        source = ROOT / "assets_local/language-fonts" / name
        if source.is_file():
            shutil.copyfile(source, destination / "fonts" / name)
    for source in sorted((ROOT / "assets_local/language-fonts/licenses").glob("*")):
        if source.is_file():
            target = destination / "fonts/licenses" / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    return languages
