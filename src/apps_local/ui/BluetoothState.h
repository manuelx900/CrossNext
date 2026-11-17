#pragma once
#include <BlePageTurner.h>
#include <I18n.h>

namespace bluetoothui {
enum class State : uint8_t {
  Off,
  Connected,
  Connecting,
  Scanning,
  Starting,
  Stopping,
  Wifi,
  Memory,
  Idle,
  Unpaired,
  Ready,
  Failed,
  Unavailable
};
inline State state(bool enabled, bool paired, const bleturner::Status& status, bleturner::Why why,
                   bleturner::Why refusal) {
  if (!status.compiledIn) return State::Unavailable;
  if (!enabled) return status.stopping ? State::Stopping : State::Off;
  if (why == bleturner::Why::StorageBusy) return State::Wifi;
  if (status.stopping) return State::Stopping;
  if (status.starting) return State::Starting;
  if (status.connected) return State::Connected;
  if (status.connecting) return State::Connecting;
  if (status.scanning) return State::Scanning;
  if (status.idleStopped) return State::Idle;
  if (!status.running && (refusal == bleturner::Why::HeapLow || refusal == bleturner::Why::HeapInPieces))
    return State::Memory;
  if (!status.running && refusal != bleturner::Why::Ok) return State::Failed;
  if (!paired) return State::Unpaired;
  return State::Ready;
}
inline StrId label(State value) {
  switch (value) {
    case State::Off:
      return StrId::STR_BT_STATUS_OFF;
    case State::Connected:
      return StrId::STR_BT_STATUS_CONNECTED;
    case State::Connecting:
      return StrId::STR_CONNECTING;
    case State::Scanning:
      return StrId::STR_SCANNING;
    case State::Starting:
      return StrId::STR_BT_STATUS_STARTING;
    case State::Stopping:
      return StrId::STR_BT_STATUS_STOPPING;
    case State::Wifi:
      return StrId::STR_BT_STATUS_PAUSED_WIFI;
    case State::Memory:
      return StrId::STR_BT_STATUS_PAUSED_MEMORY;
    case State::Idle:
      return StrId::STR_BT_STATUS_IDLE;
    case State::Unpaired:
      return StrId::STR_BT_STATUS_UNPAIRED;
    case State::Ready:
      return StrId::STR_BT_STATUS_DISCONNECTED;
    case State::Failed:
      return StrId::STR_BT_CONNECTION_FAILED;
    case State::Unavailable:
      return StrId::STR_BT_NOT_AVAILABLE;
  }
  return StrId::STR_BT_STATUS_DISCONNECTED;
}
inline State current(const bleturner::Config& config) {
  return state(config.enabled != 0, config.peerAddr[0] != '\0', bleturner::status(), bleturner::why(),
               bleturner::lastStartRefusal());
}
inline StrId shortLabel(State state) {
  switch (state) {
    case State::Off:
      return StrId::STR_BT_QUICK_OFF;
    case State::Connected:
      return StrId::STR_BT_QUICK_CONNECTED;
    case State::Wifi:
    case State::Memory:
      return StrId::STR_BT_QUICK_PAUSED;
    case State::Unpaired:
      return StrId::STR_BT_QUICK_UNPAIRED;
    case State::Starting:
      return StrId::STR_BT_QUICK_STARTING;
    case State::Idle:
      return StrId::STR_BT_QUICK_IDLE;
    case State::Connecting:
      return StrId::STR_CONNECTING;
    case State::Scanning:
      return StrId::STR_SCANNING;
    case State::Stopping:
      return StrId::STR_BT_STATUS_STOPPING;
    case State::Ready:
    case State::Failed:
    case State::Unavailable:
      return StrId::STR_BT_QUICK_DISCONNECTED;
  }
  return StrId::STR_BT_QUICK_DISCONNECTED;
}
inline StrId bannerLabel(State state) {
  switch (state) {
    case State::Connected:
      return StrId::STR_BT_QUICK_CONNECTED;
    case State::Starting:
      return StrId::STR_BT_QUICK_STARTING;
    case State::Unpaired:
      return StrId::STR_BT_QUICK_UNPAIRED;
    case State::Wifi:
      return StrId::STR_BT_BANNER_WIFI;
    case State::Memory:
      return StrId::STR_BT_BANNER_MEMORY;
    case State::Failed:
      return StrId::STR_BT_BANNER_FAILED;
    default:
      return label(state);
  }
}
}  // namespace bluetoothui
