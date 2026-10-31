#include "LibraryFilter.h"

#include <Utf8.h>

#include <algorithm>

#include "LibraryText.h"

namespace library {
uint16_t recentFilterRow(uint16_t position, uint16_t total, const uint16_t* ascendingPins, size_t count) {
  if (position >= total) return UINT16_MAX;
  uint16_t rows[LibraryIndexFile::MAX_IDENTITY_LOOKUPS];
  size_t used = 0;
  for (size_t n = 0; ascendingPins && n < std::min(count, LibraryIndexFile::MAX_IDENTITY_LOOKUPS); ++n) {
    if (ascendingPins[n] >= total) continue;
    const uint16_t row = total - 1 - ascendingPins[n];
    if (std::find(rows, rows + used, row) == rows + used) rows[used++] = row;
  }
  if (position < used) return rows[position];
  uint16_t row = position - used;
  std::sort(rows, rows + used);
  for (size_t n = 0; n < used; ++n)
    if (rows[n] <= row) ++row;
  return row;
}

uint16_t recentSortRow(SortOrder order, uint16_t position, uint16_t total, const uint16_t* ascendingPins,
                       size_t count) {
  if (position >= total) return UINT16_MAX;
  const bool ascending = order == SortOrder::RecentAsc;
  const uint16_t row = recentFilterRow(ascending ? total - 1 - position : position, total, ascendingPins, count);
  return row == UINT16_MAX ? row : ascending ? total - 1 - row : row;
}

bool readFacet(LibraryIndexFile& index, const ClixRecord& record, Facet facet, std::string& value) {
  switch (facet) {
    case Facet::Author:
    case Facet::AuthorFirst:
      return index.readAuthorField(record, value);
    case Facet::Series:
      return index.readSeries(record, value);
    case Facet::Genre:
      return index.readGenre(record, value);
    case Facet::TitleInitial: {
      uint32_t letter = foldedGroupInitial(std::string_view(record.fold, record.foldLen));
      value.clear();
      if (!letter)
        value = "#";
      else {
        if (letter >= 'a' && letter <= 'z') letter -= 'a' - 'A';
        utf8AppendCodepoint(letter, value);
      }
      return true;
    }
  }
  return false;
}

SortOrder facetOrder(Facet facet) {
  switch (facet) {
    case Facet::Author:
      return SortOrder::AuthorAsc;
    case Facet::AuthorFirst:
      return SortOrder::AuthorFirstAsc;
    case Facet::Series:
      return SortOrder::SeriesAsc;
    case Facet::Genre:
      return SortOrder::GenreAsc;
    case Facet::TitleInitial:
      return SortOrder::TitleAsc;
  }
  return SortOrder::TitleAsc;
}

bool FacetFilter::foldEqual(std::string_view a, std::string_view b) { return fold(a) == fold(b); }

FacetFilter& LibraryFilter::forFacet(Facet facet) {
  switch (facet) {
    case Facet::Author:
    case Facet::AuthorFirst:
      return author;
    case Facet::Series:
      return series;
    case Facet::Genre:
      return genre;
    case Facet::TitleInitial:
      return initial;
  }
  return author;
}

bool LibraryFilter::matches(LibraryIndexFile& index, const ClixRecord& record, std::string& scratch) const {
  if (author.enabled && (!index.readAuthorField(record, scratch) || !author.matches(scratch))) return false;
  if (series.enabled && (!index.readSeries(record, scratch) || !series.matches(scratch))) return false;
  if (genre.enabled && (!index.readGenre(record, scratch) || !genre.matches(scratch))) return false;
  if (initial.enabled && (!readFacet(index, record, Facet::TitleInitial, scratch) || !initial.matches(scratch)))
    return false;
  return true;
}
}  // namespace library
