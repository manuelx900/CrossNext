#include <ReleaseJsonParser.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/network/FirmwareBoardTag.h"
#include "../../src/network/ReleaseSources.h"

namespace {
int checks = 0, failed = 0;
void check(bool ok, const char* message) {
  ++checks;
  if (!ok) {
    ++failed;
    std::fprintf(stderr, "FAIL: %s\n", message);
  }
}
std::string asset(const char* name, const char* url, int size) {
  return "{\"size\":" + std::to_string(size) + ",\"browser_download_url\":\"" + url + "\",\"name\":\"" + name + "\"}";
}
void parse(const std::string& assets, bool expected) {
  for (size_t chunk : {size_t(1), size_t(2), size_t(7), size_t(512)}) {
    // Assets before tag_name also has to work with a streaming response.
    const std::string json = "{\"assets\":[" + assets + "],\"tag_name\":\"v1.0.1\"}";
    ReleaseJsonParser parser;
    parser.setFirmwareAssetName(CROSSPOINT_RELEASE_ASSET);
    for (size_t at = 0; at < json.size(); at += chunk) parser.feed(json.data() + at, std::min(chunk, json.size() - at));
    check(parser.foundTag(), "release tag parsed");
    check(std::strcmp(parser.getTagName(), "v1.0.1") == 0, "release tag retained");
    check(parser.foundFirmware() == expected, "only exact CrossNext OTA alias matches");
    if (expected) {
      check(
          std::strcmp(parser.getFirmwareUrl(),
                      "https://github.com/manuelx900/CrossNext/releases/download/v1.0.1/CrossNext-X4PRO-OTA.bin") == 0,
          "selected URL belongs to the application alias");
      check(parser.getFirmwareSize() == 7500000, "size belongs to selected asset");
    }
  }
}
}  // namespace

int main() {
  check(release_sources::kCount == 1, "one authoritative source");
  check(
      std::strcmp(release_sources::kUrls[0], "https://api.github.com/repos/manuelx900/CrossNext/releases/latest") == 0,
      "CrossNext GitHub release source");
  check(std::strcmp(CROSSPOINT_RELEASE_ASSET, "CrossNext-X4PRO-OTA.bin") == 0, "stable X4 Pro alias");
  const std::string application =
      asset(CROSSPOINT_RELEASE_ASSET,
            "https://github.com/manuelx900/CrossNext/releases/download/v1.0.1/CrossNext-X4PRO-OTA.bin", 7500000);
  const std::string others =
      asset("firmware.bin", "https://example.invalid/upstream.bin", 1) + "," +
      asset("CrossNext-X4PRO-Firmware-v1.0.1-factory.bin", "https://example.invalid/factory.bin", 2) + "," +
      asset("CrossNext-X4PRO-Firmware-v1.0.1.zip", "https://example.invalid/firmware.zip", 3) + "," +
      asset("CrossNext-STICKY-OTA.bin", "https://example.invalid/other-board.bin", 4);
  parse(application + "," + others, true);
  parse(others + "," + application, true);
  parse(others, false);
  parse("", false);
  std::printf("crossnext-ota: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
