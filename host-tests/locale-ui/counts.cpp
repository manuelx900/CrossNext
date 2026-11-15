#include <I18nKeys.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  int cases = 0;
  for (int language = 0; language < getLanguageCount(); ++language) {
    std::ifstream input(std::string(argv[1]) + "/" + std::to_string(language) + ".bin", std::ios::binary);
    if (!input) return 2;
    std::string data{std::istreambuf_iterator<char>(input), {}};
    std::vector<const char*> labels;
    labels.reserve(static_cast<size_t>(StrId::_COUNT));
    for (size_t offset = 0; offset < data.size();) {
      const auto end = data.find('\0', offset);
      if (end == std::string::npos) return 2;
      labels.push_back(data.c_str() + offset);
      offset = end + 1;
    }
    if (labels.size() != static_cast<size_t>(StrId::_COUNT)) return 2;
    const bool englishSuffix =
        language == int(Language::EN) || language == int(Language::ES) || language == int(Language::OOK);
    for (const auto& [key, capacity] :
         {std::pair{StrId::STR_UI_SOME_REVIEWS_DID_NOT_SAVE_BB1305, size_t(48)},
          std::pair{StrId::STR_UI_YOUR_DECKS_ARE_IN_STUDY_READY_4AF729, size_t(64)},
          std::pair{StrId::STR_UI_THIS_READER_LOST_THE_DATE_SYNC_C75B46, size_t(48)},
          std::pair{StrId::STR_UI_YOUR_DECKS_ARE_UNCHANGED_CHANGE_THEM_47AD38, size_t(192)},
          std::pair{StrId::STR_UI_YOUR_REVIEWS_ARE_SAFELY_SENT_THE_89C9F7, size_t(192)}}) {
      std::array<char, 192> output{};
      const int length = std::snprintf(output.data(), capacity, "%s", labels.at(static_cast<size_t>(key)));
      if (length < 0 || size_t(length) >= capacity) {
        std::fprintf(stderr, "Locale %d, key %u exceeds its %zu-byte UTF-8 buffer\n", language, unsigned(key),
                     capacity);
        return 1;
      }
      ++cases;
    }
    for (const int count : {1, 2, 1000000, 2147483647}) {
      static constexpr std::pair<StrId, size_t> countFields[] = {
          {StrId::STR_UI_STREAK_D_D_RECALL_D_REVIEW_96E50A, 80},
          {StrId::STR_UI_D_REVIEW_S_NONE_THIS_FORTNIGHT_BBD3F6, 80},
          {StrId::STR_UI_D_REVIEW_S_SENT_0360CA, 128},
          {StrId::STR_UI_D_DECK_S_REBUILT_WITH_YOUR_E08002, 128},
          {StrId::STR_UI_D_DECK_S_UPDATED_7E2BCA, 128},
          {StrId::STR_UI_D_DECK_S_HERE_CANNOT_BE_725445, 48},
          {StrId::STR_UI_D_WAITING_IN_YOUR_OTHER_DECK_166F23, 64}};
      for (const auto& [key, capacity] : countFields) {
        std::array<char, 128> noSuffix{}, suffix{};
        const char* format = labels.at(static_cast<size_t>(key));
        const auto render = [&](char* output, const char* plural) {
          if (key == StrId::STR_UI_STREAK_D_D_RECALL_D_REVIEW_96E50A)
            return std::snprintf(output, capacity, format, count, 100, count, plural);
          return std::snprintf(output, capacity, format, count, plural);
        };
        const int first = render(noSuffix.data(), "");
        const int second = render(suffix.data(), "S");
        if (first < 0 || second < 0 || size_t(first) >= capacity || size_t(second) >= capacity ||
            (!englishSuffix && std::strcmp(noSuffix.data(), suffix.data()) != 0)) {
          std::fprintf(stderr, "Locale %d, key %u, count %d: truncated text or English plural suffix\n", language,
                       unsigned(key), count);
          return 1;
        }
        ++cases;
      }
    }
    std::array<char, 64> cardRecord{};
    const int timelineLength =
        std::snprintf(cardRecord.data(), cardRecord.size(),
                      labels.at(size_t(StrId::STR_UI_2_WEEKS_BACK_TODAY_D_DUE_618EB7)), 2147483647);
    if (timelineLength < 0 || size_t(timelineLength) >= cardRecord.size()) {
      std::fprintf(stderr, "Locale %d: timeline exceeds its 64-byte UTF-8 buffer\n", language);
      return 1;
    }
    ++cases;
    const int length = std::snprintf(cardRecord.data(), cardRecord.size(),
                                     labels.at(size_t(StrId::STR_UI_SEEN_U_LAPSED_U_NOW_S_A8CBBF)), 4294967295u,
                                     4294967295u, "123456789012345");
    if (length < 0 || size_t(length) >= cardRecord.size()) {
      std::fprintf(stderr, "Locale %d: card statistics exceed their 64-byte UTF-8 buffer\n", language);
      return 1;
    }
    ++cases;
  }
  std::printf("%d study-text cases: no truncation and no English suffix in external natural languages\n", cases);
  return 0;
}
