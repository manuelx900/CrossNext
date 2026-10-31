#pragma once

#include "components/UiAppHost.h"

class ContentsReturnUi : private UiAppHost {
 public:
  explicit ContentsReturnUi(GfxRenderer& renderer);
  void render(bool grayMask = false);
  bool activated(const MappedInputManager& input);

 private:
  GfxRenderer& renderer;
  bool pending = false;
  int builtWidth = 0, builtHeight = 0, builtOrientation = -1;
  static void build(UiScreen& screen, void* context);
};
