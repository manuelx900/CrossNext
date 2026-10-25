"""Exercise production scan/retry methods with journal and storage test adapters.

The action prefix is extracted through its dispatch point so Back uses the same
draft/reset ordering as the activity. Rendering and unrelated actions are stubbed.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(sys.argv[1]).resolve()
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / "src/apps_local/pet/PetActivity.cpp").read_text("utf-8")


def method(name):
    start = source.index("PetActivity::" + name + "(")
    start = source.rfind("\n", 0, start) + 1
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


prefix = method("action").split("  switch (action) {", 1)[0]
prefix += "  if (action == petui::Back) back();\n}\n"
methods = "\n".join(method(n) for n in ["scan", "finishScan", "updateHabits", "archiveDraft", "updateCare", "commit", "back", "show", "drawPetArt"])
program = r'''
#include <HalStorage.h>
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <ctime>
#include <vector>
#include "src/apps_local/pet/PetState.h"
#include "src/apps_local/pet/PetStore.h"
#include "src/apps_local/pet/PetHistory.h"
#include "src/apps_local/pet/PetReadingTimeline.h"
#include "src/apps_local/reading/ReadingStatsCore.h"
#define tr(id) #id
namespace fui { using ActionId = uint16_t; constexpr ActionId NO_ACTION = 0; struct DrawTarget {}; struct Rect {}; }
namespace petui { enum class NoticeKind { Information, Fed }; struct FeedResult { uint8_t before=0,after=0; bool visible=false; }; constexpr fui::ActionId Back = 1, Read = 2, AcknowledgeDeath = 4; constexpr unsigned PETTING_HAND_ART = 63; }
namespace shelf { void leave(int, int) {} }
struct Settings { bool weekStartsSunday = false; } SETTINGS;
namespace readingstats { enum class ScanResult { Record, More, Finished, Error }; }
struct Journal {
  std::vector<readingstats::Record> records;
  size_t cursor = 0;
  void close() {}
  bool hadDamage() const { return false; }
  readingstats::ScanResult next(readingstats::Record& record) {
    if (cursor == records.size()) return readingstats::ScanResult::Finished;
    record = records[cursor++]; return readingstats::ScanResult::Record;
  }
};
struct ProfileStore {
  pet::Store real;
  bool failOnce = false;
  bool save(pet::Profile& p, bool writable) {
    if (failOnce) { failOnce = false; return false; }
    return real.save(p, writable);
  }
};
struct PetActivity {
  struct ArtProbe { unsigned draws=0, count=63; } art;
  bool pettingHandAvailable=false;
  static bool drawArt(void* p, fui::DrawTarget&, fui::Rect, uint32_t id, bool) { auto& a=*static_cast<ArtProbe*>(p); ++a.draws; return id<a.count; }
  static bool drawPetArt(void*, fui::DrawTarget&, fui::Rect, uint32_t, bool);
  pet::Profile profile, draft;
  pet::History history, historyDraft;
  pet::HistoryStore historyStore;
  ProfileStore store;
  Journal readings;
  readingstats::Summary summary;
  readingstats::Record record;
  pet::ReadingTimeline readingTimeline;
  bool scanChanged=false, loading=true, loaded=true, historyChanged=false, reconciled=false;
  bool writable=true, historyWritable=true;
  int today=0, todaySeconds=0, weekDays=0, page=0, renderer=0, mappedInput=0;
  int64_t lastReadingAt=0, scanNow=0;
  pet::View view=pet::View::Dashboard, noticeReturn=view, returnView=view;
  pet::View lifeReturn=pet::View::Lives, memoryReturn=pet::View::Memories, affectionReturn=pet::View::Care;
  pet::Mode selectedMode=pet::Mode::Normal;
  uint8_t selectedGoal=1;
  pet::Tab tab=pet::Tab::Pet;
  struct Clearable { void clear() {} } pendingTap,pendingSlide;
  struct Cursor { void setFocusedIndex(int) {} } interactions;
  uint32_t viewRevision=0;
  int careResult=0,gameResult=0;
  petui::FeedResult feedResult;
  petui::NoticeKind noticeKind=petui::NoticeKind::Information;
  void syncCollectionArt() {}
  void show(pet::View);
  void feedAction(int value);
  void notice(const char*, pet::View next, petui::NoticeKind kind=petui::NoticeKind::Information) { noticeKind=kind; if(next!=pet::View::Notice)noticeReturn=next;show(pet::View::Notice); }
  void syncFarewellArt() {}
  void acknowledge() {}
  void changeTab(int) {}
  bool refreshDay() { return updateCare(); }
  void requestUpdate() {}
  bool ready=false;
  void action(fui::ActionId, int=0);
  void scan(); void finishScan(bool); bool updateHabits(); bool archiveDraft(); bool updateCare(); bool commit(); void back();
};
'''
feed = method("action").split("    case petui::Feed:\n",1)[1].split("    case petui::Stroke:",1)[0]
program += methods + prefix + "void PetActivity::feedAction(int value) { draft=profile;\n" + feed + "}\n"
program += r'''
readingstats::Record reading(int64_t at, uint32_t seconds) {
  readingstats::Record r; r.epoch=at; r.seconds=seconds;
  time_t stamp=at; tm local{}; localtime_r(&stamp,&local);
  r.localDay=readingstats::dayIndex({local.tm_year+1900,local.tm_mon+1,local.tm_mday});
  r.secondOfDay=unsigned(local.tm_hour*3600+local.tm_min*60+local.tm_sec);
  r.book.lastEpoch=at+seconds;
  return r;
}
void seed(PetActivity& app, bool egg=false) {
  testStorage={};
  const auto now=std::time(nullptr);
  app.profile.created=true; app.profile.mode=pet::Mode::Normal;
  app.profile.readingSeconds=egg?0:1200; app.profile.noticedSeconds=app.profile.readingSeconds;
  app.profile.noticedStage=pet::stage(app.profile.readingSeconds);
  app.profile.lastReadAt=now-8*86400; app.profile.careAt=app.profile.lastReadAt;
  app.profile.bornDay=reading(app.profile.lastReadAt-1200,1200).localDay;
  app.today=reading(now,1).localDay; app.summary.anchorDay=app.today;
  app.draft=app.profile; app.scanNow=now; app.readingTimeline.begin(app.profile,now);
  if(!egg)app.readings.records.push_back(reading(app.profile.lastReadAt-1200,1200));
}
int main() {
  for(unsigned sat : {0u,55u,90u,99u}) for(bool failed : {false,true}) {
    PetActivity app; seed(app); app.profile.mode=pet::Mode::Relaxed;
    app.profile.readingSeconds=72000; app.profile.earnedFood=240; app.profile.saciety=uint8_t(sat); app.view=pet::View::Care;
    const auto oldFood=pet::food(app.profile); app.store.failOnce=failed;
    app.feedAction(0);
    assert(app.view==pet::View::Notice);
    if(failed) {
      assert(app.profile.saciety==sat && pet::food(app.profile)==oldFood);
      assert(app.noticeKind==petui::NoticeKind::Information && !app.feedResult.visible);
    } else {
      assert(app.profile.saciety==std::min(100u,sat+25) && pet::food(app.profile)==oldFood-1);
      assert(app.noticeKind==petui::NoticeKind::Fed && app.feedResult.visible);
      assert(app.feedResult.before==sat && app.feedResult.after==app.profile.saciety);
      app.back(); assert(app.view==pet::View::Care && !app.feedResult.visible);
    }
  }
  {
    PetActivity app; fui::DrawTarget target;
    assert(!PetActivity::drawPetArt(&app,target,{},63,false) && app.art.draws==0);
    assert(PetActivity::drawPetArt(&app,target,{},0,false) && app.art.draws==1);
    app.pettingHandAvailable=true; app.art.count=64;
    assert(PetActivity::drawPetArt(&app,target,{},63,false) && app.art.draws==2);
  }
  {
    PetActivity app; seed(app);
    const auto renewed=app.profile.lastReadAt+6*86400;
    app.readings.records.push_back(reading(renewed,300));
    app.store.failOnce=true; app.scan();
    assert(app.view==pet::View::Notice && !app.reconciled);
    assert(app.draft.readingSeconds==1500 && app.profile.readingSeconds==1200);
    assert(app.profile.earnedFood==0 && app.draft.earnedFood==1);
    app.action(petui::Back);
    app.action(3);
    assert(app.reconciled && app.profile.readingSeconds==1500 && app.profile.lastReadAt==renewed+300);
    assert(app.profile.earnedFood==1);
    app.updateCare(); assert(app.profile.lifeStatus==pet::LifeStatus::Active && app.profile.earnedFood==1);
  }
  for(bool archiveFailure : {false,true}) {
    PetActivity app; seed(app,true);
    if(archiveFailure)testStorage.available=false;else app.store.failOnce=true;
    app.scan();
    assert(app.view==pet::View::Notice && !app.reconciled && app.profile.lifeStatus==pet::LifeStatus::Active);
    app.action(petui::Back); testStorage.available=true;
    app.action(3);
    assert(app.reconciled && app.profile.lifeStatus==pet::LifeStatus::Lost && app.history.count==1);
    assert(app.profile.endedAt==app.profile.lastReadAt+pet::absenceGrace(app.profile.mode));
  }
  {
    PetActivity app; seed(app);
    app.readings.records.push_back(reading(std::time(nullptr)-600,300));
    app.scan();
    assert(app.reconciled && app.profile.lifeStatus==pet::LifeStatus::Lost && app.profile.readingSeconds==1200);
  }
  {
    PetActivity app;
    app.today=20000; app.summary.anchorDay=app.today;
    app.profile.created=true; app.profile.bornDay=app.today-8; app.profile.birthDaySeconds=600;
    app.summary.dailySeconds[8]=600; app.summary.dailySeconds[7]=299;
    for(unsigned n : {6u,5u,4u,2u,1u}) app.summary.dailySeconds[n]=300;
    app.draft=app.profile;
    assert(app.updateHabits() && app.draft.streak==3 && app.draft.sharedDays==5);
    app.profile=app.draft;
    std::fill(std::begin(app.summary.dailySeconds),std::end(app.summary.dailySeconds),0);
    assert(!app.updateHabits() && app.draft.streak==3);
    for(unsigned n=0;n<5;++n) app.summary.dailySeconds[n]=300;
    assert(app.updateHabits() && app.draft.streak==5);
    app.profile=app.draft;
    for(unsigned goal=0;goal<4;++goal) {
      app.profile.goalIndex=uint8_t(goal);app.draft=app.profile;
      assert(!app.updateHabits() && app.draft.streak==5);
    }
    app.profile.lifeStatus=pet::LifeStatus::Retired;app.draft=app.profile;
    assert(!app.updateHabits());
  }
  {
    PetActivity app;
    app.today=20000;app.summary.anchorDay=app.today;
    app.profile.created=true;app.profile.bornDay=app.today-4;app.profile.birthDaySeconds=600;
    for(unsigned n=0;n<4;++n)app.summary.dailySeconds[n]=300;
    app.summary.dailySeconds[4]=899;app.draft=app.profile;
    assert(app.updateHabits() && app.draft.streak==4 && app.draft.sharedDays==4);
    app.summary.dailySeconds[4]=900;
    assert(app.updateHabits() && app.draft.streak==5 && app.draft.sharedDays==5);
  }
  std::puts("PET Activity scan: chronological renewal, late reading, failed profile/archive writes notice Back retry and best-streak preservation passed");
}
'''
(OUT / "activity-scan.cpp").write_text(program, "utf-8")
wasm = os.name == "nt"
compiler = ROOT / ".venv/emsdk/upstream/emscripten/em++.exe" if wasm else "c++"
binary = OUT / ("activity-scan.cjs" if wasm else "activity-scan")
flags = ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1"] if wasm else []
subprocess.run([str(compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                "-I.", "-Ihost-tests/habits/stubs", str(OUT / "activity-scan.cpp"),
                "src/apps_local/pet/PetState.cpp", "src/apps_local/pet/PetStore.cpp",
                "src/apps_local/pet/PetHistory.cpp", "src/apps_local/reading/ReadingStatsCore.cpp",
                *flags, "-o", str(binary)], cwd=ROOT, check=True)
command = [str(ROOT / ".venv/emsdk/node/24.19.0_64bit/node.exe")] if wasm else []
subprocess.run([*command, str(binary)], cwd=ROOT, check=True)
