#!/usr/bin/env python3
"""Validate every catalogue and report context-sensitive translation candidates.

This is a structural and regression check, not a native-language certification.
The report separates corrected, source-referenced cases from heuristic candidates.
Only catalogue/format errors and reintroduced known mistranslations fail the run.
"""
import argparse
from collections import Counter
import functools
import importlib.util
import json
from pathlib import Path
import re
import unicodedata

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('language_packs', ROOT / 'tools_local/language_packs.py')
packs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packs)

# These terms are review signals only. Some have valid meanings in narratives.
RISK = {
    'arabic': r'سيارة|استثمار|إنقاذ|توفير', 'azerbaijani': r'qənaət|xilas|adaçayı',
    'belarusian': r'эканом|вырат', 'bosnian': r'ušted|spasit|kadulj|žalfij',
    'bulgarian': r'спест|спас', 'chinese-simplified': r'汽车|拯救|节省|储蓄',
    'chinese-traditional': r'汽車|拯救|節省|儲蓄', 'dutch': r'bespar|gered|\bsparen\b',
    'finnish': r'pelast|sääst', 'french': r'économi|épargn',
    'german': r'erspar|investier|gerettet', 'hebrew': r'חיסכון|חסכון|חוסך',
    'hungarian': r'megtakar|menthető meg', 'japanese': r'節約|貯蓄',
    'indonesia': r'menghemat|tabungan|diselamat', 'italian': r'risparmi|investi',
    'lithuanian': r'taup|išgelb', 'norwegian': r'bespar|reddet|redde|\bsparing\b|\bsparer\b',
    'persian': r'نجات|سرمایه', 'polish': r'oszcz[eę]d|uratowa',
    'portuguese-BR': r'automóvel|poupan|economiz', 'portuguese-PT': r'automóvel|poupan|economiz',
    'romanian': r'economis', 'russian': r'эконом|спас', 'slovak': r'ušetr|zachráni|úspor',
    'slovenian': r'prihran|rešene|varčevan', 'swedish': r'bespar|rädd|sparande',
    'turkish': r'kurtar|tasarruf|adaçayı', 'ukrainian': r'заощад|врят',
    'vietnamese': r'tiết kiệm|được cứu',
}
CRITIQUE = {
    'belarusian': r'агляд', 'bosnian': r'recenzij', 'bulgarian': r'отзив',
    'chinese-simplified': r'评论', 'chinese-traditional': r'評論', 'czech': r'recenz',
    'danish': r'anmeld', 'dutch': r'recensi', 'finnish': r'arvostel',
    'french': r'\bavis\b', 'german': r'bewertung', 'hebrew': r'ביקורות',
    'indonesia': r'ulasan', 'italian': r'recension', 'japanese': r'レビュー',
    'kazakh': r'пікір', 'lithuanian': r'apžvalg', 'norwegian': r'anmeld',
    'polish': r'recenz', 'russian': r'реценз', 'slovak': r'recenz',
    'swedish': r'recension', 'thai': r'บทวิจารณ์', 'turkish': r'yorum', 'ukrainian': r'реценз',
}
RETAINED_SIGNALS = {
    ('german', 'STR_WORDS_SPACING_HELP'):
        'Bewertung names the learner’s grade; Wiederholung names the next learning review. Both are intentional.',
}


def source_references():
    result = {}
    for base in ('src', 'lib'):
        for path in sorted((ROOT / base).rglob('*')):
            if path.suffix not in ('.cpp', '.h', '.c') or path.name in packs.i18n._GENERATED_FILENAMES:
                continue
            for line_number, line in enumerate(path.read_text('utf-8', errors='replace').splitlines(), 1):
                for key in set(re.findall(r'\bSTR_\w+\b', line)):
                    result.setdefault(key, []).append(f'{path.relative_to(ROOT).as_posix()}:{line_number}')
    return result


