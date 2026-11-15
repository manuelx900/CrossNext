#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>

class ChapterParser {
 public:
  enum class ParseStatus { More, Done, Error };
  virtual ~ChapterParser() = default;
  virtual void setCaptureFootnotes(bool capture) = 0;
  virtual void setForceBold(bool bold) = 0;
  virtual void setTextSpacing(int8_t character, uint8_t wordPercent) = 0;
  virtual void setParagraphIndentSpaces(uint8_t spaces) = 0;
  virtual bool beginParse() = 0;
  virtual ParseStatus parseStep() = 0;
  virtual bool finishParse() = 0;
  virtual void abortParse() = 0;
  virtual const std::deque<std::pair<std::string, uint16_t>>& getAnchors() const = 0;
  virtual bool hadEventCacheReadError() const = 0;
  virtual size_t parseBytesConsumed() = 0;
  virtual size_t parseTotalBytes() = 0;
};
