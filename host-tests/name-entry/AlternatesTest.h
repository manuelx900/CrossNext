#pragma once

#include "../../src/apps_local/ui/KeyboardAlternates.h"

static void alternateChooser() {
  using keyboardview::Alternates;
  using Contact = Alternates::Contact;
  const auto key = [](const char* output, const char* alt = nullptr) {
    fui::KeyboardKey result;
    result.output = result.label = output;
    result.alt = alt;
    return result;
  };
  const auto anchor = fui::makeRect(80, 580, 44, 52);
  Alternates popup;
  assert(popup.open(key("a"), Language::ES, anchor));
  assert(popup.count() == 9);
  assert(popup.open(key("a", "ą"), Language::PL, anchor));
  assert(std::string(popup.choice(1)) == "ą");
  assert(popup.open(key("A"), Language::ES, anchor));
  assert(std::string(popup.choice(2)) == "Á");
  assert(popup.open(key("か"), Language::JA, anchor));
  assert(std::string(popup.choice(1)) == "き");
  bool voiced = false;
  for (unsigned i = 0; i < popup.count(); ++i) voiced |= std::string(popup.choice(i)) == "が";
  assert(voiced);
  assert(popup.open(key("ا"), Language::AR, anchor));
  assert(popup.count() >= 4);
  assert(popup.open(key("א"), Language::HE, anchor));
  assert(popup.count() == 9 && std::strlen(popup.choice(1)) == 4);
  assert(!popup.open(key("a"), Language::ZH_CN, anchor));
  assert(!popup.open(key("a"), Language::EN, anchor, false, false));
  assert(popup.open(key("~", "`"), Language::ZH_TW, anchor));
  assert(!popup.open(key("b"), Language::EN, anchor));

  assert(!popup.open(key("q", "û"), Language::FR, anchor));
  assert(popup.open(key("u"), Language::FR, anchor));
  bool frenchU = false;
  for (unsigned i = 0; i < popup.count(); ++i) frenchU |= std::string(popup.choice(i)) == "û";
  assert(frenchU);
  for (auto language : {Language::AR, Language::FA, Language::TH}) {
    assert(popup.open(key("5"), language, anchor));
    const char* digit = language == Language::AR ? "٥" : language == Language::FA ? "۵" : "๕";
    assert(std::string(popup.choice(1)) == digit);
  }
  assert(popup.open(key("?"), Language::ES, anchor));
  assert(std::string(popup.choice(1)) == "¿");
  assert(popup.open(key("?"), Language::AR, anchor));
  assert(std::string(popup.choice(1)) == "؟");
  assert(popup.open(key("ă"), Language::VI, anchor));
  bool tone = false;
  for (unsigned i = 0; i < popup.count(); ++i) tone |= std::string(popup.choice(i)) == "ặ";
  assert(tone);
  assert(popup.open(key("l"), Language::CA, anchor));
  assert(std::string(popup.choice(1)) == "l·l");

  for (auto language : {Language::AR, Language::FA}) {
    assert(popup.open(key("-"), language, anchor));
    bool minus = false;
    for (unsigned i = 0; i < popup.count(); ++i) minus |= std::string(popup.choice(i)) == "−";
    assert(minus && popup.count() == 9);
    assert(popup.open(key("_"), language, anchor));
    assert(std::string(popup.choice(1)) == "ـ");
  }
  {
    Trace labels;
    fui::DeviceContext device;
    device.width = 480;
    device.height = 800;
    fui::InteractionBuffer<80> hits;
    const fui::InputSnapshot empty;
    fui::Frame<80> frame(labels, device, empty, hits);
    assert(popup.open(key("."), Language::AR, anchor));
    popup.draw(frame, fui::makeRect(12, 80, 456, 700), 1700, 4);
    assert(!labels.find("◌ٰ").empty());
  }

  for (bool wide : {false, true})
    for (bool inverted : {false, true}) {
      (void)inverted;
      Trace target;
      fui::DeviceContext device;
      device.width = wide ? 800 : 480;
      device.height = wide ? 480 : 800;
      fui::InteractionBuffer<80> hits;
      auto build = [&] {
        hits.beginPublishCycle();
        const fui::InputSnapshot input;
        fui::Frame<80> frame(target, device, input, hits);
        popup.draw(frame, fui::makeRect(12, 72, device.width - 24, device.height - 84), 1700, 4);
        hits.publish();
      };
      auto event = [&](Contact contact) {
        contact.painted = paintclock::painted();
        assert(popup.capture(contact));
        return popup.poll(hits);
      };
      assert(popup.open(key("a"), Language::ES, anchor));
      build();
      assert(hits.publishedCount() == 9);
      for (size_t i = 0; i < hits.publishedCount(); ++i) {
        const auto rect = hits.publishedData()[i].rect;
        assert(rect.x >= 12 && rect.y >= 72 && rect.right() <= device.width - 12 &&
               rect.bottom() <= device.height - 12);
        assert(rect.width >= 44 && rect.height >= 44);
      }
      const auto rect = hits.publishedData()[2].rect;
      const int16_t x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
      // This tap was observed before the popup paint, even if poll runs after it.
      Contact stale;
      stale.tap = true;
      stale.x = x;
      stale.y = y;
      stale.painted = paintclock::painted();
      assert(popup.capture(stale));
      paintclock::notePainted();
      assert(!*popup.poll(hits).output);
      Contact tap;
      tap.tap = true;
      tap.x = x;
      tap.y = y;
      assert(std::string(event(tap).output) == "á");
      // The old popup remains modal until the normal keyboard really appears.
      assert(popup.active());
      build();
      Contact old;
      old.held = true;
      old.x = x;
      old.y = y;
      assert(popup.capture(old));
      paintclock::notePainted();
      Contact release;
      release.ended = true;
      release.painted = paintclock::painted();
      assert(popup.capture(release));
      Contact fresh;
      fresh.tap = true;
      fresh.painted = paintclock::painted();
      assert(!popup.capture(fresh) && !popup.active());

      // Releasing the opening contact on its original key leaves the choices open.
      assert(popup.open(key("e"), Language::EN, anchor));
      build();
      Contact early;
      early.ended = true;
      early.x = anchor.x;
      early.y = anchor.y;
      assert(!*event(early).output);
      paintclock::notePainted();
      Contact cancel;
      cancel.cancel = true;
      assert(event(cancel).changed);
      build();
      paintclock::notePainted();
      Contact idle;
      idle.painted = paintclock::painted();
      assert(!popup.capture(idle));

      // A visible drag may choose a variant on release, without a tap gesture.
      assert(popup.open(key("a"), Language::ES, anchor));
      build();
      paintclock::notePainted();
      Contact drag;
      drag.held = true;
      drag.x = x;
      drag.y = y;
      event(drag);
      release.x = x;
      release.y = y;
      assert(std::string(event(release).output) == "á");
      build();
      paintclock::notePainted();
      idle.painted = paintclock::painted();
      popup.capture(idle);

      // The real SDK reports a short drag as a tap at its initial point.
      // A tracked release must retain the live hovered option instead.
      assert(popup.open(key("a"), Language::ES, anchor));
      build();
      paintclock::notePainted();
      MappedInputManager live;
      live.x = anchor.x + anchor.width / 2;
      live.y = anchor.y + anchor.height / 2;
      live.held = true;
      live.liveX = x;
      live.liveY = y;
      assert(popup.capture(live));
      popup.poll(hits);
      live.held = false;
      live.back = live.backGesture = true;
      live.ended = live.tap = true;
      assert(popup.capture(live));
      assert(std::string(popup.poll(hits).output) == "á");
      build();
      paintclock::notePainted();
      idle.painted = paintclock::painted();
      popup.capture(idle);

      // Header Back is another wasBackGesture source, not a chooser drag.
      assert(popup.open(key("a"), Language::ES, anchor));
      build();
      paintclock::notePainted();
      MappedInputManager header;
      header.ended = true;
      header.x = anchor.x;
      header.y = anchor.y;
      assert(popup.capture(header));
      popup.poll(hits);
      header.ended = false;
      header.held = true;
      header.x = 20;
      header.y = 30;
      assert(popup.capture(header));
      popup.poll(hits);
      header.held = false;
      header.ended = true;
      header.back = header.backGesture = true;
      assert(popup.capture(header));
      const auto cancelled = popup.poll(hits);
      assert(cancelled.changed && !*cancelled.output);
      build();
      paintclock::notePainted();
      idle.painted = paintclock::painted();
      assert(!popup.capture(idle));

      // The Confirm release that opened the popup cannot select the base letter.
      assert(popup.open(key("a"), Language::ES, anchor, true));
      build();
      paintclock::notePainted();
      Contact confirm;
      confirm.confirm = true;
      assert(!*event(confirm).output);
      Contact next;
      next.next = true;
      event(next);
      event(next);
      assert(std::string(event(confirm).output) == "á");
      popup.reset();
    }
  std::printf(
      "Alternatives: locale catalog, four viewports, delayed paint, drag/tap, modal close and physical input passed; "
      "workspace %zu bytes\n",
      sizeof(Alternates));
}

