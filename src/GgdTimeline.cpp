#include "GgdTimeline.h"

#include <algorithm>
#include <cmath>

namespace GgdTimeline
{
namespace
{
// Positions closer than this (in bars or loops) to the next boundary belong
// to the next bar. Host positions arrive as doubles derived from sample
// counts, so an exact bar line can land a hair below the boundary.
constexpr double boundaryEpsilon = 1.0e-9;
constexpr double minimumBarQuarters = 1.0 / 64.0;

std::int64_t positiveModulo(std::int64_t value, std::int64_t divisor)
{
    const std::int64_t result = value % divisor;
    return result < 0 ? result + divisor : result;
}

// Moves the anchor's reference bar so that ppq lies inside it.
void normaliseBar(const Anchor& anchor, double ppq, double& barStart, std::int64_t& barNumber)
{
    const double length = anchor.barQuarters;
    const double relative = (ppq - anchor.barStartPpq) / length;
    const auto shift = static_cast<std::int64_t>(std::floor(relative + boundaryEpsilon));
    barStart = anchor.barStartPpq + static_cast<double>(shift) * length;
    barNumber = anchor.barNumber + shift;
}
}

double barQuartersForMeter(int numerator, int denominator)
{
    if (!isUsableMeter(numerator, denominator))
        return 4.0;
    return static_cast<double>(numerator) * 4.0 / static_cast<double>(denominator);
}

bool isUsableMeter(int numerator, int denominator)
{
    return numerator >= 1 && numerator <= 64
        && denominator >= 1 && denominator <= 64;
}

Anchor resolveAnchor(SyncMode mode,
                     const HostPosition& host,
                     const PatternShape& shape,
                     double originPpq,
                     double speed)
{
    Anchor anchor;
    anchor.originPpq = originPpq;
    anchor.ticksPerQuarterScaled = ticksPerQuarter * (speed > 0.0 ? speed : 1.0);

    const bool normalSpeed = std::abs(speed - 1.0) < 1.0e-9;
    if (mode != SyncMode::hostBars || !normalSpeed || std::abs(originPpq) > 1.0e-9)
        return anchor;

    double barLength = host.hasTimeSignature && isUsableMeter(host.numerator, host.denominator)
        ? barQuartersForMeter(host.numerator, host.denominator)
        : shape.barQuarters();
    barLength = std::max(minimumBarQuarters, barLength);

    anchor.barAnchored = true;
    anchor.barQuarters = barLength;

    if (host.hasBarStart)
    {
        anchor.barStartPpq = host.barStartPpq;
        anchor.barNumber = host.hasBarNumber
            ? host.barNumber
            : static_cast<std::int64_t>(std::llround(host.barStartPpq / barLength));
    }
    else
    {
        // Without host bar information assume the meter has been constant
        // since song start, which is exactly what the host would report.
        const auto bar = static_cast<std::int64_t>(
            std::floor(host.ppq / barLength + boundaryEpsilon));
        anchor.barNumber = bar;
        anchor.barStartPpq = static_cast<double>(bar) * barLength;
    }

    return anchor;
}

int segmentsForRange(const Anchor& anchor,
                     const PatternShape& shape,
                     double ppqStart,
                     double ppqEnd,
                     Segment* output,
                     int maxSegments)
{
    if (output == nullptr || maxSegments <= 0 || !(ppqEnd > ppqStart)
        || shape.barTicks <= 0 || shape.bars <= 0)
        return 0;

    int count = 0;

    if (anchor.barAnchored)
    {
        double barStart = 0.0;
        std::int64_t barNumber = 0;
        normaliseBar(anchor, ppqStart, barStart, barNumber);

        const double patternBarQuarters = shape.barQuarters();
        double position = ppqStart;

        while (position < ppqEnd && count < maxSegments)
        {
            const double barEnd = barStart + anchor.barQuarters;
            const double segmentEnd = std::min(ppqEnd, barEnd);
            const double offset = std::max(0.0, position - barStart);

            if (offset < patternBarQuarters)
            {
                const double end = std::min(segmentEnd, barStart + patternBarQuarters);
                if (end > position)
                {
                    const auto patternBar = positiveModulo(barNumber, shape.bars);
                    Segment& segment = output[count++];
                    segment.ppqStart = position;
                    segment.ppqEnd = end;
                    segment.tickStart = static_cast<double>(patternBar) * shape.barTicks
                                      + offset * ticksPerQuarter;
                    segment.ticksPerQuarterScaled = ticksPerQuarter;
                    segment.cycle = barNumber;
                }
            }

            position = segmentEnd;
            barStart = barEnd;
            ++barNumber;
        }

        return count;
    }

    const double length = static_cast<double>(shape.lengthTicks());
    const double scale = anchor.ticksPerQuarterScaled > 0.0
        ? anchor.ticksPerQuarterScaled : ticksPerQuarter;

    const double absoluteTicks = (ppqStart - anchor.originPpq) * scale;
    auto cycle = static_cast<std::int64_t>(std::floor(absoluteTicks / length + boundaryEpsilon));
    double tick = absoluteTicks - static_cast<double>(cycle) * length;
    if (tick < 0.0)
        tick = 0.0;

    double position = ppqStart;
    while (position < ppqEnd && count < maxSegments)
    {
        const double remaining = length - tick;
        const double segmentEnd = std::min(ppqEnd, position + remaining / scale);
        if (segmentEnd > position)
        {
            Segment& segment = output[count++];
            segment.ppqStart = position;
            segment.ppqEnd = segmentEnd;
            segment.tickStart = tick;
            segment.ticksPerQuarterScaled = scale;
            segment.cycle = cycle;
        }

        position = segmentEnd;
        tick = 0.0;
        ++cycle;
    }

    return count;
}

double tickAt(const Anchor& anchor, const PatternShape& shape, double ppq)
{
    if (shape.barTicks <= 0 || shape.bars <= 0)
        return -1.0;

    if (anchor.barAnchored)
    {
        double barStart = 0.0;
        std::int64_t barNumber = 0;
        normaliseBar(anchor, ppq, barStart, barNumber);

        const double offsetTicks = std::max(0.0, ppq - barStart) * ticksPerQuarter;
        if (offsetTicks >= shape.barTicks)
            return -1.0;

        const auto patternBar = positiveModulo(barNumber, shape.bars);
        return static_cast<double>(patternBar) * shape.barTicks + offsetTicks;
    }

    const double length = static_cast<double>(shape.lengthTicks());
    const double scale = anchor.ticksPerQuarterScaled > 0.0
        ? anchor.ticksPerQuarterScaled : ticksPerQuarter;
    double tick = std::fmod((ppq - anchor.originPpq) * scale, length);
    if (tick < 0.0)
        tick += length;
    return tick;
}
}
