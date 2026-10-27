#pragma once

#include "FitText.h"
#include "PixelMenu.h"

namespace networkui {
namespace fui = freeink::ui;

inline fui::Rect landscapeBody(const fui::DeviceContext& device, int subHeaderHeight) {
  const auto safe = device.safeRect();
  const int top = pixelui::geometry(device).header.bottom() + 4 + subHeaderHeight + 12;
  return fui::makeRect(safe.x + 16, top, safe.width - 32, std::max(0, safe.bottom() - 8 - top));
}

struct TransferLayout {
  fui::Rect firstHeading{}, firstQr{}, firstAddress{}, secondHeading{}, secondQr{}, url{}, alternate{};
};
inline TransferLayout transferLayout(const fui::DeviceContext& device, int subHeaderHeight, bool hotspot) {
  TransferLayout out;
  const auto body = landscapeBody(device, subHeaderHeight);
  if (hotspot) {
    const int width = (body.width - 24) / 2;
    const int side = std::max(1, std::min(198, int(body.height) - 124));
    out.firstHeading = fui::makeRect(body.x, body.y, width, 48);
    out.secondHeading = fui::makeRect(body.x + width + 24, body.y, width, 48);
    out.firstQr = fui::makeRect(body.x + (width - side) / 2, body.y + 56, side, side);
    out.secondQr = fui::makeRect(out.secondHeading.x + (width - side) / 2, out.firstQr.y, side, side);
    out.firstAddress = fui::makeRect(body.x, out.firstQr.bottom() + 8, width, 56);
    out.url = fui::makeRect(out.secondHeading.x, out.secondQr.bottom() + 8, width, 28);
    out.alternate = fui::makeRect(out.url.x, out.url.bottom(), width, 28);
  } else {
    const int left = body.width * 2 / 5;
    const int side = std::max(1, std::min(198, int(body.height) - 24));
    out.firstQr = fui::makeRect(body.x + (left - side) / 2, body.y + (body.height - side) / 2, side, side);
    const int x = body.x + left + 24, width = body.right() - x;
    const int top = body.y + std::max(0, (int(body.height) - 204) / 2);
    out.firstHeading = fui::makeRect(x, top, width, 64);
    out.firstAddress = fui::makeRect(x, top + 72, width, 48);
    out.url = fui::makeRect(x, top + 132, width, 36);
    out.alternate = fui::makeRect(x, top + 176, width, 28);
  }
  return out;
}

inline void transferText(fui::DrawTarget& target, const TransferLayout& layout, bool hotspot, const char* ssid,
                         const char* url, const char* alternate) {
  fittext::draw(target, layout.firstHeading, hotspot ? tr(STR_CONNECT_WIFI_HINT) : tr(STR_OPEN_URL_HINT), 0, 2,
                fui::TextAlign::Center);
  fittext::draw(target, layout.firstAddress, hotspot ? ssid : tr(STR_SCAN_QR_HINT), 0, 2, fui::TextAlign::Center);
  if (hotspot) fittext::draw(target, layout.secondHeading, tr(STR_OPEN_URL_HINT), 0, 2, fui::TextAlign::Center);
  fittext::draw(target, layout.url, url, 0, 1, fui::TextAlign::Center);
  fittext::draw(target, layout.alternate, alternate, 3, 1, fui::TextAlign::Center);
}

struct CalibreLayout {
  fui::Rect title{}, instructions[4]{}, status{}, receiving{}, progress{}, complete{};
};
inline CalibreLayout calibreLayout(const fui::DeviceContext& device, int subHeaderHeight) {
  CalibreLayout out;
  const auto body = landscapeBody(device, subHeaderHeight);
  const int width = (body.width - 28) / 2;
  out.title = fui::makeRect(body.x, body.y, width, 44);
  const int stride = (body.height - 52) / 4;
  for (int n = 0; n < 4; ++n) out.instructions[n] = fui::makeRect(body.x, body.y + 52 + n * stride, width, stride - 8);
  const int x = body.x + width + 28;
  const int top = body.y + std::max(0, (int(body.height) - 228) / 2);
  out.status = fui::makeRect(x, top, width, 44);
  out.receiving = fui::makeRect(x, top + 52, width, 72);
  out.progress = fui::makeRect(x, top + 136, width, 24);
  out.complete = fui::makeRect(x, top + 176, width, 52);
  return out;
}
inline void calibreText(fui::DrawTarget& target, const CalibreLayout& layout, bool receiving, const char* uploadName,
                        const char* completedName) {
  fittext::draw(target, layout.title, tr(STR_CALIBRE_SETUP), 1, 2);
  static constexpr StrId steps[] = {StrId::STR_CALIBRE_INSTRUCTION_1, StrId::STR_CALIBRE_INSTRUCTION_2,
                                    StrId::STR_CALIBRE_INSTRUCTION_3, StrId::STR_CALIBRE_INSTRUCTION_4};
  for (int n = 0; n < 4; ++n) fittext::draw(target, layout.instructions[n], I18N.get(steps[n]), 0, 3);
  fittext::draw(target, layout.status, tr(STR_CALIBRE_STATUS), 1, 1);
  if (receiving) {
    const auto& r = layout.receiving;
    fittext::draw(target, fui::makeRect(r.x, r.y, r.width, 28), tr(STR_CALIBRE_RECEIVING), 0, 1);
    fittext::draw(target, fui::makeRect(r.x, r.y + 28, r.width, r.height - 28), uploadName, 3, 2);
  }
  if (completedName && *completedName) {
    const auto& r = layout.complete;
    fittext::draw(target, fui::makeRect(r.x, r.y, r.width, 24), tr(STR_CALIBRE_RECEIVED), 0, 1);
    fittext::draw(target, fui::makeRect(r.x, r.y + 24, r.width, r.height - 24), completedName, 3, 1);
  }
}
}  // namespace networkui
