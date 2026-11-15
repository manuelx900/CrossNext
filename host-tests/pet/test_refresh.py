"""Exercise PET's production animation scheduling and display dispatch on paper/browser.

Instrument only the renderer/pack adapters; extract the activity's actual dispatch
blocks so tests fail if a frame re-enters the full grayscale waveform.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(sys.argv[1]).resolve()
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'src/apps_local/pet/PetActivity.cpp').read_text(encoding='utf-8')


def block(first, last):
    assert source.count(first) == 1 and source.count(last) == 1
    start = source.index(first)
    return source[start:source.index(last, start)]


ambient = block('  if (companionClock.scheduleIdle(', '  pet::View before = view;')
presentation = block('  bool grayShown = false;', '  if (model.resourcesReady) {\n    feedingClock.painted(')
program = r'''
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "src/apps_local/pet/PetDisplayPolicy.h"
#include "src/apps_local/pet/PetSleepAnimation.h"
#include "src/apps_local/pet/PetFloating.h"
#include "src/apps_local/pet/PetCompanion.h"
#include "src/apps_local/pet/PetFeeding.h"
uint32_t now = 0;
uint32_t millis() { return now; }
struct Clock { bool running=false; bool active() const { return running; } };
struct Pack { bool available=true; bool ready() const { return available; } };
struct Target { int deviceContext() const { return 0; } };
struct Model { bool resourcesReady=true; };
struct Renderer {
  enum Mode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB } mode=BW;
  bool supported=true, acceptsBase=true;
  unsigned bases=0, grays=0, fasts=0, cleanups=0, clears=0, lsb=0, msb=0;
  struct Caps { bool yes; bool supported() const { return yes; } };
  Caps grayscaleCapabilities(int) const { return {supported}; }
  bool displayGrayscaleBase(int) { ++bases; return acceptsBase; }
  void clearScreen() { ++clears; }
  void setRenderMode(Mode value) { mode=value; }
  void copyGrayscaleLsbBuffers() { ++lsb; }
  void copyGrayscaleMsbBuffers() { ++msb; }
  void displayGrayBuffer() { ++grays; }
  void cleanupGrayscaleWithFrameBuffer() { ++cleanups; }
  void displayBuffer() { ++fasts; }
};
using GfxRenderer=Renderer;
namespace HalDisplay { struct GrayscaleMode { static constexpr int Absolute=0; }; }
bool packReady=true;
unsigned draws=0, failAt=0;
namespace petui {
struct GrayTarget { GrayTarget(Target&,bool) {} };
template<class T> void render(T&,int,int,int,int,const Model&,int) {
  ++draws;
  if(failAt && draws==failAt) packReady=false;
}
}
namespace toybox { void reportOverflow(int,const char*,Renderer*) {} }
struct Activity {
  pet::Profile profile;
  pet::View view=pet::View::Dashboard;
  Clock feedingClock,reactionClock,restClock,curiosityClock;
  petcompanion::Clock companionClock;
  petsleep::Clock sleepClock;
  petfeed::Art feedingArt=petfeed::Art::Extended;
  Pack sleepArt,farewellArt;
  bool loading=false,fullRoomBackgroundsAvailable=true,ready=false;
  unsigned requests=0;
  Model model;
  Renderer renderer;
  Target target;
  int input=0,interactions=0,status=0,work=0;
  void requestUpdate() { ++requests; }
  bool resourcesReady() const { return packReady; }
  void tick() {
    const bool roomFree=!loading && view==pet::View::Dashboard;
AMBIENT
  }
  void paint() {
    petui::render(target, target.deviceContext(), input, interactions, status, model, work);
PRESENTATION
  }
};
int main() {
  // Two minutes of untouched PET: every formerly looping actor and hidden views.
  for(unsigned scenario=0;scenario<8;++scenario) {
    Activity app;
    app.profile.created=true;
    app.profile.saciety=app.profile.joy=app.profile.energy=app.profile.hygiene=100;
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Adult);
    if(scenario==0) app.profile.readingSeconds=0;
    if(scenario==2) app.profile.resting=true;
    if(scenario>=3) app.profile.lifeStatus=pet::LifeStatus::Lost;
    if(scenario==4) app.view=pet::View::Death;
    if(scenario==5) app.view=pet::View::Album;
    if(scenario==6) app.view=pet::View::Home;
    if(scenario==7) { app.view=pet::View::Dashboard; app.loading=true; }
    for(now=0;now<=120000;now+=100) app.tick();
    assert((app.requests>0)==(petdisplay::AMBIENT_MOTION && scenario<5));
  }
  // Awake mood badges do not own a periodic clock, even without idle pose artwork.
  for (const bool sick : {false,true}) {
    Activity app;
    app.profile.created=true;
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Adult);
    app.profile.illness=sick?pet::Illness::Cold:pet::Illness::Healthy;
    app.feedingArt=petfeed::Art::Legacy;
    for(now=0;now<=120000;now+=100) app.tick();
    assert(app.requests==0 && app.sleepClock.step()==0);
  }
  // Every finite-clock combination uses fast paper frames, then gray once settled.
  for(unsigned mask=0;mask<32;++mask) {
    Activity app;
    app.feedingClock.running=mask&1;
    app.reactionClock.running=mask&2;
    app.restClock.running=mask&4;
    if(mask&8) app.companionClock.start(petcompanion::Kind::Greet);
    app.curiosityClock.running=mask&16;
    draws=0;packReady=true;failAt=0;
    app.paint();
    const bool gray=petdisplay::NATIVE_GRAYSCALE;
    assert(app.renderer.bases==unsigned(gray) && app.renderer.grays==unsigned(gray));
    assert(app.renderer.fasts==unsigned(!gray) && app.renderer.cleanups==unsigned(gray));
    assert(app.renderer.mode==Renderer::BW && app.ready);
    assert(draws==(gray?4u:1u));
    app.feedingClock.running=app.reactionClock.running=app.restClock.running=app.curiosityClock.running=false;
    app.companionClock.cancel();
    app.renderer={};draws=0;
    app.paint();
    assert(app.renderer.grays==unsigned(gray) && app.renderer.fasts==unsigned(!gray));
    assert(app.renderer.cleanups==unsigned(gray) && draws==(gray?4u:1u));
  }
  for(unsigned failure=0;failure<5;++failure) {
    Activity app;
    draws=0;packReady=true;failAt=0;
    if(failure==0) app.model.resourcesReady=false;
    if(failure==1) app.fullRoomBackgroundsAvailable=false;
    if(failure==2) app.renderer.supported=false;
    if(failure==3) app.renderer.acceptsBase=false;
    if(failure==4) failAt=2;
    app.paint();
    assert(app.renderer.grays==0 && app.renderer.fasts==1 && app.renderer.mode==Renderer::BW);
    assert(app.renderer.cleanups==unsigned(failure==4 && petdisplay::NATIVE_GRAYSCALE));
  }
  std::printf("PET refresh: %s PASS (idle loops, 32 sequence combinations, settle and failure paths)\n",
              petdisplay::AMBIENT_MOTION ? "simulator" : "physical e-ink");
}
'''.replace('\nAMBIENT\n', '\n' + ambient + '\n').replace('\nPRESENTATION\n', '\n' + presentation + '\n')
path = OUT / 'refresh.cpp'
path.write_text(program, encoding='utf-8')
wasm = os.name == 'nt'
compiler = ROOT / '.venv/emsdk/upstream/emscripten/em++.exe' if wasm else 'c++'
flags = ['-sENVIRONMENT=node', '-sEXIT_RUNTIME=1'] if wasm else []
runtime = [str(ROOT / '.venv/emsdk/node/24.19.0_64bit/node.exe')] if wasm else []
for simulator in (False, True):
    binary = OUT / (('simulator' if simulator else 'paper') + ('.cjs' if wasm else ''))
    subprocess.run([str(compiler), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I.',
                    *(['-DSIMULATOR'] if simulator else []), str(path), 'src/apps_local/pet/PetState.cpp',
                    *flags, '-o', str(binary)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(binary)], cwd=ROOT, check=True)
