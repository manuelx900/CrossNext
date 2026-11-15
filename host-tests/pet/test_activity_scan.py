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
methods = "\n".join(method(n) for n in ["scan", "finishScan", "updateHabits", "archiveDraft", "updateCare", "commit", "back", "show", "beginRestTransition", "drawPetArt", "nextTab", "changeTab", "createCompanion", "react", "acknowledge", "wakeBeforeCare"])
program = r'''
#include <HalStorage.h>
#include "src/apps_local/pet/PetReadingDisplay.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <ctime>
#include <memory>
#include <vector>
#include "src/apps_local/pet/PetState.h"
#include "src/apps_local/pet/PetSleepAnimation.h"
#include "src/apps_local/pet/PetFeeding.h"
#include "src/apps_local/pet/PetCuriosity.h"
#include "src/apps_local/pet/PetCompanion.h"
#include "src/apps_local/pet/PetRestTransition.h"
#include "src/apps_local/pet/PetReaction.h"
#include "src/apps_local/pet/PetStore.h"
#include "src/apps_local/pet/PetHistory.h"
#include "src/apps_local/pet/PetReadingTimeline.h"
#include "src/apps_local/reading/ReadingStatsCore.h"
#define tr(id) #id
namespace fui { using ActionId = uint16_t; constexpr ActionId NO_ACTION = 0; struct DrawTarget {}; struct Rect {}; }
namespace petui { enum class NoticeKind { Information, Fed }; struct FeedResult { uint8_t before=0,after=0; bool visible=false; }; constexpr fui::ActionId Back = 1, Read = 2, AcknowledgeDeath = 4; constexpr unsigned PETTING_HAND_ART = 63; }
namespace petui { constexpr fui::ActionId OpenFood=5,Feed=6,SelectFood=7,Stroke=8,Clean=9,Cure=10,Rest=11; }
namespace petui {
enum class GameOutcome { Rewarded, Practice, CareNeeded };
struct GameResult { uint8_t joyBefore=0,joyAfter=0,energyBefore=0,energyAfter=0;GameOutcome outcome=GameOutcome::Practice; };
struct CareResult { uint8_t hygieneBefore=0,hygieneAfter=0,joyBefore=0,joyAfter=0,medicineSpent=0;bool visible=false; };
}
namespace shelf { void leave(int, int) {} }
struct Settings { bool weekStartsSunday = false; } SETTINGS;
uint32_t fakeNow=1000;
inline uint32_t millis() { return fakeNow; }
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
  unsigned saves = 0;
  bool save(pet::Profile& p, bool writable) {
    ++saves;
    if (failOnce) { failOnce = false; return false; }
    return real.save(p, writable);
  }
};
struct PetActivity {
  petcompanion::Clock companionClock;
  petcompanion::Kind nextTouchPose=petcompanion::Kind::Sit;
  petrest::Clock restClock;
  petrest::Kind restPending=petrest::Kind::None;
  bool restArtAvailable=false,discoveryPending=false;
  petfeed::Art feedingArt=petfeed::Art::Progressive;
  petsleep::Clock sleepClock;
  petfeed::Clock feedingClock;
  petcurious::Clock curiosityClock;
  petreact::Clock reactionClock;
  pet::GameState game;
  pet::Affection affection;
  bool curiosityAvailable=false, curiosityOnEntry=true;
  bool choosingFood=false;
  uint8_t selectedFood=0;
  bool animationRebasePending = false;
  struct ArtProbe { unsigned draws=0, count=63; } art;
  bool pettingHandAvailable=false;
  static bool drawArt(void* p, fui::DrawTarget&, fui::Rect, uint32_t id, bool) { auto& a=*static_cast<ArtProbe*>(p); ++a.draws; return id<a.count; }
  static bool drawPetArt(void*, fui::DrawTarget&, fui::Rect, uint32_t, bool);
  pet::Profile profile, draft;
  pet::Species chosenSpecies=pet::Species::Silva;
  char nameDraft[pet::NAME_BYTES]{};
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
  pet::Tab tab=pet::Tab::Pet;
  struct Clearable { void clear() {} } pendingTap,pendingSlide;
  struct { void close() {} } nameKeyboard;
  struct Cursor { void setFocusedIndex(int) {} } interactions;
  uint32_t viewRevision=0;
  petui::CareResult careResult;
  petui::GameResult gameResult;
  petui::FeedResult feedResult;
  petui::ReadingDisplay readingDisplay=petui::ReadingDisplay::Project;
  petui::NoticeKind noticeKind=petui::NoticeKind::Information;
  void syncCollectionArt() {}
  void beginRestTransition();
  void show(pet::View);
  void feedAction(int value);
  void touchAction();
  void foodPageAction(int value);
  void selectFoodAction(int value);
  void openFoodAction();
  void strokeAction();
  void entryGreeting();
  void roomAdvance();
  void advanceRest();
  void cleanAction();void cureAction();void gameDoneAction();void continueAction();void dismissReactionAction();void restAction();
  void toggleReadingAction();
  void react(petreact::Kind);
  void notice(const char*, pet::View next, petui::NoticeKind kind=petui::NoticeKind::Information) { noticeKind=kind; if(next!=pet::View::Notice)noticeReturn=next;show(pet::View::Notice); }
  void syncFarewellArt() {}
  void acknowledge(bool collectProject=false);
  void changeTab(int); int nextTab(int) const; void createCompanion();
  bool refreshDay() { return updateCare(); }
  bool resourcesReady() const { return true; }
  void requestUpdate() {}
  bool ready=false;
  void action(fui::ActionId, int=0);
  bool wakeBeforeCare(fui::ActionId);
  void scan(); void finishScan(bool); bool updateHabits(); bool archiveDraft(); bool updateCare(); bool commit(); void back();
};
'''
feed = method("action").split("    case petui::Feed:\n",1)[1].split("    case petui::Stroke:",1)[0]
touch=method("action").split("    case petui::TouchCompanion:\n",1)[1].split("    case petui::OpenAlbum:",1)[0]
program += "void PetActivity::touchAction() {\n" + touch + "}\n"
program += methods + prefix + "void PetActivity::feedAction(int value) { draft=profile;\n" + feed + "}\n"
for name, case, following, args in [('foodPageAction','FoodPage','SelectFood','int value'),('openFoodAction','OpenFood','FoodPage','')]:
    body=method("action").split("    case petui::"+case+":\n",1)[1].split("    case petui::"+following+":",1)[0]
    program += "void PetActivity::"+name+"("+args+") {\n"+body+"}\n"
