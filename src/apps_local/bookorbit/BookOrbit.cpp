#include "BookOrbit.h"

#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <MD5Builder.h>
#include <ObfuscationUtils.h>
#include <SecureHttpClient.h>
#include <WiFi.h>

#ifndef SIMULATOR
#include <esp_wifi.h>
#endif

#include <algorithm>
#include <cstring>
#include <string_view>

#include "WifiCredentialStore.h"
#include "util/DeviceHostname.h"

namespace bookorbit {
namespace {
constexpr char API_PATH[] = "/api/v1/koreader";
constexpr char IDENTITY_PATH[] = "/api/v1/server-identity";
constexpr char HISTORY_DIR[] = "/.crosspoint/bookorbit";
constexpr uint32_t NETWORK_ATTEMPT_TIMEOUT_MS = 7500;
constexpr uint32_t JOIN_TIMEOUT_MS = 2 * NETWORK_ATTEMPT_TIMEOUT_MS;

bool hasWifiAddress() { return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0); }

bool startSavedNetwork(const WifiCredential& credential) {
  const auto* password = credential.password.empty() ? nullptr : credential.password.c_str();
#ifdef SIMULATOR
  return WiFi.begin(credential.ssid.c_str(), password) != WL_CONNECT_FAILED;
#else
  if (!WiFi.STA.begin() || !WiFi.STA.connect(credential.ssid.c_str(), password, 0, nullptr, false)) return false;

  // Apply the scan setting to this attempt, not the Arduino station defaults.
  wifi_config_t configuration{};
  static_assert(sizeof(configuration) < 256);
  if (esp_wifi_get_config(WIFI_IF_STA, &configuration) != ESP_OK) return false;
  configuration.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  configuration.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  return esp_wifi_set_config(WIFI_IF_STA, &configuration) == ESP_OK && esp_wifi_connect() == ESP_OK;
#endif
}

bool readPosition(JsonObjectConst source, Position& position) {
  const float fraction = source["fraction"] | -1.0f;
  if (!(fraction >= 0 && fraction <= 1)) return false;
  const char* anchor = source["anchor"] | "";
  if (std::strlen(anchor) > 1024) return false;
  position = {anchor, fraction};
  return true;
}

std::string readInstanceId(std::string_view id) {
  if (id.size() != 36) return {};
  std::string normalized(id);
  for (size_t index = 0; index < id.size(); ++index) {
    const char ch = id[index];
    if (index == 8 || index == 13 || index == 18 || index == 23) {
      if (ch != '-') return {};
    } else if (ch >= 'A' && ch <= 'F') {
      normalized[index] = ch - 'A' + 'a';
    } else if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
      return {};
    }
  }
  return normalized;
}
}  // namespace

void Store::setUrl(std::string value) {
  if (value == "https://" || value == "http://") value.clear();
  if (!value.empty() && value.find("://") == std::string::npos) value.insert(0, "https://");
  while (!value.empty() && value.back() == '/') value.pop_back();
  if (value.ends_with(API_PATH)) value.resize(value.size() - sizeof(API_PATH) + 1);
  if (value.ends_with("/api/v1")) value.resize(value.size() - 7);
  if ((!value.starts_with("https://") && !value.starts_with("http://")) ||
      value.find_first_of(" \r\n?#@", value.find("://") + 3) != std::string::npos) {
    value.clear();
  }
  if (url != value) authenticationVerified = false;
  url = std::move(value);
}

bool Store::setAuthenticated(const bool verified) {
  const bool wasVerified = authenticationVerified;
  authenticationVerified = verified && configured();
  return wasVerified != authenticationVerified;
}

void Store::toJson(JsonDocument& doc) const {
  doc["server"] = url;
  doc["username"] = username;
  doc["password_obf"] = obfuscation::obfuscateToBase64(password);
  doc["authenticated"] = authenticated();
  doc["downloadFolder"] = downloadFolder;
  doc["smartSync"] = smartSync;
  doc["syncOnSleep"] = syncOnSleep;
  doc["syncOnWake"] = syncOnWake;
}

bool Store::fromJson(JsonVariantConst doc) {
  if (!doc.is<JsonObjectConst>()) return false;
  setUrl(doc["server"] | "");
  username = doc["username"] | "";
  bool resave = false, valid = false;
  password = extractPassword(doc, resave, 64, valid);
  if (resave) requestResave();
  setAuthenticated(doc["authenticated"] | false);
  downloadFolder.clear();
  setDownloadFolder(doc["downloadFolder"] | "");
  smartSync = doc["smartSync"] | false;
  syncOnSleep = doc["syncOnSleep"] | false;
  syncOnWake = doc["syncOnWake"] | false;
  return true;
}

