#include "PieceDesign.h"

#include <HalStorage.h>

namespace chessstyle {
Design loadPreference() {
  char line[64]{};
  if (!Storage.exists(kSettingsPath) || !Storage.readFileToBuffer(kSettingsPath, line, sizeof(line))) return kDefault;
  return parsePreference(line);
}
}  // namespace chessstyle
