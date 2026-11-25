#pragma once

#include <string>
#include <string_view>

namespace homecover {
// Home's render task owns this memo. Without a thumbnail path, only consult RAM.
bool knownMissingCover(const std::string& bookPath, std::string_view thumbnailPath = {});
}  // namespace homecover
