#pragma once
#include <CrossPointPosition.h>
#include <Epub.h>
#include <KOReaderSyncClient.h>

#include <cstdio>
#include <memory>
#include <optional>
struct SavedProgressPosition {
  std::string xpath;
  float percentage;
};
class ProgressMapper {
 public:
  static SavedProgressPosition toSavedProgress(const std::shared_ptr<Epub>&, const CrossPointPosition& pos) {
    const int offset = pos.hasVisibleTextOffset ? pos.visibleTextOffset : pos.pageNumber * 100;
    return {"/body/DocFragment[" + std::to_string(pos.spineIndex + 1) + "]/body/p[1]/text()." + std::to_string(offset),
            (pos.spineIndex * 10000 + offset) / 100000.0f};
  }
  static CrossPointPosition toCrossPoint(const std::shared_ptr<Epub>&, const SavedProgressPosition& pos, GfxRenderer&) {
    CrossPointPosition out{};
    int chapter = 0, offset = 0;
    if (std::sscanf(pos.xpath.c_str(), "/body/DocFragment[%d]/body/p[1]/text().%d", &chapter, &offset) == 2) {
      out.spineIndex = chapter - 1;
      out.visibleTextOffset = offset;
      out.hasVisibleTextOffset = out.hasResolvedSpineIndex = out.hasMappedPage = true;
      out.pageNumber = offset / 100;
    } else {
      out.spineIndex = int(pos.percentage * 10);
      out.pageNumber = int(pos.percentage * 1000) % 100;
    }
    out.totalPages = 100;
    return out;
  }
  static std::optional<CrossPointPosition> fromRichPosition(const std::shared_ptr<Epub>&,
                                                            const KOReaderRichPosition& pos, GfxRenderer&, bool) {
    CrossPointPosition out{};
    out.spineIndex = pos.spineIndex;
    out.pageNumber = pos.pageNumber;
    out.totalPages = pos.totalPages;
    out.hasResolvedSpineIndex = out.hasMappedPage = true;
    return out;
  }
};
