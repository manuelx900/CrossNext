#pragma once

#include <string>

namespace vocabulary {
// Shrinks HTML in place: keep meanings/examples, omit pronunciation metadata,
// and make dictionary block boundaries explicit to the shared chapter parser.
void prepareDictionaryHtml(std::string& html);
}  // namespace vocabulary
