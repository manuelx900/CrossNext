#pragma once
constexpr int WL_CONNECTED = 3;
struct WifiMock {
  int status() { return 0; }
};
inline WifiMock WiFi;