def candidates(language, values, reference, refs):
    found = []
    for key, text in values.items():
        source = reference[key]
        # Story rescue and financial narrative are not storage operations.
        storage = (re.search(r'\bsav(?:e[sd]?|ing)\b', source, re.I) and
                   not re.search(r'_(?:TEXT|CHOICE)_|^STR_QUEST_CAM_', key))
        sage = key.startswith('STR_QUEST_') and re.search(r'\bsage\b', source, re.I)
        auto = source.lower() in ('auto', 'automatic', 'invert', 'inverted')
        if (storage or sage or auto) and language in RISK and re.search(RISK[language], text, re.I):
            found.append(dict(key=key, reason='storage / automatic / wise-person homonym',
                              english=source, translation=text, references=refs.get(key, [])))
        study = (key.startswith(('STR_WORDS_', 'STR_FLASH_', 'STR_UI_')) and
                 re.search(r'\breview(?:s|ing)?\b', source, re.I))
        if study and language in CRITIQUE and re.search(CRITIQUE[language], text, re.I):
            found.append(dict(key=key, reason='study review versus published critique',
                              english=source, translation=text, references=refs.get(key, [])))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--packs', type=Path, help='Also export active external .cplang files only; fonts/keyboards stay unchanged')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.packs:
        args.packs.mkdir(parents=True, exist_ok=True)
    # One immutable English snapshot for the entire audit/pack build.
    packs.reference = functools.lru_cache(maxsize=1)(packs.reference)
    reference = packs.reference()
    refs = source_references()
    ledger = json.loads(Path(__file__).with_name('context-regressions.json').read_text('utf-8'))
    extended = json.loads(Path(__file__).with_name('complete-regressions.json').read_text('utf-8'))
    ledger.extend(extended)
    blocked = {}
    for entry in ledger:
        blocked.setdefault((entry['language'], entry['key']), set()).add(entry['before'])
    errors, languages, review, retained = [], [], [], []
    study_source = (ROOT / 'src/apps_local/study/StudyScreens.cpp').read_text('utf-8')
    study_header = (ROOT / 'src/apps_local/study/StudyScreens.h').read_text('utf-8')
    study_activity = (ROOT / 'src/apps_local/study/StudyActivity.cpp').read_text('utf-8')
    record_capacity = int(re.search(r'\bchar\s+record\s*\[\s*(\d+)\s*\]', study_source)[1])
    fact_capacity = int(re.search(r'\bfactLines\s*\[\s*\d+\s*\]\s*\[\s*(\d+)\s*\]', study_header)[1])
    warning_capacity = int(re.search(r'\bchar\s+warning\s*\[\s*(\d+)\s*\]', study_source)[1])
    waiting_capacity = int(re.search(r'\bchar\s+waiting\s*\[\s*(\d+)\s*\]', study_activity)[1])
    body_capacity = int(re.search(r'\bchar\s+body\s*\[\s*(\d+)\s*\]', study_header)[1])
    for path in sorted((ROOT / 'lib/I18n/translations').glob('*.yaml')):
        parsed = packs.i18n.parse_yaml_file(str(path))
        language = path.stem
        code = parsed['_bcp47']
        values = {k: v for k, v in parsed.items() if k.startswith('STR_')}
        counts = Counter(re.findall(r'^(STR_\w+)\s*:', path.read_text('utf-8'), re.M))
        errors.extend(f'{language}: duplicate key {k}' for k, count in counts.items() if count != 1)
        if code != 'ook' and set(values) != set(reference):
            errors.append(f'{language}: source key set differs from English')
        for key, text in values.items():
            if key not in reference:
                errors.append(f'{language}: unknown {key}')
                continue
            if not unicodedata.is_normalized('NFC', text):
                errors.append(f'{language}: non-NFC {key}')
            if not text and reference[key] and code != 'ook':
                errors.append(f'{language}: empty {key}')
            if text in blocked.get((language, key), set()):
                errors.append(f'{language}: known contextual error reintroduced in {key}')
        # Actual snprintf destinations in StudyScreens.cpp and StudyScreens.h.
        # Exercise the full nonnegative int counter range and both suffixes.
        count_formats = {
            'STR_UI_STREAK_D_D_RECALL_D_REVIEW_96E50A': (record_capacity, (2147483647, 100, 2147483647)),
            'STR_UI_D_REVIEW_S_NONE_THIS_FORTNIGHT_BBD3F6': (record_capacity, (2147483647,)),
            'STR_UI_D_REVIEW_S_SENT_0360CA': (fact_capacity, (2147483647,)),
            'STR_UI_D_DECK_S_REBUILT_WITH_YOUR_E08002': (fact_capacity, (2147483647,)),
            'STR_UI_D_DECK_S_UPDATED_7E2BCA': (fact_capacity, (2147483647,)),
            'STR_UI_D_DECK_S_HERE_CANNOT_BE_725445': (warning_capacity, (2147483647,)),
            'STR_UI_D_WAITING_IN_YOUR_OTHER_DECK_166F23': (64, (2147483647,)),
        }
        for key, (capacity, numbers) in count_formats.items():
            if key not in values:
                continue
            variants = []
            for suffix in ('', 'S'):
                try:
                    rendered = values[key] % (*numbers, suffix)
                    variants.append(rendered)
                    if len(rendered.encode('utf-8')) >= capacity:
                        errors.append(f'{language}: {key} exceeds its {capacity}-byte UTF-8 buffer')
                except (ValueError, TypeError) as error:
                    errors.append(f'{language}: {key}: {error}')
            if code not in ('en', 'es', 'ook') and len(variants) == 2 and variants[0] != variants[1]:
                errors.append(f'{language}: {key} displays the caller’s English plural suffix')
        timeline_key = 'STR_UI_2_WEEKS_BACK_TODAY_D_DUE_618EB7'
        if timeline_key in values and len((values[timeline_key] % 2147483647).encode('utf-8')) >= 64:
            errors.append(f'{language}: {timeline_key} exceeds its 64-byte UTF-8 buffer')
        for key, capacity in {
            'STR_UI_SOME_REVIEWS_DID_NOT_SAVE_BB1305': warning_capacity,
            'STR_UI_YOUR_DECKS_ARE_IN_STUDY_READY_4AF729': waiting_capacity,
            'STR_UI_YOUR_REVIEWS_ARE_SAFELY_SENT_THE_89C9F7': body_capacity,
            'STR_UI_YOUR_DECKS_ARE_UNCHANGED_CHANGE_THEM_47AD38': body_capacity,
            'STR_UI_THIS_READER_LOST_THE_DATE_SYNC_C75B46': warning_capacity,
        }.items():
            if key in values and len(values[key].encode('utf-8')) >= capacity:
                errors.append(f'{language}: {key} exceeds its {capacity}-byte UTF-8 buffer')
        items = list(values)
        for start in range(0, len(items), 1024):
            try:
                packs.compile_pack({k: values[k] for k in items[start:start + 1024]},
                                   code, parsed['_language_name'], skip_invalid=code == 'ook')
            except ValueError as error:
                errors.append(f'{language}: {error}')
        pack_bytes = 0
        if args.packs and code not in ('en', 'es'):
            try:
                blob, warnings = packs.compile_pack(values, code, parsed['_language_name'],
                                                    included_keys=set(refs), skip_invalid=code == 'ook')
                if warnings and code != 'ook':
                    errors.extend(f'{language}: {warning}' for warning in warnings)
                (args.packs / (code + '.cplang')).write_bytes(blob)
                pack_bytes = len(blob)
            except ValueError as error:
                errors.append(f'{language}: {error}')
        if code not in ('en', 'es', 'ook'):
            for entry in candidates(language, values, reference, refs):
                reason = RETAINED_SIGNALS.get((language, entry['key']))
                if reason:
                    retained.append(dict(language=language, **entry, retainedBecause=reason))
                else:
                    review.append(dict(language=language, **entry))
        if code == 'en' and (values['STR_QUEST_FEMININE'] in ('Feminile', 'Féminin', 'Feminin') or
                             values['STR_QUEST_MASCULINE'] in ('Maschile', 'Masculin')):
            errors.append('English character labels contain untranslated words from another language')
        languages.append(dict(language=language, code=code, keys=len(values), fictional=code == 'ook',
                              activeKeys=len(set(values) & set(refs)), exportedPackBytes=pack_bytes,
                              correctedValues=sum(e['language'] == language for e in ledger)))
    corrected = [dict(entry, active=entry['key'] in refs, references=refs.get(entry['key'], [])) for entry in ledger]
    report = dict(referenceKeys=len(reference), activeKeys=len(set(reference) & set(refs)),
                  catalogues=languages, structuralErrors=errors,
                  correctedValues=len(corrected), correctedActiveValues=sum(e['active'] for e in corrected),
                  latestBatchValues=len(extended),
                  correctedKeys=len({e['key'] for e in corrected}),
                  heuristicCandidates=len(review), retainedReviewedSignals=len(retained), linguisticCertification=False,
                  scope='All catalogues structurally checked. Context corrections are recorded individually. '
                        'Heuristics are review signals, not proof. Long narratives and unreviewed copy still need native editorial review.')
    for name, data in (('summary.json', report), ('corrections.json', corrected),
                       ('needs-review.json', review), ('retained-reviewed.json', retained)):
        (args.output / name).write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', 'utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'catalogues'}, ensure_ascii=False))
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
