#include "GgdPatternOps.h"

#include <vector>

namespace GgdPatternOps
{
int maxBarsFor(int numerator, int denominator)
{
    const int barTicks = GgdEventPattern::barTicksFor(numerator, denominator);
    return juce::jlimit(1, GGD_MAX_PATTERN_BARS, GGD_MAX_PATTERN_TICKS / juce::jmax(1, barTicks));
}

GeometryChange changeGeometry(GgdEventPattern& pattern, int numerator, int denominator, int bars)
{
    GeometryChange result;

    // Normalise the request exactly as the pattern would.
    GgdEventPattern probe;
    probe.activate(numerator, denominator, 1);
    numerator = probe.getNumerator();
    denominator = probe.getDenominator();
    bars = juce::jlimit(1, maxBarsFor(numerator, denominator), bars);

    if (pattern.isActive()
        && pattern.getNumerator() == numerator
        && pattern.getDenominator() == denominator
        && pattern.getBars() == bars)
        return result;

    const int oldBarTicks = pattern.getBarTicks();
    std::vector<GgdParkedHit> live;
    live.reserve(static_cast<size_t>(pattern.getEventCount()));
    for (int i = 0; i < pattern.getEventCount(); ++i)
    {
        const auto* event = pattern.getEvent(i);
        GgdParkedHit hit;
        hit.bar = static_cast<std::int16_t>(event->tick / oldBarTicks);
        hit.offset = event->tick % oldBarTicks;
        hit.row = event->row;
        hit.velocity = event->velocity;
        hit.probability = event->probability;
        hit.durationTicks = event->durationTicks;
        live.push_back(hit);
    }

    std::vector<GgdParkedHit> parked;
    parked.reserve(static_cast<size_t>(pattern.getParkedCount()));
    for (int i = 0; i < pattern.getParkedCount(); ++i)
        parked.push_back(*pattern.getParked(i));

    pattern.clear(true);
    pattern.clearParked();
    pattern.setGeometry(numerator, denominator, bars);
    const int newBarTicks = pattern.getBarTicks();

    auto fits = [&](const GgdParkedHit& hit) { return hit.bar < bars && hit.offset < newBarTicks; };
    auto place = [&](const GgdParkedHit& hit)
    {
        const int tick = hit.bar * newBarTicks + hit.offset;
        if (pattern.findEvent(hit.row, tick) < 0) // a live hit owns the spot otherwise
            pattern.addEvent(hit.row, tick, hit.velocity, hit.probability, hit.durationTicks);
    };
    auto park = [&](const GgdParkedHit& hit)
    {
        if (!pattern.addParked(hit))
            ++result.dropped;
    };

    for (const auto& hit : live)
    {
        if (fits(hit))
            place(hit);
        else
        {
            park(hit);
            ++result.hidden;
        }
    }
    for (const auto& hit : parked)
    {
        if (fits(hit))
        {
            place(hit);
            ++result.restored;
        }
        else
        {
            park(hit);
        }
    }

    result.changed = true;
    return result;
}

bool duplicateBars(GgdEventPattern& pattern, int* discardedParked)
{
    if (discardedParked != nullptr)
        *discardedParked = 0;
    if (!pattern.isActive())
        return false;

    const int bars = pattern.getBars();
    const int numerator = pattern.getNumerator();
    const int denominator = pattern.getDenominator();
    if (bars * 2 > maxBarsFor(numerator, denominator)
        || pattern.getEventCount() * 2 > GGD_MAX_EVENTS_PER_PATTERN)
        return false;

    const int length = pattern.getLengthTicks();
    const int barTicks = pattern.getBarTicks();
    std::vector<GgdSequenceEvent> events;
    events.reserve(static_cast<size_t>(pattern.getEventCount()));
    for (int i = 0; i < pattern.getEventCount(); ++i)
        events.push_back(*pattern.getEvent(i));

    pattern.setGeometry(numerator, denominator, bars * 2);
    for (const auto& event : events)
        pattern.addEvent(event.row, event.tick + length, event.velocity,
                         event.probability, event.durationTicks);

    // Hidden hits from the bars the repeat now fills are superseded by it.
    std::vector<GgdParkedHit> keep;
    for (int i = 0; i < pattern.getParkedCount(); ++i)
    {
        const auto& hit = *pattern.getParked(i);
        const bool superseded = hit.bar >= bars && hit.bar < bars * 2 && hit.offset < barTicks;
        if (superseded)
        {
            if (discardedParked != nullptr)
                ++*discardedParked;
        }
        else
        {
            keep.push_back(hit);
        }
    }
    pattern.clearParked();
    for (const auto& hit : keep)
        pattern.addParked(hit);
    return true;
}
}
