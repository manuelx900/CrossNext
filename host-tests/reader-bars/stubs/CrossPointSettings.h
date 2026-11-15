#pragma once
struct FakeSettings {
  unsigned clockFormat = 0;
};
inline FakeSettings settings;
#define SETTINGS settings
