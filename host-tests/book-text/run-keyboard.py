"""Check shared keyboard geometry and routing for every shipped language layout.

The Latin previews use the firmware's built-in fonts. Non-Latin cases exercise
the actual layout loader, bounds and input; they are not glyph-rendering proofs.
"""
from pathlib import Path
import subprocess
import argparse

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, default=root/".pio/host-tests/keyboard")
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
stubs = out / "stubs"
stubs.mkdir(exist_ok=True)
storage = (root / "test/language_pack/stubs/HalStorage.h").read_text(encoding="utf-8")
storage = storage.replace("  bool seek(size_t pos) {", """  size_t size() const { return fileSize64(); }
  void close() { bytes = nullptr; directory = false; cursor = 0; }
  bool seekSet(size_t pos) { return seek(pos); }
  bool seek(size_t pos) {""")
(stubs / "HalStorage.h").write_text(storage, encoding="utf-8")
(stubs / "MappedInputManager.h").write_text(
    '#pragma once\nclass MappedInputManager {public: enum class Button {Back,NavNext,NavPrevious,Confirm};'
    'bool wasReleased(Button)const{return false;} bool wasBackGesture()const{return false;} bool wasScreenTouchDown(int&,int&)const{return false;}'
    'bool isScreenTouchHeld(int&,int&)const{return false;} bool wasScreenTouchReleased()const{return false;}'
    'bool wasScreenTapped(int&,int&)const{return false;}};\n',encoding="utf-8")
source = (root / "src/activities/util/KeyboardEntryActivity.cpp").read_text(encoding="utf-8")


def method(text, name):
    first = text.index(name)
    return text[first:text.index("\n}", first) + 2]


