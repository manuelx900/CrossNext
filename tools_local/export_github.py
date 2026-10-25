"""Export a clean source archive using the project's ignore rules, without deleting local work."""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "dist/CrossNext-source.zip"
EXCLUDED = {".git", ".pio", ".pio-cache", ".venv", ".venv-study", ".cache", "qa-artifacts",
            "fs_agent", "dist", "resources", "outputs", "playground-submission", ".cloudflare", ".board", ".wrangler"}
PRIVATE_NAMES = {"wifi.json", "credentials.json", ".env", "settings.local.json"}
PRIVATE_DIRECTORIES = {".agents", ".claude", ".codex", ".cursor", ".windsurf"}
PRIVATE_INSTRUCTIONS = {"AGENTS.md", "CLAUDE.md", "GEMINI.md", ".cursorrules", ".windsurfrules"}
PRIVATE_DOCUMENTS = {
    "docs/github-publication.md", "docs/games-at-scale.md",
    "tools_local/themesweep/review-brief.md", ".github/copilot-instructions.md",
    "docs/apps/instapaper-plan.md",
    "docs/apps/study-installer-plan.md",
    "docs/apps/study-sync-bridge-plan.md",
    "docs/apps/wikipedia-plan.md",
    "docs/apps/xkcd-viewing-plan.md",
    "docs/apps/wavelength-teams.md",
    "tools_local/trivia/us_shift_check.py",
}


def is_private_path(path):
    path = Path(path)
    name = path.as_posix()
    return (bool(PRIVATE_DIRECTORIES.intersection(path.parts))
            or path.name in PRIVATE_INSTRUCTIONS
            or name in PRIVATE_DOCUMENTS
            or name.startswith(("docs/workflow/", "tools_local/trivia/local_panel/")))


def main():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="crossnext-export-") as temporary:
        subprocess.run(["git", "init", "-q", temporary], check=True)
        command = ["git", "--git-dir="+str(Path(temporary)/".git"), "--work-tree="+str(ROOT),
                   "ls-files", "--others", "--exclude-standard", "-z"]
        names = subprocess.check_output(command, cwd=ROOT).decode("utf-8").split("\0")
    expanded = []
    for name in filter(None, names):
        directory = ROOT / name
        if name.endswith("/") and directory.is_dir() and (directory / ".git").exists():
            members = subprocess.check_output(["git", "-C", str(directory), "ls-files", "--cached", "--others",
                                               "--exclude-standard", "-z"]).decode("utf-8").split("\0")
            expanded.extend(name + member for member in filter(None, members))
        else:
            expanded.append(name)
    names = expanded
    files = []
    for name in sorted(filter(None, names)):
        path = Path(name)
        if path.parts[0] in EXCLUDED or any(p in {".git", "__pycache__", "node_modules"} for p in path.parts):
            continue
        if is_private_path(path):
            continue
        if name.startswith("site/emulator/"):
            continue
        # Retired Mandarin demo includes KaiTi fonts without a redistribution grant.
        if name.startswith("tools_local/wasm/sdcard/study/"):
            continue
        if path.name in PRIVATE_NAMES or path.suffix in {".key", ".pem", ".p12", ".download"}:
            continue
        source = ROOT / path
        if source.is_symlink() or not source.resolve().is_relative_to(ROOT.resolve()):
            continue
        if source.is_file():
            files.append((name, source))
    manifest = []
    with zipfile.ZipFile(OUTPUT, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, path in files:
            archive.write(path, "CrossNext/"+name)
            manifest.append(dict(file=name, bytes=path.stat().st_size,
                                 sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    (OUTPUT.parent/"CrossNext-source-manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print(f"{OUTPUT}: {len(files)} files, {OUTPUT.stat().st_size:,} bytes")


if __name__ == "__main__":
    main()
