#include "ReadingProgressBooks.h"

#include <ChapterXPathResolver.h>
#include <ContentVisibility.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <KOReaderCredentialStore.h>
#include <KOReaderDocumentId.h>
#include <Memory.h>
#include <ProgressMapper.h>
#include <Xtc.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Epub/Section.h"
#include "activities/reader/ProgressFile.h"

namespace readingstats::cloud {
namespace {
uint16_t word(const uint8_t* data) { return uint16_t(data[0]) | uint16_t(data[1]) << 8; }
uint32_t dword(const uint8_t* data) { return uint32_t(word(data)) | uint32_t(word(data + 2)) << 16; }
bool readSaved(const ProgressBook& book, uint8_t* bytes, size_t& size) {
  const auto path = book.cache + "/progress.bin";
  size = 0;
  if (!Storage.exists(path.c_str())) return true;
  HalFile file;
  if (!Storage.openFileForRead("PROGRESS-SYNC", path, file)) return false;
  size = file.fileSize();
  return size <= 10 && file.read(bytes, size) == static_cast<int>(size) &&
         (book.fixedPages ? size == 4 : (size == 4 || size == 6 || size == 10));
}
// The unique owner outlives every synchronous mapper/Section call. Aliasing an
// empty shared owner borrows the Epub without allocating a control block.
std::shared_ptr<Epub> borrow(Epub* book) { return {std::shared_ptr<Epub>{}, book}; }
Result openEpub(const ProgressBook& book, std::unique_ptr<Epub>& epub) {
  // One book's metadata at a time; released before every HTTP request.
  epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
  if (!epub) {
    LOG_ERR("PROGRESS-SYNC", "OOM: Epub metadata");
    return Result::LowMemory;
  }
  epub->setupCacheDir();
  return epub->load(true, true) ? Result::Success : Result::StorageError;
}
CrossPointPosition mapRemote(const std::shared_ptr<Epub>& epub, const KOReaderProgress& remote, GfxRenderer& renderer) {
  auto result = ProgressMapper::toCrossPoint(epub, {remote.progress, remote.percentage}, renderer);
  if (!result.hasVisibleTextOffset && remote.position) {
    const auto rich =
        ProgressMapper::fromRichPosition(epub, *remote.position, renderer, remote.position->xpath == remote.progress);
    if (rich) result = *rich;
  }
  if (!result.hasVisibleTextOffset && !result.hasMappedPage && epub->getBookSize() > 0) {
    const size_t previous = result.spineIndex > 0 ? epub->getCumulativeSpineItemSize(result.spineIndex - 1) : 0;
    const size_t end = epub->getCumulativeSpineItemSize(result.spineIndex);
    const float intra =
        end > previous ? std::clamp((remote.percentage * epub->getBookSize() - previous) / (end - previous), 0.0f, 1.0f)
                       : 0.0f;
    const auto xpath = ChapterXPathResolver::findXPathForProgress(epub, result.spineIndex, intra);
    if (!xpath.empty()) result = ProgressMapper::toCrossPoint(epub, {xpath, remote.percentage}, renderer);
  }
  return result;
}
CrossPointPosition mapFixed(const ProgressBook& book, const KOReaderProgress& remote) {
  CrossPointPosition result{};
  result.pageNumber = static_cast<int>(
      std::clamp(std::ceil(double(remote.percentage) * book.pages - 0.0001) - 1.0, 0.0, double(book.pages - 1)));
  uint32_t page = 0;
  bool numeric = !remote.progress.empty();
  for (const unsigned char digit : remote.progress) {
    if (digit < '0' || digit > '9' || page > book.pages / 10 ||
        (page == book.pages / 10 && uint32_t(digit - '0') > book.pages % 10)) {
      numeric = false;
      break;
    }
    page = page * 10 + digit - '0';
  }
  if (numeric && page > 0 && page <= book.pages) result.pageNumber = static_cast<int>(page - 1);
  result.hasMappedPage = result.hasResolvedSpineIndex = true;
  return result;
}
}  // namespace
Result ProgressBooks::begin() {
  close();
  folder.reserve(maxPath);
  folder = "/";
  directories[0].file = Storage.open("/");
  if (!directories[0].file || !directories[0].file.isDirectory()) return Result::StorageError;
  depth = 1;
  entries = 0;
  return Result::Busy;
}
void ProgressBooks::close() {
  for (auto& directory : directories) directory.file.close();
  depth = 0;
}
Result ProgressBooks::next(ProgressBook& book) {
  if (!depth) return Result::Empty;
  if (++entries > 200000) return Result::LimitExceeded;
  auto& current = directories[depth - 1];
  bool ioError = false;
  HalFile entry = current.file.openNextFile(ioError);
  if (ioError) return Result::StorageError;
  if (!entry) {
    current.file.close();
    folder.resize(current.prefix);
    --depth;
    return depth ? Result::Busy : Result::Empty;
  }
  const size_t length = entry.getName(name, sizeof(name));
  if (!length || length >= sizeof(name) - 1) return Result::LimitExceeded;
  if (name[0] == '.' || (depth == 1 && entry.isDirectory() && content::appFolder(name))) return Result::Busy;
  if (folder.size() + length + 1 >= maxPath) return Result::LimitExceeded;
  if (entry.isDirectory()) {
    if (depth == maxDepth) return Result::LimitExceeded;
    directories[depth].prefix = folder.size();
    if (folder.back() != '/') folder += '/';
    folder += name;
    directories[depth++].file = std::move(entry);
    return Result::Busy;
  }
  if (!FsHelpers::hasReflowableBookExtension(std::string_view(name)) &&
      !FsHelpers::hasXtcExtension(std::string_view(name)))
    return Result::Busy;
  std::destroy_at(&book);
  std::construct_at(&book);
  book.path = folder;
  if (book.path.back() != '/') book.path += '/';
  book.path += name;
  book.fixedPages = FsHelpers::hasXtcExtension(std::string_view(name));
  book.cache = std::string("/.crosspoint/") + (book.fixedPages ? "xtc_" : "epub_") +
               std::to_string(std::hash<std::string>{}(book.path));
  size_t size = 0;
  if (!readSaved(book, book.saved, size)) return Result::StorageError;
  book.savedSize = static_cast<uint8_t>(size);
  const bool filename = KOREADER_STORE.getMatchMethod() == DocumentMatchMethod::FILENAME;
  book.local.document =
      filename ? KOReaderDocumentId::calculateFromFilename(book.path) : KOReaderDocumentId::calculate(book.path);
  book.alternate =
      filename ? KOReaderDocumentId::calculate(book.path) : KOReaderDocumentId::calculateFromFilename(book.path);
  return book.local.document.empty() || book.alternate.empty() ? Result::StorageError : Result::Success;
}
Result ProgressBooks::prepare(ProgressBook& book) {
  if (!book.savedSize) return Result::Success;
  if (book.fixedPages) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("PROGRESS-SYNC", "OOM: Xtc metadata");
      return Result::LowMemory;
    }
    if (!xtc->load() || !xtc->getPageCount() || xtc->getPageCount() > INT32_MAX) return Result::StorageError;
    book.pages = xtc->getPageCount();
    const auto page = dword(book.saved);
    if (page >= book.pages || page > INT32_MAX) return Result::StorageError;
    book.position.pageNumber = static_cast<int>(page);
    book.position.hasMappedPage = book.position.hasResolvedSpineIndex = true;
    book.local.percentage = float(double(page + 1) / book.pages);
    book.local.progress = std::to_string(page + 1);
    if (KOREADER_STORE.getSendMetadata()) {
      book.local.metadata.emplace();
      book.local.metadata->title = xtc->getTitle();
      book.local.metadata->authors = xtc->getAuthor();
    }
  } else {
    std::unique_ptr<Epub> owner;
    const auto opened = openEpub(book, owner);
    if (opened != Result::Success) return opened;
    const auto epub = borrow(owner.get());
    book.position.spineIndex = word(book.saved);
    book.position.pageNumber = word(book.saved + 2);
    book.position.totalPages = book.savedSize >= 6 ? word(book.saved + 4) : 0;
    book.position.hasResolvedSpineIndex = true;
    book.position.hasMappedPage = true;
    if (book.position.spineIndex >= epub->getSpineItemsCount()) return Result::StorageError;
    if (book.savedSize == 10) {
      book.position.visibleTextOffset = dword(book.saved + 6);
      book.position.hasVisibleTextOffset = true;
    }
    Section section(epub, book.position.spineIndex, renderer);
    if (book.position.totalPages <= 0) {
      const auto pages = section.getCachedPageCount();
      if (!pages || *pages <= 0) return Result::StorageError;
      book.position.totalPages = *pages;
    }
    if (book.position.pageNumber >= book.position.totalPages) return Result::StorageError;
    const auto saved = ProgressMapper::toSavedProgress(epub, book.position);
    book.local.percentage = saved.percentage;
    book.local.progress = saved.xpath;
    KOReaderRichPosition rich;
    rich.pctQ = static_cast<uint32_t>(saved.percentage * 1000000.0f + 0.5f);
    rich.spineIndex = book.position.spineIndex;
    rich.pageNumber = book.position.pageNumber;
    rich.totalPages = book.position.totalPages;
    if (saved.xpath.size() <= 120) rich.xpath = saved.xpath;
    book.local.position = std::move(rich);
    if (KOREADER_STORE.getSendMetadata()) {
      book.local.metadata.emplace();
      book.local.metadata->title = epub->getTitle();
      book.local.metadata->authors = epub->getAuthor();
    }
  }
  if (book.local.metadata) {
    auto& metadata = *book.local.metadata;
    metadata.filename = book.path.substr(book.path.rfind('/') + 1);
    if (metadata.title.size() > 512 || metadata.authors.size() > 512) return Result::LimitExceeded;
    Storage.readFileToString("PROGRESS-SYNC", book.path + ".meta.json", 2048, metadata.extraJson);
  }
  return book.local.progress.empty() || book.local.progress.size() > 4096 || !std::isfinite(book.local.percentage) ||
                 book.local.percentage < 0 || book.local.percentage > 1
             ? Result::InvalidResponse
             : Result::Success;
}
Result ProgressBooks::resolve(ProgressBook& book, const KOReaderProgress* primary, const KOReaderProgress* alternate,
                              ProgressComparison& comparison, bool& useAlternate, CrossPointPosition& destination) {
  std::unique_ptr<Epub> owner;
  if (!book.fixedPages) {
    const auto opened = openEpub(book, owner);
    if (opened != Result::Success) return opened;
  } else if (!book.pages) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("PROGRESS-SYNC", "OOM: Xtc metadata");
      return Result::LowMemory;
    }
    if (!xtc->load() || !xtc->getPageCount() || xtc->getPageCount() > INT32_MAX) return Result::StorageError;
    book.pages = xtc->getPageCount();
  }
  const auto epub = borrow(owner.get());
  const auto map = [&](const KOReaderProgress& remote) {
    return book.fixedPages ? mapFixed(book, remote) : mapRemote(epub, remote, renderer);
  };
  const auto first = primary ? map(*primary) : CrossPointPosition{};
  const auto second = alternate ? map(*alternate) : CrossPointPosition{};
  useAlternate = alternate && (!primary || selectRemoteRecord(first, primary->percentage, second,
                                                              alternate->percentage) == RemoteRecordChoice::Alternate);
  destination = useAlternate ? second : first;
  const auto& remote = useAlternate ? *alternate : *primary;
  comparison = book.savedSize ? compareProgress(book.position, book.local.percentage, destination, remote.percentage)
                              : ProgressComparison::RemoteAhead;
  if (comparison == ProgressComparison::RemoteAhead && !book.fixedPages && !destination.hasVisibleTextOffset &&
      !destination.hasMappedPage)
    return Result::InvalidResponse;
  return Result::Success;
}
Result ProgressBooks::unchanged(const ProgressBook& book) {
  uint8_t bytes[10];
  size_t size = 0;
  if (!readSaved(book, bytes, size)) return Result::StorageError;
  return size != book.savedSize || std::memcmp(bytes, book.saved, size) ? Result::LocalChanged : Result::Success;
}
Result ProgressBooks::apply(const ProgressBook& book, const CrossPointPosition& destination) {
  const auto check = unchanged(book);
  if (check != Result::Success) return check;
  if (!Storage.ensureDirectoryExists(book.cache.c_str())) return Result::StorageError;
  uint8_t bytes[10];
  size_t size = 0;
  if (book.fixedPages) {
    const uint32_t page = destination.pageNumber;
    for (size_t n = 0; n < 4; ++n) bytes[n] = uint8_t(page >> (8 * n));
    return ProgressFile::writeAtomic(book.cache, bytes, 4) ? Result::Success : Result::StorageError;
  }
  if (destination.spineIndex < 0 || destination.spineIndex > 65535 || destination.pageNumber < 0 ||
      destination.pageNumber > 65535 || destination.totalPages < 0 || destination.totalPages > 65535)
    return Result::InvalidResponse;
  const uint16_t fields[] = {uint16_t(destination.spineIndex), uint16_t(destination.pageNumber),
                             uint16_t(destination.totalPages)};
  for (size_t n = 0; n < 3; ++n) {
    bytes[n * 2] = fields[n] & 255;
    bytes[n * 2 + 1] = fields[n] >> 8;
  }
  size = 6;
  if (destination.hasVisibleTextOffset) {
    for (size_t n = 0; n < 4; ++n) bytes[6 + n] = uint8_t(destination.visibleTextOffset >> (8 * n));
    size = 10;
  }
  return ProgressFile::writeAtomic(book.cache, bytes, size) ? Result::Success : Result::StorageError;
}
}  // namespace readingstats::cloud