static void embeddedAccents() {
  EmbeddedKeyboard keyboard;
  assert(keyboard.begin(Language::ES, 24, 12, true));
  Trace target;
  fui::DeviceContext device;
  device.width = 480;
  device.height = 800;
  const auto box = fui::makeRect(12, 520, 456, 260);
  auto paint = [&] {
    target.labels.clear();
    keyboard.draw(target, device, box);
    paintclock::notePainted();
    keyboard.didPaint();
  };
  paint();
  const auto a = target.find("a");
  assert(!a.empty());
  MappedInputManager input;
  input.held = true;
  input.x = a.x + a.width / 2;
  input.y = a.y + a.height / 2;
  for (unsigned i = 0; i < 1000; ++i) keyboard.poll(input);
  assert(!*keyboard.text());
  paint();
  const auto accent = target.find("á");
  assert(!accent.empty());
  input.held = false;
  input.ended = true;
  keyboard.poll(input);
  input.ended = false;
  input.tap = true;
  input.x = accent.x + accent.width / 2;
  input.y = accent.y + accent.height / 2;
  assert(keyboard.poll(input) == EmbeddedKeyboard::Event::Changed);
  assert(std::string(keyboard.text()) == "á");
  paint();
  input.tap = false;
  keyboard.poll(input);
  std::puts("Production embedded hold-to-choose: accented name inserted once, after choice");
}

