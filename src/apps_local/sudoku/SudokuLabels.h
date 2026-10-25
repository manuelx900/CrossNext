#pragma once
#include <I18n.h>

#include "SudokuCore.h"
namespace sudokulabels {
inline const char* techniqueName(sudoku::Technique value) {
  static constexpr StrId labels[] = {
      StrId::STR_UI_NOTHING_C462E8,   StrId::STR_UI_ONLY_DIGIT_15BB44, StrId::STR_UI_ONLY_CELL_FB02D6,
      StrId::STR_UI_LOCKED_DA6BAC,    StrId::STR_UI_CELL_PAIR_CD9795,  StrId::STR_UI_DIGIT_PAIR_BB042F,
      StrId::STR_UI_CELL_TRIO_EDD718, StrId::STR_UI_DIGIT_TRIO_4784B1, StrId::STR_UI_RECTANGLE_B3F3D5,
      StrId::STR_UI_Y_CHAIN_A8816A,   StrId::STR_UI_SWORDFISH_78C392};
  const int index = static_cast<int>(value);
  return I18N.get(labels[index >= 0 && index < 11 ? index : 0]);
}
inline const char* levelName(sudoku::Level value) {
  static constexpr StrId labels[] = {StrId::STR_UI_EASY_493F11, StrId::STR_UI_MEDIUM_F68525, StrId::STR_UI_HARD_A06A58,
                                     StrId::STR_UI_EXPERT_F63794};
  const int index = static_cast<int>(value);
  return I18N.get(labels[index >= 0 && index < 4 ? index : 0]);
}
}  // namespace sudokulabels
