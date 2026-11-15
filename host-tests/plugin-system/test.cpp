#include <cassert>
#include <cstdio>

#include "util/FirmwareIdentity.h"
#include "util/PluginLocations.h"
#include "util/UrlUtils.h"
int main() {
  using namespace PluginLocations;
  assert(isDiscoverableEntry(false, true, false, false));
  assert(!isDiscoverableEntry(true, true, true, true));
  assert(!isDiscoverableEntry(false, false, false, false));
  assert(classifyDeviceManifest(true, true) == DeviceKind::Catalog);
  assert(classifyDeviceManifest(false, true) == DeviceKind::Background);
  assert(pickerAction(DeviceKind::Catalog, false) == PickerAction::Catalog);
  assert(pickerAction(DeviceKind::Background, true) == PickerAction::Readme);
  assert(pickerAction(DeviceKind::None, false) == PickerAction::None);
  assert(shouldReuseInstalledBundleDir("/plugins", "demo"));
  assert(!shouldReuseInstalledBundleDir("/plugins", "demo/nested"));
  assert(!shouldReuseInstalledBundleDir("/books", "demo"));
  assert(isNewerVersion("2.0.0", "1.9.9"));
  assert(!isNewerVersion("2.0.0", "2.0.0"));
  assert(!isNewerVersion("2.0.0", "2.0.1"));
  assert(isNewerVersion("1.0.0", "unknown"));
  for (auto invalid : {"1.0", "1.0.0-rc1", "1.0.0.1", "-1.0.0", "1.0.4294967296", " 1.0.0"})
    assert(!isNewerVersion(invalid, "0.0.0"));
  std::string path = "/files/book.epub";
  assert(UrlUtils::resolveServerPath(path, "https://example.org:8443/api/list?q=2"));
  assert(path == "https://example.org:8443/files/book.epub");
  path = "https://cdn.example.org/book.epub";
  assert(!UrlUtils::resolveServerPath(path, "https://example.org/api"));
  path = "//cdn.example.org/file";
  assert(!UrlUtils::resolveServerPath(path, "https://example.org/api"));
  assert(firmwareidentity::PLUGIN_API_VERSION == 1);
  std::puts("Plugin quarantine, discovery, version tracking, bundle roots and server-path tests passed.");
}
