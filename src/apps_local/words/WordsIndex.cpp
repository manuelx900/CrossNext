#include "WordsIndex.h"

#include <Logging.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "WordsLabel.h"

namespace vocabulary {
namespace {
constexpr const char* paths[] = {"/.crosspoint/vocabulary/.list-words.tmp",
                                 "/.crosspoint/vocabulary/.list-language.tmp"};
void scheduling(WordsIndex::Entry& entry, const Word& word) {
  entry.card = word.card;
  entry.failures = word.failures;
  entry.rating = word.lastRating;
  entry.known = word.known;
  entry.checksum = checksum(reinterpret_cast<const uint8_t*>(&entry), offsetof(WordsIndex::Entry, checksum));
}
}  // namespace
bool WordsIndex::begin() {
  cancel();
  for (unsigned n = 0; n < 2; ++n) {
    if (!Storage.openFileForWrite("WORDS", paths[n], files_[n])) {
      cancel();
      return false;
    }
  }
  return true;
}
void WordsIndex::cancel() {
  ready_ = false;
  updating_ = false;
  for (unsigned n = 0; n < 2; ++n) {
    files_[n].close();
    counts_[n] = 0;
    if (Storage.ready() && Storage.exists(paths[n])) Storage.remove(paths[n]);
  }
}
bool WordsIndex::append(const Word& word, const Context& context) {
  const auto n = static_cast<unsigned>(context.purpose);
  auto& entry = batch_[0];
  copy(entry.context, sizeof(entry.context), context.id);
  copy(entry.word, sizeof(entry.word), word.id);
  wordsui::wordLabel(entry.label, sizeof(entry.label), word.headword, word.language);
  scheduling(entry, word);
  if (counts_[n] == UINT32_MAX / sizeof(Entry) || files_[n].write(&entry, sizeof(entry)) != sizeof(entry)) {
    LOG_ERR("WORDS", "Navigation index unavailable; using original records");
    cancel();
    return false;
  }
  ++counts_[n];
  return true;
}
bool WordsIndex::finish() {
  for (unsigned n = 0; n < 2; ++n) {
    files_[n].flush();
    files_[n].close();
    if (!Storage.openFileForUpdate("WORDS", paths[n], files_[n]) || files_[n].size() != counts_[n] * sizeof(Entry)) {
      cancel();
      return false;
    }
  }
  ready_ = true;
  return true;
}
bool WordsIndex::read(Purpose purpose, uint32_t first, uint32_t count) {
  const auto n = static_cast<unsigned>(purpose);
  if (!ready_ || count > kBatch || first > counts_[n] || count > counts_[n] - first ||
      !files_[n].seekSet(first * sizeof(Entry)) ||
      files_[n].read(batch_, count * sizeof(Entry)) != static_cast<int>(count * sizeof(Entry))) {
    cancel();
    return false;
  }
  for (unsigned i = 0; i < count; ++i) {
    const auto& entry = batch_[i];
    if (entry.checksum != checksum(reinterpret_cast<const uint8_t*>(&entry), offsetof(Entry, checksum)) ||
        !validId(entry.context) || !validId(entry.word) || !std::memchr(entry.label, 0, sizeof(entry.label))) {
      cancel();
      return false;
    }
  }
  return true;
}
void WordsIndex::unpack(unsigned i, Word& word, Context& context) const {
  const auto& entry = batch_[i];
  copy(context.id, sizeof(context.id), entry.context);
  copy(context.word, sizeof(context.word), entry.word);
  copy(word.id, sizeof(word.id), entry.word);
  copy(word.headword, sizeof(word.headword), entry.label);
  word.card = entry.card;
  word.failures = entry.failures;
  word.lastRating = entry.rating;
  word.known = entry.known;
}
bool WordsIndex::update(const Word& word) {
  if (!ready_) return false;
  // A second answer may arrive before maintenance finishes. Drop only the
  // disposable cache; original records and the active review remain valid.
  if (updating_) {
    cancel();
    return false;
  }
  auto& pending = batch_[kBatch - 1];
  copy(pending.word, sizeof(pending.word), word.id);
  scheduling(pending, word);
  updateAt_ = updateCollection_ = 0;
  updating_ = true;
  return true;
}
void WordsIndex::stepUpdate() {
  if (!updating_) return;
  auto& file = files_[updateCollection_];
  const auto count = std::min(kBatch - 1, counts_[updateCollection_] - updateAt_);
  if (!count) {
    file.flush();
    updateAt_ = 0;
    if (++updateCollection_ == 2) updating_ = false;
    return;
  }
  // The last existing batch slot owns the pending summary until completion.
  if (!read(static_cast<Purpose>(updateCollection_), updateAt_, count)) return;
  const auto& pending = batch_[kBatch - 1];
  for (unsigned i = 0; i < count; ++i) {
    auto& entry = batch_[i];
    if (std::strcmp(entry.word, pending.word)) continue;
    entry.card = pending.card;
    entry.failures = pending.failures;
    entry.rating = pending.rating;
    entry.known = pending.known;
    entry.checksum = checksum(reinterpret_cast<const uint8_t*>(&entry), offsetof(Entry, checksum));
    if (!file.seekSet((updateAt_ + i) * sizeof(Entry)) || file.write(&entry, sizeof(entry)) != sizeof(entry)) {
      cancel();
      return;
    }
  }
  updateAt_ += count;
}
}  // namespace vocabulary
