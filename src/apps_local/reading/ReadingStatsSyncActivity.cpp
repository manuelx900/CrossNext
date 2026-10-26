#include "ReadingStatsSyncActivity.h"

#include <I18n.h>
#include <KOReaderCredentialStore.h>
#include <Memory.h>
#include <TrustedTime.h>
#include <WiFi.h>

#include "../ui/PixelMenuTheme.h"
#include "DevMode.h"
#include "ReadingStatsStore.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/settings/KOReaderSettingsActivity.h"
#include "components/UITheme.h"
#include "network/WifiPowerSaveGuard.h"

using readingstats::cloud::Result;
namespace fui = freeink::ui;
void ReadingStatsSyncActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  requestUpdate();
}
void ReadingStatsSyncActivity::onExit() {
  if (importer) {
    importer->cancel();
    importer.reset();
  }
  if (uploader) {
    uploader->cancel();
    uploader.reset();
  }
  client.close();
  Activity::onExit();
  if (ownsWifi && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    silentRestart();
  }
}
const char* ReadingStatsSyncActivity::message() const {
  switch (result) {
    case Result::Idle:
      return tr(STR_READING_SYNC_HELP);
    case Result::Busy:
      return tr(STR_READING_SYNC_HELP);
    case Result::Success:
      return tr(STR_READING_SYNC_SAFE);
    case Result::NoCredentials:
      return tr(STR_READING_SYNC_NO_CREDS);
    case Result::LocalChanged:
      return tr(STR_READING_SYNC_LOCAL_CHANGED);
    case Result::NewerCache:
      return tr(STR_READING_SYNC_NEWER);
    case Result::Unsupported:
      return tr(STR_READING_SYNC_UNSUPPORTED);
    case Result::InvalidResponse:
      return tr(STR_READING_SYNC_INVALID);
    case Result::LimitExceeded:
      return tr(STR_READING_SYNC_LIMIT);
    case Result::AuthFailed:
      return tr(STR_AUTH_FAILED);
    case Result::LowMemory:
      return tr(STR_MEMORY_ERROR);
    case Result::StorageError:
      return tr(STR_READING_STORAGE_ERROR);
    case Result::Cancelled:
      return tr(STR_CANCEL);
    case Result::Empty:
    case Result::NetworkError:
      return tr(STR_READING_SYNC_FAILED);
  }
  return tr(STR_READING_SYNC_FAILED);
}
void ReadingStatsSyncActivity::done() {
  if (result == Result::Busy) {
    cancelFlag = true;
    return;
  }
  ActivityResult value;
  value.isCancelled = result != Result::Success;
  setResult(std::move(value));
  finish();
}
void ReadingStatsSyncActivity::start() {
  if (result == Result::Busy) return;
  client.close();
  cancelFlag = false;
  if (!KOREADER_STORE.hasCredentials()) {
    RenderLock lock;
    result = Result::NoCredentials;
    ready = false;
    requestUpdate();
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    beginDownload();
    return;
  }
  auto wifi = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!wifi) {
    RenderLock lock;
    result = Result::LowMemory;
    requestUpdate();
    return;
  }
  ownsWifi = !devmode::holdsRadio();
  {
    RenderLock lock;
    result = Result::Busy;
    waitingWifi = true;
    ready = false;
  }
  startActivityForResult(std::move(wifi), [this](const ActivityResult& connection) {
    waitingWifi = false;
    if (connection.isCancelled || WiFi.status() != WL_CONNECTED) {
      RenderLock lock;
      result = Result::Cancelled;
      ready = false;
      requestUpdate();
    } else
      beginDownload();
  });
}
void ReadingStatsSyncActivity::beginDownload() {
  uploader = makeUniqueNoThrow<readingstats::cloud::Uploader>();
  if (!uploader) {
    LOG_ERR("STATS-SYNC", "OOM: upload workspace");
    RenderLock lock;
    result = Result::LowMemory;
    requestUpdate();
    return;
  }
  const auto next = uploader->begin(readingstats::today(), client.deviceTag(), client.accountId(),
                                    &readingstats::cloud::Client::exchange, &client);
  downloading = false;
  RenderLock lock;
  result = next;
  books = 0;
  ready = false;
  requestUpdate();
}
bool ReadingStatsSyncActivity::cancelRequest(void* context) {
  auto& self = *static_cast<ReadingStatsSyncActivity*>(context);
  self.mappedInput.update();
  if (self.mappedInput.isPressed(MappedInputManager::Button::Back) ||
      self.mappedInput.wasReleased(MappedInputManager::Button::Back) || self.mappedInput.wasBackGesture())
    self.cancelFlag = true;
  int x = 0, y = 0;
  if (self.mappedInput.wasScreenTapped(x, y)) {
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = x;
    input.touchY = y;
    if (self.interactions.route(input).action == readingui::ImportDone) self.cancelFlag = true;
  }
  return self.cancelFlag;
}
void ReadingStatsSyncActivity::action(fui::ActionId selected) {
  switch (selected) {
    case readingui::ImportStart:
      start();
      return;
    case readingui::ImportDone:
      done();
      return;
    case readingui::ImportSetup: {
      if (result == Result::Busy) return;
      auto settings = makeUniqueNoThrow<KOReaderSettingsActivity>(renderer, mappedInput);
      if (!settings) {
        LOG_ERR("STATS-SYNC", "OOM: account settings");
        return;
      }
      startActivityForResult(std::move(settings), [this](const ActivityResult&) {
        RenderLock lock;
        result = Result::Idle;
        ready = false;
        requestUpdate();
      });
      return;
    }
    default:
      return;
  }
}
void ReadingStatsSyncActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    done();
    return;
  }
  if (result == Result::Busy && !waitingWifi) {
    if (!ready) requestUpdateAndWait();
    if (cancelFlag) {
      if (importer) importer->cancel();
      if (uploader) uploader->cancel();
      importer.reset();
      uploader.reset();
      client.close();
      RenderLock lock;
      result = Result::Cancelled;
      ready = false;
      requestUpdate();
      return;
    }
    WifiPowerSaveGuard power;
    auto next = downloading ? importer->step() : uploader->step();
    if (!downloading && next == Result::Success) {
      importer = makeUniqueNoThrow<readingstats::cloud::Importer>();
      if (importer) {
        importer->setDevice(uploader->deviceId());
        next = importer->begin(readingstats::today(), trustedtime::trustedNow(), client.accountId(),
                               &readingstats::cloud::Client::fetch, &client, &uploader->data());
        downloading = true;
      } else {
        LOG_ERR("STATS-SYNC", "OOM: download workspace");
        next = Result::LowMemory;
      }
    }
    const auto count = downloading ? importer->booksRead() : uploader->data().count();
    if (next != Result::Busy) {
      client.close();
      importer.reset();
      uploader.reset();
    }
    RenderLock lock;
    if (next != result || count != books) {
      result = next;
      books = count;
      ready = false;
      requestUpdate();
    }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    action(result == Result::Success ? readingui::ImportDone : readingui::ImportStart);
    return;
  }
  int x = 0, y = 0;
  if (!ready || !mappedInput.wasScreenTapped(x, y)) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = x;
  input.touchY = y;
  action(interactions.route(input).action);
}
void ReadingStatsSyncActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const fui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions);
  toybox::Screen screen(frame);
  readingui::ImportModel model;
  model.state = result == Result::Busy      ? readingui::ImportState::Importing
                : result == Result::Success ? readingui::ImportState::Done
                : result == Result::Idle    ? readingui::ImportState::Ready
                                            : readingui::ImportState::Error;
  model.message = message();
  model.books = books;
  model.setup = !KOREADER_STORE.hasCredentials() || result == Result::AuthFailed;
  GUI.applyHeaderStatus(renderer, work.status);
  model.battery = work.status.status.battery.percent;
  readingui::buildImport(screen, model, work);
  ready = true;
  toybox::reportOverflow(interactions, "ReadingStatsImport");
  renderer.displayBuffer();
}