names = ["fui::Rect KeyboardEntryActivity::keyboardRect()", "int KeyboardEntryActivity::inputFieldTop()"]
helpers = source[source.index("constexpr int KEYBOARD_MIN_GAP"):source.index("// ---------------------------------------------------------------------------")]
choice = source[source.index("props.labelText.font = ", source.index("props.okLabel = tr(STR_OK_BUTTON)")):]
choice = choice[:choice.index(";") + 1]
choice = choice.replace("fui::GfxRendererTarget::FONT_SMALL", "0").replace("fui::GfxRendererTarget::FONT_BODY", "1")
canvas = (root / "test/reader_font_sheet/ReaderFontSheetTest.cpp").read_text(encoding="utf-8").split("int main(")[0]
bridge = r"""struct FontBridge {
  std::vector<fui::FontGlyph> glyphs = std::vector<fui::FontGlyph>(0x2100 - 32);
  fui::BitmapFont font{};
  explicit FontBridge(const EpdFontData& source) {
    for (uint32_t i = 0; i < source.intervalCount; ++i) {
      const auto& interval = source.intervals[i];
      const uint32_t first = std::max<uint32_t>(32, interval.first), last = std::min<uint32_t>(0x20ff, interval.last);
      for (uint32_t cp = first; cp <= last; ++cp) {
        const auto& g = source.glyph[interval.offset + cp - interval.first];
        glyphs[cp - 32] = {uint16_t(g.dataOffset), g.width, g.height, uint8_t(fp4::toPixel(g.advanceX)), int8_t(g.left), int8_t(-g.top)};
      }
    }
    font = {source.bitmap, glyphs.data(), 32, 0x20ff, source.advanceY, uint8_t(source.ascender), 64, 64, 1};
  }
};"""
start = canvas.index("struct FontBridge {")
end = canvas.index("\n};", start) + 3
canvas = canvas[:start] + bridge + canvas[end:]
canvas = canvas.replace('#pragma GCC diagnostic pop', '#include "lib/EpdFont/builtinFonts/ubuntu_12_regular.h"\n#include "lib/EpdFont/builtinFonts/notosans_8_regular.h"\n#pragma GCC diagnostic pop')
program = canvas + r'''
#include "src/apps_local/ui/KeyboardViewport.h"
#include "src/apps_local/ui/KeyboardStyle.h"
#include "src/apps_local/ui/KeyboardSymbols.h"
#include "src/apps_local/ui/KeyboardAlternates.h"
#include "src/activities/util/KeyboardLayoutSet.h"
#include "src/activities/util/LanguageKeyboard.h"
#include "src/activities/util/PinyinInput.h"
#include "host-tests/book-text/KeyboardCoverageTest.h"
#include "host-tests/book-text/KeyboardMobileTest.h"
#include <fstream>
#include <iterator>
#include <set>
using Rect = fui::Rect;
struct ThemeMetrics {
  int keyboardKeySpacing=0, keyboardKeyHeight=56, verticalSpacing=10, keyboardVerticalOffset=-13;
  int buttonHintsHeight=0, topPadding=5, headerHeight=84;
};
struct TestRenderer {
  fui::DeviceContext d;
  int getScreenWidth() const { return d.width; }
  int getScreenHeight() const { return d.height; }
  fui::DeviceContext device() const { return d; }
};
struct UITheme {
  static UITheme& getInstance() { static UITheme instance; return instance; }
  ThemeMetrics metrics;
  const ThemeMetrics& getMetrics() const { return metrics; }
  static Rect getContentArea(const TestRenderer& renderer) { return renderer.d.safeRect(); }
};
HELPERS
struct KeyboardEntryActivity {
  TestRenderer renderer;
  struct Input { bool touch=true; bool hasTouch() const {return touch;} } mappedInput;
  const fui::KeyboardLayout* layout;
  const fui::KeyboardLayout& currentLayout() const {return *layout;}
  fui::Rect keyboardRect() const;
  int inputFieldTop() const;
};
METHODS
struct WidthTrace : fui::DrawTarget {
  fui::FontId drawn=-1;
  fui::Size measureText(fui::FontId font,const char* text,fui::TextStyle) const override {
    const std::string label(text);
    const int width=label=="i"?7:label=="edge"?(font==0?17:25):label=="huge"?(font==0?17:40):
                    label=="unfit"?(font==0?44:70):(font==0?17:26);
    return {int16_t(width),int16_t(font==0?20:29)};
  }
  int16_t lineHeight(fui::FontId font) const override {return font==0?20:29;}
  void fill(fui::Rect,fui::Paint,uint8_t,uint8_t) override {}
  void stroke(fui::Rect,fui::Paint,uint8_t,uint8_t,uint8_t) override {}
  void line(fui::Point,fui::Point,uint8_t,fui::Paint) override {}
  void triangle(fui::Point,fui::Point,fui::Point,fui::Paint) override {}
  void bitmap(fui::Rect,fui::BitmapRef,fui::BitmapMode,fui::Paint,fui::Rotation) override {}
  void text(fui::Rect,const char* text,fui::TextStyle style) override {
    if (std::string(text)!="hint") drawn=style.font;
  }
};
void checkFontFit() {
  for (bool alternate:{false,true}) for (const char* label:{"i","edge","wide","huge","unfit"}) {
    WidthTrace target;
    fui::DeviceContext device;device.width=480;device.height=800;
    fui::InteractionBuffer<80> hits;const fui::InputSnapshot input;
    fui::Frame<80> frame(target,device,input,hits);
    const fui::KeyboardKey key{label,label,fui::KeyKind::Normal,fui::StateNormal,1000,1,true,
                               alternate?"hint":nullptr};
    const fui::KeyboardRow row{&key,1};const fui::KeyboardLayout layout{&row,1};
    fui::KeyboardProps props;props.layout=&layout;props.labelText.font=1;props.altText.font=0;
    // 33px key: 25px plain content or 29px with a separately drawn alternate.
    fui::keyboard(frame,fui::makeRect(0,0,41,64),props);
    CHECK(target.drawn==((!alternate&&std::string(label)=="wide")||std::string(label)=="huge"?0:1));
  }
}
bool install(const std::string& local, const std::string& path) {
  std::ifstream input(local, std::ios::binary);
  if (!input) return false;
  Storage.files[path] = std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
  return true;
}
unsigned scenarios=0;
void verify(const fui::KeyboardLayout& layout, Language language, fui::KeyboardLayoutId id,
            bool shifted, bool symbols, const std::string& proof, bool preview=false) {
  for (unsigned orientation=0; orientation<4; ++orientation) {
    const auto size=orientation<2?std::pair<int,int>{480,800}:std::pair<int,int>{800,480};
    I18N.setLanguage(language);
      Canvas c(size.first,size.second);
      c.device.safeArea = orientation==0?fui::Insets{10,0,0,0}:orientation==1?fui::Insets{0,0,10,0}:
                          orientation==2?fui::Insets{0,0,0,10}:fui::Insets{0,10,0,0};
      FontBridge bodyFont(ubuntu_12_regular), smallFont(notosans_8_regular);
      fui::InteractionBuffer<80> hits; fui::InputSnapshot noInput;
      fui::Frame<80> keysFrame(c.target,c.device,noInput,hits);
      KeyboardEntryActivity entry;
      entry.renderer.d=c.device;
      entry.layout=&layout;
      const auto& metrics=UITheme::getInstance().getMetrics();
      const auto box=entry.keyboardRect();
      const int height=keyboardKeysHeight(metrics,entry.layout->rowCount,true,size.first>size.second,box.width);
      const auto keys=fui::makeRect(box.x,box.y+(box.height-height)/2,box.width,height);
      CHECK(entry.inputFieldTop()+46<keys.y && keys.bottom()<=c.device.safeRect().bottom());
      CHECK(layout.rowCount>=4 && layout.rowCount<=6);
      pixelui::Header header;header.title=language==Language::ES?"Buscar en el libro":"Search book";
      header.back=true;header.battery=82;
      pixelui::header(c.target,c.device,header);
      c.target.setFont(1,bodyFont.font);c.target.setFont(0,smallFont.font);c.target.setFont(3,smallFont.font);
      const int field=entry.inputFieldTop();
      c.target.line({36,int16_t(field)},{36,int16_t(field+30)},2,fui::Paint::solid(fui::Color::Black));
      const int underline=field+46;
      c.target.line({36,int16_t(underline)},{int16_t(size.first-36),int16_t(underline)},1,
          fui::Paint::solid(fui::Color::Black));
      pixelui::text(c.target,fui::makeRect(12,underline+(box.y-underline-24)/2,size.first-24,24),
          tr(STR_KEY_TOUCH_HINT),3,fui::Color::Black,fui::TextAlign::Center);
      fui::KeyboardProps props;
      props.layout=entry.layout;props.keyAction=1;props.okLabel=tr(STR_OK_BUTTON);
      props.inputMask=fui::InputTouch|fui::InputLongPress;
      auto& renderer=entry.renderer;
      [[maybe_unused]] const auto layoutId=id;
      FONT_CHOICE
      const bool compact=size.first>size.second;
      CHECK(props.labelText.font==(compact?0:1));
      props.altText.font=0;props.controlText.font=props.labelText.font;
      props.gap=props.rowGap=keyboardGap(metrics);
      props.background=fui::Paint::solid(fui::Color::White);
      keyboardview::style(props,shifted,symbols);
      fui::keyboard(keysFrame,keys,props);
      paintclock::notePainted();
      CHECK(!hits.overflowed());
      unsigned enabled=0;
      std::set<int> values;
      for (unsigned row=0;row<layout.rowCount;++row) for (unsigned col=0;col<layout.rows[row].count;++col) {
        const auto& key=layout.rows[row].keys[col];
        if (!key.enabled || key.kind==fui::KeyKind::Disabled) continue;
        ++enabled;
        CHECK(values.insert(key.value).second);
        if (key.output) {
          const char* output=fui::keyboardOutputFor(layout,key.value);
          CHECK(output && std::string(output)==key.output);
        }
        if (key.alt) {
          const char* alternate=fui::keyboardAltOutputFor(layout,key.value);
          CHECK(alternate && std::string(alternate)==key.alt);
        }
      }
      CHECK(hits.count()==enabled);
      for(size_t n=0;n<hits.count();++n) {
        const auto& key=hits.data()[n];
        CHECK(key.rect.x>=c.device.safeRect().x && key.rect.right()<=c.device.safeRect().right() &&
              key.rect.y>=keys.y && key.rect.bottom()<=c.device.safeRect().bottom());
        CHECK(key.rect.width>=28 && key.rect.height>=40);
        CHECK(key.rect.height<=keyboardview::keyHeight(metrics.keyboardKeyHeight,size.first>size.second));
        fui::InputSnapshot tap;tap.touchReleased=true;tap.touchX=key.rect.x+key.rect.width/2;
        tap.touchY=key.rect.y+key.rect.height/2;
        const auto routed=hits.route(tap);
        if(routed.value!=key.value && failures<20) std::printf("route context lang=%d shift=%d sym=%d orient=%u value=%d got=%d rect=%d,%d,%d,%d\n",int(language),shifted,symbols,orientation,key.value,routed.value,key.rect.x,key.rect.y,key.rect.width,key.rect.height);
        CHECK(routed.value==key.value);
        const auto* definition=static_cast<const fui::KeyboardKey*>(nullptr);
        for(unsigned r=0;r<layout.rowCount;++r)for(unsigned k=0;k<layout.rows[r].count;++k)
          if(layout.rows[r].keys[k].value==key.value)definition=&layout.rows[r].keys[k];
        CHECK(definition);
        if(definition) {
          const auto kind=definition->kind;
          const bool dark=kind==fui::KeyKind::Mode||kind==fui::KeyKind::Delete||kind==fui::KeyKind::Ok||
                          (kind==fui::KeyKind::Shift&&(shifted||symbols));
          const int px=key.rect.x+key.rect.width/2,py=key.rect.y+4;
          const bool white=(c.pixels[py*((c.width+7)/8)+px/8]&(128>>(px%8)))!=0;
          CHECK(white!=dark);
        }
        fui::TouchHoldRouter hold;
        CHECK(!hold.update(hits,true,tap.touchX,tap.touchY,false,0,0,true,10).event);
        const auto held=hold.update(hits,true,tap.touchX,tap.touchY,false,0,0,true,1010);
        CHECK(held.event && held.event.value==key.value && held.event.longPress);
        CHECK(!hold.update(hits,false,0,0,true,tap.touchX,tap.touchY,false,1020).event);
      }
      if (preview && (orientation==0||orientation==2))
        c.write(proof+"/keyboard-"+std::to_string(int(language))+"-"+std::to_string(size.first)+
          (symbols?"-symbols":"-letters")+(shifted?"-upper":"-lower")+".pbm");
      if (preview && !symbols && language==Language::ES && orientation==0) {
        keyboardview::Alternates popup;
        const auto& letter=layout.rows[1].keys[0];
        CHECK(popup.open(letter,language,keys));
        popup.draw(keysFrame,fui::makeRect(12,84,size.first-24,size.second-96),1,1);
        c.write(proof+"/popup-"+std::to_string(size.first)+(shifted?"-upper":"-lower")+".pbm");
      }
      ++scenarios;
  }
}
int main(int argc,char** argv) {
  checkFontFit();
  const std::string proof=argc>1?argv[1]:".";
  auditKeyboardCoverage(proof);
  auditMobileKeyboards();
  unsigned packs=0, languages=0;
  install("assets_local/language-keyboards/pinyin-sc.bin","/languages/keyboards/pinyin-sc.bin");
  install("assets_local/language-keyboards/pinyin-tc.bin","/languages/keyboards/pinyin-tc.bin");
  // Include every built-in even when an optional pack supersedes it.
  for (const auto& item:keyboard_layouts::ALL) for(bool shifted:{false,true}) for(bool symbols:{false,true})
    verify(symbols?keyboardview::symbols(shifted,true):fui::builtinKeyboardLayout(item.id,shifted,false,false,true),item.language,item.id,
           shifted,symbols,proof,item.language==Language::EN||item.language==Language::ES);
  for(bool shifted:{false,true}) for(bool symbols:{false,true})
    verify(symbols?keyboardview::symbols(shifted,true):fui::builtinKeyboardLayout(fui::KeyboardLayoutId::QwertyEn,shifted,false,false,true),
           Language::OOK,fui::KeyboardLayoutId::QwertyEn,shifted,symbols,proof);
  for (unsigned n=0;n<unsigned(Language::_COUNT);++n) {
    const auto language=Language(n);
    const auto code=std::string(LANGUAGE_CODES[n]);
    const bool installed=install("assets_local/language-keyboards/"+code+".cpkbd",
                                 "/languages/keyboards/"+code+".cpkbd");
    bool builtin=language==Language::OOK;
    for (const auto& item:keyboard_layouts::ALL) builtin|=item.language==language;
    CHECK(installed||builtin);
    ++languages;
    if (!installed) continue;
    ++packs;
    LanguageKeyboard keyboard;
    CHECK(keyboard.load(language));
    auto id=fui::KeyboardLayoutId::QwertyEn;
    for(const auto& item:keyboard_layouts::ALL) if(item.language==language) id=item.id;
    for(bool shifted:{false,true}) for(bool symbols:{false,true})
      verify(symbols?keyboardview::symbols(shifted,true):keyboard.layout(shifted),
             language,id,shifted,symbols,proof);
    PinyinInput pinyin;
    if (language==Language::ZH_CN||language==Language::ZH_TW) {
      CHECK(pinyin.begin(language));
      for (int state=0;state<3;++state) {
        if (state==1) CHECK(pinyin.type('s')&&pinyin.type('h')&&pinyin.type('i'));
        if (state==2) CHECK(pinyin.page(1));
        keyboard.candidates(pinyin);
        for(bool shifted:{false,true})
          verify(keyboard.layout(shifted),language,id,shifted,false,proof);
      }
    }
  }
  CHECK(languages==39 && packs==30);
  std::printf("Keyboard reference: %u languages, %u packs, %u built-ins, %u scenarios; %d checks, %d failures\n",
              languages,packs,unsigned(keyboard_layouts::COUNT),scenarios,checks,failures);
  return failures?1:0;
}
'''
program = program.replace("HELPERS", helpers).replace("METHODS", "\n".join(method(source, name) for name in names))
program = program.replace("FONT_CHOICE", choice)
program = program.replace("toybox::makeTarget(renderer).deviceContext()", "renderer.device()")
import json,unicodedata
specs=json.loads((root/"assets_local/language-keyboards/SOURCE.json").read_text(encoding="utf-8"))
expected=[]
for tag,spec in specs.items():
    code=next(json.loads(line.split(":",1)[1]) for path in (root/"lib/I18n/translations").glob("*.yaml")
              if any(line.startswith("_bcp47:") and json.loads(line.split(":",1)[1])==tag for line in path.read_text(encoding="utf-8").splitlines())
              for line in path.read_text(encoding="utf-8").splitlines() if line.startswith("_language_code:"))
    values=sorted(set("".join(spec["rows"]+spec["shift"])) | set(spec.get("alts",{}).values()))
    expected.extend((code,unicodedata.normalize("NFC",value)) for value in values)
