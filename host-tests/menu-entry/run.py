"""Exercise the real menu-entry/persistence bodies with a recording SD boundary.

This focused host test does not emulate the display or measure panel latency.
It verifies ordering of SD writes, coalescing, exit fallback and failed writes,
plus Settings entry and category publication at the render-lock boundary.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def definition(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emsdk', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / '.cache/menu-entry')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    shelf = (ROOT / 'src/apps_local/Shelf.cpp').read_text('utf8')
    folder = (ROOT / 'src/apps_local/ShelfFolderActivity.cpp').read_text('utf8')
    settings = (ROOT / 'src/activities/settings/SettingsActivity.cpp').read_text('utf8')
    bodies = '\n'.join(definition(shelf, signature) for signature in [
        'void saveState(', 'void saveIfChanged(', 'void setOpenTitle(',
        'void openFolder(', 'void flushPendingState('])
    harness = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "ShelfState.h"
#define SIMULATOR 1
#define LOG_ERR(...) ((void)0)
using String = std::string;
struct GfxRenderer {};
struct MappedInputManager { bool touchReleased = false; };
bool lockHeld = false, lockAvailable = true;
int lockAcquisitions = 0;
struct RenderLock {
  enum class Mode { Blocking, Try };
  bool own = false;
  explicit RenderLock(Mode mode = Mode::Blocking) {
    if (mode == Mode::Try && !lockAvailable) return;
    assert(!lockHeld);
    lockHeld = own = true;
    ++lockAcquisitions;
  }
  ~RenderLock() { if (own) lockHeld = false; }
  bool ownsLock() const { return own; }
  static bool heldByCurrentTask() { return lockHeld; }
};
namespace freeink { namespace ui {} }
int millis() { return 0; }
int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
struct RecordingStorage {
  int writes = 0;
  bool fail = false;
  std::string bytes;
  bool writeFile(const char*, const String& text) {
    ++writes;
    if (fail) return false;
    bytes = text;
    return true;
  }
} Storage;
struct Activity { virtual void onExit() {} };
bool allocationFails = false;
struct ShelfFolderActivity : Activity {
  bool interactionsReady = false, saveAfterPresentation = true;
  MappedInputManager mappedInput;
  struct Navigation { bool capture(MappedInputManager&, int) { return true; } } navigation;
  static std::unique_ptr<int> create(GfxRenderer&, MappedInputManager&, int) {
    return allocationFails ? nullptr : std::make_unique<int>(1);
  }
  void onExit() override;
  void loop();
};
namespace shelf {
constexpr int kFolderCount = 2;
const char* kStatePath = "/.crosspoint/shelf.cfg";
State state;
bool stateDirty = false, homeReturn = false;
int openFolderIndex = -1;
struct Folder { const char* title; };
const Folder kFolders[] = {{"Games"}, {"Apps"}};
void ensureLoaded() {}
int replacements = 0;
bool replaceWith(std::unique_ptr<int> activity, const char*) { ++replacements; return bool(activity); }
@@SHELF@@
}
@@FOLDER@@
struct CrossPointSettings {
  enum QUICK_RESUME_SLEEP_SCREEN { QUICK_RESUME_AFTER_TIMEOUT = 1 };
  int quickResumeSleepScreen = 0;
} SETTINGS;
struct UiTabListActivity { void onEnter() {} };
struct SettingsActivity : UiTabListActivity {
  int selectedCategoryIndex = 6, homeCalls = 0, registryBuilds = 0;
  bool preserveQuickResumeTimeoutOn = false, quickResumeTimeoutAutoEnabled = true;
  bool categoryHome = true, settingsListsReady = false, readingResourcesReady = false;
  using SettingInfo = int;
  std::vector<int> displaySettings{1}, readerSettings{2}, controlsSettings{3}, librarySettings{4};
  std::vector<int> connectivitySettings{5}, systemSettings{6}, deviceSettings{7};
  const std::vector<int>* currentSettings = nullptr;
  int settingsCount = 0, rowRebuilds = 0;
  struct Navigation { int top = 0; } nav;
  Navigation& activeNav() { return nav; }
  void syncQuickResumeTimeoutForSleepScreen(bool, bool) {}
  void showCategoryHome() { ++homeCalls; }
  void rebuildSettingsLists() {
    assert(RenderLock::heldByCurrentTask());
    ++registryBuilds;
    settingsListsReady = true;
    readingResourcesReady = selectedCategoryIndex == 1;
    selectCategory(selectedCategoryIndex);
  }
  void rebuildRowItems() { assert(RenderLock::heldByCurrentTask()); ++rowRebuilds; }
  void onEnter();
  void selectCategory(int);
};
@@SETTINGS@@
int main() {
  GfxRenderer renderer;
  MappedInputManager input;
  ShelfFolderActivity activity;
  shelf::state.lastFolder = 0;
  shelf::state.resumeRow[0] = 7;
  shelf::state.resumeRow[1] = 5;
  std::strcpy(shelf::state.openTitle, "CHESS");
  shelf::state.origin = shelf::LaunchOrigin::Home;
  shelf::openFolder(1, renderer, input);
  CHECK(Storage.writes == 0);
  CHECK(shelf::state.lastFolder == 1 && shelf::state.openTitle[0] == '\0');
  CHECK(shelf::state.origin == shelf::LaunchOrigin::Folder);
  CHECK(shelf::state.resumeRow[0] == 7 && shelf::state.resumeRow[1] == 5);
  activity.loop();
  CHECK(Storage.writes == 0);
  activity.interactionsReady = true;
  // A pending render must not cause persistence before its frame completes.
  lockAvailable = false;
  activity.loop();
  CHECK(Storage.writes == 0);
  lockAvailable = true;
  activity.mappedInput.touchReleased = true;
  activity.loop();
  CHECK(Storage.writes == 1);
  CHECK(activity.mappedInput.touchReleased);
  CHECK(!shelf::stateDirty);
  shelf::State restored;
  const int limits[] = {15, 15};
  shelf::parseState(Storage.bytes.c_str(), 2, limits, restored);
  CHECK(restored.lastFolder == 1 && restored.openTitle[0] == '\0');
  CHECK(restored.resumeRow[0] == 7 && restored.resumeRow[1] == 5);
  shelf::openFolder(1, renderer, input);
  activity.loop();
  CHECK(Storage.writes == 1);
  shelf::openFolder(0, renderer, input);
  CHECK(Storage.writes == 1);
  // Leaving before idle must not lose the folder position.
  activity.onExit();
  CHECK(Storage.writes == 2 && !shelf::stateDirty);
  shelf::openFolder(1, renderer, input);
  Storage.fail = true;
  activity.saveAfterPresentation = true;
  activity.loop();
  CHECK(shelf::stateDirty && Storage.writes == 3);
  for (int n = 0; n < 1000; ++n) activity.loop();
  CHECK(shelf::stateDirty && Storage.writes == 3);
  Storage.fail = false;
  activity.loop();
  CHECK(Storage.writes == 3);
  activity.onExit();
  CHECK(!shelf::stateDirty && Storage.writes == 4);
  allocationFails = true;
  shelf::openFolder(0, renderer, input);
  CHECK(shelf::state.lastFolder == 1 && !shelf::stateDirty);
  CHECK(Storage.writes == 4);
  allocationFails = false;
  shelf::openFolder(0, renderer, input);
  shelf::saveIfChanged(0, 8);
  CHECK(Storage.writes == 5 && !shelf::stateDirty);
  activity.loop();
  CHECK(Storage.writes == 5);
  shelf::openFolder(-1, renderer, input);
  shelf::openFolder(2, renderer, input);
  CHECK(Storage.writes == 5 && shelf::state.lastFolder == 0);
  SettingsActivity settings;
  settings.onEnter();
  CHECK(settings.homeCalls == 1 && settings.registryBuilds == 0);
  CHECK(settings.selectedCategoryIndex == 0 && !settings.quickResumeTimeoutAutoEnabled);
  CHECK(Storage.writes == 5);
  const int acquired = lockAcquisitions;
  settings.selectCategory(0);
  CHECK(settings.registryBuilds == 1 && settings.rowRebuilds == 1);
  CHECK(!lockHeld && lockAcquisitions == acquired + 1);
  CHECK(settings.currentSettings == &settings.displaySettings && settings.settingsCount == 1);
  {
    RenderLock caller;
    const int heldCount = lockAcquisitions;
    settings.selectCategory(1);
    CHECK(lockHeld && lockAcquisitions == heldCount);
    CHECK(settings.readingResourcesReady && settings.currentSettings == &settings.readerSettings);
  }
  CHECK(!lockHeld);
  std::printf("Menu entry: %d checks, 0 writes before first presentation, 1 coalesced write after it\n", checks);
}
'''
    harness = harness.replace('@@SHELF@@', bodies)
    # Execute the real loop prologue through persistence. Routing after it is
    # outside this fixture; it needs the full input/display simulator.
    loop = definition(folder, 'void ShelfFolderActivity::loop(')
    loop = loop[:loop.index('  const auto swipe =')] + '}\n'
    harness = harness.replace('@@FOLDER@@', definition(folder, 'void ShelfFolderActivity::onExit(') + '\n' + loop)
    harness = harness.replace('@@SETTINGS@@', '\n'.join(definition(settings, name) for name in [
        'void SettingsActivity::onEnter(', 'void SettingsActivity::selectCategory(']))
    cpp = out / 'menu-entry.cpp'
    cpp.write_text(harness, 'utf8')
    flags = ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-Isrc/apps_local']
    if args.emsdk:
        sdk = args.emsdk.resolve()
        compiler = str(sdk / 'upstream/emscripten/em++.exe' if os.name == 'nt' else sdk / 'upstream/emscripten/em++')
        flags += ['-sENVIRONMENT=node', '-sALLOW_MEMORY_GROWTH=1', '-sEXIT_RUNTIME=1']
        program = out / 'menu-entry.cjs'
        node = sorted((sdk / 'node').glob('*'))[-1] / ('node.exe' if os.name == 'nt' else 'bin/node')
        runtime = [str(node)]
    else:
        compiler = os.environ.get('CXX', 'c++')
        program = out / ('menu-entry.exe' if os.name == 'nt' else 'menu-entry')
        runtime = []
    env = dict(os.environ)
    if args.emsdk:
        env.setdefault('EM_CACHE', str(ROOT / '.pio/em-cache-release-tests'))
    subprocess.run([compiler, *flags, str(cpp), 'src/apps_local/ShelfState.cpp', '-o', str(program)], cwd=ROOT, env=env, check=True)
    return subprocess.run([*runtime, str(program)], cwd=ROOT, env=env).returncode


if __name__ == '__main__':
    sys.exit(main())
