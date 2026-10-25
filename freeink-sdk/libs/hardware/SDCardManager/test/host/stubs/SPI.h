#pragma once
struct FakeSPI {
  void begin(int, int, int, int) {}
};
inline FakeSPI SPI;
