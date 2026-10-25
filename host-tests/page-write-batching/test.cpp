#include <PageCacheWriter.h>

#include <cassert>
#include <cstdio>

int main() {
  std::vector<uint8_t> input(15000);
  for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<uint8_t>(i * 37);
  {
    HalFile file;
    const auto allocations = HalMemory::allocations;
    PageCacheWriter writer(file);
    assert(HalMemory::requested == 4096 && HalMemory::active == 1);
    assert(writer.write(nullptr, 0) == 0 && file.calls == 0);
    assert(writer.write(input.data(), 100) == 100 && file.bytes.empty());
    assert(writer.drain() && file.bytes.size() == 100);
    assert(writer.write(input.data() + 100, input.size() - 100) == input.size() - 100);
    assert(writer.drain() && file.bytes == input && file.calls == 5);
    assert(writer.drain() && file.calls == 5);
    assert(HalMemory::allocations == allocations + 1);
  }
  assert(HalMemory::active == 0);
  {
    HalFile file;
    {
      PageCacheWriter writer(file);
      assert(writer.write(input.data(), 99) == 99);
    }
    assert(file.bytes.empty() && file.calls == 0 && HalMemory::active == 0);
  }
  for (const size_t allowance : {size_t(0), size_t(10), size_t(4096), size_t(4100)}) {
    HalFile file;
    file.allowance = allowance;
    PageCacheWriter writer(file);
    const size_t accepted = writer.write(input.data(), 5000);
    assert(accepted == 0 || accepted == 5000);
    assert(!writer.drain());
    const auto calls = file.calls;
    assert(writer.write(uint8_t(1)) == 0 && !writer.drain() && calls == file.calls);
    assert(file.bytes.size() == allowance);
  }
  HalMemory::fail = true;
  {
    HalFile file;
    PageCacheWriter writer(file);
    assert(HalMemory::active == 0);
    assert(writer.write(nullptr, 0) == 0 && file.calls == 0);
    assert(writer.write(input.data(), input.size()) == input.size());
    assert(writer.drain() && file.bytes == input && file.calls == 1);
    file.allowance = 1;
    assert(writer.write(input.data(), 5) == 1);
    const auto calls = file.calls;
    assert(!writer.drain() && writer.write(uint8_t(2)) == 0 && file.calls == calls);
  }
  assert(HalMemory::active == 0);
  puts("PASS: page boundaries, byte identity, allocation lifetime, discard, short writes, direct fallback");
}