legacy=json.loads((root/"test/language_pack/keyboard-repertoire.json").read_text(encoding="utf-8"))
key_header=(root/"lib/I18n/I18nKeys.h").read_text(encoding="utf-8")
import re
locale_enum={tag:enum for enum,tag in re.findall(r"^  (\w+)\s*=\s*\d+,\s*//\s*(\S+)",key_header,re.M)}
(out/"mobile-keyboard-repertoire.h").write_text("struct LegacyKeyboardOutput { Language language; const char* output; };\ninline constexpr LegacyKeyboardOutput legacyKeyboardOutputs[]={\n"+
    "".join("{Language::"+locale_enum[tag]+","+json.dumps(value,ensure_ascii=False)+"},\n" for tag,values in legacy.items() for value in values)+"};\n",encoding="utf-8")
(out/"expected-keyboard-outputs.h").write_text("struct ExpectedOutput { Language language; const char* output; };\ninline constexpr ExpectedOutput expectedOutputs[]={\n"+
    "".join("{Language::"+code+","+json.dumps(value,ensure_ascii=False)+"},\n" for code,value in expected)+"};\n",encoding="utf-8")
cpp = out / "keyboard.cpp"
cpp.write_text(program, encoding="utf-8")
compiler = root / ".venv/emsdk/upstream/emscripten/em++.exe"
includes = [str(out), str(stubs), ".", "test/reader_font_sheet", "test/language_pack/stubs", "host-tests/study-suite/stubs",
            "lib/I18n", "lib/EpdFont", "lib/Utf8", "freeink-sdk/libs/ui/FreeInkUI/include", "freeink-sdk/libs/assets/Icons/include"]
sources = [str(cpp), "src/apps_local/ui/KeyboardAlternates.cpp", "src/apps_local/ui/PixelMenu.cpp", "lib/I18n/I18n.cpp", "lib/I18n/I18nStrings.cpp",
           "lib/I18n/LanguagePack.cpp", "src/activities/util/LanguageKeyboard.cpp",
           "src/activities/util/PinyinInput.cpp", "lib/Utf8/Utf8.cpp",
           "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"]
exe = out / "keyboard.cjs"
subprocess.run([str(compiler), "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror", "-Wno-comment",
                *["-I" + p for p in includes], *sources, "-sENVIRONMENT=node", "-sNODERAWFS=1",
                "-sEXIT_RUNTIME=1", "-sSTACK_SIZE=1048576", "-sALLOW_MEMORY_GROWTH=1", "-o", str(exe)],
               cwd=root, check=True)
subprocess.run(["node", str(exe), out.as_posix()], cwd=root, check=True)
