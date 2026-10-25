#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/words-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Isrc/apps_local/words host-tests/words/test_words.cpp src/apps_local/words/Vocabulary.cpp src/apps_local/study/StudyScheduler.cpp src/apps_local/study/StudyFsrs.cpp src/apps_local/study/StudyDeck.cpp -o "$out/test_words"
"$out/test_words"
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror -Ihost-tests/words/stubs -Ihost-tests/study-suite/stubs -Ilib/Memory -Isrc -Ilib/Epub -Isrc/apps_local/words host-tests/words/test_storage.cpp src/apps_local/words/Vocabulary.cpp src/apps_local/words/VocabularyStorage.cpp src/util/HtmlToPlainText.cpp lib/Epub/Epub/htmlEntities.cpp -o "$out/test_storage"
"$out/test_storage"

"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror -Ihost-tests/words/stubs -Ihost-tests/study-suite/stubs -Ilib/Memory -Isrc -Ilib/Epub -Isrc/apps_local/words host-tests/words/test_export.cpp src/apps_local/words/Vocabulary.cpp src/apps_local/words/VocabularyStorage.cpp src/apps_local/words/VocabularyExport.cpp src/util/HtmlToPlainText.cpp lib/Epub/Epub/htmlEntities.cpp -o "$out/test_export"
"$out/test_export"

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror host-tests/words/test_review.cpp src/apps_local/words/Vocabulary.cpp src/apps_local/study/StudyScheduler.cpp src/apps_local/study/StudyFsrs.cpp src/apps_local/study/StudyDeck.cpp -o "$out/test_review"
"$out/test_review"
