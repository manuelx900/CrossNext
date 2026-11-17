#pragma once

namespace radiofailure {
// Main-loop failure paths only. No credentials or network identifiers are recorded.
void write(const char* operation, int code, int detail = 0);
}  // namespace radiofailure
