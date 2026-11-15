"""Export the complete campaign from the same source used by the app."""

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
from scene_content import MILESTONES, load as load_paths
ROMAN = ("I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII")


def export_story(brief, language="es"):
    title = "Aventura Lectora" if language == "es" else "Reading Adventure"
    prologue = "Prólogo" if language == "es" else "Prologue"
    paragraphs = []
    result = [f"# {title}", "## Saga I · " + brief["sagas"][0]["title"][language], f"### {prologue}"]
    for number, section in enumerate(brief["prologue"], 1):
        result.append(f"#### {number}. {section['title'][language]}")
        text = section["text"][language]
        result.append(text)
        paragraphs.append(text)
    for n, (saga, roman) in enumerate(zip(brief["sagas"], ROMAN)):
        if n:
            result.append(f"## Saga {roman} · {saga['title'][language]}")
        result.append("### Relato de Saga" if language == "es" else "### Saga account")
        for milestone, version in zip(MILESTONES, saga["versions"]):
            label = "caminos completados" if language == "es" else "paths completed"
            result.extend([f"#### {milestone} {label}", version[language]])
            paragraphs.append(version[language])
        result.append("### Camino" if language == "es" else "### Path")
        for number, path in enumerate(brief["paths"][n * 10 : (n + 1) * 10], 1):
            result.extend([f"#### {number}. {path['title'][language]}", path["approach"][language]])
            paragraphs.append(path["approach"][language])
    result.extend(["## Epílogo" if language == "es" else "## Epilogue", brief["epilogue"][language]])
    paragraphs.append(brief["epilogue"][language])
    return "\n\n".join(result) + "\n", sum(len(text.split()) for text in paragraphs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--language", choices=("es", "en"), default="es")
    parser.add_argument("--output", type=Path, default=ROOT / "outputs/aventura-lectora-historia-progresiva.md")
    args = parser.parse_args()
    document, words = export_story(load_paths(ROOT), args.language)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(document, encoding="utf-8")
    print(f"Exported 12 sagas and 120 path previews to {args.output}; {words} words across all variants.")


if __name__ == "__main__":
    main()
