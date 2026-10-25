#include <Logging.h>
#include <Memory.h>

#include "Vocabulary.h"
#include "activities/reader/EpubReaderActivity.h"

std::unique_ptr<Activity> EpubReaderActivity::openVocabulary(GfxRenderer& r, MappedInputManager& input,
                                                             const vocabulary::Context& source) {
  auto reader = makeUniqueNoThrow<EpubReaderActivity>(r, input, source.path, false);
  if (!reader) {
    LOG_ERR("WORDS", "OOM: reader activity %u bytes", unsigned(sizeof(EpubReaderActivity)));
    return nullptr;
  }
  reader->vocabularySpine = source.spine;
  reader->vocabularyPage = source.page;
  reader->vocabularyOffset = source.offset;
  return reader;
}
