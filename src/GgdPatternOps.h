#pragma once

#include "GgdEventModel.h"

#include <vector>

// Whole-pattern operations shared by the editor and the tests.
namespace GgdPatternOps
{
// A hit that does not fit the current meter or bar count. It is kept in
// musical coordinates (bar + offset into the bar) so it can return when the
// pattern grows again.
struct ParkedHit
{
    int bar = 0;
    int offset = 0;
    int row = 0;
    int velocity = 100;
    int probability = SEQ_PROB_ON;
    int durationTicks = GGD_DEFAULT_EVENT_DURATION_TICKS;
};

// Largest bar count that fits the engine's pattern length limit.
int maxBarsFor(int numerator, int denominator);

// Changes meter and bar count without moving hits between bars: every hit
// keeps its bar and its offset from the bar line. Hits that no longer fit
// (offset beyond the new bar length, or bar beyond the new bar count) are
// appended to `parked`; parked hits that fit again are restored when their
// spot is free. Returns true if the pattern changed.
bool changeGeometry(GgdEventPattern& pattern,
                    int numerator,
                    int denominator,
                    int bars,
                    std::vector<ParkedHit>& parked);

// Doubles the bar count by repeating the existing bars. Returns false when
// the doubled pattern would exceed the length limit.
bool duplicateBars(GgdEventPattern& pattern);
}
