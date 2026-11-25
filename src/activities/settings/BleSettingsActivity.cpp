#include "BleSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <cstring>
#include <iterator>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "apps_local/Shelf.h"
#include "apps_local/network/RadioFailureReport.h"
#include "apps_local/ui/BluetoothConfirmation.h"
#include "apps_local/ui/BluetoothState.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "apps_local/ui/RemoteBindingActions.h"
#include "apps_local/ui/SettingsMenuStyle.h"
#include "apps_local/ui/SettingsOptionIcons.h"

namespace fui = freeink::ui;
namespace {
enum class Menu : int16_t { Enable, Scan, Paired, Configure };
constexpr uint32_t SCAN_MS = 15000;
struct ExtraChoice {
  bleturner::Action action;
  StrId label;
  StrId help = StrId::STR_BUTTON_READER_ONLY;
};
constexpr ExtraChoice EXTRA[] = {
    {bleturner::Action::PrevPage, StrId::STR_PREV_PAGE},
    {bleturner::Action::NextChapter, StrId::STR_BT_NEXT_CHAPTER},
    {bleturner::Action::PrevChapter, StrId::STR_BT_PREV_CHAPTER},
    {bleturner::Action::SaveQuote, StrId::STR_SAVE_CLIPPING},
    {bleturner::Action::SideLeft, StrId::STR_SIDE_LEFT_ACTION, StrId::STR_BT_PHYSICAL_BUTTON_HELP},
    {bleturner::Action::SideRight, StrId::STR_SIDE_RIGHT_ACTION, StrId::STR_BT_PHYSICAL_BUTTON_HELP},
    {bleturner::Action::Power, StrId::STR_POWER_SHORT_ACTION, StrId::STR_BT_PHYSICAL_BUTTON_HELP}};
struct Help {
  StrId title, body;
};
constexpr Help HELP[] = {{StrId::STR_BT_COMPATIBILITY_TITLE, StrId::STR_BT_COMPATIBILITY_HELP},
                         {StrId::STR_BT_HELP_STATE_TITLE, StrId::STR_BT_HELP_CONNECTION},
                         {StrId::STR_BT_HELP_HOME_TITLE, StrId::STR_BT_HELP_HOME},
                         {StrId::STR_BT_HELP_PAUSE_TITLE, StrId::STR_BT_HELP_PAUSE},
                         {StrId::STR_BT_HELP_IDLE_TITLE, StrId::STR_BT_HELP_IDLE},
                         {StrId::STR_BT_CONFIGURE_BUTTONS, StrId::STR_BT_HELP_BUTTONS},
                         {StrId::STR_BT_HELP_QUICK_TITLE, StrId::STR_BT_HELP_QUICK}};
const shelf::Item* appAt(int index) {
  for (int n = 0; n < shelf::folderCount(); ++n) {
    const auto& folder = shelf::folders()[n];
    if (index < folder.count) return index < 0 ? nullptr : &folder.items[index];
    index -= folder.count;
  }
  return nullptr;
}
int appCount() {
  int count = 0;
  for (int i = 0; i < shelf::folderCount(); ++i) count += shelf::folders()[i].count;
  return count;
}
int buttonNumber(const bleturner::RemoteTable& table, int index) {
  int number = 0;
  for (int i = 0; i <= index; ++i) {
    bool first = true;
    const auto code = table.bindings[i] & bleturner::kCodeMask;
    for (int j = 0; j < i; ++j)
      if ((table.bindings[j] & bleturner::kCodeMask) == code) first = false;
    if (first) ++number;
    if (code == (table.bindings[index] & bleturner::kCodeMask)) return number;
  }
  return number;
}
constexpr auto SEARCH_BITS = [] {
  std::array<uint8_t, 128> bits{};
  bits.fill(0xFF);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x) {
      const int d = (x - 13) * (x - 13) + (y - 13) * (y - 13);
      if ((d >= 49 && d <= 81) || (x >= 19 && y >= 19 && x <= 28 && y <= 28 && x - y >= -1 && x - y <= 1))
        bits[y * 4 + x / 8] &= uint8_t(~(0x80u >> (x % 8)));
    }
  return bits;
}();
constexpr fui::BitmapRef SEARCH_ICON{SEARCH_BITS.data(), 32, 32, fui::BitmapFormat::Mask1, false};
}  // namespace
void BleSettingsActivity::onEnter() {
  labels.reserve(28);
  rows.reserve(25);
  useCompactSettingsRows();
  UiListActivity::onEnter();
  app.on(
      ACTION_USER,
      [](const fui::ActionEvent&, void* context) { static_cast<BleSettingsActivity*>(context)->showView(View::Help); },
      this);
  app.on(
      ACTION_USER + 1,
      [](const fui::ActionEvent& event, void* context) {
        static_cast<BleSettingsActivity*>(context)->confirmRemoval(event.value);
      },
      this);
  radioVisit = activityManager.visit();
  if (SETTINGS.ble.enabled && !bleturner::switchOn())
    radiofailure::write("bluetooth-start", int(bleturner::lastStartRefusal()));
  refresh();
  requestUpdate();
}
void BleSettingsActivity::loop() {
  if (radioVisit != activityManager.visit()) {
    radioVisit = activityManager.visit();
    if (SETTINGS.ble.enabled && !bleturner::switchOn())
      radiofailure::write("bluetooth-start", int(bleturner::lastStartRefusal()));
  }
  UiListActivity::loop();
  if (millis() - lastPollMs < 250) return;
  lastPollMs = millis();
  uint32_t passkey;
  const bool passkeyChanged = bleturner::takePairingPasskey(passkey);
  if (passkeyChanged) {
    RenderLock lock(*this);
    snprintf(pairingCode, sizeof(pairingCode), tr(STR_BT_PAIRING_CODE), static_cast<unsigned long>(passkey));
    pairingCodeVisible = true;
  }
  if (SETTINGS.ble.enabled) {
    bleturner::service();
    if (bleturner::hasPendingRemoteSelectionSave() && SETTINGS.saveToFile(true)) bleturner::remoteSelectionSaved();
  }
  stepCapture();
  if (passkeyChanged || signature() != shownSignature) {
    refresh();
    requestUpdate();
  }
}
int BleSettingsActivity::listCount() const {
  if (view == View::Actions) return remoteshortcuts::COUNT + int(std::size(EXTRA)) + 2;
  if (view == View::Apps) return appCount() + 1;
  return int(rows.size());
}
const char* BleSettingsActivity::headerTitle() const {
  switch (view) {
    case View::Menu:
      return tr(STR_BT_SHORT_NAME);
    case View::Scan:
      return tr(STR_BT_SCAN);
    case View::Paired:
      return tr(STR_BT_PAIRED_REMOTES);
    case View::Bindings:
    case View::Capture:
    case View::Actions:
    case View::Apps:
      return tr(STR_BT_BUTTONS_TITLE);
    case View::Help:
      return tr(STR_BT_HELP_TITLE);
  }
  return tr(STR_BT_SHORT_NAME);
}
uint32_t BleSettingsActivity::signature() const {
  const auto st = bleturner::status();
  uint32_t value = uint32_t(bluetoothui::current(SETTINGS.ble)) | uint32_t(bleturner::bondCount()) << 8 |
                   uint32_t(bleturner::foundCount()) << 16 | (pairingCodeVisible ? 1u << 24 : 0u) |
                   (st.running ? 1u << 25 : 0u);
  if (st.connected) {
    const auto peer = bleturner::linked();
    for (size_t i = 0; i < 17 && peer.addr[i]; ++i) value = (value ^ uint8_t(peer.addr[i])) * 16777619u;
    for (size_t i = 0; i < 31 && peer.name[i]; ++i) value = (value ^ uint8_t(peer.name[i])) * 16777619u;
  }
  return value;
}
void BleSettingsActivity::showView(View target) {
  {
    RenderLock lock(*this);
    view = target;
    nav.reset();
    if (view == View::Actions || view == View::Apps) nav.selected = 1;
  }
  refresh();
  requestUpdate();
}
void BleSettingsActivity::onBackButton() {
  switch (view) {
    case View::Menu:
      finish();
      return;
    case View::Scan:
      bleturner::scan(0);
      showView(View::Menu);
      return;
    case View::Capture:
    case View::Actions:
    case View::Apps:
      note = nullptr;
      showView(View::Bindings);
      return;
    case View::Bindings:
    case View::Help:
    case View::Paired:
      showView(View::Menu);
      return;
  }
}
const char* BleSettingsActivity::actionLabel(const bleturner::RemoteTable& table, bleturner::Action action) const {
  if (const auto* choice = remotebindings::choice(table, action)) {
    if (choice->action == uint8_t(SideButtonAction::OpenShortcut))
      return shelf::label(table.shortcuts[action == bleturner::Action::Shortcut1 ? 0 : 1].title);
    return I18N.get(choice->label);
  }
  for (const auto& item : EXTRA)
    if (item.action == action) return I18N.get(item.label);
  return tr(STR_NOT_SET);
}
void BleSettingsActivity::refresh() {
  RenderLock lock(*this);
  const auto st = bleturner::status();
  if (st.connected || !SETTINGS.ble.enabled) pairingCodeVisible = false;
  auto state = bluetoothui::current(SETTINGS.ble);
  char failure[48];
  if (bleturner::takeConnectFailure(failure, sizeof(failure)) && state == bluetoothui::State::Ready)
    state = bluetoothui::State::Failed;
  if (pairingCodeVisible)
    snprintf(status, sizeof(status), "%s", pairingCode);
  else if (state == bluetoothui::State::Connected) {
    const auto peer = bleturner::linked();
    snprintf(status, sizeof(status), "%s: %s", I18N.get(bluetoothui::label(state)),
             peer.name[0] ? peer.name : peer.addr);
  } else
    snprintf(status, sizeof(status), "%s", I18N.get(bluetoothui::label(state)));
  shownSignature = signature();
  labels.clear();
  const auto found = st.running ? bleturner::foundCount() : 0;
  const auto bonds = st.running ? bleturner::bondCount() : 0;
  for (int i = 0; i < found; ++i) {
    const auto peer = bleturner::found(i);
    labels.emplace_back(peer.name[0] ? peer.name : peer.addr);
  }
  for (int i = 0; i < bonds; ++i) {
    const auto peer = bleturner::bond(i);
    labels.emplace_back(peer.name[0] ? peer.name : peer.addr);
  }
  rows.clear();
  const auto add = [this](const char* label, int code, bool enabled = true) -> fui::ListItem& {
    fui::ListItem row;
    row.label = label;
    row.actionValue = int16_t(code);
    row.enabled = enabled;
    rows.push_back(row);
    return rows.back();
  };
  switch (view) {
    case View::Menu: {
      auto& enabled = add(tr(STR_BT_ALLOW_CONNECTION), int(Menu::Enable), st.compiledIn);
      enabled.icon = settingsicons::Bluetooth;
      enabled.toggle = true;
      enabled.toggleChecked = SETTINGS.ble.enabled != 0;
      enabled.subtitle = status;
      add(tr(STR_BT_SCAN), int(Menu::Scan), st.running && !st.connecting).icon = SEARCH_ICON;
      add(tr(STR_BT_PAIRED_REMOTES), int(Menu::Paired), st.running).icon = settingsrowart::Display;
      auto& config = add(tr(STR_BT_CONFIGURE_BUTTONS), int(Menu::Configure), st.connected);
      config.icon = settingsrowart::Gear;
      config.subtitle = st.connected ? tr(STR_BT_CONFIGURE_HINT) : tr(STR_BT_CONNECT_REMOTE);

      break;
    }
    case View::Help:
      for (const auto& item : HELP) add(I18N.get(item.title), 0).subtitle = I18N.get(item.body);
      break;
    case View::Scan:
      add(st.scanning ? tr(STR_BT_STOP_SCAN) : tr(STR_BT_SCAN), -1, st.running).labelBold = true;
      for (int i = 0; i < found; ++i) add(labels[i].c_str(), i);
      if (found == 0) add(st.scanning ? tr(STR_SCANNING) : tr(STR_BT_NO_DEVICES), -1, false);
      break;
    case View::Paired:
      for (int i = 0; i < bonds; ++i) add(labels[found + i].c_str(), i).value = tr(STR_FORGET_BUTTON);
      if (bonds == 0) add(tr(STR_BT_NO_REMOTES), -1, false);
      break;
    case View::Bindings:
    case View::Capture: {
      auto& capture =
          add(view == View::Capture ? tr(STR_BT_CAPTURE_WAIT) : tr(STR_BT_CAPTURE_BUTTON), -1, st.connected);
      capture.subtitle = note ? note : tr(STR_BT_CAPTURE_HINT);
      const auto peer = bleturner::linked();
      const auto* table = bleturner::tableFor(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
      if (table)
        for (uint8_t i = 0; i < table->count; ++i) {
          snprintf(buttonLabels[i], sizeof(buttonLabels[i]), tr(STR_BT_BUTTON_NUMBER),
                   unsigned(buttonNumber(*table, i)));
          auto& item = add(buttonLabels[i], i, st.connected);
          item.subtitle = actionLabel(*table, bleturner::actionOf(table->bindings[i]));
          item.value = (table->bindings[i] & bleturner::kHoldBit) ? tr(STR_BT_HOLD) : tr(STR_BT_TAP);
        }
      if (!st.connected) capture.subtitle = status;
      break;
    }
    case View::Actions:
    case View::Apps:
      break;
  }
  if (nav.selected >= listCount()) nav.selected = std::max(0, listCount() - 1);
}
void BleSettingsActivity::provideRow(uint16_t index, fui::ListItem& row) {
  if ((view == View::Actions || view == View::Apps) && index == 0) {
    row.label = note ? note : tr(STR_BT_CHOOSE_ACTION);
    row.subtitle = hold ? tr(STR_BT_HOLD) : tr(STR_BT_TAP);
    row.actionValue = 0;
    return;
  }
  const auto original = index;
  if (view == View::Actions || view == View::Apps) --index;
  if (view == View::Actions) {
    if (index < remoteshortcuts::COUNT) {
      const auto& choice = remoteshortcuts::CHOICES[index];
      row.label = I18N.get(choice.label);
      row.subtitle = I18N.get(choice.help);
    } else if (index < remoteshortcuts::COUNT + std::size(EXTRA)) {
      row.label = I18N.get(EXTRA[index - remoteshortcuts::COUNT].label);
      row.subtitle = I18N.get(EXTRA[index - remoteshortcuts::COUNT].help);
    } else {
      row.label = tr(STR_BT_RESTORE_BUTTON);
      row.subtitle = tr(STR_BT_RESTORE_HELP);
    }
  } else if (view == View::Apps) {
    if (const auto* item = appAt(index)) row.label = shelf::label(item->title);
  } else if (index < rows.size())
    row = rows[index];
  row.actionValue = int16_t(original);
}
void BleSettingsActivity::activateIndex(int index) {
  if (index < 0 || index >= listCount()) return;
  if (view != View::Actions && view != View::Apps && !rows[index].enabled) return;
  app.clearTapFlash();
  switch (view) {
    case View::Menu:
      switch (Menu(rows[index].actionValue)) {
        case Menu::Configure:
          startCapture();
          return;
        case Menu::Enable:
          SETTINGS.ble.enabled = SETTINGS.ble.enabled ? 0 : 1;
          if (SETTINGS.ble.enabled) {
            if (!bleturner::switchOn()) radiofailure::write("bluetooth-start", int(bleturner::lastStartRefusal()));
          } else
            bleturner::switchOff();
          SETTINGS.saveToFile();
          break;
        case Menu::Scan:
          bleturner::scan(SCAN_MS);
          showView(View::Scan);
          return;
        case Menu::Paired:
          showView(View::Paired);
          return;
      }
      break;
    case View::Scan:
      if (rows[index].actionValue < 0)
        bleturner::scan(bleturner::status().scanning ? 0 : SCAN_MS);
      else {
        const auto peer = bleturner::found(rows[index].actionValue);
        char address[18], name[32];
        snprintf(address, sizeof(address), "%s", peer.addr);
        snprintf(name, sizeof(name), "%s", peer.name);
        bleturner::scan(0);
        bleturner::pair(address, name);
        showView(View::Menu);
        return;
      }
      break;
    case View::Paired: {
      const auto peer = bleturner::bond(rows[index].actionValue);
      std::array<char, 18> address{};
      snprintf(address.data(), address.size(), "%s", peer.addr);
      auto confirm =
          makeUniqueNoThrow<BluetoothConfirmation>(renderer, mappedInput, tr(STR_BT_SHORT_NAME),
                                                   tr(STR_BT_FORGET_CONFIRM), tr(STR_CANCEL), tr(STR_FORGET_BUTTON));
      if (!confirm) {
        LOG_ERR("BLE", "OOM: forget confirmation");
        return;
      }
      startActivityForResult(std::move(confirm), [this, address](const ActivityResult& result) {
        if (!result.isCancelled && bleturner::forget(address.data())) SETTINGS.saveToFile();
        refresh();
        requestUpdate();
      });
      return;
      break;
    }
    case View::Bindings:
    case View::Capture: {
      const int slot = rows[index].actionValue;
      if (slot < 0) {
        startCapture();
        return;
      }
      const auto peer = bleturner::linked();
      const auto* table = bleturner::tableFor(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
      if (!table || slot >= table->count) return;
      {
        RenderLock lock(*this);
        snprintf(learnAddr, sizeof(learnAddr), "%s", peer.addr);
        learnCode = table->bindings[slot] & bleturner::kCodeMask;
        hold = (table->bindings[slot] & bleturner::kHoldBit) != 0;
        note = nullptr;
      }
      showView(View::Actions);
      return;
    }
    case View::Actions:
      if (index == 0) return;
      --index;
      if (index < remoteshortcuts::COUNT) {
        const auto host = remoteshortcuts::CHOICES[index].action;
        if (host == uint8_t(SideButtonAction::OpenShortcut)) {
          showView(View::Apps);
          return;
        }
        saveBinding(remotebindings::directAction(host));
        return;
      }
      if (index < remoteshortcuts::COUNT + int(std::size(EXTRA)))
        saveBinding(EXTRA[index - remoteshortcuts::COUNT].action);
      else {
        auto confirm = makeUniqueNoThrow<BluetoothConfirmation>(renderer, mappedInput, tr(STR_BT_RESTORE_BUTTON),
                                                                tr(STR_BT_RESTORE_CONFIRM));
        if (!confirm) {
          LOG_ERR("BLE", "OOM: restore confirmation");
          return;
        }
        startActivityForResult(std::move(confirm), [this](const ActivityResult& result) {
          if (!result.isCancelled) restoreButton();
        });
      }
      return;
    case View::Apps:
      if (index > 0)
        if (const auto* item = appAt(index - 1)) saveBinding(bleturner::Action::None, item->title);
      return;
    case View::Help:
      return;
  }
  refresh();
  requestUpdate();
}
void BleSettingsActivity::startCapture() {
  const auto peer = bleturner::linked();
  if (!bleturner::status().connected) return;
  {
    RenderLock lock(*this);
    bleturner::Event event;
    while (bleturner::pollEvent(event)) {
    }
    snprintf(learnAddr, sizeof(learnAddr), "%s", peer.addr);
    learnCode = 0;
    hold = false;
    note = nullptr;
    learnSinceMs = millis();
    view = View::Capture;
    nav.reset();
  }
  refresh();
  requestUpdate();
}
void BleSettingsActivity::stepCapture() {
  if (view != View::Capture && view != View::Actions && view != View::Apps) return;
  const auto peer = bleturner::linked();
  if (!bleturner::status().connected || std::strcmp(peer.addr, learnAddr)) {
    note = tr(STR_BT_CAPTURE_DISCONNECTED);
    showView(View::Bindings);
    return;
  }
  if (view != View::Capture) {
    bleturner::Event event;
    while (bleturner::pollEvent(event)) {
    }
    return;
  }
  bool captured = false, timedOut = false;
  {
    RenderLock lock(*this);
    bleturner::Event event;
    while (!captured && bleturner::pollEvent(event)) {
      if (event.pressed) {
        learnCode = event.code;
        learnPressMs = event.atMs;
        learnSinceMs = millis();
      } else if (learnCode && event.code == learnCode) {
        if (event.wasRest)
          learnCode = 0;
        else {
          hold = event.atMs - learnPressMs >= bleturner::kHoldMs;
          captured = true;
        }
      }
    }
    if (!captured && millis() - learnSinceMs >= bleturner::kWaitMs) {
      if (learnCode)
        captured = true;
      else
        timedOut = true;
    }
  }
  if (captured) {
    note = nullptr;
    showView(View::Actions);
  } else if (timedOut) {
    note = tr(STR_BT_NO_KEY);
    showView(View::Bindings);
  }
}
bool BleSettingsActivity::saveBinding(bleturner::Action action, const char* title) {
  const auto peer = bleturner::linked();
  if (!bleturner::status().connected || std::strcmp(peer.addr, learnAddr)) {
    note = tr(STR_BT_CAPTURE_DISCONNECTED);
    showView(View::Bindings);
    return false;
  }
  bool saved = false;
  {
    RenderLock lock(*this);
    const auto oldCount = SETTINGS.ble.remoteCount;
    auto* table = bleturner::editableTable(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
    if (!table)
      note = tr(STR_BT_KEYS_NOT_SAVED);
    else {
      const bleturner::RemoteTable previous = *table;
      bool valid = true;
      if (title) {
        const int slot = remotebindings::appSlot(*table, learnCode, hold, title);
        if (slot < 0) {
          note = tr(STR_BT_APP_LIMIT);
          valid = false;
        } else {
          table->shortcuts[slot].action = uint8_t(SideButtonAction::OpenShortcut);
          snprintf(table->shortcuts[slot].title, sizeof(table->shortcuts[slot].title), "%s", title);
          action = slot ? bleturner::Action::Shortcut2 : bleturner::Action::Shortcut1;
        }
      }
      if (valid && !remotebindings::assign(*table, learnCode, hold, action)) {
        note = tr(STR_BT_BINDING_LIMIT);
        valid = false;
      }
      if (valid) saved = SETTINGS.saveToFile();
      if (!saved) {
        *table = previous;
        SETTINGS.ble.remoteCount = oldCount;
        if (valid) note = tr(STR_BT_KEYS_NOT_SAVED);
      }
    }
  }
  if (saved) {
    startCapture();
    note = tr(STR_BT_SAVED_NEXT);
  }
  refresh();
  requestUpdate();
  return saved;
}
void BleSettingsActivity::restoreButton() {
  const auto peer = bleturner::linked();
  if (!bleturner::status().connected || std::strcmp(peer.addr, learnAddr)) return;
  {
    RenderLock lock(*this);
    const auto count = SETTINGS.ble.remoteCount;
    auto* table = bleturner::editableTable(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
    if (!table) return;
    const auto previous = *table;
    const bool fits = remotebindings::restoreDefault(*table, peer.name, learnCode, hold);
    if (!fits || !SETTINGS.saveToFile()) {
      *table = previous;
      SETTINGS.ble.remoteCount = count;
      note = fits ? tr(STR_BT_KEYS_NOT_SAVED) : tr(STR_BT_BINDING_LIMIT);
    } else
      note = tr(STR_BT_MAPPING_CLEARED);
  }
  showView(View::Bindings);
}
void BleSettingsActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  uiTarget.setFont(0, UI_10_FONT_ID);
  uiTarget.setFont(3, SMALL_FONT_ID);
  pixelui::begin(screen);
  fui::ListProps props;
  settingsui::styleRows(props);
  props.rowProvider = [](void* ctx, uint16_t i, fui::ListItem& row) {
    static_cast<BleSettingsActivity*>(ctx)->provideRow(i, row);
  };
  props.rowProviderCtx = this;
  props.count = uint16_t(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  props.labelText.font = 0;
  props.labelText.bold = view == View::Help;
  props.labelText.maxLines = 3;
  props.subtitleText.font = 3;
  props.subtitleText.maxLines = 8;
  props.valueText.font = 3;
  props.valueText.bold = false;
  props.rowHeight = 56;
  props.rowPaddingY = 12;
  props.sidePadding = 14;
  props.iconSize = 34;
  props.textGap = 12;
  props.rowGap = 10;
  props.rowRadius = lightui::radius();
  props.separatorPaint = fui::Paint::solid(fui::Color::Transparent);
  props.rowStyles.normal.borderWidth = 1;
  props.rowStyles.selected = props.rowStyles.focused = props.rowStyles.active = props.rowStyles.normal;
  props.toggleCheckbox = true;
  props.toggleWidth = props.toggleHeight = 26;
  props.selectionMarker = fui::SelectionMarker::None;
  props.markerThickness = 1;
  auto body = screen.body();
  if (view == View::Menu) body.height = std::max<int>(0, body.height - 60);
  if (view == View::Bindings || view == View::Capture)
    buildBindings(screen, props);
  else
    fui::list(screen.frame(), body, props);
  if (view == View::Menu) {
    fui::ButtonProps help;
    help.label = tr(STR_BT_HELP_BUTTON);
    help.text.font = 3;
    help.text.align = fui::TextAlign::Center;
    help.styles = pixelui::buttonStyles();
    help.styles.normal.borderWidth = 1;
    help.action = ACTION_USER;
    help.radius = lightui::radius();
    const auto safe = screen.body();
    const int width = std::min<int>(280, safe.width);
    screen.button(help, fui::makeRect(safe.right() - width, safe.bottom() - 48, width, 44));
  }
}

void BleSettingsActivity::confirmRemoval(int index) {
  if (view != View::Bindings && view != View::Capture) return;
  if (index <= 0 || index >= listCount()) return;
  const auto peer = bleturner::linked();
  const auto* table = bleturner::tableFor(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
  const int slot = rows[index].actionValue;
  if (!table || slot < 0 || slot >= table->count) return;
  const auto binding = table->bindings[slot];
  std::array<char, 18> address{};
  snprintf(address.data(), address.size(), "%s", peer.addr);
  auto confirm = makeUniqueNoThrow<BluetoothConfirmation>(
      renderer, mappedInput, tr(STR_DELETE), tr(STR_BT_DELETE_BINDING_CONFIRM), tr(STR_CANCEL), tr(STR_DELETE));
  if (!confirm) {
    LOG_ERR("BLE", "OOM: button confirmation");
    return;
  }
  app.clearTapFlash();
  showView(View::Bindings);
  startActivityForResult(std::move(confirm), [this, address, binding](const ActivityResult& result) {
    if (!result.isCancelled)
      removeBinding(address.data(), binding & bleturner::kCodeMask, (binding & bleturner::kHoldBit) != 0);
    refresh();
    requestUpdate();
  });
}
void BleSettingsActivity::removeBinding(const char* address, uint32_t code, bool held) {
  const auto peer = bleturner::linked();
  if (std::strcmp(address, peer.addr) || !bleturner::status().connected) return;
  RenderLock lock(*this);
  const auto count = SETTINGS.ble.remoteCount;
  auto* table = bleturner::editableTable(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
  if (!table) {
    note = tr(STR_BT_KEYS_NOT_SAVED);
    return;
  }
  const auto previous = *table;
  if (!remotebindings::remove(*table, code, held) || !SETTINGS.saveToFile()) {
    *table = previous;
    SETTINGS.ble.remoteCount = count;
    note = tr(STR_BT_KEYS_NOT_SAVED);
  } else
    note = tr(STR_BT_MAPPING_CLEARED);
}
void BleSettingsActivity::buildBindings(UiScreen& screen, fui::ListProps& props) {
  props.rowProvider = nullptr;
  props.nav = nullptr;
  props.count = 1;
  props.scrollIndicator = false;
  props.rowInset = 0;
  props.topIndex = 0;
  const auto viewport = screen.body();
  const auto body = fui::makeRect(viewport.x, viewport.y, viewport.width - 5, viewport.height);
  int y = body.y, drawn = 0;
  const int top = std::clamp<int>(nav.top, 0, listCount() - 1);
  for (int i = top; i < listCount(); ++i) {
    fui::ListItem row;
    provideRow(i, row);
    const int actionWidth = i > 0 ? 48 : 0;
    const auto measure =
        fui::measureListRow(screen.target(), screen.frame().assets(), body.width - actionWidth - 2, props, row);
    const int height = measure.height + 2;
    if (y + height > body.bottom()) break;
    const auto card = fui::makeRect(body.x, y, body.width, height);
    screen.target().stroke(card, lightui::ink, 1, lightui::radius());
    props.rowStyles.normal.borderWidth = 0;
    props.rowStyles.selected = props.rowStyles.focused = props.rowStyles.active = props.rowStyles.normal;
    props.items = &row;
    props.selectedIndex = nav.selected == i ? 0 : -1;
    fui::list(screen.frame(), fui::makeRect(card.x + 1, card.y + 1, card.width - actionWidth - 2, card.height - 2),
              props);
    if (i > 0) {
      const auto target = fui::makeRect(card.right() - actionWidth - 1, card.y + (height - 44) / 2, 44, 44);
      const auto icon = fui::makeRect(target.x + 9, target.y + 9, 26, 26);
      screen.target().bitmap(icon, settingsicons::Trash, fui::BitmapMode::Contain, lightui::ink);
      screen.frame().hit(target, ACTION_USER + 1, i, fui::InputTouch,
                         row.enabled ? fui::StateNormal : fui::StateDisabled);
    }
    y += height + props.rowGap;
    ++drawn;
  }
  fui::drawListScrollIndicator(screen.target(), viewport, listCount(), drawn, top, 3);
  nav.onListRendered(top, drawn, nav.selected >= top && nav.selected < top + drawn);
  if (drawn) nav.drawnCount = listCount();
}
