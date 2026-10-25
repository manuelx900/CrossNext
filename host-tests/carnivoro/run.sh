#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/carnivoro-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Isrc/apps_local/carnivoro host-tests/carnivoro/test_core.cpp src/apps_local/carnivoro/CarnivoroCore.cpp -o "$out/core"
"$out/core"

"${PYTHON:-python3}" host-tests/carnivoro/extract_storage.py "$out/activity_storage.inc"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Isrc/apps_local/carnivoro -I"$out" \
  host-tests/carnivoro/test_storage.cpp src/apps_local/carnivoro/CarnivoroCore.cpp -o "$out/storage"
"$out/storage"

"${PYTHON:-python3}" host-tests/carnivoro/extract_flow.py "$out/activity_flow.inc"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Isrc/apps_local/carnivoro -I"$out" -Ifreeink-sdk/libs/ui/FreeInkUI/include \
  -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n \
  host-tests/carnivoro/test_flow.cpp src/apps_local/carnivoro/CarnivoroCore.cpp -o "$out/flow"
"$out/flow"

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n \
  host-tests/carnivoro/test_screens.cpp src/apps_local/carnivoro/CarnivoroScreens.cpp src/apps_local/carnivoro/CarnivoroCore.cpp \
  src/apps_local/ui/PixelMenu.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp \
  freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/screens"
"$out/screens" "$@"
