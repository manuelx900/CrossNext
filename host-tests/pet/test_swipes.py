"""Check production horizontal paging dispatch without rendering or persistence."""
from pathlib import Path
import os, subprocess, sys
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(sys.argv[1]).resolve();OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'src/apps_local/pet/PetActivity.cpp').read_text(encoding='utf-8')
def method(name):
 start=source.index('PetActivity::'+name+'(');start=source.rfind('\n',0,start)+1
 opening=source.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end].replace('PetActivity::','')
program=r'''#include <cassert>
#include <algorithm>
#include "src/apps_local/pet/PetScreens.h"
struct PetActivity {
 pet::Profile profile;
 pet::Tab tab=pet::Tab::Pet;
 pet::View view=pet::View::Dashboard;
 pet::Game selectedGame=pet::Game::Pairs;
 bool choosingFood=false;
 int page=0; struct {int pages=3;} work;
 freeink::ui::ActionId dispatched=freeink::ui::NO_ACTION;int value=0;
 void action(freeink::ui::ActionId a,int v){dispatched=a;value=v;}
 METHODS
};
int main(){
 PetActivity app; app.profile.created=true;app.profile.readingSeconds=1200;
 auto expect=[&](int dx,int dy,freeink::ui::ActionId action,int value){
  app.dispatched=freeink::ui::NO_ACTION;app.value=0;app.swipePages(dx,dy);
  assert(app.dispatched==action);if(action)assert(app.value==value);
 };
 for(auto view:{pet::View::Dashboard,pet::View::Care,pet::View::Home,pet::View::Album}){
  app.view=view;
  for(int t=0;t<4;++t){app.tab=pet::Tab(t);
   expect(-1,0,petui::ChangeTab,(t+1)%4);expect(1,0,petui::ChangeTab,(t+3)%4);
   expect(0,1,0,0);expect(-1,1,0,0);expect(0,0,0,0);expect(2,0,0,0);
  }
 }
 app.view=pet::View::Dashboard;app.tab=pet::Tab::Pet;app.profile.readingSeconds=1199;
 expect(-1,0,petui::ChangeTab,int(pet::Tab::Album));expect(1,0,petui::ChangeTab,int(pet::Tab::Album));
 app.profile.readingSeconds=1200;
 app.view=pet::View::Care;app.choosingFood=true;
 expect(-1,0,petui::FoodPage,1);expect(1,0,petui::FoodPage,-1);
 for(auto view:{pet::View::Projects,pet::View::Memories,pet::View::Lives}){
  app.view=view;app.page=0;expect(1,0,0,0);expect(-1,0,petui::Page,1);
  app.page=1;expect(1,0,petui::Page,-1);expect(-1,0,petui::Page,1);
  app.page=2;expect(-1,0,0,0);expect(1,0,petui::Page,-1);
 }
 for(auto view:{pet::View::Memories,pet::View::Lives}){
  app.view=view;app.work.pages=7;app.page=8;
  expect(1,0,petui::Page,-1);expect(-1,0,0,0);
  app.page=-2;expect(1,0,0,0);expect(-1,0,petui::Page,1);
  app.work.pages=1;app.page=8;expect(1,0,0,0);expect(-1,0,0,0);
 }
 app.view=pet::View::ProjectPreview;expect(-1,0,petui::PreviewStep,1);expect(1,0,petui::PreviewStep,-1);
 app.view=pet::View::Games;
 for(int g=0;g<3;++g){app.selectedGame=pet::Game(g);
  expect(-1,0,g==2?0:petui::SelectGame,g+1);expect(1,0,g==0?0:petui::SelectGame,g-1);
 }
 for(auto view:{pet::View::New,pet::View::Info,pet::View::Rules,pet::View::Settings,pet::View::SettingsConfirm,
    pet::View::Name,pet::View::Play,pet::View::Evolution,pet::View::Reward,pet::View::Notice,pet::View::Memory,
    pet::View::Death,pet::View::Retire,pet::View::Health,pet::View::HealthHelp,pet::View::LifeDetail,
    pet::View::Collection,pet::View::Affection,pet::View::ReadingInfo}){
  app.view=view;expect(-1,0,0,0);expect(1,0,0,0);
 }
 puts("PET swipe routing passed");
}
'''
program=program.replace('METHODS','\n'.join(method(n) for n in ['nextTab','swipeCatalog','swipePages']))
cpp=OUT/'swipes.cpp';cpp.write_text(program,encoding='utf-8')
wasm=os.name=='nt'
compiler=ROOT/'.venv/emsdk/upstream/emscripten/em++.exe' if wasm else 'c++'
binary=OUT/('swipes.cjs' if wasm else 'swipes')
flags=['-sENVIRONMENT=node','-sEXIT_RUNTIME=1'] if wasm else []
subprocess.run([str(compiler),'-std=c++20','-O2','-Wall','-Wextra','-Werror','-I.', '-Isrc','-Ilib/I18n','-Ilib/hal',
 '-Ifreeink-sdk/libs/ui/FreeInkUI/include','-Ifreeink-sdk/libs/assets/Icons/include','-Ilib/EpdFont',
 str(cpp),'src/apps_local/pet/PetState.cpp',*flags,'-o',str(binary)],cwd=ROOT,check=True)
command=[str(ROOT/'.venv/emsdk/node/24.19.0_64bit/node.exe')] if wasm else []
subprocess.run([*command,str(binary)],cwd=ROOT,check=True)
