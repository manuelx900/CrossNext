"""Generate the bounded hunt catalogue and its English/Spanish text tables."""
import json
from pathlib import Path
import yaml
from bestiary_catalog import load

ROOT = Path(__file__).resolve().parents[2]


def build(update_translations=True):
    entries = load(ROOT / 'assets_local/quest/bestiary')
    header = '#pragma once\n#include "QuestCore.h"\nnamespace quest {\n'
    header += 'struct BestiarySpec { uint16_t minutes; uint8_t paths, chapter; HuntRank rank; };\n'
    header += 'inline constexpr BestiarySpec BESTIARY[] = {\n'
    for entry in entries:
        header += f"    {{{entry['minutes']}, {entry['paths']}, {entry['chapter']}, HuntRank::{entry['rank']}}},\n"
    header += '};\nstatic_assert(sizeof(BESTIARY) / sizeof(BESTIARY[0]) == kBestiaryCount);\n}\n'
    (ROOT / 'src/apps_local/quest/QuestBestiaryData.h').write_text(header, 'utf8')
    text = '#pragma once\n#include <I18n.h>\nnamespace questbestiary {\n'
    for kind in ('NAME', 'OUTFIT'):
        text += f'inline constexpr StrId {kind}[] = {{\n'
        text += ''.join(f'    StrId::STR_QUEST_BESTIARY_{kind}_{index:02d},\n' for index in range(len(entries)))
        text += '};\n'
    text += '}\n'
    (ROOT / 'src/apps_local/quest/QuestBestiaryText.h').write_text(text, 'utf8')
    if not update_translations:
        return
    for language, code in (('english', 'en'), ('spanish', 'es')):
        path = ROOT / f'lib/I18n/translations/{language}.yaml'
        source = path.read_text('utf8')
        values = yaml.safe_load(source)
        for index, entry in enumerate(entries):
            for kind in ('NAME', 'OUTFIT'):
                key = f'STR_QUEST_BESTIARY_{kind}_{index:02d}'
                value = entry[kind.lower()][code]
                line = key + ': ' + json.dumps(value, ensure_ascii=False)
                if key in values:
                    import re
                    source = re.sub(r'^' + key + r':.*$', lambda _: line, source, flags=re.M)
                else:
                    source = source.rstrip() + '\n' + line + '\n'
        path.write_text(source, 'utf8')


if __name__ == '__main__':
    build()
