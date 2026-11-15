#include <Arduino.h>
#include <Performance.h>

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>

std::string logText;
void logPrintf(const char*, const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  logText += line;
  logText += '\n';
}

int main() {
  using namespace performance;
  const void* owner = reinterpret_cast<void*>(12);
  auto first = begin(Operation::Navigation, owner);
  const auto old = stamp();
  auto latest = begin(Operation::Format, owner);
  finish(first, owner);
  assert(current() == latest);
  storageIo(old, Io::Write, 99);  // stale I/O cannot enter the new operation
  const auto io = stamp();
  testTimeMs += 2;
  storageIo(io, Io::Read, 32);
  storageWait(io);
  span(io, Stage::Layout);
  count(Counter::LayoutHit);
  finish(latest, reinterpret_cast<void*>(13));
  assert(current() == latest);
  finish(latest, owner);
  assert(current() == 0);
  assert(logText.find("kind=format") != std::string::npos);
  assert(logText.find("reads=1 read_bytes=32 read_us=2000") != std::string::npos);
  assert(logText.find("writes=0 write_bytes=0") != std::string::npos);
  assert(logText.find("wait_us=2000") != std::string::npos);
  assert(logText.find("name=layout_hit value=1") != std::string::npos);
  auto pop = begin(Operation::Navigation, nullptr);
  assert(begin(Operation::Navigation, owner) == pop);
  finish(pop, owner);
  assert(current() == 0);
  const auto requestedAt = timeMicros();
  testTimeMs += 20;
  const auto delayed = begin(Operation::PageTurn, owner, requestedAt);
  ++testTimeMs;
  finish(delayed, owner);
  assert(logText.find("kind=page_turn") != std::string::npos);
  assert(logText.find("dur=21000") != std::string::npos);
  puts("Performance operation ownership, stale completion, I/O totals and Home continuation passed");
}
