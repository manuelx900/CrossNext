#pragma once

#include <string_view>

#include "LibraryIndexFile.h"

namespace library {
enum class Facet : uint8_t { Author, AuthorFirst, Series, Genre, TitleInitial };
bool readFacet(LibraryIndexFile& index, const ClixRecord& record, Facet facet, std::string& value);
SortOrder facetOrder(Facet facet);
// Arrival-order row for a recent-first display position. Invalid and duplicate
// pins are ignored; the remaining rows keep their existing relative order.
uint16_t recentFilterRow(uint16_t position, uint16_t total, const uint16_t* ascendingPins, size_t count);

uint16_t recentSortRow(SortOrder order, uint16_t position, uint16_t total, const uint16_t* ascendingPins, size_t count);

struct FacetFilter {
  bool enabled = false;
  std::string value;
  bool matches(std::string_view candidate) const { return !enabled || foldEqual(candidate, value); }
  static bool foldEqual(std::string_view a, std::string_view b);
};

struct LibraryFilter {
  FacetFilter author, series, genre, initial;
  bool active() const { return author.enabled || series.enabled || genre.enabled || initial.enabled; }
  FacetFilter& forFacet(Facet facet);
  bool matches(LibraryIndexFile& index, const ClixRecord& record, std::string& scratch) const;
};
}  // namespace library
