#pragma once
#include <HalStorage.h>
#include <InflateStream.h>

// A single entry whose file and inflater survive between bounded reads.
class ZipEntryReader {
 public:
  using Status = InflateStream::Status;
  ~ZipEntryReader() { close(); }
  bool open(const std::string& archive, const char* entry);
  Status read(uint8_t* output, size_t capacity, size_t& produced);
  void close();
  explicit operator bool() const { return file.isOpen(); }

 private:
  HalFile file;
  InflateStream inflate;
  uint8_t input[1024]{};
  size_t remaining = 0, expected = 0, total = 0;
  uint16_t method = 0;
  static size_t fill(void*, const uint8_t**);
};
