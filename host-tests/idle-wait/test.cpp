#include <cassert>
#include <cstdint>
#include <cstdio>

#include "../../src/util/LoopDeadline.h"

int main() {
  using loopdeadline::Deadline;
  Deadline idle(100);
  assert(idle.delayMs() == 1000);
  idle.after(0, 400);
  idle.after(80, 100);
  assert(idle.delayMs() == 80);
  idle.limit(10);
  assert(idle.delayMs() == 10);
  idle.after(0, 100);
  assert(idle.delayMs() == 0);
  idle.limit(1000);
  assert(idle.delayMs() == 0);

  Deadline wrap(20);
  wrap.after(UINT32_MAX - 79, 400);
  assert(wrap.delayMs() == 300);
  wrap.after(UINT32_MAX - 79, 100);
  assert(wrap.delayMs() == 0);
  for (uint32_t elapsed = 0; elapsed <= 1200; ++elapsed) {
    Deadline ordinary(5000 + elapsed), wrapped(uint32_t(UINT32_MAX - 600 + elapsed));
    ordinary.after(5000, 1000);
    wrapped.after(UINT32_MAX - 600, 1000);
    assert(ordinary.delayMs() == wrapped.delayMs());
  }
  std::puts("PASS deadline ordering, overdue work and uint32 rollover");
}
