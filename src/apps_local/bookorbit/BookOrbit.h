#pragma once

#include <KOReaderSyncClient.h>
#include <PersistableStore.h>

#include <cstdint>
#include <functional>
#include <string>

#include "BookOrbitPolicy.h"

class Epub;

namespace bookorbit {

enum class SyncTrigger : uint8_t { Sleep, Wake };

struct SyncCallbacks {
  std::function<bool()> cancelled;
  void (*showProgress)(void*, const Epub&);
  void* context;
};

class Store final : public PersistableStore<Store> {
 public:
  static const char* getFilePath() { return "/.crosspoint/bookorbit.json"; }
  static Store& get() { return getLoadedInstance(); }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  bool configured() const { return !url.empty() && !username.empty() && !password.empty(); }
  bool authenticated() const { return authenticationVerified && configured(); }
  bool setAuthenticated(bool verified);
  KOReaderEndpoint endpoint(const std::function<bool()>& cancelled = {}) const;
  void setUrl(std::string value);
  bool setDownloadFolder(std::string value);
  std::string url;
  std::string username;
  std::string password;
  std::string downloadFolder;
  bool smartSync = false;
  bool syncOnSleep = false;
  bool syncOnWake = false;

 private:
  bool authenticationVerified = false;
};

ServerIdentity readServerIdentity(const Store& account, const std::function<bool()>& cancelled = {});
bool readBaseline(const std::string& document, Baseline& baseline);
void writeBaseline(const std::string& document, const ServerIdentity& identity, const Position& local,
                   const Position& remote, const LandingPage& landing = {});
KOReaderSyncClient::Error uploadAndConfirm(const KOReaderProgress& progress, const KOReaderEndpoint& endpoint,
                                           Position& confirmed, bool classifyMissingBook = false);

class SyncConnection {
 public:
  ~SyncConnection();
  bool connect(const SyncCallbacks& callbacks, const Epub& book);

 private:
  bool started = false;
};

}  // namespace bookorbit
