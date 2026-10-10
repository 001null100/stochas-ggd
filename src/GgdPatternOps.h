#pragma once

#include "GgdEventModel.h"

// Whole-pattern operations shared by the editor and the tests.
namespace GgdPatternOps
{
// Largest bar count that fits the engine's pattern length limit.
int maxBarsFor(int numerator, int denominator);

struct GeometryChange
{
    bool changed = false;
    int hidden = 0;   // hits that no longer fit and were parked
    int restored = 0; // parked hits that fit again
    int dropped = 0;  // hits lost because the parked store was full
};

// Changes meter and bar count without moving hits between bars: every hit
// keeps its bar and its offset from the bar line. Hits that no longer fit
// (offset beyond the new bar length, or bar beyond the new bar count) are
// parked inside the pattern; parked hits that fit again come back unless a
// live hit already owns their spot.
GeometryChange changeGeometry(GgdEventPattern& pattern, int numerator, int denominator, int bars);

// Doubles the bar count by repeating the existing bars. Parked hits that
// belonged to the bars now filled by the repeat are discarded and counted in
// `discardedParked`. Returns false when the doubled pattern would exceed the
// length limit.
bool duplicateBars(GgdEventPattern& pattern, int* discardedParked = nullptr);
}
