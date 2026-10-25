#pragma once
class HalDisplay {
 public:
  int getDisplayWidth() const { return 800; }
  int getDisplayHeight() const { return 480; }
};
inline HalDisplay display;

struct HostESP {
  unsigned getFreeHeap() const { return 1u << 30; }
};
inline HostESP ESP;
