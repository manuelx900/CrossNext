#include "KeyboardAlternates.h"

#include <MappedInputManager.h>
#include <Utf8.h>

namespace keyboardview {
namespace {
// Base, then related accents. A layout's explicit language alternate is added
// first, ahead of this shared fallback, without changing the SD pack format.
constexpr const char* LATIN[] = {"a|à|á|â|ä|æ|ã|å|ā",
                                 "A|À|Á|Â|Ä|Æ|Ã|Å|Ā",
                                 "e|è|é|ê|ë|ē|ė|ę",
                                 "E|È|É|Ê|Ë|Ē|Ė|Ę",
                                 "i|ì|í|î|ï|ī|į|ı",
                                 "I|Ì|Í|Î|Ï|Ī|Į|İ",
                                 "o|ò|ó|ô|ö|œ|ø|õ|ō",
                                 "O|Ò|Ó|Ô|Ö|Œ|Ø|Õ|Ō",
                                 "u|ù|ú|û|ü|ū|ů|ű|ų",
                                 "U|Ù|Ú|Û|Ü|Ū|Ů|Ű|Ų",
                                 "c|ç|ć|č|ĉ|ċ",
                                 "C|Ç|Ć|Č|Ĉ|Ċ",
                                 "n|ñ|ń|ň|ņ|ŋ",
                                 "N|Ñ|Ń|Ň|Ņ|Ŋ",
                                 "s|ß|ś|š|ş|ș|ŝ",
                                 "S|SS|Ś|Š|Ş|Ș|Ŝ",
                                 "l|ł|ľ|ĺ|ļ",
                                 "L|Ł|Ľ|Ĺ|Ļ",
                                 "d|ď|đ|ð",
                                 "D|Ď|Đ|Ð",
                                 "g|ğ|ģ|ĝ|ġ",
                                 "G|Ğ|Ģ|Ĝ|Ġ",
                                 "r|ř|ŕ|ŗ",
                                 "R|Ř|Ŕ|Ŗ",
                                 "t|ť|ţ|ț|þ",
                                 "T|Ť|Ţ|Ț|Þ",
                                 "z|ž|ź|ż",
                                 "Z|Ž|Ź|Ż",
                                 "y|ý|ÿ|ŷ",
                                 "Y|Ý|Ÿ|Ŷ",
                                 "j|ĵ",
                                 "J|Ĵ",
                                 "h|ĥ|ħ",
                                 "H|Ĥ|Ħ",
                                 "w|ŵ",
                                 "W|Ŵ",
                                 "k|ķ",
                                 "K|Ķ"};
// Existing packs keep their character keys; related forms share the matching
// base's callout. Vietnamese tone series fit the fixed choice capacity.
constexpr const char* VIETNAMESE[] = {"a|à|á|ả|ã|ạ|ă|ằ|ắ|ẳ|ẵ|ặ|â|ầ|ấ|ẩ|ẫ|ậ",
                                      "A|À|Á|Ả|Ã|Ạ|Ă|Ằ|Ắ|Ẳ|Ẵ|Ặ|Â|Ầ|Ấ|Ẩ|Ẫ|Ậ",
                                      "ă|ằ|ắ|ẳ|ẵ|ặ",
                                      "Ă|Ằ|Ắ|Ẳ|Ẵ|Ặ",
                                      "â|ầ|ấ|ẩ|ẫ|ậ",
                                      "Â|Ầ|Ấ|Ẩ|Ẫ|Ậ",
                                      "e|è|é|ẻ|ẽ|ẹ|ê|ề|ế|ể|ễ|ệ",
                                      "E|È|É|Ẻ|Ẽ|Ẹ|Ê|Ề|Ế|Ể|Ễ|Ệ",
                                      "ê|ề|ế|ể|ễ|ệ",
                                      "Ê|Ề|Ế|Ể|Ễ|Ệ",
                                      "i|ì|í|ỉ|ĩ|ị",
                                      "I|Ì|Í|Ỉ|Ĩ|Ị",
                                      "o|ò|ó|ỏ|õ|ọ|ô|ồ|ố|ổ|ỗ|ộ|ơ|ờ|ớ|ở|ỡ|ợ",
                                      "O|Ò|Ó|Ỏ|Õ|Ọ|Ô|Ồ|Ố|Ổ|Ỗ|Ộ|Ơ|Ờ|Ớ|Ở|Ỡ|Ợ",
                                      "ô|ồ|ố|ổ|ỗ|ộ",
                                      "Ô|Ồ|Ố|Ổ|Ỗ|Ộ",
                                      "ơ|ờ|ớ|ở|ỡ|ợ",
                                      "Ơ|Ờ|Ớ|Ở|Ỡ|Ợ",
                                      "u|ù|ú|ủ|ũ|ụ|ư|ừ|ứ|ử|ữ|ự",
                                      "U|Ù|Ú|Ủ|Ũ|Ụ|Ư|Ừ|Ứ|Ử|Ữ|Ự",
                                      "ư|ừ|ứ|ử|ữ|ự",
                                      "Ư|Ừ|Ứ|Ử|Ữ|Ự",
                                      "y|ỳ|ý|ỷ|ỹ|ỵ",
                                      "Y|Ỳ|Ý|Ỷ|Ỹ|Ỵ",
                                      "d|đ",
                                      "D|Đ"};
constexpr const char* COMMON_SYMBOLS[] = {
    "?|¿", "!|¡",     "-|–|—|−", ".|…|·",   "'|‘|’|‚|‹|›", "\"|“|”|„|«|»",      "0|°|º|ª",
    "%|‰", "+|±|×|÷", "=|≠|≈|∞", "<|≤|«|‹", ">|≥|»|›",     "$|€|£|¥|¢|₽|₹|₩|₺", "~|`|^|´|¨"};
constexpr const char* ARABIC_SYMBOLS[] = {"?|؟", ",|،|٬", ";|؛", "%|٪", ".|٫|ً|ٌ|ٍ|ٰ", "-|َ|ُ|ِ|ّ|ْ", "_|ـ"};
constexpr const char* HEBREW_MARKS[] = {".|ּ|ׁ|ׂ", "'|׳", "\"|״"};
constexpr const char* CJK_SYMBOLS[] = {".|。", ",|，|、", "[|「|『", "]|」|』"};
constexpr const char* CATALAN[] = {"l|l·l|ŀ", "L|L·L|Ŀ"};

bool relatedLatinAlternate(const fui::KeyboardKey& key) {
  if (!key.alt || !key.output || !key.output[0] || key.output[1]) return true;
  for (const auto* group : LATIN) {
    const char* start = group;
    while (*start) {
      const char* end = std::strchr(start, '|');
      if (!end) end = start + std::strlen(start);
      if (size_t(end - start) == std::strlen(key.alt) && !std::memcmp(start, key.alt, end - start))
        return group[0] == key.output[0];
      start = *end ? end + 1 : end;
    }
  }
  return true;
}
struct LocaleAlternates {
  Language language;
  const char* lower;
  const char* upper;
};
constexpr LocaleAlternates LOCALE_ALTERNATES[] = {
    {Language::FR, "a|à|â|æ", "A|À|Â|Æ"},
    {Language::FR, "e|é|è|ê|ë", "E|É|È|Ê|Ë"},
    {Language::FR, "i|î|ï", "I|Î|Ï"},
    {Language::FR, "o|ô|œ", "O|Ô|Œ"},
    {Language::FR, "u|ù|û|ü", "U|Ù|Û|Ü"},
    {Language::FR, "c|ç", "C|Ç"},
    {Language::FR, "y|ÿ", "Y|Ÿ"},
    {Language::AZ, "e|ə", "E|Ə"},
    {Language::AZ, "g|ğ", "G|Ğ"},
    {Language::AZ, "i|ı", "İ|I"},
    {Language::AZ, "o|ö", "O|Ö"},
    {Language::AZ, "s|ş", "S|Ş"},
    {Language::AZ, "u|ü", "U|Ü"},
    {Language::AZ, "c|ç", "C|Ç"},
    {Language::AZ, "v|w|ŵ", "V|W|Ŵ"},
    {Language::BS, "c|č|ć", "C|Č|Ć"},
    {Language::BS, "d|đ", "D|Đ"},
    {Language::BS, "s|š", "S|Š"},
    {Language::BS, "z|ž", "Z|Ž"},
    {Language::CA, "a|à", "A|À"},
    {Language::CA, "e|è|é", "E|È|É"},
    {Language::CA, "i|í|ï", "I|Í|Ï"},
    {Language::CA, "o|ò|ó", "O|Ò|Ó"},
    {Language::CA, "u|ú|ü", "U|Ú|Ü"},
    {Language::CA, "c|ç", "C|Ç"},
    {Language::CA, "l|l·l|ŀ", "L|L·L|Ŀ"},
    {Language::CAV, "a|à", "A|À"},
    {Language::CAV, "e|è|é", "E|È|É"},
    {Language::CAV, "i|í|ï", "I|Í|Ï"},
    {Language::CAV, "o|ò|ó", "O|Ò|Ó"},
    {Language::CAV, "u|ú|ü", "U|Ú|Ü"},
    {Language::CAV, "c|ç", "C|Ç"},
    {Language::CAV, "l|l·l|ŀ", "L|L·L|Ŀ"},
    {Language::CS, "a|á", "A|Á"},
    {Language::CS, "c|č", "C|Č"},
    {Language::CS, "d|ď", "D|Ď"},
    {Language::CS, "e|é|ě", "E|É|Ě"},
    {Language::CS, "i|í", "I|Í"},
    {Language::CS, "n|ň", "N|Ň"},
    {Language::CS, "r|ř", "R|Ř"},
    {Language::CS, "s|š", "S|Š"},
    {Language::CS, "t|ť", "T|Ť"},
    {Language::CS, "u|ú|ů", "U|Ú|Ů"},
    {Language::CS, "y|ý", "Y|Ý"},
    {Language::CS, "z|ž", "Z|Ž"},
    {Language::DA, "a|å|æ", "A|Å|Æ"},
    {Language::DA, "o|ø", "O|Ø"},
    {Language::DA, "e|é", "E|É"},
    {Language::FI, "a|ä|å", "A|Ä|Å"},
    {Language::FI, "o|ö", "O|Ö"},
    {Language::HU, "a|á", "A|Á"},
    {Language::HU, "e|é", "E|É"},
    {Language::HU, "i|í", "I|Í"},
    {Language::HU, "o|ó|ö|ő", "O|Ó|Ö|Ő"},
    {Language::HU, "u|ú|ü|ű", "U|Ú|Ü|Ű"},
    {Language::IT, "a|à", "A|À"},
    {Language::IT, "e|è|é", "E|È|É"},
    {Language::IT, "i|ì", "I|Ì"},
    {Language::IT, "o|ò", "O|Ò"},
    {Language::IT, "u|ù", "U|Ù"},
    {Language::LT, "a|ą", "A|Ą"},
    {Language::LT, "c|č", "C|Č"},
    {Language::LT, "e|ę|ė", "E|Ę|Ė"},
    {Language::LT, "i|į", "I|Į"},
    {Language::LT, "s|š", "S|Š"},
    {Language::LT, "u|ų|ū", "U|Ų|Ū"},
    {Language::LT, "z|ž", "Z|Ž"},
    {Language::NB, "a|å|æ", "A|Å|Æ"},
    {Language::NB, "o|ø", "O|Ø"},
    {Language::NB, "e|é", "E|É"},
    {Language::NL, "e|é|ë", "E|É|Ë"},
    {Language::NL, "i|ï", "I|Ï"},
    {Language::NL, "o|ö", "O|Ö"},
    {Language::NL, "u|ü", "U|Ü"},
    {Language::PL, "a|ą", "A|Ą"},
    {Language::PL, "c|ć", "C|Ć"},
    {Language::PL, "e|ę", "E|Ę"},
    {Language::PL, "l|ł", "L|Ł"},
    {Language::PL, "n|ń", "N|Ń"},
    {Language::PL, "o|ó", "O|Ó"},
    {Language::PL, "s|ś", "S|Ś"},
    {Language::PL, "z|ż|ź", "Z|Ż|Ź"},
    {Language::PT, "a|á|à|â|ã", "A|Á|À|Â|Ã"},
    {Language::PT, "c|ç", "C|Ç"},
    {Language::PT, "e|é|ê", "E|É|Ê"},
    {Language::PT, "i|í", "I|Í"},
    {Language::PT, "o|ó|ô|õ", "O|Ó|Ô|Õ"},
    {Language::PT, "u|ú", "U|Ú"},
    {Language::P2, "a|á|à|â|ã", "A|Á|À|Â|Ã"},
    {Language::P2, "c|ç", "C|Ç"},
    {Language::P2, "e|é|ê", "E|É|Ê"},
    {Language::P2, "i|í", "I|Í"},
    {Language::P2, "o|ó|ô|õ", "O|Ó|Ô|Õ"},
    {Language::P2, "u|ú", "U|Ú"},
    {Language::RO, "a|ă|â", "A|Ă|Â"},
    {Language::RO, "i|î", "I|Î"},
    {Language::RO, "s|ș", "S|Ș"},
    {Language::RO, "t|ț", "T|Ț"},
    {Language::SK, "a|á|ä", "A|Á|Ä"},
    {Language::SK, "c|č", "C|Č"},
    {Language::SK, "d|ď", "D|Ď"},
    {Language::SK, "e|é", "E|É"},
    {Language::SK, "i|í", "I|Í"},
    {Language::SK, "l|ľ|ĺ", "L|Ľ|Ĺ"},
    {Language::SK, "n|ň", "N|Ň"},
    {Language::SK, "o|ó|ô", "O|Ó|Ô"},
    {Language::SK, "r|ŕ", "R|Ŕ"},
    {Language::SK, "s|š", "S|Š"},
    {Language::SK, "t|ť", "T|Ť"},
    {Language::SK, "u|ú", "U|Ú"},
    {Language::SK, "y|ý", "Y|Ý"},
    {Language::SK, "z|ž", "Z|Ž"},
    {Language::SI, "c|č", "C|Č"},
    {Language::SI, "s|š", "S|Š"},
    {Language::SI, "z|ž", "Z|Ž"},
    {Language::SV, "a|ä|å", "A|Ä|Å"},
    {Language::SV, "o|ö", "O|Ö"},
    {Language::TR, "c|ç", "C|Ç"},
    {Language::TR, "g|ğ", "G|Ğ"},
    {Language::TR, "i|ı", "İ|I"},
    {Language::TR, "o|ö", "O|Ö"},
    {Language::TR, "s|ş", "S|Ş"},
    {Language::TR, "u|ü", "U|Ü"},
    {Language::BG, "и|ѝ", "И|Ѝ"},
    {Language::TH, "ข|ฃ", "ข|ฃ"},
    {Language::TH, "ค|ฅ", "ค|ฅ"},
    {Language::TH, "ํ|๎", "ํ|๎"},
    {Language::TH, "ฯ|๏|๚|๛", "ฯ|๏|๚|๛"},
    {Language::UK, "'|’|ʼ", "'|’|ʼ"},
};
constexpr const char* RUSSIAN[] = {"е|ё", "Е|Ё"};
constexpr const char* UKRAINIAN[] = {"г|ґ", "Г|Ґ", "і|ї", "І|Ї", "е|є", "Е|Є"};
constexpr const char* BELARUSIAN[] = {"е|ё", "Е|Ё", "и|і", "И|І", "у|ў", "У|Ў"};
constexpr const char* KAZAKH[] = {"а|ә", "А|Ә", "г|ғ",   "Г|Ғ",   "к|қ", "К|Қ", "н|ң", "Н|Ң",
                                  "о|ө", "О|Ө", "у|ұ|ү", "У|Ұ|Ү", "х|һ", "Х|Һ", "и|і", "И|І"};
constexpr const char* ARABIC[] = {"ا|أ|إ|آ|ٱ", "و|ؤ", "ي|ى|ئ", "ه|ة"};
constexpr const char* PERSIAN[] = {"ا|آ|أ|إ|ء", "و|ؤ", "ک|ك", "ی|ي|ئ", "ه|هٔ|ۀ|ة"};
constexpr const char* KANA[] = {"あ|い|う|え|お|ぁ|ぃ|ぅ|ぇ|ぉ|ゔ",
                                "か|き|く|け|こ|が|ぎ|ぐ|げ|ご|ゕ|ゖ",
                                "さ|し|す|せ|そ|ざ|じ|ず|ぜ|ぞ",
                                "た|ち|つ|て|と|だ|ぢ|づ|で|ど|っ",
                                "な|に|ぬ|ね|の",
                                "は|ひ|ふ|へ|ほ|ば|び|ぶ|べ|ぼ|ぱ|ぴ|ぷ|ぺ|ぽ",
                                "ま|み|む|め|も",
                                "や|ゆ|よ|ゃ|ゅ|ょ",
                                "ら|り|る|れ|ろ",
                                "わ|を|ゎ",
                                "ん|ー",
                                "゙|゚",
                                "ア|イ|ウ|エ|オ|ァ|ィ|ゥ|ェ|ォ|ヴ",
                                "カ|キ|ク|ケ|コ|ガ|ギ|グ|ゲ|ゴ|ヵ|ヶ",
                                "サ|シ|ス|セ|ソ|ザ|ジ|ズ|ゼ|ゾ",
                                "タ|チ|ツ|テ|ト|ダ|ヂ|ヅ|デ|ド|ッ",
                                "ナ|ニ|ヌ|ネ|ノ",
                                "ハ|ヒ|フ|ヘ|ホ|バ|ビ|ブ|ベ|ボ|パ|ピ|プ|ペ|ポ",
                                "マ|ミ|ム|メ|モ",
                                "ヤ|ユ|ヨ|ャ|ュ|ョ",
                                "ラ|リ|ル|レ|ロ",
                                "ワ|ヲ|ヮ",
                                "ン|ー"};
bool inGroup(const char* group, const char* output) {
  const size_t bytes = std::strlen(output);
  for (const char* start = group; *start;) {
    const char* end = std::strchr(start, '|');
    if (!end) end = start + std::strlen(start);
    if (size_t(end - start) == bytes && !std::memcmp(start, output, bytes)) return true;
    start = *end ? end + 1 : end;
  }
  return false;
}
}  // namespace

void Alternates::reset() {
  phase_.store(Phase::Closed);
  built_.store(false);
  first_ = queued_ = count_ = 0;
  openingTouch_ = openingConfirm_ = closingContact_ = trackedContact_ = captureOpeningTouch_ = false;
}
void Alternates::add(const char* value, size_t bytes) {
  if (!value || !bytes || bytes >= sizeof(choices_[0]) || count_ == Capacity) return;
  for (unsigned i = 0; i < count_; ++i)
    if (std::strlen(choices_[i]) == bytes && !std::memcmp(choices_[i], value, bytes)) return;
  std::memcpy(choices_[count_], value, bytes);
  choices_[count_++][bytes] = 0;
}
void Alternates::addGroup(const char* group) {
  for (const char* start = group; *start;) {
    const char* end = std::strchr(start, '|');
    if (!end) end = start + std::strlen(start);
    add(start, end - start);
    start = *end ? end + 1 : end;
  }
}
bool Alternates::open(const fui::KeyboardKey& key, Language language, fui::Rect anchor, bool physical,
                      bool genericLetters, bool allowHalfSpace) {
  if (key.kind != fui::KeyKind::Normal || !key.output || !*key.output ||
      ((language == Language::ZH_CN || language == Language::ZH_TW) &&
       ((key.output[0] >= 'a' && key.output[0] <= 'z') || (key.output[0] >= 'A' && key.output[0] <= 'Z'))))
    return false;
  reset();
  language_ = language;
  add(key.output, std::strlen(key.output));
  if (key.alt && (!genericLetters || relatedLatinAlternate(key))) add(key.alt, std::strlen(key.alt));
  // National digits remain available from the shared ASCII number page.
  if (key.output[0] >= '0' && key.output[0] <= '9' && !key.output[1]) {
    uint32_t zero = 0;
    switch (language) {
      case Language::AR:
        zero = 0x660;
        break;
      case Language::FA:
        zero = 0x6f0;
        break;
      case Language::TH:
        zero = 0xe50;
        break;
      default:
        break;
    }
    if (zero) {
      char digit[8];
      char* end = utf8AppendCodepoint(digit, zero + key.output[0] - '0');
      *end = 0;
      add(digit, end - digit);
    }
  }
  if (language == Language::AR || language == Language::FA)
    for (const auto* group : ARABIC_SYMBOLS)
      if (inGroup(group, key.output)) addGroup(group);
  if (language == Language::HE)
    for (const auto* group : HEBREW_MARKS)
      if (inGroup(group, key.output)) addGroup(group);
  if (language == Language::JA || language == Language::ZH_CN || language == Language::ZH_TW)
    for (const auto* group : CJK_SYMBOLS)
      if (inGroup(group, key.output)) addGroup(group);
  if (allowHalfSpace && language == Language::FA && !std::strcmp(key.output, "-")) add("\xe2\x80\x8c", 3);
  if (language == Language::VI) {
    static constexpr const char* toneMarks = "~|̀|́|̃|̉|̣";
    if (inGroup(toneMarks, key.output)) addGroup(toneMarks);
  }
  for (const auto* group : COMMON_SYMBOLS)
    if (inGroup(group, key.output)) addGroup(group);
  if (genericLetters) {
    for (const auto& locale : LOCALE_ALTERNATES) {
      if (locale.language != language) continue;
      if (inGroup(locale.lower, key.output)) addGroup(locale.lower);
      if (inGroup(locale.upper, key.output)) addGroup(locale.upper);
    }
    const char* const* groups = LATIN;
    size_t size = std::size(LATIN);
    switch (language) {
      case Language::VI:
        groups = VIETNAMESE;
        size = std::size(VIETNAMESE);
        break;
      case Language::JA:
        groups = KANA;
        size = std::size(KANA);
        break;
      case Language::RU:
        groups = RUSSIAN;
        size = std::size(RUSSIAN);
        break;
      case Language::UK:
        groups = UKRAINIAN;
        size = std::size(UKRAINIAN);
        break;
      case Language::BE:
        groups = BELARUSIAN;
        size = std::size(BELARUSIAN);
        break;
      case Language::KK:
        groups = KAZAKH;
        size = std::size(KAZAKH);
        break;
      case Language::AR:
        groups = ARABIC;
        size = std::size(ARABIC);
        break;
      case Language::FA:
        groups = PERSIAN;
        size = std::size(PERSIAN);
        break;
      default:
        break;
    }
    if (language == Language::CA || language == Language::CAV)
      for (const auto* group : CATALAN)
        if (inGroup(group, key.output)) addGroup(group);
    for (size_t i = 0; i < size; ++i) {
      const size_t baseBytes = std::strcspn(groups[i], "|");
      const bool matches = language == Language::VI
                               ? std::strlen(key.output) == baseBytes && !std::memcmp(groups[i], key.output, baseBytes)
                               : inGroup(groups[i], key.output);
      if (matches) addGroup(groups[i]);
    }
    const auto* cursor = reinterpret_cast<const unsigned char*>(key.output);
    const auto cp = utf8NextCodepoint(&cursor);
    if (language == Language::HE && cp >= 0x5d0 && cp <= 0x5ea && !*cursor) {
      constexpr uint16_t vowels[] = {0x5b0, 0x5b4, 0x5b5, 0x5b6, 0x5b7, 0x5b8, 0x5b9, 0x5bb};
      for (auto vowel : vowels) {
        char value[16];
        char* end = utf8AppendCodepoint(value, cp);
        end = utf8AppendCodepoint(end, vowel);
        *end = 0;
        add(value, end - value);
      }
    }
    if ((language == Language::AR || language == Language::FA) && cp >= 0x620 && cp <= 0x6ff && !*cursor) {
      static constexpr uint16_t vowels[] = {0x64e, 0x650, 0x64f, 0x64b, 0x64d, 0x64c, 0x651, 0x652};
      for (const auto vowel : vowels) {
        char value[16];
        char* end = utf8AppendCodepoint(value, cp);
        *utf8AppendCodepoint(end, vowel) = 0;
        utf8ComposeNfcInPlace(value);
        add(value, std::strlen(value));
      }
    }
  }
  if (language == Language::VI) {
    const char letter = key.output[0];
    if (genericLetters && !key.output[1] && ((letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z')) &&
        !std::strchr("aeiouyAEIOUY", letter)) {
      static constexpr uint16_t tones[] = {0x300, 0x301, 0x303, 0x309, 0x323};
      for (const auto tone : tones) {
        char value[16] = {letter};
        *utf8AppendCodepoint(value + 1, tone) = 0;
        utf8ComposeNfcInPlace(value);
        add(value, std::strlen(value));
      }
    }
  }
  if (count_ < 2) {
    reset();
    return false;
  }
  anchor_ = anchor;
  lastX_ = anchor.x + anchor.width / 2;
  lastY_ = anchor.y + anchor.height / 2;
  selected_ = 0;
  captureOpeningTouch_ = openingTouch_ = !physical;
  openingConfirm_ = physical;
  phase_.store(Phase::Open);
  return true;
}
bool Alternates::capture(const MappedInputManager& input) {
  Contact contact;
  contact.painted = paintclock::painted();
  int x = 0, y = 0, hx = 0, hy = 0;
  const bool down = input.wasScreenTouchDown(x, y);
  const bool held = input.isScreenTouchHeld(hx, hy);
  contact.held = down || held;
  if (held) {
    x = hx;
    y = hy;
  }
  if (contact.held) {
    lastX_ = x;
    lastY_ = y;
    trackedContact_ = true;
  }
  contact.ended = input.wasScreenTouchReleased();
  contact.tap = input.wasScreenTapped(x, y);
  const bool liveRelease = contact.ended && trackedContact_;
  contact.x = contact.tap && !liveRelease ? x : lastX_;
  contact.y = contact.tap && !liveRelease ? y : lastY_;
  const bool openingDrag = liveRelease && captureOpeningTouch_;
  if (contact.ended || contact.tap) trackedContact_ = captureOpeningTouch_ = false;
  contact.cancel = input.wasReleased(MappedInputManager::Button::Back) && !(openingDrag && input.wasBackGesture());
  contact.next = input.wasReleased(MappedInputManager::Button::NavNext);
  contact.previous = input.wasReleased(MappedInputManager::Button::NavPrevious);
  contact.confirm = input.wasReleased(MappedInputManager::Button::Confirm);
  return capture(contact);
}
bool Alternates::capture(Contact contact) {
  const auto phase = phase_.load();
  if (phase == Phase::Closed) return false;
  if (phase == Phase::Closing) {
    if (visible(contact.painted) && !closingContact_) {
      reset();
      return false;
    }
    if (contact.held) closingContact_ = true;
    if (contact.ended || contact.tap) closingContact_ = false;
    return true;
  }
  if (!contact.held && !contact.ended && !contact.tap && !contact.cancel && !contact.next && !contact.previous &&
      !contact.confirm)
    return true;
  // Hover samples may coalesce; critical releases retain their original stamp.
  if (queued_ && contact.held && !contact.ended && !contact.tap && !contact.cancel && !contact.next &&
      !contact.previous && !contact.confirm) {
    auto& last = queue_[(first_ + queued_ - 1) % std::size(queue_)];
    if (last.held && !last.ended && !last.tap && !last.cancel && !last.next && !last.previous && !last.confirm) {
      last = contact;
      return true;
    }
  }
  if (queued_ == std::size(queue_)) {
    first_ = (first_ + 1) % std::size(queue_);
    --queued_;
  }
  queue_[(first_ + queued_++) % std::size(queue_)] = contact;
  return true;
}
void Alternates::close(bool contact) {
  built_.store(false);
  closingContact_ = contact;
  first_ = queued_ = 0;
  phase_.store(Phase::Closing);
}
void Alternates::consume(Contact contact, int hit, Result& result) {
  if (contact.cancel) {
    close(contact.held);
    result.changed = true;
    return;
  }
  if (openingConfirm_ && contact.confirm) {
    openingConfirm_ = false;
    return;
  }
  const bool releasedOpening = openingTouch_ && (contact.ended || contact.tap);
  if (releasedOpening) openingTouch_ = false;
  if (!visible(contact.painted)) return;
  if (contact.next || contact.previous) {
    selected_ = (selected_ + count_ + (contact.next ? 1 : -1)) % count_;
    result.changed = true;
  }
  if (contact.held && hit >= 0 && hit != selected_) {
    selected_ = hit;
    result.changed = true;
  }
  if (contact.confirm || ((contact.tap || contact.ended) && hit >= 0)) {
    const int selected = contact.confirm ? selected_ : hit;
    std::memcpy(result.output, choices_[selected], sizeof(result.output));
    close(contact.held);
    result.changed = true;
  } else if (contact.tap && !releasedOpening) {
    close(contact.held);
    result.changed = true;
  }
}
const char* Alternates::displayLabel(const char* value, char (&label)[24]) {
  if (!std::strcmp(value, "\xe2\x80\x8c")) {
    return tr(STR_KEY_HALF_SPACE);
  }
  const auto* cursor = reinterpret_cast<const unsigned char*>(value);
  const auto cp = utf8NextCodepoint(&cursor);
  char* end = label;
  if (utf8IsCombiningMark(cp) || (cp >= 0x591 && cp <= 0x5c7) || (cp >= 0x64b && cp <= 0x65f) || cp == 0x670 ||
      cp == 0xe31 || (cp >= 0xe34 && cp <= 0xe3a) || (cp >= 0xe47 && cp <= 0xe4e))
    end = utf8AppendCodepoint(end, 0x25cc);
  const size_t bytes = std::min<size_t>(std::strlen(value), sizeof(label) - (end - label) - 1);
  std::memcpy(end, value, bytes);
  end[bytes] = 0;
  return label;
}
}  // namespace keyboardview
