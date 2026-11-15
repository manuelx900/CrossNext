"""Editing the canonical story survives regeneration and reaches translated tables."""
import importlib.util
import json
from pathlib import Path
import tempfile
import shutil
import sys


def check(root, output):
    sys.path.insert(0, str(root / "tools_local/quest"))
    spec = importlib.util.spec_from_file_location("quest_content", root / "tools_local/quest/seed_campaign_content.py")
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    with tempfile.TemporaryDirectory(prefix="content-", dir=output) as directory:
        sandbox = Path(directory).resolve()
        assert sandbox.is_relative_to(output.resolve())
        generator.ROOT = sandbox
        generator.SOURCE = sandbox / "assets_local/quest/campaign"
        generator.SOURCE.mkdir(parents=True)
        (sandbox / "src/apps_local/quest").mkdir(parents=True)
        (sandbox / "assets_local/quest/expansion").mkdir(parents=True)
        shutil.copy2(root / "assets_local/quest/expansion/catalog.json", sandbox / "assets_local/quest/expansion/catalog.json")
        translations = sandbox / "lib/I18n/translations"
        translations.mkdir(parents=True)
        for name in ("english", "spanish"):
            (translations / (name + ".yaml")).write_text('STR_OTHER_APP: "Keep me"\n', "utf-8")
        content = json.loads((root / "assets_local/quest/campaign/story.json").read_text("utf-8"))
        content["chapters"][0]["title"]["en"] = "A revised chapter"
        content['journey']['regions'][0]['dailyRequest']['es'] = 'Una petición revisada de Liora.'
        content['chapters'][0]['encounters'][0]['kind'] = 'restore'
        paths = json.loads((root / "assets_local/quest/campaign/paths.json").read_text("utf-8"))
        paths["prologue"][1]["title"]["en"] = "The revised opening"
        paths["prologue"][0]["text"]["es"] = "Una apertura revisada."
        paths["sagas"][0]["versions"][2]["es"] = "El relato revisado."
        paths["paths"][0]["title"] = {"en": "The revised bell", "es": "La campanilla revisada"}
        paths["paths"][0]["approach"]["en"] = "A revised arrival"
        paths_source = generator.SOURCE / "paths.json"
        paths_original = json.dumps(paths, ensure_ascii=False).encode("utf-8")
        paths_source.write_bytes(paths_original)
        source = generator.SOURCE / "story.json"
        original = json.dumps(content, ensure_ascii=False).encode("utf-8")
        source.write_bytes(original)
        requests = json.loads((root / "assets_local/quest/campaign/requests.json").read_text("utf-8"))
        requests['requests'][0]['request']['en'] = 'A revised request with a clear reason.'
        requests['requests'][0]['readingLink']['en'] = 'A revised reading connection.'
        requests_source = generator.SOURCE / 'requests.json'
        requests_original = json.dumps(requests, ensure_ascii=False).encode('utf-8')
        requests_source.write_bytes(requests_original)
        generator.build()
        assert requests_source.read_bytes() == requests_original
        assert source.read_bytes() == original
        assert paths_source.read_bytes() == paths_original
        english = (translations / "english.yaml").read_text("utf-8")
        assert 'STR_QUEST_CAM_JOURNEY2_CHAPTER_0: "The revised bell"' in english
        assert 'STR_QUEST_CAM_JOURNEY2_PROLOGUE_TITLE_1: "The revised opening"' in english
        assert 'STR_QUEST_CAM_HUNT_ITEM_1: "Songbook quill"' in english
        assert 'STR_OTHER_APP: "Keep me"' in english
        assert 'STR_QUEST_CAM_REQ_0_REQUEST: "A revised request with a clear reason."' in english
        assert 'A revised reading connection.' in english
        assert 'STR_QUEST_CAM_STEP_APPROACH_' not in english
        kinds = (sandbox / 'src/apps_local/quest/QuestEncounterKinds.h').read_text('utf8')
        assert kinds.split('ENCOUNTER_KINDS[] = {', 1)[1].lstrip().split(',', 1)[0] == '3'
        assert 'STR_QUEST_CAM_JOURNEY2_APPROACH_0: "A revised arrival"' in english
        scene_table = (sandbox / "src/apps_local/quest/QuestSceneData.h").read_text("utf8")
        assert "StrId::STR_QUEST_CAM_JOURNEY2_APPROACH_0," in scene_table
        spanish = (translations / "spanish.yaml").read_text("utf-8")
        assert 'STR_QUEST_CAM_JOURNEY2_PROLOGUE_TEXT_0: "Una apertura revisada."' in spanish
        assert 'STR_QUEST_CAM_JOURNEY2_SAGA_0_6: "El relato revisado."' in spanish
        assert 'STR_QUEST_CAM_JOURNEY_DAILYREQUEST_0: \"Una petición revisada de Liora.\"' in spanish
        generated = (sandbox / "src/apps_local/quest/QuestCampaignPacing.h").read_bytes()
        generator.build()
        assert generated == (sandbox / "src/apps_local/quest/QuestCampaignPacing.h").read_bytes()
        assert english == (translations / "english.yaml").read_text("utf-8")
    print("Campaign content: canonical edits retained, translations propagated, unrelated strings preserved, deterministic regeneration")
