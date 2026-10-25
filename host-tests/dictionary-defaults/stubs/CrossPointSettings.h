#pragma once
#include <cstdint>
class CrossPointSettings {
 public:
  uint8_t language = 0;
  char dictionaryName[32] = "";
  char translationDictionaryName[32] = "";
  bool dictionaryAuto = true;
  bool translationDictionaryAuto = true;
};
