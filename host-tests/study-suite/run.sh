#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/study-suite-tests-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$BUILD_DIR"
"${PYTHON:-python3}" fixtures.py "$BUILD_DIR"
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror -I../../lib/I18n -I../../lib/Epub test_core.cpp \
 ../../src/apps_local/flashcards/AnkiSqlite.cpp ../../src/apps_local/flashcards/FlashText.cpp ../../lib/Epub/Epub/htmlEntities.cpp \
 ../../src/apps_local/tools/ToolState.cpp ../../src/apps_local/quest/QuestCore.cpp \
 ../../src/apps_local/intercept/InterceptCore.cpp ../../src/apps_local/intercept/InterceptData.cpp \
 ../../src/apps_local/reading/ReadingStatsCore.cpp -o "$BUILD_DIR/test_core"
"$BUILD_DIR/test_core" "$BUILD_DIR"

"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror -Istubs -I../../lib/I18n -I../../lib/Epub -I../../lib/Memory -I../../lib/zstd/src \
 test_import.cpp ../../src/apps_local/notes/NotesCore.cpp ../../src/apps_local/notes/NotesLibrary.cpp ../../src/apps_local/flashcards/FlashImport.cpp ../../src/apps_local/flashcards/FlashMedia.cpp ../../src/apps_local/flashcards/FlashText.cpp ../../lib/Epub/Epub/htmlEntities.cpp \
 ../../src/apps_local/flashcards/AnkiSqlite.cpp ../../src/apps_local/study/StudyImages.cpp ../../src/apps_local/study/StudyDeck.cpp ../../src/apps_local/study/StudyFsrs.cpp \
 ../../src/apps_local/tools/ToolState.cpp ../../src/apps_local/tools/ToolStore.cpp \
 ../../src/apps_local/quest/QuestCore.cpp ../../src/apps_local/quest/QuestStore.cpp \
 ../../src/apps_local/intercept/InterceptCore.cpp ../../src/apps_local/intercept/InterceptData.cpp ../../src/apps_local/intercept/InterceptStore.cpp \
 ../../src/apps_local/reading/ReadingStatsCore.cpp -Wno-unneeded-internal-declaration -x c++ ../../lib/zstd/src/zstddeclib.c -o "$BUILD_DIR/test_import"
"$BUILD_DIR/test_import" "$BUILD_DIR"