static void nameUnitDeletion() {
  EmbeddedKeyboard keyboard;
  assert(keyboard.begin(Language::ES, 24, 12, true, "กิกิกิ"));
  Trace target;
  fui::DeviceContext device;
  device.width = 480;
  device.height = 800;
  const auto box = fui::makeRect(12, 500, 456, 280);
  const auto paint = [&] {
    target.labels.clear();
    target.outlines.clear();
    keyboard.draw(target, device, box);
    paintclock::notePainted();
    keyboard.didPaint();
  };
  paint();
  const auto m = target.find("m");
  assert(!m.empty());
  fui::Rect erase;
  for (const auto& rect : target.outlines) {
    if (rect.y <= m.y && rect.bottom() >= m.bottom() && rect.x > m.right() && (erase.empty() || rect.x < erase.x))
      erase = rect;
  }
  assert(!erase.empty());
  MappedInputManager input;
  input.tap = true;
  input.x = erase.x + erase.width / 2;
  input.y = erase.y + erase.height / 2;
  assert(keyboard.poll(input) == EmbeddedKeyboard::Event::Changed);
  assert(std::string(keyboard.text()) == "กิกิ");
  paint();
  input.tap = false;
  input.held = true;
  for (unsigned n = 0; n < 1000; ++n) keyboard.poll(input);
  assert(!*keyboard.text());
  std::puts("Name units: short Delete removes the base with its mark; held Delete clears all");
}

static void embeddedUkrainianApostrophe() {
  EmbeddedKeyboard keyboard;
  assert(keyboard.begin(Language::UK, 24, 12, true, "Лук"));
  Trace target;
  fui::DeviceContext device;
  device.width = 480;
  device.height = 800;
  const auto box = fui::makeRect(12, 480, 456, 300);
  const auto paint = [&] {
    target.labels.clear();
    keyboard.draw(target, device, box);
    paintclock::notePainted();
    keyboard.didPaint();
  };
  const auto tap = [&](const char* label) {
    const auto r = target.find(label);
    assert(!r.empty());
    MappedInputManager input;
    input.tap = true;
    input.x = r.x + r.width / 2;
    input.y = r.y + r.height / 2;
    return keyboard.poll(input);
  };
  paint();
  tap("123");
  paint();
  const auto quote = target.find("'");
  assert(!quote.empty());
  MappedInputManager input;
  input.held = true;
  input.x = quote.x + quote.width / 2;
  input.y = quote.y + quote.height / 2;
  for (unsigned n = 0; n < 1000; ++n) keyboard.poll(input);
  paint();
  const auto modifier = target.find("ʼ");
  assert(!modifier.empty());
  input.held = false;
  input.ended = true;
  keyboard.poll(input);
  input.ended = false;
  input.tap = true;
  input.x = modifier.x + modifier.width / 2;
  input.y = modifier.y + modifier.height / 2;
  assert(keyboard.poll(input) == EmbeddedKeyboard::Event::Changed);
  assert(std::string(keyboard.text()) == "Лукʼ");
  std::puts("Ukrainian modifier apostrophe: production embedded name owner inserts it once");
}
