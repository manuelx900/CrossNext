#pragma once

#include <ContentVisibility.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

namespace foldervisibility {
inline constexpr char Marker[] = ".crossnext-hidden";
inline constexpr uint8_t MarkerVersion = 'H';

inline bool defaultHidden(const char* parent, const char* name) {
  return name[0] == '.' ||
         (std::strcmp(parent, "/") == 0 && (content::appFolder(name) || content::asciiEqual(name, "notes")));
}

inline bool markerPath(char* out, size_t capacity, const char* parent, const char* name = nullptr) {
  const size_t length = std::strlen(parent);
  if (!length) return false;
  const char* separator = parent[length - 1] == '/' ? "" : "/";
  const int count = name ? std::snprintf(out, capacity, "%s%s%s/%s", parent, separator, name, Marker)
                         : std::snprintf(out, capacity, "%s%s%s", parent, separator, Marker);
  return count >= 0 && size_t(count) < capacity;
}

inline bool hidden(const char* parent, const char* name, bool showHidden, char* scratch, size_t capacity) {
  if (showHidden) return false;
  if (defaultHidden(parent, name)) return true;
  return markerPath(scratch, capacity, parent, name) && Storage.exists(scratch);
}

inline bool marked(const char* path, char* scratch, size_t capacity) {
  return markerPath(scratch, capacity, path) && Storage.exists(scratch);
}

inline bool setMarked(const char* path, bool hide, char* scratch, size_t capacity) {
  if (!markerPath(scratch, capacity, path)) {
    LOG_ERR("BROWSE", "Hidden folder path too long");
    return false;
  }
  const bool exists = Storage.exists(scratch);
  if (hide == exists) return true;
  HalFile marker;
  if (hide) {
    if (!Storage.openFileForWrite("BROWSE", scratch, marker)) return false;
    const bool written = marker.write(MarkerVersion) == 1;
    const bool closed = marker.close();
    if (written && closed) return true;
    LOG_ERR("BROWSE", "Failed to save visibility marker");
    Storage.remove(scratch);
    return false;
  }
  if (!Storage.openFileForRead("BROWSE", scratch, marker)) return false;
  if (marker.isDirectory() || marker.size() != 1 || marker.read() != MarkerVersion) {
    LOG_ERR("BROWSE", "Refusing to remove an unknown visibility marker");
    return false;
  }
  marker.close();  // Close before removing the same path.
  return Storage.remove(scratch);
}
}  // namespace foldervisibility
