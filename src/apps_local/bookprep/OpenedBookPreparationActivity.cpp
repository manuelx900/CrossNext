#include "OpenedBookPreparationActivity.h"

#include <I18n.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "../../CrossPointSettings.h"
#include "../../components/UITheme.h"
#include "../home/CoverSceneCache.h"
#include "../ui/PixelMenuTheme.h"
#include "BookPreparationGeometry.h"
#include "BookPreparationScreens.h"

namespace bookprep {
namespace {
std::unique_ptr<Job> newJob(GfxRenderer& renderer) {
  // One job and one chapter/page graph, never a whole-book RAM index.
  auto job = makeUniqueNoThrow<Job>();
  if (!job) {
    LOG_ERR("PREP", "OOM: opened book preparation");
    return nullptr;
  }
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const int margin = SETTINGS.screenMargin;
  top += UITheme::getReaderTopBarHeight();
  job->spec = SETTINGS.readerRenderSpec(renderer.getScreenWidth() - left - right - 2 * margin,
                                        renderer.getScreenHeight() - top - bottom - margin -
                                            std::max(margin, int(UITheme::getInstance().getStatusBarHeight())));
  job->rotateXtc = SETTINGS.xtcCoverRotation != 0;
  collectCoverSizes(*job, renderer);
  return job;
}
std::unique_ptr<Activity> activityFor(GfxRenderer& renderer, MappedInputManager& input, std::unique_ptr<Job> job,
                                      HalMemory::PsramBuffer background = {}) {
  if (job->phase == Phase::Complete) return nullptr;
  auto activity =
      makeUniqueNoThrow<OpenedBookPreparationActivity>(renderer, input, std::move(job), std::move(background));
  if (!activity) LOG_ERR("PREP", "OOM: preparation progress activity");
  return activity;
}
}  // namespace
void showOpeningProgress(GfxRenderer& renderer) {
  RenderLock lock;
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, true);
  // The hit table exceeds the loader's stack budget; this one-off table is
  // released immediately after the initial paint, before book parsing starts.
  auto interactions = makeUniqueNoThrow<toybox::Interactions>();
  if (!interactions) {
    LOG_ERR("PREP", "OOM: initial progress hit table");
    GUI.drawPopup(renderer, tr(STR_INDEXING));
    return;
  }
  const freeink::ui::InputSnapshot input{};
  toybox::Frame frame(target, target.deviceContext(), input, *interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  bookprepui::Model model;
  model.running = true;
  model.status = tr(STR_INDEXING);
  model.total = 1;
  bookprepui::openedProgress(screen, model);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
std::unique_ptr<Activity> onOpen(GfxRenderer& renderer, MappedInputManager& input, const std::shared_ptr<Epub>& book,
                                 HalMemory::PsramBuffer background) {
  RenderLock lock;
  auto job = newJob(renderer);
  if (!job || !job->beginBook(book)) return nullptr;
  return activityFor(renderer, input, std::move(job), std::move(background));
}
std::unique_ptr<Activity> onOpen(GfxRenderer& renderer, MappedInputManager& input, const std::shared_ptr<Xtc>& book) {
  RenderLock lock;
  auto job = newJob(renderer);
  if (!job || !job->beginBook(book)) return nullptr;
  return activityFor(renderer, input, std::move(job));
}
}  // namespace bookprep

OpenedBookPreparationActivity::OpenedBookPreparationActivity(GfxRenderer& renderer, MappedInputManager& input,
                                                             std::unique_ptr<bookprep::Job> next,
                                                             HalMemory::PsramBuffer background)
    : Activity("OpenedBookPreparation", renderer, input),
      job(std::move(next)),
      backdrop(std::move(background)),
      bookOrientation(renderer.getOrientation()) {
  setInterfaceFontSection(menufont::Section::Reader);
}
void OpenedBookPreparationActivity::onEnter() {
  Activity::onEnter();
  {
    RenderLock lock;
    if (!backdrop) {
      backdrop = HalMemory::allocatePsram(renderer.getBufferSize());
      if (backdrop)
        std::memcpy(backdrop.get(), renderer.getFrameBuffer(), renderer.getBufferSize());
      else
        LOG_ERR("PREP", "No PSRAM for progress backdrop");
    }
  }
  requestUpdate();
}
void OpenedBookPreparationActivity::runIdleWork() {
  if (cancel.exchange(false))
    job->cancel();
  else if (painted.load()) {
    const auto orientation = renderer.getOrientation();
    renderer.setOrientation(bookOrientation);
    job->step(renderer);
    renderer.setOrientation(orientation);
  }
  phase.store(job->phase);
  if (!bookprep::running(job->phase) || millis() - lastPaint >= 1000) requestUpdate();
}
void OpenedBookPreparationActivity::loop() {
  if (phase.load() == bookprep::Phase::Complete || phase.load() == bookprep::Phase::Cancelled) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (bookprep::running(phase.load()))
      cancel.store(true);
    else
      finish();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && !bookprep::running(phase.load())) finish();
  int x, y;
  if (mappedInput.wasScreenTapped(x, y) && painted.load()) {
    RenderLock lock(*this);
    freeink::ui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = x;
    input.touchY = y;
    const auto event = interactions.route(input);
    if (event.action == bookprepui::Cancel || event.action == bookprepui::Back) {
      if (bookprep::running(phase.load()))
        cancel.store(true);
      else
        finish();
    }
  }
  if (bookprep::running(phase.load()) || cancel.load()) activityManager.requestIdleWork(*this);
}
bool OpenedBookPreparationActivity::handleHomeGesture() {
  if (!bookprep::running(phase.load())) return false;
  cancel.store(true);
  activityManager.requestIdleWork(*this);
  return true;
}
void OpenedBookPreparationActivity::prepareForSleep() { job->cancel(); }
void OpenedBookPreparationActivity::onExit() {
  job.reset();
  backdrop.reset();
  homecover::invalidateScenes();
  Activity::onExit();
}
void OpenedBookPreparationActivity::render(RenderLock&&) {
  if (backdrop)
    std::memcpy(renderer.getFrameBuffer(), backdrop.get(), renderer.getBufferSize());
  else
    renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, true);
  const freeink::ui::InputSnapshot input{};
  toybox::Frame frame(target, target.deviceContext(), input, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  bookprepui::Model model;
  model.running = bookprep::running(phase.load());
  switch (phase.load()) {
    case bookprep::Phase::Complete:
      model.status = tr(STR_PREPARE_COMPLETE);
      break;
    case bookprep::Phase::Cancelled:
      model.status = tr(STR_PREPARE_CANCELLED);
      break;
    case bookprep::Phase::Failed:
      model.status = tr(STR_PREPARE_FAILED);
      break;
    default:
      model.status = tr(STR_INDEXING);
      break;
  }
  const char* name = std::strrchr(job->path, '/');
  model.detail = name ? name + 1 : job->path;
  model.note = tr(STR_PREPARE_KEPT);
  model.done = job->progressDone();
  model.total = std::max(1U, job->progressTotal());
  std::snprintf(counts, sizeof(counts), "%u%%", unsigned(model.done * 100 / model.total));
  model.counts = counts;
  bookprepui::openedProgress(screen, model);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  lastPaint = millis();
  painted.store(true);
}
