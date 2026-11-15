"""Check that the exported manuscript includes the exact text used by the app."""
import sys


def check(root):
    sys.path.insert(0, str(root / "tools_local/quest"))
    from export_story import export_story
    from scene_content import MILESTONES, load
    manuscript = load(root)
    table = (root / "src/apps_local/quest/QuestSceneData.h").read_text(encoding="utf-8")
    for lang in ("en", "es"):
        document, _ = export_story(manuscript, lang)
        cursor = 0
        for part in manuscript["prologue"]:
            text = part["text"][lang]
            cursor = document.index(text, cursor) + len(text)
        for book, saga in enumerate(manuscript["sagas"]):
            for threshold, version in zip(MILESTONES, saga["versions"]):
                text = version[lang]
                cursor = document.index(text, cursor) + len(text)
                assert f"StrId::STR_QUEST_CAM_JOURNEY2_SAGA_{book}_{threshold}," in table or (
                    f"StrId::STR_QUEST_CAM_JOURNEY2_SAGA_{book}_{threshold}" + "}" in table
                )
            for path in manuscript["paths"][book * 10 : (book + 1) * 10]:
                text = path["approach"][lang]
                cursor = document.index(text, cursor) + len(text)
                assert f"StrId::STR_QUEST_CAM_JOURNEY2_APPROACH_{path['id']}," in table
        assert document.index(manuscript["epilogue"][lang], cursor) >= cursor
    print("Narrative source passed: prologue, 60 saga versions, 120 path previews and epilogue")
