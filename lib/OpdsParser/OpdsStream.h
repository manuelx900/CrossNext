#pragma once

#include <OpdsFeedParser.h>
#include <Stream.h>

class OpdsParserStream : public Stream {
 public:
  explicit OpdsParserStream(OpdsFeedParser& parser);

  // That functions are not implemented for that stream
  int available() override;
  int peek() override;
  int read() override;

  virtual size_t write(uint8_t c) override;
  virtual size_t write(const uint8_t* buffer, size_t size) override;

  ~OpdsParserStream() override;

 private:
  OpdsFeedParser& parser;
};
