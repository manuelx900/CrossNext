#include "BookPreparationActivity.h"

#include <FontCacheManager.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "../../CrossPointSettings.h"
#include "../../ReaderFontSizes.h"
#include "../../SdCardFontSystem.h"
#include "../../activities/reader/ReaderUtils.h"
#include "../../components/CoverGridHomeUi.h"
#include "../../components/UITheme.h"
#include "../home/CoverSceneCache.h"
#include "../home/HomeScreen.h"
#include "../home/LicoriceLayout.h"
#include "../library/LibraryControls.h"
#include "../ui/PixelMenuTheme.h"
#include "BookPreparationGeometry.h"
#include "BookPreparationScreens.h"
#include "components/SideButtonNavigation.h"

void BookPreparationActivity::configureJob() {
  const auto orientation = renderer.getOrientation();
  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const int margin = SETTINGS.screenMargin;
  top += UITheme::getReaderTopBarHeight();
  const int width = renderer.getScreenWidth() - left - right - 2 * margin;
  const int height = renderer.getScreenHeight() - top - bottom - margin -
                     std::max(margin, int(UITheme::getInstance().getStatusBarHeight()));
  baseSpec = SETTINGS.readerRenderSpec(width, height);
  baseSpec.engine = engine;
  job->rotateXtc = SETTINGS.xtcCoverRotation != 0;
  renderer.setOrientation(orientation);
  bookprep::collectCoverSizes(*job, renderer);
}

