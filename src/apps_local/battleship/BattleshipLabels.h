#pragma once
#include <I18n.h>
namespace battleshiplabels {
inline const char* shipName(int index) {
  static constexpr StrId names[] = {StrId::STR_BATTLESHIP_SHIP_0, StrId::STR_BATTLESHIP_SHIP_1,
                                    StrId::STR_BATTLESHIP_SHIP_2, StrId::STR_BATTLESHIP_SHIP_3,
                                    StrId::STR_BATTLESHIP_SHIP_4};
  return index >= 0 && index < 5 ? I18N.get(names[index]) : "";
}
}  // namespace battleshiplabels
