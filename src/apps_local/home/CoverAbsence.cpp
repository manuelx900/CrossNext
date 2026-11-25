#include "CoverAbsence.h"

#include <Epub.h>
#include <HalStorage.h>
#include <LibraryFormat.h>

#include <algorithm>
#include <array>

namespace homecover {
namespace {
constexpr size_t MAX_ABSENT = 8;
std::array<uint64_t, MAX_ABSENT> absent{};
size_t next = 0;
uint32_t mediaRevision = 0;
uint32_t resourceRevision = 0;
}  // namespace

bool knownMissingCover(const std::string& bookPath, std::string_view thumbnailPath) {
  const auto media = Storage.mediaRevision();
  const auto resources = Storage.resourceRevision();
  if (mediaRevision != media || resourceRevision != resources) {
    absent.fill(0);
    next = 0;
    mediaRevision = media;
    resourceRevision = resources;
  }
  if (thumbnailPath.empty() && !absent.front()) return false;
  const uint64_t key = library::clixPathHash(bookPath.data(), bookPath.size());
  if (key && std::find(absent.begin(), absent.end(), key) != absent.end()) return true;

  const auto slash = thumbnailPath.rfind('/');
  if (slash == std::string_view::npos || !Epub::knownMissingCover(bookPath, thumbnailPath.substr(0, slash)))
    return false;

  if (key && media && resources && media == Storage.mediaRevision() && resources == Storage.resourceRevision()) {
    absent[next] = key;
    next = (next + 1) % absent.size();
  }
  return true;
}
}  // namespace homecover
