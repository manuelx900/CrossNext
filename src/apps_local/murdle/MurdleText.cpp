#include "MurdleText.h"

#include <I18n.h>

#include <cstdio>
#include <cstring>

#include "../ui/ToyboxFormat.h"
#include "MurdleLabels.h"
#include "MurdlePeople.h"

namespace murdletext {

using murdle::Anchor;
using murdle::Cat;
using murdle::Clue;
using murdle::kMaxCats;
using murdle::kNoAttr;
using murdle::kNobodySpeaks;
using murdle::Puzzle;

namespace {

int popcount(uint8_t mask) {
  int n = 0;
  while (mask) {
    mask &= static_cast<uint8_t>(mask - 1);
    ++n;
  }
  return n;
}

int lowestBit(const uint8_t mask) {
  for (int i = 0; i < 8; ++i) {
    if (mask & static_cast<uint8_t>(1u << i)) return i;
  }
  return 0;
}

int highestBit(const uint8_t mask) {
  int found = 0;
  for (int i = 0; i < 8; ++i) {
    if (mask & static_cast<uint8_t>(1u << i)) found = i;
  }
  return found;
}

// The one item a mask leaves out, when it leaves out exactly one.
int missingBit(const uint8_t mask, const int items) {
  for (int i = 0; i < items; ++i) {
    if ((mask & static_cast<uint8_t>(1u << i)) == 0) return i;
  }
  return 0;
}

const murdle::SuspectEntry& suspectOf(const Puzzle& p, const int item) {
  return murdle::kSuspects[p.cast[static_cast<int>(Cat::Suspect)][item]];
}

const char* fixture(const Puzzle& p, Cat cat, int item, int field) {
  const int index = p.cast[static_cast<int>(cat)][item];
  const auto& entry = cat == Cat::Weapon     ? murdlelabels::weapons[index]
                      : cat == Cat::Location ? murdlelabels::places[index]
                                             : murdlelabels::motives[index];
  return I18N.get(field == 0 ? entry.name : field == 1 ? entry.phrase : entry.trait);
}
// The form a fixture takes inside a sentence, preposition and article already
// attached. Suspects are their own name; everything else was written with its
// grammar built in, so a template never has to decide between "a" and "an".
const char* phraseOf(const Puzzle& p, const int cat, const int item) {
  switch (static_cast<Cat>(cat)) {
    case Cat::Suspect:
      return murdlepeople::name(p, item);
    case Cat::Weapon:
      return fixture(p, Cat::Weapon, item, 1);
    case Cat::Location:
      return fixture(p, Cat::Location, item, 1);
    case Cat::Motive:
      return fixture(p, Cat::Motive, item, 1);
  }
  return "";
}

void appendCapitalised(char* out, const int cap, const char* text) {
  const int len = static_cast<int>(std::strlen(out));
  std::snprintf(out + len, static_cast<size_t>(cap - len), "%s", text);
  if (out[len] >= 'a' && out[len] <= 'z') out[len] = static_cast<char>(out[len] - 'a' + 'A');
}

void append(char* out, const int cap, const char* fmt, const char* a, const char* b = nullptr) {
  const int len = static_cast<int>(std::strlen(out));
  if (b) {
    std::snprintf(out + len, static_cast<size_t>(cap - len), fmt, a, b);
  } else {
    std::snprintf(out + len, static_cast<size_t>(cap - len), fmt, a);
  }
}

// ---------------------------------------------------------------------------
// Who the clue is about

// ONLY THE BODY CLUE NAMES A THING BY ITS DETAIL. Every clue here says "the
// hammer", never "the thing with the split handle".
//
// This was the other way round for four rounds, and round five made it worse by
// extending trait-naming from weapons to places as well, on the theory that an
// extra lookup is most of the difference between an easy tier and a hard one.
// Four play-testers demolished that in the same breath, independently:
//
//   "a seven-word detour to avoid saying 'the wire'"
//   "a pointer dereference"
//   "clue 6 is just cosplaying clue 13"
//
// They are right, and the reason is structural rather than a matter of taste.
// The case file has to print every weapon and place with its detail attached --
// it cannot print only the crime scene's, or the body clue would give itself
// away -- so the mapping from detail to name is sitting on the page. Naming by
// detail can therefore only ever be a synonym, and a longer one. It buys no
// difficulty, costs a line of a small screen, and does real harm: one case
// called the same place "the garden" in one clue and "the place with trampled
// flowers" two clues later, and a reader who misses that they are the same
// thing is stuck on a puzzle that is not actually hard.
//
// The body clue keeps the device because there it IS the conceit -- the game's
// last step is recognising a scene from a detail, and it has nowhere else to
// hide the murderer.
//
// THE REGISTER IS FIXED PER CATEGORY, NOT PER CASE, AND THAT IS THE THIRD
// ATTEMPT AT IT.
//
// First it varied per clue, and a play-tester got "Whoever carried the fork"
// beside "The one with the oar" in one case and said it read like several
// people wrote it. So it became one coin flip per case -- which is how a later
// tester ended up counting TEN OF TWELVE CLUES opening with the literal words
// "The one". Consistency bought at the price of a metronome.
//
// Both complaints are about the same thing from opposite ends: the variation
// was arbitrary. Keying it to the anchor's category makes it systematic
// instead. A weapon anchor always reads "the suspect with", a place anchor
// always "the one", a motive anchor always "the suspect driven by" -- so a case
// mixes phrasings without ever contradicting itself, and the same clue shape
// always sounds the same way. Nothing is random, so nothing can look sloppy.
void anchorPhrase(const Puzzle& p, const Clue& clue, char* out, const int cap) {
  out[0] = '\0';
  if (clue.anchor == Anchor::Murderer) {
    std::snprintf(out, static_cast<size_t>(cap), "%s", tr(STR_UI_THE_KILLER_F14FF9));
    return;
  }
  const int cat = clue.anchorCat;
  const int item = clue.anchorItem;
  switch (static_cast<Cat>(cat)) {
    case Cat::Suspect:
      std::snprintf(out, static_cast<size_t>(cap), "%s", murdlepeople::name(p, item));
      return;
    case Cat::Weapon:
      append(out, cap, tr(STR_UI_THE_SUSPECT_WITH_S_8F5B28), fixture(p, Cat::Weapon, item, 1));
      return;
    case Cat::Location:
      // "The one in the study", not "whoever was in the study". Every place has
      // an occupant -- it is a bijection -- so the conditional form asks the
      // reader to discharge a vacuous case that cannot arise, and a play-tester
      // reported hesitating over exactly that.
      append(out, cap, tr(STR_UI_THE_ONE_S_BE5F73), fixture(p, Cat::Location, item, 1));
      return;
    case Cat::Motive:
      // Only one wording here, and it is not a style choice. "Whoever wanted an
      // inheritance" reads well and "whoever wanted jealousy" does not, so a
      // wording that is right for two thirds of the table ships broken
      // sentences the rest of the time.
      append(out, cap, tr(STR_UI_THE_SUSPECT_DRIVEN_BY_S_A27F7D), fixture(p, Cat::Motive, item, 1));
      return;
  }
}

// ---------------------------------------------------------------------------
// What is said about them

const char* handedWord(const uint8_t v) {
  return v == 0 ? tr(STR_UI_LEFT_HANDED_1F1E5E) : tr(STR_UI_RIGHT_HANDED_D3C4BD);
}

const char* eyeWord(const uint8_t v) {
  static constexpr UiLabel words[4] = {trLabel(STR_UI_BROWN_9166EE), trLabel(STR_UI_BLUE_4C9A82),
                                       trLabel(STR_UI_GREEN_BC74F4), trLabel(STR_UI_HAZEL_F29AE3)};
  return words[v & 3];
}

const char* hairWord(const uint8_t v) {
  static constexpr UiLabel words[5] = {trLabel(STR_UI_BLACK_466BC8), trLabel(STR_UI_BROWN_9166EE),
                                       trLabel(STR_UI_BLOND_F198A0), trLabel(STR_UI_RED_789880),
                                       trLabel(STR_UI_GREY_7D1F8F)};
  return I18N.getLanguage() == Language::ES && v == 1 ? tr(STR_MURDLE_HAIR_BROWN) : words[v < 5 ? v : 0].get();
}

bool attributePhrase(const Puzzle& p, const uint8_t attr, char* out, const int cap) {
  if (attr == kNoAttr) return false;
  if (attr == murdle::kAttrTallest) {
    std::snprintf(out, static_cast<size_t>(cap), "%s", tr(STR_UI_WAS_THE_TALLEST_OF_THEM_EE59D9));
    return true;
  }
  if (attr == murdle::kAttrShortest) {
    std::snprintf(out, static_cast<size_t>(cap), "%s", tr(STR_UI_WAS_THE_SHORTEST_OF_THEM_CFC46F));
    return true;
  }
  // The reference suspect is named, so this reads as a fact about two people
  // and is checked against the dossier the same way the others are.
  if (attr >= murdle::kAttrTallerThan && attr < murdle::kAttrTallerThan + murdle::kMaxItems) {
    const int who = attr - murdle::kAttrTallerThan;
    std::snprintf(out, static_cast<size_t>(cap), tr(STR_UI_WAS_TALLER_THAN_S_1F3F3C), murdlepeople::name(p, who));
    return true;
  }
  if (attr >= murdle::kAttrShorterThan && attr < murdle::kAttrShorterThan + murdle::kMaxItems) {
    const int who = attr - murdle::kAttrShorterThan;
    std::snprintf(out, static_cast<size_t>(cap), tr(STR_UI_WAS_SHORTER_THAN_S_032802), murdlepeople::name(p, who));
    return true;
  }
  const uint8_t kind = static_cast<uint8_t>(attr / 16u);
  const uint8_t value = static_cast<uint8_t>(attr % 16u);
  switch (static_cast<murdle::AttrKind>(kind)) {
    case murdle::AttrKind::Handed:
      std::snprintf(out, static_cast<size_t>(cap),
                    I18N.getLanguage() == Language::ES ? tr(STR_MURDLE_USED_HAND) : tr(STR_UI_WAS_S_D3F24B),
                    handedWord(value));
      return true;
    case murdle::AttrKind::Eyes:
      std::snprintf(out, static_cast<size_t>(cap), tr(STR_UI_HAD_S_EYES_05CFF7), eyeWord(value));
      return true;
    case murdle::AttrKind::Hair:
      std::snprintf(out, static_cast<size_t>(cap), tr(STR_UI_HAD_S_HAIR_3C3E04), hairWord(value));
      return true;
  }
  return false;
}

// Templates per category, in the three shapes a mask can take. Written out
// rather than assembled from parts, because "was not driven by greed" and "did
// not carry the crowbar" do not share a shape and pretending they do is how a
// generated sentence starts reading like a form letter.
struct Forms {
  const char* positive;
  const char* negative;
  const char* either;
};

Forms formsFor(const int cat) {
  switch (static_cast<Cat>(cat)) {
    case Cat::Suspect:
      return Forms{tr(STR_UI_WAS_S_D3F24B), tr(STR_UI_WAS_NOT_S_218758), tr(STR_UI_WAS_EITHER_S_OR_S_594B9F)};
    case Cat::Weapon:
      return Forms{tr(STR_UI_CARRIED_S_E9C779), tr(STR_UI_DID_NOT_CARRY_S_A19513),
                   tr(STR_UI_CARRIED_EITHER_S_OR_S_E266A0)};
    case Cat::Location:
      return Forms{tr(STR_MURDLE_AT_PLACE), tr(STR_MURDLE_NOT_AT_PLACE), tr(STR_MURDLE_EITHER_PLACE)};
    case Cat::Motive:
      return Forms{tr(STR_UI_WAS_DRIVEN_BY_S_5C4D77), tr(STR_UI_WAS_NOT_DRIVEN_BY_S_2939A1),
                   tr(STR_UI_WAS_DRIVEN_BY_S_OR_BY_D331C8)};
  }
  return Forms{tr(STR_UI_WAS_S_D3F24B), tr(STR_UI_WAS_NOT_S_218758), tr(STR_UI_WAS_EITHER_S_OR_S_594B9F)};
}

// "FELIX did not carry the pan", not "whoever carried the pan was not FELIX".
//
// Both say the same thing and only one is English. When a clue's target is a
// suspect and its mask names one or two of them, the sentence reads far better
// turned round to start with the person -- the old form buried the identity at
// the end of a relative clause and a play-tester called it backwards on sight.
// Attribute clues are exempt: "whoever was at the inn was left-handed" is
// already the natural order.
bool suspectSidePhrase(const Puzzle& p, const Clue& clue, char* out, const int cap) {
  if (static_cast<Cat>(clue.targetCat) != Cat::Suspect) return false;
  if (clue.attr != kNoAttr || clue.anchor == Anchor::Murderer) return false;

  const int items = p.shape.items;
  const int set = popcount(clue.targetMask);
  const char* what = phraseOf(p, clue.anchorCat, clue.anchorItem);
  const char* did = nullptr;
  const char* didNot = nullptr;
  switch (static_cast<Cat>(clue.anchorCat)) {
    case Cat::Weapon:
      did = tr(STR_UI_S_CARRIED_S_3777D5);
      didNot = tr(STR_UI_S_DID_NOT_CARRY_S_1D4E9A);
      break;
    case Cat::Location:
      did = tr(STR_UI_S_WAS_S_2FCC6A);
      didNot = tr(STR_UI_S_WAS_NOT_S_B8E788);
      break;
    case Cat::Motive:
      did = tr(STR_UI_S_WAS_DRIVEN_BY_S_1B6721);
      didNot = tr(STR_UI_S_WAS_NOT_DRIVEN_BY_S_542490);
      break;
    default:
      return false;
  }

  out[0] = '\0';
  if (set == 1) {
    append(out, cap, did, murdlepeople::name(p, lowestBit(clue.targetMask)), what);
    return true;
  }
  if (set == items - 1) {
    append(out, cap, didNot, murdlepeople::name(p, missingBit(clue.targetMask, items)), what);
    return true;
  }
  if (set == 2) {
    // "%s or %s", two names.
    constexpr int kPairChars = 2 * kLabelMax + toybox::literalChars(" or ") + 1;
    char pair[kPairChars];
    std::snprintf(pair, sizeof(pair), tr(STR_UI_S_OR_S_9067B5), murdlepeople::name(p, lowestBit(clue.targetMask)),
                  murdlepeople::name(p, highestBit(clue.targetMask)));
    append(out, cap, did, pair, what);
    // "Either X or Y carried the pan" reads better than "X or Y carried".
    //
    // Prepended in place rather than composed into a second buffer and copied
    // back. That round trip could not be sized: a buffer wide enough to hold
    // "Either " plus a full-length sentence is by definition wider than `out`,
    // so the copy back cut the tail off the longest clue in the game -- and a
    // clue missing its last words is a clue that cannot be solved. Moving the
    // sentence along instead means the only thing that can be dropped is the
    // word "Either", and "X or Y carried the pan." is still true English.
    const int len = static_cast<int>(std::strlen(out));
    const int kEitherChars = static_cast<int>(std::strlen(tr(STR_UI_EITHER_DE72EC)));
    if (len + kEitherChars < cap) {
      std::memmove(out + kEitherChars, out, static_cast<size_t>(len) + 1);
      std::memcpy(out, tr(STR_UI_EITHER_DE72EC), static_cast<size_t>(kEitherChars));
    }
    return true;
  }
  return false;
}

void predicatePhrase(const Puzzle& p, const Clue& clue, char* out, const int cap) {
  out[0] = '\0';
  if (attributePhrase(p, clue.attr, out, cap)) return;

  const int items = p.shape.items;
  const uint8_t mask = clue.targetMask;
  const int set = popcount(mask);
  const Forms forms = formsFor(clue.targetCat);

  if (set == 1) {
    append(out, cap, forms.positive, phraseOf(p, clue.targetCat, lowestBit(mask)));
    return;
  }
  // A mask missing exactly one item reads as a denial, and it reads better that
  // way than as an either/or over the rest -- which at three items is the same
  // set of words and twice the length.
  if (set == items - 1) {
    append(out, cap, forms.negative, phraseOf(p, clue.targetCat, missingBit(mask, items)));
    return;
  }
  append(out, cap, forms.either, phraseOf(p, clue.targetCat, lowestBit(mask)),
         phraseOf(p, clue.targetCat, highestBit(mask)));
}

}  // namespace

// ---------------------------------------------------------------------------

const char* label(const Puzzle& p, const int cat, const int item) {
  switch (static_cast<Cat>(cat)) {
    case Cat::Suspect:
      return murdlepeople::name(p, item);
    case Cat::Weapon:
      return fixture(p, Cat::Weapon, item, 0);
    case Cat::Location:
      return fixture(p, Cat::Location, item, 0);
    case Cat::Motive:
      return fixture(p, Cat::Motive, item, 0);
  }
  return "";
}

// ONLY THE CATEGORY THE BODY CLUE USES SHOWS ITS DETAILS.
//
// Since the body clue became the only clue that names a thing by its detail,
// the other category's details were printed on every case file and referenced
// by nothing. Two play-testers counted them: "not one is referenced", and the
// sharper version -- it "trains the player to ignore parentheticals right
// before the last clue requires them".
//
// Hiding them leaks nothing. The body clue announces which category it is
// about in its own first six words ("beside the weapon with..." against "where
// there was..."), so a player who has read it already knows. What it buys is
// four fewer lines on a screen that has to hold a cast list and a grid key at
// the same time, and it makes the surviving details obviously load-bearing.
//
// The whole category is shown or none of it. Printing the detail for only the
// crime scene would name the crime scene.
const char* trait(const Puzzle& p, const int cat, const int item) {
  int detailCat = -1;
  for (int i = 0; i < p.clueCount; ++i) {
    if (p.clues[i].anchor == Anchor::Murderer) detailCat = p.clues[i].targetCat;
  }
  if (cat != detailCat) return "";

  switch (static_cast<Cat>(cat)) {
    case Cat::Weapon:
      return fixture(p, Cat::Weapon, item, 2);
    case Cat::Location:
      return fixture(p, Cat::Location, item, 2);
    case Cat::Suspect:
    case Cat::Motive:
      return "";
  }
  return "";
}

const char* categoryName(const int cat) {
  switch (static_cast<Cat>(cat)) {
    case Cat::Suspect:
      return tr(STR_UI_SUSPECTS_10BF05);
    case Cat::Weapon:
      return tr(STR_UI_WEAPONS_59D845);
    case Cat::Location:
      return tr(STR_UI_PLACES_3CC3CF);
    case Cat::Motive:
      return tr(STR_UI_MOTIVES_B5897D);
  }
  return "";
}

void axisLetters(const Puzzle& p, const int cat, char out[murdle::kMaxItems + 1]) {
  const int items = p.shape.items;
  uint32_t used = 0;
  for (int i = 0; i < items; ++i) {
    const char* name = label(p, cat, i);
    char chosen = 0;
    for (int n = 0; name[n] && !chosen; ++n) {
      const char c = name[n] >= 'a' && name[n] <= 'z' ? char(name[n] - 'a' + 'A') : name[n];
      if (c >= 'A' && c <= 'Z' && !(used & (1u << (c - 'A')))) chosen = c;
    }
    for (char c = 'A'; !chosen && c <= 'Z'; ++c)
      if (!(used & (1u << (c - 'A')))) chosen = c;
    out[i] = chosen;
    used |= 1u << (chosen - 'A');
  }
  out[items] = '\0';
}

void clueLine(const Puzzle& p, const int clueIndex, char* out, const int cap) {
  out[0] = '\0';
  if (clueIndex < 0 || clueIndex >= p.clueCount || cap < 8) return;
  const Clue& clue = p.clues[clueIndex];
  // `clue.voice` used to be overwritten here with a per-case register byte.
  // Nothing reads it any more: anchorPhrase keys its wording to the anchor's
  // category instead, which is systematic where this was a coin flip. The field
  // stays on Clue because it is part of the fingerprint a save is validated
  // against, and it still carries the generator's per-clue entropy for any
  // future choice that genuinely wants to vary within a case.

  // The murder clue names the crime scene by something in it rather than by its
  // name, which is what keeps the last step of a case a deduction instead of an
  // announcement.
  if (clue.anchor == Anchor::Murderer) {
    const int which = lowestBit(clue.targetMask);
    if (static_cast<Cat>(clue.targetCat) == Cat::Weapon) {
      std::snprintf(out, static_cast<size_t>(cap), tr(STR_UI_THE_BODY_WAS_FOUND_BESIDE_THE_16FD7D),
                    fixture(p, Cat::Weapon, which, 2));
      return;
    }
    const int place = which;
    // "where there was", and this is the third wording.
    //
    // "next to flour on the floor" broke for anything that is a property of a
    // room rather than an object in it. "in the place with" survived all
    // sixteen but leaked the generator's own category name onto the page, in
    // the single most important sentence of the case file -- two play-testers
    // pulled it up, one calling it "a template variable". Now that a place's
    // detail is always a STATE of the room rather than a thing lying in it,
    // "where there was" fits every one of them and reads like a sentence
    // somebody wrote: where there was an echo, a barking dog, a stopped clock.
    std::snprintf(out, static_cast<size_t>(cap), tr(STR_UI_THE_BODY_WAS_FOUND_WHERE_THERE_5E8733),
                  fixture(p, Cat::Location, place, 2));
    return;
  }

  // A statement is the suspect's own voice, so it is first person and it is in
  // quotes. Every statement this generator makes is a plain positive claim,
  // true from everyone but the murderer.
  if (clue.speaker != kNobodySpeaks) {
    const int item = lowestBit(clue.targetMask);
    const bool aboutSelf = clue.anchorItem == clue.speaker;
    const bool weapon = static_cast<Cat>(clue.targetCat) == Cat::Weapon;
    char body[kLineMax];
    body[0] = '\0';
    if (aboutSelf) {
      append(body, kLineMax, weapon ? tr(STR_UI_I_CARRIED_S_BA8B6D) : tr(STR_UI_I_WAS_S_2F762E),
             phraseOf(p, clue.targetCat, item));
    } else {
      // A witness talking about somebody else, which is the version of this
      // mechanic that has teeth: the murderer's lie lands on another suspect's
      // row rather than only denying one fact about themselves.
      append(body, kLineMax, weapon ? tr(STR_UI_S_CARRIED_S_3777D5) : tr(STR_UI_S_WAS_S_2FCC6A),
             murdlepeople::name(p, clue.anchorItem), phraseOf(p, clue.targetCat, item));
    }
    std::snprintf(out, static_cast<size_t>(cap), tr(STR_UI_S_SAYS_S_D59F86), murdlepeople::name(p, clue.speaker), body);
    return;
  }

  // A clue about a suspect is a whole sentence on its own, starting with the
  // person. It cannot go through the anchor-plus-predicate path below or it
  // would come out with two subjects: "Whoever carried the pan FELIX did not
  // carry the pan."
  char whole[kLineMax];
  if (suspectSidePhrase(p, clue, whole, kLineMax)) {
    out[0] = '\0';
    appendCapitalised(out, cap, whole);
    const int at = static_cast<int>(std::strlen(out));
    std::snprintf(out + at, static_cast<size_t>(cap - at), ".");
    return;
  }

  char who[kLineMax];
  char what[kLineMax];
  anchorPhrase(p, clue, who, kLineMax);
  predicatePhrase(p, clue, what, kLineMax);

  out[0] = '\0';
  appendCapitalised(out, cap, who);
  const int len = static_cast<int>(std::strlen(out));
  std::snprintf(out + len, static_cast<size_t>(cap - len), " %s.", what);
}

// ONLY THE DOSSIER COLUMNS THIS CASE ACTUALLY ASKS ABOUT.
//
// Mario, having played several: "can you make sure that we are actually using
// all the info that we're giving? Having seen a single clue with the height, or
// the hair, or the left handed stuff." Every play-tester said a version of it
// too, and one put the cost precisely -- printing four columns where one is
// live teaches a player to skip the dossier, which then burns them on the case
// that needs it.
//
// Forcing every column to appear in every case is not the answer: an Elementary
// case is under six clues long and four of them cannot be dossier lookups. So
// the guarantee runs the other way, and holds by construction rather than by
// tuning: print the columns the clues use, and nothing else. What is on the
// page is what the case is about, one hundred percent of the time.
//
// It is also what lets a suspect be one line like every other fixture, which is
// what makes the whole cast fit a single page. Same reasoning as trait().
void suspectAttributes(const Puzzle& p, const int item, char* out, const int cap) {
  const murdle::SuspectEntry& s = suspectOf(p, item);
  out[0] = '\0';

  bool used[4] = {};
  for (int i = 0; i < p.clueCount; ++i) {
    const uint8_t tag = p.clues[i].attr;
    if (tag == kNoAttr) continue;
    if (tag >= murdle::kAttrTallerThan) {
      used[3] = true;  // every height form, superlative or comparison
    } else {
      const uint8_t kind = static_cast<uint8_t>(tag / 16u);
      if (kind < 3) used[kind] = true;
    }
  }

  const auto add = [&](const char* fmt, auto... args) {
    const int len = static_cast<int>(std::strlen(out));
    if (len > 0) std::snprintf(out + len, static_cast<size_t>(cap - len), ", ");
    const int at = static_cast<int>(std::strlen(out));
    std::snprintf(out + at, static_cast<size_t>(cap - at), fmt, args...);
  };

  if (used[0])
    add("%s", s.handed == murdle::Handed::Left ? tr(STR_UI_LEFT_HANDED_1F1E5E) : tr(STR_UI_RIGHT_HANDED_D3C4BD));
  if (used[1] || (!used[0] && !used[2] && !used[3]))
    add(tr(STR_UI_S_EYES_78AB5E), eyeWord(static_cast<uint8_t>(s.eyes)));
  if (used[2]) add(tr(STR_UI_S_HAIR_1A8D5A), hairWord(static_cast<uint8_t>(s.hair)));
  if (used[3]) add("%d'%d\"", s.inches / 12, s.inches % 12);
}

void accusationLine(const Puzzle& p, const uint8_t picks[kMaxCats], char* out, const int cap) {
  const int cats = p.shape.cats;
  char tail[kLineMax];
  tail[0] = '\0';
  if (cats > static_cast<int>(Cat::Motive)) {
    append(tail, kLineMax, tr(STR_MURDLE_ACCUSATION_TAIL),
           fixture(p, Cat::Motive, picks[static_cast<int>(Cat::Motive)], 1));
  }
  std::snprintf(out, static_cast<size_t>(cap), tr(STR_MURDLE_ACCUSATION),
                murdlepeople::name(p, picks[static_cast<int>(Cat::Suspect)]),
                fixture(p, Cat::Weapon, picks[static_cast<int>(Cat::Weapon)], 1),
                fixture(p, Cat::Location, picks[static_cast<int>(Cat::Location)], 1), tail);
}

void blockedLine(const murdle::TapResult& result, char* out, const int cap) {
  out[0] = '\0';
  if (!result.blocked()) return;
  std::snprintf(out, static_cast<size_t>(cap), "%s", kBlockedNotice.get());
}

}  // namespace murdletext