bool Store::setDownloadFolder(std::string value) {
  const auto first = value.find_first_not_of(" \t/");
  if (first == std::string::npos) {
    downloadFolder.clear();
    return true;
  }
  const auto last = value.find_last_not_of(" \t/");
  value = "/" + value.substr(first, last - first + 1);
  if (value.size() > 63 || value.find_first_of("\\:*?\"<>|") != std::string::npos) return false;
  for (unsigned char ch : value)
    if (ch < 32) return false;
  for (size_t start = 1; start < value.size();) {
    const size_t end = value.find('/', start);
    const auto part = std::string_view(value).substr(start, end - start);
    if (part.empty() || part.front() == '.' || part.back() == '.' || part.back() == ' ') return false;
    if (end == std::string::npos) break;
    start = end + 1;
  }
  downloadFolder = std::move(value);
  return true;
}

KOReaderEndpoint Store::endpoint(const std::function<bool()>& cancelled) const {
  MD5Builder md5;
  md5.begin();
  md5.add(password.c_str());
  md5.calculate();
  return {url + API_PATH, username, md5.toString().c_str(), devicehost::mdnsName(), cancelled};
}

ServerIdentity readServerIdentity(const Store& account, const std::function<bool()>& cancelled) {
  if (account.url.starts_with("https://")) {
    const auto heap = HalMemory::getDefaultHeap();
    if (heap.freeBytes < 35000 || heap.largestBlockBytes < 20000) {
      LOG_ERR("ORBIT", "Insufficient heap for server identity request");
      return {};
    }
  }
  freeink::SecureHttpClient http;
  http.setInsecure();
  http.setTimeout(3000);
  if (!http.begin(account.url + IDENTITY_PATH)) return {};
  http.addHeader("Accept", "application/json");
  constexpr size_t MAX_BODY_SIZE = 256;
  std::string body;
  const int status = http.GET(
      [&](const uint8_t* data, size_t size) {
        if (size > MAX_BODY_SIZE - body.size()) return false;
        body.append(reinterpret_cast<const char*>(data), size);
        return true;
      },
      cancelled);
  if (http.aborted()) return {};
  // Some older servers return a full proxy error page for an unknown route.
  if (status == 404 || status == 405) return {IdentityStatus::Legacy, {}};
  if (status != 200 || !http.responseComplete()) return {};
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body) || std::string_view(doc["product"] | "") != "bookorbit" ||
      !doc["protocolVersion"].is<int>() || doc["protocolVersion"].as<int>() != 1)
    return {};
  auto instanceId = readInstanceId(doc["instanceId"] | "");
  if (instanceId.empty()) return {};
  return {IdentityStatus::Verified, std::move(instanceId)};
}

bool readBaseline(const std::string& document, Baseline& baseline) {
  JsonDocument doc;
  const auto& account = Store::get();
  const std::string path = std::string(HISTORY_DIR) + "/" + document + ".json";
  if (!PersistableStoreBase::readDocFromFile(path.c_str(), doc) || account.username != (doc["username"] | "") ||
      !readPosition(doc["local"].as<JsonObjectConst>(), baseline.local) ||
      !readPosition(doc["remote"].as<JsonObjectConst>(), baseline.remote))
    return false;
  baseline.serverUrl = doc["server"] | "";
  baseline.instanceId = doc["instanceId"] | "";
  if (baseline.serverUrl.empty()) return false;
  baseline.landing.spine = doc["landing"]["spine"] | -1;
  baseline.landing.page = doc["landing"]["page"] | -1;
  if (doc["landing"]["offset"].is<uint32_t>()) baseline.landing.offset = doc["landing"]["offset"].as<uint32_t>();
  return true;
}

void writeBaseline(const std::string& document, const ServerIdentity& identity, const Position& local,
                   const Position& remote, const LandingPage& landing) {
  if (identity.status == IdentityStatus::Unavailable) return;
  JsonDocument doc;
  const auto& account = Store::get();
  doc["server"] = account.url;
  doc["username"] = account.username;
  if (identity.status == IdentityStatus::Verified) doc["instanceId"] = identity.instanceId;
  doc["local"]["anchor"] = local.anchor;
  doc["local"]["fraction"] = local.fraction;
  doc["remote"]["anchor"] = remote.anchor;
  doc["remote"]["fraction"] = remote.fraction;
  if (landing.spine >= 0) {
    doc["landing"]["spine"] = landing.spine;
    doc["landing"]["page"] = landing.page;
    if (landing.offset) doc["landing"]["offset"] = *landing.offset;
  }
  Storage.mkdir(HISTORY_DIR);
  const std::string path = std::string(HISTORY_DIR) + "/" + document + ".json";
  PersistableStoreBase::writeDocToFile(path.c_str(), doc, true);
}

