#pragma once
// HTTP boundary adapter for host integration tests; device TLS is not emulated.
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
extern "C" {
int test_http_request(const char*, const char*, const char*, const char*);
int test_http_size();
int test_http_complete();
void test_http_copy(unsigned char*, int, int);
}
namespace freeink {
class SecureHttpClient {
  std::string url, headers;
  bool complete = false;

 public:
  void setInsecure() {}
  void setTimeout(int) {}
  bool begin(const std::string& value) {
    url = value;
    headers.clear();
    return true;
  }
  void addHeader(const std::string& name, const std::string& value) { headers += name + ": " + value + "\n"; }
  int sendRequest(const char* method, const uint8_t* payload, size_t,
                  const std::function<bool(const uint8_t*, size_t)>& data, const std::function<bool()>& abort) {
    const int status = test_http_request(url.c_str(), headers.c_str(), method, reinterpret_cast<const char*>(payload));
    complete = false;
    uint8_t chunk[256];
    for (int at = 0; at < test_http_size(); at += 256) {
      if (abort && abort()) return status;
      const int n = std::min(256, test_http_size() - at);
      test_http_copy(chunk, at, n);
      if (!data(chunk, n)) return status;
    }
    complete = test_http_complete();
    return status;
  }
  bool responseComplete() const { return complete; }
};
}  // namespace freeink
