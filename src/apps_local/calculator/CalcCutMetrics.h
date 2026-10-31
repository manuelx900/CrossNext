#pragma once

// The vertical facts about each cut, freestanding.
//
// Two numbers per cut: line advance and digit ink height.
//
// They are HERE, as constants, rather than read from the font at layout time,
// because the display's height is DERIVED from them -- it is the pending line
// plus the number's band plus the rule, and nothing else -- and a layout derived
// from a number the host suite cannot see is a layout the host suite cannot
// check. Toybox solves the same problem the same way in ToyboxTokens.h.
//
// They are not a second opinion: host-tests/calculator/label_fit.py checks every
// one against the real generated header, so a regenerated cut cannot silently
// move a band and take the difference out of the key rows.

#include "CalcFontIds.h"

namespace calc {

struct CutMetrics {
  int lineHeight = 0;  // EpdFontData::advanceY
  int capHeight = 0;   // ink height of '8', which is what the eye measures
};

constexpr CutMetrics kSmallCut{34, 18};
constexpr CutMetrics kLabelCut{45, 24};
constexpr CutMetrics kFinestCut{51, 27};
constexpr CutMetrics kTinyCut{74, 41};
constexpr CutMetrics kMidCut{96, 53};
constexpr CutMetrics kNumberCut{119, 65};

constexpr CutMetrics cutFor(const int fontId) {
  return fontId == kSmallFontId    ? kSmallCut
         : fontId == kLabelFontId  ? kLabelCut
         : fontId == kFinestFontId ? kFinestCut
         : fontId == kTinyFontId   ? kTinyCut
         : fontId == kMidFontId    ? kMidCut
                                   : kNumberCut;
}

constexpr int numberFontFor(const int rung) {
  return rung <= 0 ? kNumberFontId : (rung == 1 ? kMidFontId : (rung == 2 ? kTinyFontId : kFinestFontId));
}

}  // namespace calc
