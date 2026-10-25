#pragma once

#include <cstddef>
#include <cstdint>

#include "../study/StudyScheduler.h"

namespace vocabulary {
inline constexpr size_t kIdBytes = 33;
inline constexpr size_t kWireBytes = 4096;
struct Word {
  char id[kIdBytes]{};
  char surface[192]{};
  char headword[192]{};
  char meaning[2048]{};
  char dictionary[128]{};
  char language[32]{};
  char latest[kIdBytes]{};
  study::CardState card;
  uint32_t encounters = 0;
  bool known = false;
  bool shortened = false;
  uint8_t lastRating = 0;
  uint16_t failures = 0;
};
enum class Purpose : uint8_t { Vocabulary, Language };
struct Context {
  Purpose purpose = Purpose::Vocabulary;
  char id[kIdBytes]{};
  char word[kIdBytes]{};
  char path[512]{};
  char book[192]{};
  char author[128]{};
  char chapter[192]{};
  char surface[192]{};
  char sentence[1536]{};
  uint32_t offset = UINT32_MAX;
  uint16_t spine = 0;
  uint16_t page = 0;
  bool fragment = false;
};
enum class Read : uint8_t { Missing, Ok, Error };
// A successful replacement is durable and recoverable as one complete record.
class Records {
 public:
  virtual ~Records() = default;
  virtual Read word(const char* id, Word& out) = 0;
  virtual Read context(const char* id, Context& out) = 0;
  virtual bool put(const Word&) = 0;
  virtual bool put(const Context&) = 0;
  virtual bool removeWord(const char*) = 0;
  virtual bool removeContext(const char*) = 0;
};
struct Undo {
  char word[kIdBytes]{};
  char context[kIdBytes]{};
  char previousContext[kIdBytes]{};
  uint32_t previousEncounters = 0;
  bool newWord = false;
  bool newContext = false;
  bool valid = false;
};
enum class Capture : uint8_t { Saved, Updated, Rejected, Failed };
// Scratch belongs to the lookup/app, never the task stack. Existing scheduling
// state and "already known" survive a fresh encounter, including another book.
Capture capture(Records&, Word& incoming, Context&, Word& scratch, Context& contextScratch, Undo&);
bool undo(Records&, Undo&, Word& scratch);
bool validId(const char*);
void identify(Word&);
void identify(Context&);
// Bounded UTF-8 copies: false means it did not fit. A shortened definition or
// visible-page context is labelled; identities and source paths must fit whole.
bool copy(char* out, size_t capacity, const char* in);
bool sentenceEnd(const char*);
void cloze(const char* sentence, const char* surface, char* out, size_t capacity);
void reviewQuestion(const Context&, bool withGap, const char* fragmentLabel, char* out, size_t capacity);
size_t encode(const Word&, uint8_t*, size_t);
size_t encode(const Context&, uint8_t*, size_t);
bool decode(const uint8_t*, size_t, Word&);
bool decode(const uint8_t*, size_t, Context&);
uint32_t checksum(const uint8_t*, size_t);
}  // namespace vocabulary
