#include "SequenceData.h"

#include <algorithm>
#include <cmath>

GgdEventPattern* SequenceLayer::getEventPattern(int pat)
{
    if (pat < 0)
        pat = mCurrentPattern;
    jassert(pat >= 0 && pat < SEQ_MAX_PATTERNS);
    return &mPats[pat].mEvents;
}

const GgdEventPattern* SequenceLayer::getEventPattern(int pat) const
{
    if (pat < 0)
        pat = mCurrentPattern;
    jassert(pat >= 0 && pat < SEQ_MAX_PATTERNS);
    return &mPats[pat].mEvents;
}

bool SequenceLayer::legacyPatternHasData(int pat) const
{
    if (pat < 0)
        pat = mCurrentPattern;
    if (pat < 0 || pat >= SEQ_MAX_PATTERNS)
        return false;

    for (int row = 0; row < SEQ_MAX_ROWS; ++row)
        for (int step = 0; step < SEQ_MAX_STEPS; ++step)
            if (mPats[pat].mRows[row].mSteps[step].prob != SEQ_PROB_OFF)
                return true;
    return false;
}

void SequenceLayer::migrateLegacyPatternToEvents(int pat,
                                                 int numerator,
                                                 int denominator,
                                                 int bars)
{
    if (pat < 0)
        pat = mCurrentPattern;
    if (pat < 0 || pat >= SEQ_MAX_PATTERNS)
        return;

    auto& destination = mPats[pat].mEvents;
    if (destination.isActive())
        return;

    destination.activate(numerator, denominator, bars);
    const int patternLength = destination.getLengthTicks();

    for (int row = 0; row < SEQ_MAX_ROWS; ++row)
    {
        for (int step = 0; step < SEQ_MAX_STEPS; ++step)
        {
            const auto& cell = mPats[pat].mRows[row].mSteps[step];
            if (cell.prob == SEQ_PROB_OFF)
                continue;

            const int baseTick = step * GGD_TICKS_PER_16TH;
            if (baseTick >= patternLength)
                continue;

            const int velocity = std::max(1, static_cast<int>(cell.velo));
            const int probability = std::max(0, static_cast<int>(cell.prob));

            if (cell.length < 0)
            {
                const int triggers = std::max(2, -static_cast<int>(cell.length) + 1);
                for (int trigger = 0; trigger < triggers; ++trigger)
                {
                    const int tick = baseTick
                                   + static_cast<int>(std::llround(
                                         static_cast<double>(GGD_TICKS_PER_16TH) * trigger
                                         / static_cast<double>(triggers)));
                    if (tick >= patternLength)
                        break;
                    const int duration = std::max(
                        1, GGD_TICKS_PER_16TH / std::max(1, triggers * 2));
                    destination.addEvent(row, tick, velocity, probability, duration);
                }
                continue;
            }

            const int shiftedTick = baseTick
                                  + static_cast<int>(std::llround(
                                        static_cast<double>(cell.offset)
                                        * GGD_TICKS_PER_16TH / 100.0));
            const int tick = std::max(0, std::min(patternLength - 1, shiftedTick));
            const int duration = std::max(
                1, (static_cast<int>(cell.length) + 1) * GGD_TICKS_PER_16TH);
            destination.addEvent(row, tick, velocity, probability, duration);
        }
    }
}
