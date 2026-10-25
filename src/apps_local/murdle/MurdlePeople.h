#pragma once
#include <I18n.h>

#include "MurdleCast.h"
namespace murdlepeople {
inline constexpr int kCount = 30, kPerGender = 15;
inline constexpr StrId names[] = {
    StrId::STR_MR_PERSON_0,  StrId::STR_MR_PERSON_1,  StrId::STR_MR_PERSON_2,  StrId::STR_MR_PERSON_3,
    StrId::STR_MR_PERSON_4,  StrId::STR_MR_PERSON_5,  StrId::STR_MR_PERSON_6,  StrId::STR_MR_PERSON_7,
    StrId::STR_MR_PERSON_8,  StrId::STR_MR_PERSON_9,  StrId::STR_MR_PERSON_10, StrId::STR_MR_PERSON_11,
    StrId::STR_MR_PERSON_12, StrId::STR_MR_PERSON_13, StrId::STR_MR_PERSON_14, StrId::STR_MR_PERSON_15,
    StrId::STR_MR_PERSON_16, StrId::STR_MR_PERSON_17, StrId::STR_MR_PERSON_18, StrId::STR_MR_PERSON_19,
    StrId::STR_MR_PERSON_20, StrId::STR_MR_PERSON_21, StrId::STR_MR_PERSON_22, StrId::STR_MR_PERSON_23,
    StrId::STR_MR_PERSON_24, StrId::STR_MR_PERSON_25, StrId::STR_MR_PERSON_26, StrId::STR_MR_PERSON_27,
    StrId::STR_MR_PERSON_28, StrId::STR_MR_PERSON_29};
inline uint32_t mix(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
inline int person(const murdle::Puzzle& p, int item) {
  static constexpr uint8_t steps[] = {1, 7, 11, 13, 17, 19, 23, 29};
  const auto value = mix(p.peopleSeed);
  return int((value % kCount + steps[(value >> 8) % 8] * unsigned(item)) % kCount);
}
inline const char* name(const murdle::Puzzle& p, int item) {
  return p.peopleSeed ? I18N.get(names[person(p, item)]) : murdle::kSuspects[p.cast[0][item]].name;
}
inline int portrait(const murdle::Puzzle& p, int item) {
  static constexpr uint8_t steps[] = {1, 2, 4, 7, 8, 11, 13, 14};
  static constexpr uint8_t portraits[2][kPerGender] = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 19, 11, 12, 13, 14},
                                                       {15, 16, 17, 18, 10, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29}};
  static constexpr uint8_t legacy[] = {15, 0,  16, 1, 17, 2,  18, 3,  4,  19, 5,  6,
                                       20, 21, 7,  8, 9,  22, 23, 10, 24, 25, 11, 26};
  const int who = p.peopleSeed ? person(p, item) : legacy[p.cast[0][item]];
  const auto value = mix((p.peopleSeed ? p.peopleSeed : p.seed) ^ 0x61637472u);
  const auto index = (value % kPerGender + steps[(value >> 8) % 8] * (who % kPerGender)) % kPerGender;
  return portraits[who >= kPerGender][index];
}
}  // namespace murdlepeople
