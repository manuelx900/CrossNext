#include <PageCacheWriter.h>

#include <cassert>
#include <cstdio>
#include <cstring>

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
  {
    HalFile file;
    PageCacheWriter writer(file);
    assert(writer.deferPersistence());
    assert(writer.write(input.data(), input.size()) == input.size());
    assert(writer.drain() && file.calls == 0 && writer.position() == input.size());
    assert(!std::memcmp(writer.resident(0, input.size()), input.data(), input.size()));
    assert(!writer.resident(1, input.size()) && !writer.resident(input.size() + 1, 0));
    writer.allowPersistence();
    assert(writer.flushSome(1234) && file.bytes.size() == 1234);
    assert(writer.flushing() && writer.position() == input.size());
    assert(writer.resident(0, input.size()));
    assert(writer.flushAll() && file.bytes == input && !writer.flushing());
    assert(writer.position() == input.size());
  }
  {
    HalFile file;
    {
      PageCacheWriter writer(file);
      assert(writer.deferPersistence());
      assert(writer.write(input.data(), input.size()) == input.size());
    }
    assert(file.bytes.empty() && HalMemory::active == 0);
  }
  {
    HalFile file;
    PageCacheWriter writer(file);
    assert(writer.deferPersistence());
    std::vector<uint8_t> huge(300000, 42);
    assert(writer.write(input.data(), input.size()) == input.size());
    assert(writer.write(huge.data(), huge.size()) == huge.size());
    assert(writer.flushAll() && file.bytes.size() == input.size() + huge.size());
    assert(std::equal(input.begin(), input.end(), file.bytes.begin()));
    assert(std::equal(huge.begin(), huge.end(), file.bytes.begin() + input.size()));
  }
  for (size_t allowance : {size_t(0), size_t(3), size_t(8193)}) {
    HalFile file;
    PageCacheWriter writer(file);
    assert(writer.deferPersistence());
    assert(writer.write(input.data(), input.size()) == input.size());
    file.allowance = allowance;
    assert(!writer.flushAll());
    const auto calls = file.calls;
    assert(!writer.flushAll() && writer.write(uint8_t(0)) == 0 && calls == file.calls);
  }
  {
    HalFile file;
    PageCacheWriter writer(file);
    assert(writer.deferPersistence());
    std::vector<uint8_t> prefix(140000, 7);
    assert(writer.write(prefix.data(), prefix.size()) == prefix.size());
    assert(writer.spoolNeedsDrain() && file.calls == 0);
    writer.allowPersistence();
    while (writer.flushing()) {
      const auto before = file.bytes.size();
      assert(writer.flushSome());
      assert(file.bytes.size() - before <= 8192);
    }
    assert(file.bytes == prefix);
  }
  HalMemory::fail = true;
  {
    HalFile file;
    PageCacheWriter writer(file);
    assert(HalMemory::active == 0);
    assert(!writer.deferPersistence());
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
