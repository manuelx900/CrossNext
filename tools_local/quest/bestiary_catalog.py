"""Validate stable bestiary identities before generating any runtime artefact."""
import json


def validate(entries):
    if len(entries) != 56 or [entry['id'] for entry in entries] != list(range(306, 362)):
        raise ValueError('Bestiary IDs must remain in their original 306..361 order')
    for entry in entries:
        for key, low, high in (('minutes', 1, 1800), ('paths', 1, 119), ('chapter', 0, 119)):
            if type(entry[key]) is not int or not low <= entry[key] <= high:
                raise ValueError(f"Invalid {key} for bestiary hunt {entry['id']}")
        if entry['rank'] not in ('Elite', 'Legendary') or entry['row'] not in range(4) or entry['column'] not in (0, 3):
            raise ValueError(f"Invalid rank or sprite cell for bestiary hunt {entry['id']}")
        for key in ('name', 'outfit'):
            if not all(isinstance(entry[key].get(lang), str) and entry[key][lang].strip() for lang in ('en', 'es')):
                raise ValueError(f"Missing {key} translation for bestiary hunt {entry['id']}")
    return entries


def load(directory):
    return validate(json.loads((directory / 'catalog.json').read_text('utf8'))['monsters'])
