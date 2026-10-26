#pragma once

#include "ReadingStatsCloud.h"

namespace freeink {
class SecureHttpClient;
}

namespace readingstats::cloud {
class Client {
 public:
  using Cancel = bool (*)(void* context);
  explicit Client(Cancel cancel = nullptr, void* context = nullptr);
  ~Client();
  void close();
  static Result fetch(void* client, const char* path, const char* destination);
  static uint64_t accountId();
  static bool configured();
  static uint64_t deviceTag();
  static Result exchange(void* client, const char* method, const char* path, const char* body, const char* destination);

 private:
  Cancel cancel;
  void* context;
  std::unique_ptr<freeink::SecureHttpClient> http;
  bool cancelled() const { return cancel && cancel(context); }
  Result send(const char* method, const char* path, const char* body, const char* destination);
};
}  // namespace readingstats::cloud
