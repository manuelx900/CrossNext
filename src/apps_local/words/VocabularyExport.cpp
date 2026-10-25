#include "VocabularyExport.h"

#include <Logging.h>

#include <algorithm>
#include <cstring>

namespace vocabulary {
namespace {
constexpr char live[] = "/WORDS/vocabulary.tsv", pending[] = "/WORDS/vocabulary.tsv.tmp",
               backup[] = "/WORDS/vocabulary.tsv.bak", ids[] = "/WORDS/vocabulary.ids.tmp";
constexpr uint32_t slotBytes = 33;
constexpr char header[] =
    "#separator:tab\n#html:false\n#columns:ID\tWord\tMeaning\tSentence\tBook\tChapter\tDictionary\n";
}  // namespace
void ExportJob::close() {
  directory.close();
  output.close();
  index.close();
}
void ExportJob::cancel() {
  close();
  if (phase != Phase::Idle && Storage.ready()) {
    if (Storage.exists(pending)) Storage.remove(pending);
    if (Storage.exists(ids)) Storage.remove(ids);
  }
  phase = Phase::Idle;
}
void ExportJob::fail() {
  LOG_ERR("WORDS", "Export failed");
  cancel();
  phase = Phase::Failed;
}
bool ExportJob::begin(Purpose collection, const char* path, int spine, const char* dictionary, uint32_t count) {
  cancel();
  phase = Phase::Prepare;
  purpose = collection;
  filterPath = path ? path : "";
  filterDictionary = dictionary ? dictionary : "";
  filterSpine = spine;
  prepared = 0;
  // Two slots per matching source note bounds the load factor to at most 1/2.
  if (count > (UINT32_MAX / slotBytes - 1) / 2 || !Storage.ensureDirectoryExists("/WORDS")) {
    fail();
    return false;
  }
  if (!Storage.exists(live) && Storage.exists(backup) && !Storage.rename(backup, live)) {
    fail();
    return false;
  }
  if (!Storage.openFileForWrite("WORDS", pending, output) ||
      output.write(reinterpret_cast<const uint8_t*>(header), sizeof(header) - 1) != sizeof(header) - 1) {
    fail();
    return false;
  }
  if (!count) {
    phase = Phase::Commit;
    return true;
  }
  slots = count * 2 + 1;
  if (!Storage.openFileForWrite("WORDS", ids, index)) {
    fail();
    return false;
  }
  return true;
}
void ExportJob::readNext() {
  auto file = directory.openNextFile();
  if (!file) {
    if (!Storage.ready())
      fail();
    else
      phase = Phase::Commit;
    return;
  }
  if (!recordId(file, kContexts, buffer)) return;
  const auto result = records.context(buffer, context);
  if (result == Read::Error) {
    fail();
    return;
  }
  if (result != Read::Ok || context.purpose != purpose || (*filterPath && std::strcmp(filterPath, context.path)) ||
      (filterSpine >= 0 && filterSpine != context.spine))
    return;
  const auto found = records.word(context.word, word);
  if (found == Read::Error) {
    fail();
    return;
  }
  if (found != Read::Ok || (*filterDictionary && std::strcmp(filterDictionary, word.dictionary))) return;
  uint32_t hash = 2166136261u;
  for (const char* p = word.id; *p; ++p) hash = (hash ^ uint8_t(*p)) * 16777619u;
  slot = hash % slots;
  probes = 0;
  phase = Phase::Probe;
}
void ExportJob::probe() {
  if (++probes > slots || !index.seekSet(slot * slotBytes) || index.read(buffer, slotBytes) != int(slotBytes)) {
    fail();
    return;
  }
  if (!buffer[0]) {
    if (!index.seekSet(slot * slotBytes) || index.write(word.id, slotBytes) != slotBytes) {
      fail();
      return;
    }
    field = offset = 0;
    phase = Phase::Append;
  } else if (!std::memcmp(buffer, word.id, slotBytes))
    phase = Phase::Read;
  else
    slot = (slot + 1) % slots;
}
void ExportJob::append() {
  const char* fields[] = {word.id,      context.surface, word.meaning,   context.sentence,
                          context.book, context.chapter, word.dictionary};
  const char* value = fields[field];
  unsigned n = 0;
  while (n < sizeof(buffer) && value[offset]) {
    const char c = value[offset++];
    buffer[n++] = c == '\t' || c == '\r' || c == '\n' ? ' ' : c;
  }
  if (n < sizeof(buffer) && !value[offset]) {
    buffer[n++] = field == 6 ? '\n' : '\t';
    offset = 0;
    if (++field == 7) phase = Phase::Read;
  }
  if (output.write(buffer, n) != n) fail();
}
void ExportJob::commit() {
  output.flush();
  close();
  if (Storage.exists(live)) {
    if ((Storage.exists(backup) && !Storage.remove(backup)) || !Storage.rename(live, backup)) {
      fail();
      return;
    }
  }
  if (!Storage.rename(pending, live)) {
    if (!Storage.exists(live) && Storage.exists(backup)) Storage.rename(backup, live);
    fail();
    return;
  }
  if (Storage.exists(backup)) Storage.remove(backup);
  if (Storage.exists(ids)) Storage.remove(ids);
  phase = Phase::Done;
}
void ExportJob::step(unsigned budget) {
  if (idle()) return;
  for (unsigned n = 0; n < std::min(budget, 8u) && !finished(); ++n) {
    if (!Storage.ready()) {
      fail();
      return;
    }
    switch (phase) {
      case Phase::Prepare: {
        const auto length = std::min(uint32_t(sizeof(buffer)), slots * slotBytes - prepared);
        std::memset(buffer, 0, length);
        if (index.write(buffer, length) != length) {
          fail();
          return;
        }
        prepared += length;
        if (prepared != slots * slotBytes) break;
        index.flush();
        index.close();
        if (!Storage.openFileForUpdate("WORDS", ids, index)) {
          fail();
          return;
        }
        directory = Storage.open(kContexts);
        if (!directory || !directory.isDirectory()) {
          fail();
          return;
        }
        phase = Phase::Read;
        break;
      }
      case Phase::Read:
        readNext();
        break;
      case Phase::Probe:
        probe();
        break;
      case Phase::Append:
        append();
        break;
      case Phase::Commit:
        commit();
        break;
      case Phase::Idle:
      case Phase::Done:
      case Phase::Failed:
        return;
    }
  }
}
}  // namespace vocabulary