KOReaderSyncClient::Error uploadAndConfirm(const KOReaderProgress& progress, const KOReaderEndpoint& endpoint,
                                           Position& confirmed, bool classifyMissingBook) {
  const auto uploaded = KOReaderSyncClient::updateProgress(progress, &endpoint, classifyMissingBook);
  if (uploaded != KOReaderSyncClient::OK) return uploaded;
  KOReaderProgress response{};
  const auto fetched = KOReaderSyncClient::getProgress(progress.document, response, &endpoint);
  if (fetched != KOReaderSyncClient::OK) return fetched;
  if (!(response.percentage >= 0 && response.percentage <= 1)) return KOReaderSyncClient::JSON_ERROR;
  confirmed = {std::move(response.progress), response.percentage};
  if (samePosition({progress.progress, progress.percentage}, confirmed) ||
      (response.deviceId == endpoint.deviceId && std::abs(progress.percentage - confirmed.fraction) <= 0.0001f))
    return KOReaderSyncClient::OK;
  return KOReaderSyncClient::SERVER_ERROR;
}

SyncConnection::~SyncConnection() {
  if (!started) return;
  WiFi.disconnect(false);
  WiFi.mode(WIFI_OFF);
}

bool SyncConnection::connect(const SyncCallbacks& callbacks, const Epub& book) {
  const auto& cancelled = callbacks.cancelled;
  if (cancelled()) return false;
  const auto initialMode = WiFi.getMode();
  const bool connected = hasWifiAddress();
  if (initialMode != WIFI_MODE_NULL && initialMode != WIFI_MODE_STA && !connected) return false;
  auto count = WIFI_STORE.getCredentialCount();
  if (!connected && initialMode == WIFI_MODE_NULL && !count) {
    // BookOrbit can be the first caller to need the saved networks after a cold wake.
    WIFI_STORE.loadIfNeeded();
    if (cancelled()) return false;
    count = WIFI_STORE.getCredentialCount();
    if (!count) {
      LOG_INF("ORBIT", "Sync skipped: no saved Wi-Fi networks");
      return false;
    }
  }
  callbacks.showProgress(callbacks.context, book);
  if (cancelled()) return false;
  if (hasWifiAddress()) return true;

  const auto joinStarted = millis();
  auto waitForAddress = [&](const uint32_t budget) {
    const auto attemptStarted = millis();
    while (!cancelled() && millis() - attemptStarted < budget) {
      if (hasWifiAddress()) return true;
      delay(30);
    }
    return !cancelled() && hasWifiAddress();
  };

  const auto mode = WiFi.getMode();
  if (mode != WIFI_MODE_NULL) {
    // Give a pending station join time to finish without taking over its radio.
    if (mode == WIFI_MODE_STA && waitForAddress(NETWORK_ATTEMPT_TIMEOUT_MS)) return true;
    if (!cancelled()) LOG_INF("ORBIT", "Sync skipped: active Wi-Fi has no address");
    return false;
  }
  if (!count) return false;

  WiFi.persistent(false);
  const auto preferred = WIFI_STORE.findCredential(WIFI_STORE.getLastConnectedSsid());
  if (!WiFi.mode(WIFI_STA)) {
    LOG_ERR("ORBIT", "Could not start Wi-Fi");
    return false;
  }
  started = true;
  bool attempted = false;
  auto join = [&](const WifiCredential& credential) {
    if (cancelled()) return false;
    if (attempted) WiFi.disconnect(false);
    attempted = true;
    if (cancelled() || !startSavedNetwork(credential)) return false;
    const auto elapsed = millis() - joinStarted;
    if (elapsed >= JOIN_TIMEOUT_MS) return false;
    const auto budget = std::min<uint32_t>(NETWORK_ATTEMPT_TIMEOUT_MS, JOIN_TIMEOUT_MS - elapsed);
    if (!waitForAddress(budget)) return false;
    LOG_INF("ORBIT", "Joined Wi-Fi in %lu ms", millis() - joinStarted);
    return true;
  };

  if (preferred && join(*preferred)) return true;
  for (size_t i = 0; i < count && !cancelled() && millis() - joinStarted < JOIN_TIMEOUT_MS; ++i) {
    const auto credential = WIFI_STORE.getCredentialAt(i);
    if (credential && (!preferred || credential->ssid != preferred->ssid) && join(*credential)) return true;
  }
  if (!cancelled()) LOG_INF("ORBIT", "Could not join saved Wi-Fi (status %d)", static_cast<int>(WiFi.status()));
  return false;
}
}  // namespace bookorbit