program += "void PetActivity::selectFoodAction(int value) { draft=profile;\n" + feed + "}\n"
stroke=method("action").split("    case petui::Stroke:\n",1)[1].split("    case petui::Rest:",1)[0]
program += "void PetActivity::strokeAction() { draft=profile;\n" + stroke + "}\n"
loop=method("loop")
first=loop.index("  if (!loading && curiosityOnEntry")
last=loop.index("  if (curiosityClock.active()",first)
program += "void PetActivity::entryGreeting() {\n" + loop[first:last] + "}\n"
first=loop.index('  const auto reaction = reactionClock.kind();')
last=loop.index('  const auto feedingBefore',first)
program+='void PetActivity::roomAdvance() {\n'+loop[first:last]+'}\n'
first=loop.index('  if (restClock.active()')
last=loop.index('  if (!loading && companionClock.update',first)
program+='void PetActivity::advanceRest() {\n'+loop[first:last]+'}\n'
for name,case,following in [('cleanAction','Clean','Cure'),('cureAction','Cure','OpenLife'),('gameDoneAction','GameDone','OpenHealth'),('continueAction','Continue','DismissReaction'),('dismissReactionAction','DismissReaction','NewCompanion'),('restAction','Rest','OpenGames'),('toggleReadingAction','ToggleReadingDisplay','Read')]:
    start=method('action').split('    case petui::'+case+':',1)[1]
    if case=='DismissReaction':
        end=start.index('    case petui::') if '    case petui::' in start else -1
        body=start[:end]
    else:body=start.split('    case petui::'+following+':',1)[0]
    program+='void PetActivity::'+name+'() { draft=profile;\n'+body+'}\n'
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
  {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned;
    app.profile.created=true; app.viewRevision=10;
    app.changeTab(1); assert(app.viewRevision==10 && app.view==pet::View::Dashboard);
    app.changeTab(2); assert(app.viewRevision==10 && app.view==pet::View::Dashboard);
    assert(app.nextTab(1)==int(pet::Tab::Album) && app.nextTab(-1)==int(pet::Tab::Album));
    for (int n : {-8,-3,-2,0,2,3,8}) assert(app.nextTab(n)==int(n%2 ? pet::Tab::Album : pet::Tab::Pet));
    app.tab=pet::Tab::Album;
    for (int n : {-8,-3,-2,0,2,3,8}) assert(app.nextTab(n)==int(n%2 ? pet::Tab::Pet : pet::Tab::Album));
    assert(app.nextTab(1)==int(pet::Tab::Pet) && app.nextTab(-1)==int(pet::Tab::Pet));
    app.tab=pet::Tab::Pet;
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Newborn);
    assert(app.nextTab(1)==int(pet::Tab::Care) && app.nextTab(-1)==int(pet::Tab::Album));
    app.changeTab(0); assert(app.viewRevision==10);
    app.changeTab(1); assert(app.viewRevision==11 && app.view==pet::View::Care);
    app.changeTab(1); assert(app.viewRevision==11);
    app.changeTab(0); assert(app.viewRevision==12 && app.view==pet::View::Dashboard);
    assert(app.animationRebasePending && !app.sleepClock.update(1000,true));
  }
  {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned; seed(app);
    app.profile.readingSeconds=7200;
    app.profile.project=9; app.profile.projectStartedSeconds=6000;
    for(unsigned id=0;id<5;++id)pet::own(app.profile,id);
    app.profile.lifeStatus=pet::LifeStatus::Retired; app.profile.endedAt=std::time(nullptr);
    app.draft=app.profile; app.summary.seconds=10000; app.view=pet::View::Name;
    app.createCompanion();
    assert(app.profile.generation==2 && app.profile.project==9);
    assert(pet::projectProgressSeconds(app.profile)==1200 && pet::projectRemaining(app.profile)==2400);
    assert(pet::ownedCount(app.profile)==5 && app.view==pet::View::Dashboard);
    assert(pet::addReading(app.profile,app.profile.baselineSeconds+2400) && !pet::owns(app.profile,9));
    assert(pet::projectRemaining(app.profile)==0);
    app.view=pet::View::Reward;app.continueAction();
    assert(pet::owns(app.profile,9) && app.profile.equipped[pet::objectSlot(9)]==9);
  }
  for(unsigned sat : {0u,55u,90u,99u}) for(bool failed : {false,true}) {
    PetActivity app; seed(app); app.profile.mode=pet::Mode::Relaxed;
    app.profile.readingSeconds=72000; app.profile.earnedFood=240; app.profile.saciety=uint8_t(sat); app.view=pet::View::Care;
    const auto oldFood=pet::food(app.profile); app.store.failOnce=failed;
    app.choosingFood=true; app.selectedFood=5;
    app.selectedFood=0; app.selectFoodAction(5);
    if(failed) {
      assert(app.view==pet::View::Notice && !app.feedingClock.active());
      assert(app.profile.saciety==sat && pet::food(app.profile)==oldFood);
      assert(app.noticeKind==petui::NoticeKind::Information && !app.feedResult.visible);
    } else {
      assert(app.profile.saciety==std::min(100u,sat+25) && pet::food(app.profile)==oldFood-1);
      assert(app.view==pet::View::Dashboard && app.feedResult.visible && app.feedingClock.active());
      assert(app.profile.foodChoice==5 && !app.choosingFood);
      assert(app.feedResult.before==sat && app.feedResult.after==app.profile.saciety);
      const auto saves=app.store.saves;
      uint32_t now=1000;
      while(app.feedingClock.active()) {app.feedingClock.painted(now);now+=2400;app.feedingClock.update(now);}
      assert(app.store.saves==saves && pet::food(app.profile)==oldFood-1);
      app.feedAction(5); assert(pet::food(app.profile)==oldFood-1);
      app.show(pet::View::Care); assert(!app.feedResult.visible && !app.feedingClock.active());
    }
  }
  {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned; seed(app);
    app.profile.foodChoice=5; app.view=pet::View::Care;
    app.openFoodAction(); assert(app.choosingFood && app.selectedFood==5);
    const auto saved=app.profile;
    app.foodPageAction(1); assert(app.selectedFood==5);
    app.foodPageAction(-1); assert(app.selectedFood==0);
    app.foodPageAction(-1); assert(app.selectedFood==0);
    app.foodPageAction(2); assert(app.selectedFood==0);

    app.foodPageAction(1); assert(app.selectedFood==3);
    app.selectFoodAction(6); assert(app.selectedFood==3);
    assert(!std::memcmp(&saved,&app.profile,sizeof(saved)));
    app.back(); assert(app.view==pet::View::Care && !app.choosingFood);
    app.foodPageAction(-1); assert(app.selectedFood==3);
  }
  for (auto view : {pet::View::Dashboard,pet::View::Health,pet::View::Album}) {
    auto owned=std::make_unique<PetActivity>();auto& app=*owned;seed(app);
    app.view=view;app.profile.resting=true;app.draft=app.profile;app.draft.resting=false;
    assert(app.commit());
    assert(app.restPending==(view==pet::View::Dashboard?petrest::Kind::Wake:petrest::Kind::None));
  }
  for (bool assets : {false,true}) for (bool hasBed : {false,true}) {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned; seed(app);
    app.loading=false;app.profile.illness=pet::Illness::Healthy;app.profile.resting=false;
    app.profile.mode=pet::Mode::Relaxed;app.restArtAvailable=assets;app.feedingArt=petfeed::Art::Extended;
    if(hasBed){app.profile.equipped[0]=pet::objectAt(0,0);pet::own(app.profile,app.profile.equipped[0]);}
    app.restAction();
    assert(app.profile.resting && app.restPending==petrest::Kind::None && app.restClock.active()==assets);
    if(assets)assert(app.restClock.phase()==(hasBed?petrest::Phase::ApproachFirst:petrest::Phase::SettleFirst));
    app.entryGreeting();assert(app.restClock.active()==assets);
    app.advanceRest();assert(app.restClock.active()==assets);
    const auto saved=app.profile;const auto saves=app.store.saves;
    app.touchAction();assert(!std::memcmp(&saved,&app.profile,sizeof(saved)) && saves==app.store.saves);
    app.show(pet::View::Care);assert(!app.restClock.active() && app.restPending==petrest::Kind::None);
    app.restAction();
    assert(!app.profile.resting && app.restPending==petrest::Kind::None && app.restClock.active()==assets);
    if(assets)assert(app.restClock.phase()==(hasBed?petrest::Phase::Exit:petrest::Phase::StretchFirst));
    app.entryGreeting();
    app.advanceRest();assert(app.restClock.active()==assets);
    if(!assets)assert(app.companionClock.kind()==petcompanion::Kind::Stretch);
    app.show(pet::View::Care); app.store.failOnce=true;
    app.restAction();assert(!app.profile.resting && !app.restClock.active() && app.restPending==petrest::Kind::None);
  }
  for(bool resting : {false,true}) {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned; seed(app);
    app.profile.mode=pet::Mode::Relaxed;app.profile.saciety=80;app.profile.joy=80;app.profile.hygiene=80;
    app.profile.resting=resting;app.profile.illness=resting?pet::Illness::Cold:pet::Illness::Healthy;
    const auto saved=app.profile;const auto saves=app.store.saves;
    app.touchAction();
    assert(!std::memcmp(&saved,&app.profile,sizeof(saved)) && saves==app.store.saves);
    assert(app.companionClock.kind()==(resting?petcompanion::Kind::WakeFirst:petcompanion::Kind::Sit));
    app.show(pet::View::Care);assert(!app.companionClock.active());
    app.touchAction();assert(!app.companionClock.active());
  }
  for(unsigned choice=0;choice<petfeed::FOOD_COUNT;++choice) {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned; seed(app);
    app.feedingArt=petfeed::Art::Extended;
    app.profile.mode=pet::Mode::Relaxed;app.profile.earnedFood=20;app.profile.saciety=50;
    app.view=pet::View::Care;app.choosingFood=true;app.selectedFood=uint8_t((choice+1)%petfeed::FOOD_COUNT);
    app.selectFoodAction(int(choice));
    assert(app.view==pet::View::Dashboard && app.profile.foodChoice==choice && app.selectedFood==choice);
    assert(app.profile.saciety==75 && app.profile.spentFood==1 && app.feedingClock.active());
    const auto saves=app.store.saves;
    app.selectFoodAction(int(choice));
    assert(app.profile.spentFood==1 && app.store.saves==saves);
  }
  for(unsigned mode=0;mode<6;++mode) {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned;
    seed(app); app.profile.mode=pet::Mode::Relaxed;
    app.profile.earnedFood=10; app.profile.saciety=50; app.view=pet::View::Care;
    app.choosingFood=true;
    if(mode==0)app.writable=false;
    if(mode==1)app.profile.saciety=100;
    if(mode==2)app.profile.earnedFood=0;
    if(mode==3)app.profile.readingSeconds=0;
    if(mode==4)app.choosingFood=false;
    const auto before=app.profile;
    app.selectFoodAction(mode==5?6:0);
    assert(!app.feedingClock.active() && !app.feedResult.visible);
    assert(app.profile.spentFood==before.spentFood && app.profile.saciety==before.saciety);
  }
  {
    PetActivity app; fui::DrawTarget target;
    assert(!PetActivity::drawPetArt(&app,target,{},63,false) && app.art.draws==0);
    assert(PetActivity::drawPetArt(&app,target,{},0,false) && app.art.draws==1);
    app.pettingHandAvailable=true; app.art.count=64;
    assert(PetActivity::drawPetArt(&app,target,{},63,false) && app.art.draws==2);
  }
  for(unsigned blocked=0;blocked<4;++blocked) {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned; seed(app);
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Juvenile);
    app.profile.saciety=app.profile.energy=app.profile.hygiene=100;
    app.profile.joy=80; app.profile.lastPetDay=0;
    app.view=pet::View::Care; app.curiosityAvailable=blocked!=1;
    if(blocked==2)app.profile.saciety=29;
    if(blocked==3)app.store.failOnce=true;
    const auto before=app.profile;
    app.strokeAction();
    if(blocked<3) {
      assert(app.view==pet::View::Dashboard && app.reactionClock.kind()==petreact::Kind::Petting);
      assert(app.profile.joy==90 && app.store.saves==1);
      app.strokeAction();
      assert(app.reactionClock.active() && app.profile.joy==90 && app.store.saves==1);
      app.show(pet::View::Care); assert(!app.reactionClock.active());
    } else {
      assert(!app.reactionClock.active() && app.view==pet::View::Notice);
      if(blocked==3) assert(!std::memcmp(&before,&app.profile,sizeof(before)));
    }
  }
  for(bool evolution : {false,true}) {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned; seed(app);
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Juvenile);
    app.profile.saciety=app.profile.energy=app.profile.hygiene=app.profile.joy=100;
    app.loading=false;app.curiosityAvailable=true;
    if(evolution) {
      app.show(pet::View::Evolution);app.entryGreeting();
      assert(app.curiosityOnEntry && !app.curiosityClock.active());
    }
    app.show(pet::View::Reward);app.entryGreeting();
    assert(app.curiosityOnEntry && !app.curiosityClock.active());
    app.show(pet::View::Dashboard);app.entryGreeting();
    assert(!app.curiosityOnEntry && app.curiosityClock.active() && app.store.saves==0);
    app.curiosityClock.cancel();app.entryGreeting();
    assert(!app.curiosityClock.active() && app.store.saves==0);
  }
  for(unsigned action=0;action<3;++action) {
    if(action==0) {
      auto owned=std::make_unique<PetActivity>();auto& app=*owned;seed(app);
      app.profile.readingSeconds=pet::stageStart(pet::Stage::Juvenile);app.view=pet::View::Dashboard;
      const auto saved=app.profile;
      app.toggleReadingAction();assert(app.readingDisplay==petui::ReadingDisplay::Lifetime);
      app.toggleReadingAction();assert(app.readingDisplay==petui::ReadingDisplay::Project);
      assert(app.store.saves==0 && !std::memcmp(&saved,&app.profile,sizeof(saved)));
      pet::own(app.profile,app.profile.project);app.toggleReadingAction();
      assert(app.readingDisplay==petui::ReadingDisplay::Project && app.store.saves==0);
    }
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned; seed(app);
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Juvenile);
    app.profile.saciety=app.profile.energy=100;app.profile.joy=80;app.profile.hygiene=20;
    app.view=pet::View::Care;
    if(action==0) {app.profile.resting=false;app.profile.restAt=0;app.cleanAction();}
    if(action==1) {app.profile.illness=pet::Illness::Cold;app.cureAction();}
    if(action==2) {app.view=pet::View::Play;app.game.phase=pet::GamePhase::Finished;app.gameDoneAction();}
    assert(app.view==pet::View::Dashboard && app.reactionClock.active() && app.store.saves==1);
    assert(app.reactionClock.kind()==(action==0?petreact::Kind::Cleaning:action==1?petreact::Kind::Healing:petreact::Kind::Game));
    if(action==0)assert(app.profile.hygiene==100 && !app.profile.resting && app.profile.restAt==0 && app.careResult.hygieneBefore==20 && app.careResult.hygieneAfter==100);
    if(action==1)assert(app.profile.illness==pet::Illness::Healthy && app.profile.spentMedicine==1 && app.careResult.medicineSpent==1);
    if(action==2) {const auto wins=app.profile.gamesWon;app.gameDoneAction();assert(app.store.saves==1 && app.profile.gamesWon==wins);}
    const auto saved=app.profile;
    const auto revision=app.viewRevision;
    app.dismissReactionAction();assert(!app.reactionClock.active() && !std::memcmp(&saved,&app.profile,sizeof(saved)) && app.viewRevision>revision);
    app.react(action==0?petreact::Kind::Cleaning:action==1?petreact::Kind::Healing:petreact::Kind::Game);
    app.loading=false;const auto replayRevision=app.viewRevision;
    while(app.reactionClock.active()) {
      app.reactionClock.painted(fakeNow,app.reactionClock.kind(),app.reactionClock.phase());fakeNow+=10000;app.roomAdvance();
    }
    assert(app.view==pet::View::Dashboard && app.viewRevision>replayRevision && app.store.saves==1);
    assert(!std::memcmp(&saved,&app.profile,sizeof(saved)));fakeNow=1000;
  }
  {
    auto owned=std::make_unique<PetActivity>();auto& app=*owned;seed(app);
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Juvenile);app.profile.hygiene=44;
    app.profile.resting=false;app.profile.restAt=0;app.store.failOnce=true;
    app.cleanAction();
    assert(!app.profile.resting && app.profile.restAt==0 && app.profile.hygiene==44 && !app.reactionClock.active());
  }
  {
    auto owned=std::make_unique<PetActivity>();auto& app=*owned;seed(app);
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Juvenile);app.view=pet::View::Care;
    app.restAction();assert(app.view==pet::View::Dashboard && app.profile.resting && app.store.saves==1);
    app.restAction();assert(app.view==pet::View::Dashboard && !app.profile.resting && app.store.saves==2);
  }
  {
    auto owned=std::make_unique<PetActivity>(); auto& app=*owned;seed(app);
    app.profile.readingSeconds=pet::stageStart(pet::Stage::Juvenile);app.profile.noticedStage=pet::Stage::Baby;
    app.loading=false;app.show(pet::View::Evolution);app.continueAction();
    assert(app.reactionClock.kind()==petreact::Kind::Growth && app.store.saves==0);
    while(app.reactionClock.active()) {
      const auto phase=app.reactionClock.phase();
      app.continueAction();assert(app.reactionClock.phase()==phase && app.store.saves==0);
      app.reactionClock.painted(fakeNow,app.reactionClock.kind(),phase);fakeNow+=10000;app.roomAdvance();
    }
    assert(app.view==pet::View::Reward && app.profile.noticedStage==pet::Stage::Baby && app.store.saves==0);
    app.continueAction();assert(app.view==pet::View::Dashboard && app.profile.noticedStage==pet::Stage::Juvenile && app.store.saves==1);
    app.continueAction();assert(app.store.saves==1);fakeNow=1000;
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
    app.ready=true;
    for(auto panel : {pet::View::Care,pet::View::Games,pet::View::Home}) {
      app.view=panel;
      const auto sequence=app.profile.sequence;
      app.action(petui::Back);
      assert(app.view==pet::View::Dashboard && app.tab==pet::Tab::Pet);
      assert(app.profile.sequence==sequence);
    }
  }
  {
    PetActivity app;
    app.profile.created=true;app.profile.readingSeconds=7200;app.profile.resting=true;
    for(auto blocked : {petui::OpenFood,petui::Feed,petui::SelectFood,petui::Stroke,petui::Clean,petui::Cure}) {
      app.view=pet::View::Health;app.choosingFood=true;
      const auto before=app.profile;
      assert(app.wakeBeforeCare(blocked));
      assert(app.view==pet::View::Care && app.tab==pet::Tab::Care && !app.choosingFood);
      assert(!std::memcmp(&before,&app.profile,sizeof(before)));
    }
    assert(!app.wakeBeforeCare(petui::Rest) && !app.wakeBeforeCare(petui::Back));
    app.profile.resting=false;
    assert(!app.wakeBeforeCare(petui::Feed) && !app.wakeBeforeCare(petui::Cure));
  }
  std::puts("PET Activity scan and wake-before-care routing passed");
}
'''
(OUT / "activity-scan.cpp").write_text(program, "utf-8")
wasm = os.name == "nt"
compiler = ROOT / ".venv/emsdk/upstream/emscripten/em++.exe" if wasm else "c++"
binary = OUT / ("activity-scan.cjs" if wasm else "activity-scan")
flags = ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1", "-sSTACK_SIZE=1048576"] if wasm else []
subprocess.run([str(compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                "-I.", "-Ihost-tests/habits/stubs", str(OUT / "activity-scan.cpp"),
                "src/apps_local/pet/PetState.cpp", "src/apps_local/pet/PetStore.cpp",
                "src/apps_local/pet/PetHistory.cpp", "src/apps_local/reading/ReadingStatsCore.cpp",
                *flags, "-o", str(binary)], cwd=ROOT, check=True)
command = [str(ROOT / ".venv/emsdk/node/24.19.0_64bit/node.exe")] if wasm else []
subprocess.run([*command, str(binary)], cwd=ROOT, check=True)
