#pragma once
#include <string>
enum class KOReaderServerType { CROSSPOINT, KOSYNC, OTHER };
enum class DocumentMatchMethod { FILENAME, BINARY };
struct TestCredentials {
  std::string url, user = "test-user", key = "test-hash";
  bool present = true;
  KOReaderServerType profile = KOReaderServerType::CROSSPOINT;
  DocumentMatchMethod match = DocumentMatchMethod::FILENAME;
  bool sendMetadata = true;
  DocumentMatchMethod getMatchMethod() const { return match; }
  bool getSendMetadata() const { return sendMetadata; }
  bool hasCredentials() const { return present; }
  KOReaderServerType getServerType() const { return profile; }
  const std::string& getBaseUrl() const { return url; }
  const std::string& getUsername() const { return user; }
  const std::string& getMd5Password() const { return key; }
};
inline TestCredentials credentials;
#define KOREADER_STORE credentials