void BookPreparationActivity::onEnter() {
  Activity::onEnter();
  engine = epubEngineFromSetting(SETTINGS.epubEngine);
  sdFontSystem.refreshIfDirty();
  fonts.add("");
  const auto captureSizes = [&](int index, const SdCardFontFamilyInfo* family) {
    if (index < 0 || !family) return;
    auto& sizes = fonts.fonts[index].sizes;
    sizes = {};
    if (family->vector) {
      for (const auto point : VECTOR_READER_POINT_SIZES) sizes.add(point);
    } else {
      for (const auto& file : family->files)
        if (file.style == 0) sizes.add(file.pointSize);
    }
    if (!sizes.count()) sizes.add(12);
  };
  captureSizes(0, readerFontFamilyAt(&sdFontSystem.registry(), 0));
  const auto* registry = &sdFontSystem.registry();
  for (int n = 1; n < readerFontFamilyCount(registry); ++n)
    if (const auto* family = readerFontFamilyAt(registry, n)) {
      const int index = fonts.add(family->name.c_str());
      captureSizes(index, family);
      if (index >= 0 && family->name == SETTINGS.sdFontFamilyName) currentFont = index;
    }
  if (fonts.omitted) LOG_ERR("PREP", "Omitted %u font names exceeding reader setting bounds", fonts.omitted);
  fonts.selectOnly(currentFont);
  requestedPointSize = fonts.availableSizes().nearest(SETTINGS.fontPointSize);
  requestUpdate();
}
void BookPreparationActivity::show(bookprepui::View next) {
  if (next != view) interactions.setFocusedIndex(-1);
  view = next;
  ready = false;
  ++revision;
  requestUpdate();
}
bool BookPreparationActivity::loadFont(int index, ReaderRenderSpec& spec) {
  if (index < 0 || unsigned(index) >= fonts.count) return false;
  activeFont = index;
  activePointSize = cursor.point;
  // Even a failed load may have released the previous face.
  preparationFontLoaded = true;
  const char* family = fonts.fonts[index].family;
  const int id = sdFontSystem.loadForPreparation(renderer, family, activePointSize);
  if (!id) {
    LOG_ERR("PREP", "Could not load preparation font: %s", family);
    error = tr(STR_PREPARE_FONT_FAILED);
    return false;
  }
  spec = baseSpec;
  spec.fontId = id;
  spec.fontContentHash = *family && SETTINGS.sdFontHashResolver
                             ? SETTINGS.sdFontHashResolver(SETTINGS.sdFontResolverCtx, family, activePointSize)
                             : 0;
  return true;
}
void BookPreparationActivity::restoreReaderFont() {
  if (!preparationFontLoaded) return;
  sdFontSystem.ensureLoaded(renderer);
  preparationFontLoaded = false;
}
void BookPreparationActivity::runIdleWork() {
  // ActivityManager owns RenderLock. One parser and one loaded family at a time.
  const auto previous = phase.load();
  const auto requested = command.exchange(Command::None);
  if (requested == Command::Cancel && !job) {
    phase.store(bookprep::Phase::Cancelled);
    requestUpdate();
    return;
  }
  if (requested == Command::Start && !job) {
    job = makeUniqueNoThrow<bookprep::Job>();
    if (!job) {
      LOG_ERR("PREP", "OOM: preparation job");
      phase.store(bookprep::Phase::Failed);
      requestUpdate();
      return;
    }
    configureJob();
    job->formats = formats;
    cursor = {};
    const bool pages = formats & bookprep::FormatEpub;
    if (pages && (!fonts.advance(cursor, sizeMode, requestedPointSize) || !loadFont(cursor.font, job->spec)))
      job->abort();
    else
      job->begin(pages ? fonts.passes(sizeMode) : 1);
  }
  if (!job) return;
  if (requested == Command::Cancel)
    job->cancel();
  else if (job->phase == bookprep::Phase::AwaitingFont) {
    ReaderRenderSpec next;
    if (!fonts.advance(cursor, sizeMode, requestedPointSize) || !loadFont(cursor.font, next) || !job->nextFont(next))
      job->abort();
  } else {
    const auto previousOrientation = renderer.getOrientation();
    ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
    job->step(renderer);
    renderer.setOrientation(previousOrientation);
  }
  phase.store(job->phase);
  if (!bookprep::running(job->phase)) {
    restoreReaderFont();
    homecover::invalidateScenes();
  }
  if (previous != job->phase || millis() - lastPaint >= 1000) requestUpdate();
}
void BookPreparationActivity::action(freeink::ui::ActionId id, int value) {
  if (id == bookprepui::Cancel) {
    if (bookprep::running(phase.load())) {
      command.store(Command::Cancel);
      activityManager.requestIdleWork(*this);
    }
    return;
  }
  if (id == bookprepui::Back) {
    if (view == bookprepui::View::Fonts || view == bookprepui::View::Sizes || view == bookprepui::View::ConfirmMany) {
      page = 0;
      show(bookprepui::View::Options);
    } else if (!bookprep::running(phase.load()))
      finish();
    return;
  }
  if (phase.load() != bookprep::Phase::Ready || !ready) return;
  if (id == bookprepui::Done) {
    page = 0;
    show(bookprepui::View::Options);
    return;
  }
  if (id == bookprepui::Page && (view == bookprepui::View::Fonts || view == bookprepui::View::Sizes)) {
    auto target = toybox::makeTarget(renderer);
    const int count = view == bookprepui::View::Fonts ? int(fonts.count) : int(fonts.availableSizes().count());
    const int next = std::clamp(value, 0, std::max(0, count - 1) / bookprepui::pageSize(target.deviceContext(), view));
    if (next != page) {
      page = next;
      interactions.setFocusedIndex(-1);
      show(view);
    }
    return;
  }
  if (view == bookprepui::View::Options) {
    switch (id) {
      case bookprepui::ToggleFormat:
        if (value != bookprep::FormatEpub && value != bookprep::FormatXtc && value != bookprep::FormatXtch) return;
        formats ^= value;
        show(view);
        return;
      case bookprepui::ChooseFonts:
      case bookprepui::ChooseSizes:
        if (!(formats & bookprep::FormatEpub)) return;
        page = 0;
        show(id == bookprepui::ChooseFonts ? bookprepui::View::Fonts : bookprepui::View::Sizes);
        return;
      case bookprepui::Start:
        if (!formats || ((formats & bookprep::FormatEpub) && !fonts.selectedCount())) return;
        show(bookprepui::View::ConfirmMany);
        return;
      default:
        return;
    }
  }
  if (view == bookprepui::View::Fonts) {
    switch (id) {
      case bookprepui::ToggleFont:
        if (value < 0 || unsigned(value) >= fonts.count) return;
        fonts.fonts[value].selected = !fonts.fonts[value].selected;
        break;
      case bookprepui::CurrentFont:
        fonts.selectOnly(currentFont);
        break;
      case bookprepui::AllFonts:
        fonts.selectAll(true);
        break;
      case bookprepui::NoFonts:
        fonts.selectAll(false);
        break;
      default:
        return;
    }
    const auto available = fonts.availableSizes();
    if (available.count()) requestedPointSize = available.nearest(requestedPointSize);
    show(view);
    return;
  }
  if (view == bookprepui::View::Sizes) {
    if (id == bookprepui::AllSizes)
      sizeMode = bookprep::SizeMode::All;
    else if (id == bookprepui::ChooseSize && value > 0 && value < 256 && fonts.availableSizes().contains(value)) {
      sizeMode = bookprep::SizeMode::Single;
      requestedPointSize = uint8_t(value);
    } else
      return;
    show(view);
    return;
  }
  if (view != bookprepui::View::ConfirmMany || id != bookprepui::ConfirmStart) return;
  phase.store(bookprep::Phase::Counting);
  show(bookprepui::View::Progress);
  command.store(Command::Start);
  activityManager.requestIdleWork(*this);
}
bool BookPreparationActivity::handleHomeGesture() {
  if (!bookprep::running(phase.load())) return false;
  command.store(Command::Cancel);
  activityManager.requestIdleWork(*this);
  return true;
}
void BookPreparationActivity::loop() {
  if (ready && (view == bookprepui::View::Fonts || view == bookprepui::View::Sizes)) {
    const int swipe = sidebuttons::horizontalStep(mappedInput);
    const int direction = swipe ? swipe : sidebuttons::longStep(mappedInput);
    if (direction) {
      RenderLock lock(*this);
      action(bookprepui::Page, page + direction);
      return;
    }
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (bookprep::running(phase.load()))
      action(bookprepui::Cancel);
    else {
      RenderLock lock(*this);
      action(bookprepui::Back);
    }
    return;
  }
  int x = -1, y = -1;
  if (ready && interactions.routable() && sidebuttons::poll(interactions, mappedInput, x, y)) {
    const auto seen = paintedRevision.load();
    RenderLock lock(*this);
    if (ready && seen == revision) {
      freeink::ui::InputSnapshot input{};
      input.touchReleased = sidebuttons::hasActivation(x, y);
      input.touchX = x;
      input.touchY = y;
      const auto event = sidebuttons::route(interactions, input, mappedInput, *this);
      action(event.action, event.value);
    }
  }
  if (bookprep::running(phase.load()) || command.load() != Command::None) activityManager.requestIdleWork(*this);
}
void BookPreparationActivity::prepareForSleep() {
  command.store(Command::None);
  if (job) job->cancel();
  if (bookprep::running(phase.load())) phase.store(bookprep::Phase::Cancelled);
  restoreReaderFont();
}
void BookPreparationActivity::onExit() {
  job.reset();
  restoreReaderFont();
  if (auto* cache = renderer.getFontCacheManager()) cache->releaseSdFontCaches();
  homecover::invalidateScenes();
  Activity::onExit();
}
void BookPreparationActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, true);
  const freeink::ui::InputSnapshot input{};
  toybox::Frame frame(target, target.deviceContext(), input, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  const auto state = phase.load();
  bookprepui::Model model;
  model.engine = engine;
  model.view = view;
  model.fonts = &fonts;
  model.currentFont = currentFont;
  model.page = page;
  model.pointSize = requestedPointSize;
  model.formats = formats;
  model.sizeMode = sizeMode;
  model.sizes = fonts.availableSizes();
  model.running = bookprep::running(state);
  switch (state) {
    case bookprep::Phase::Ready:
      model.status = tr(STR_PREPARE_BOOKS);
      break;
    case bookprep::Phase::Counting:
      model.status = tr(STR_PREPARE_SCANNING);
      break;
    case bookprep::Phase::Preparing:
    case bookprep::Phase::AwaitingFont:
      model.status = tr(STR_PREPARE_WORKING);
      break;
    case bookprep::Phase::Complete:
      model.status = tr(STR_PREPARE_COMPLETE);
      break;
    case bookprep::Phase::Cancelled:
      model.status = tr(STR_PREPARE_CANCELLED);
      break;
    case bookprep::Phase::Failed:
      model.status = tr(STR_PREPARE_FAILED);
      break;
  }
  if (job) {
    model.done = uint64_t(job->fontPass) * job->total + job->processed;
    model.total = uint64_t(job->fontPasses) * job->total;
    std::snprintf(countsText, sizeof(countsText), tr(STR_PREPARE_PROGRESS), unsigned(job->processed),
                  unsigned(job->total));
    if (state == bookprep::Phase::Counting)
      std::snprintf(countsText, sizeof(countsText), tr(STR_PREPARE_FOUND), unsigned(job->total));
    model.counts = countsText;
    const char* family = activeFont >= 0 ? fonts.fonts[activeFont].family : "";
    std::snprintf(fontText, sizeof(fontText), tr(STR_PREPARE_FONT_PROGRESS), unsigned(job->fontPass + 1),
                  unsigned(job->fontPasses), *family ? family : tr(STR_MENU_FONT_CLASSIC), unsigned(activePointSize));
    model.font = formats & bookprep::FormatEpub ? fontText : tr(STR_PREPARE_COVERS_ONLY);
    if (state == bookprep::Phase::Preparing) {
      const char* name = std::strrchr(job->path, '/');
      model.detail = name ? name + 1 : job->path;
      if (job->chapterCount) {
        std::snprintf(detailText, sizeof(detailText), tr(STR_PREPARE_CHAPTER),
                      std::min(job->chapter + 1, job->chapterCount), job->chapterCount);
        model.note = detailText;
      }
    } else if (!model.running) {
      std::snprintf(detailText, sizeof(detailText), tr(STR_PREPARE_RESULT), unsigned(job->failed),
                    unsigned(job->skipped));
      model.detail = detailText;
      model.note = tr(STR_PREPARE_KEPT);
    }
  }
  if (error) model.detail = error;
  bookprepui::build(screen, model);
  toybox::reportOverflow(interactions, "BookPreparation");
  lastPaint = millis();
  renderer.displayBuffer();
  paintedRevision.store(revision);
  ready = true;
}
