#include "GgdPatternOps.h"

#include <algorithm>

namespace GgdPatternOps
{
int maxBarsFor(int numerator, int denominator)
{
    const int barTicks = GgdEventPattern::barTicksFor(numerator, denominator);
    return juce::jlimit(1, GGD_MAX_PATTERN_BARS, GGD_MAX_PATTERN_TICKS / juce::jmax(1, barTicks));
}

bool changeGeometry(GgdEventPattern& pattern,
                    int numerator,
                    int denominator,
                    int bars,
                    std::vector<ParkedHit>& parked)
{
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
        return false;

    const int oldBarTicks = pattern.getBarTicks();
    std::vector<ParkedHit> current;
    current.reserve(static_cast<size_t>(pattern.getEventCount()));
    for (int i = 0; i < pattern.getEventCount(); ++i)
    {
        const auto* event = pattern.getEvent(i);
        ParkedHit hit;
        hit.bar = event->tick / oldBarTicks;
        hit.offset = event->tick % oldBarTicks;
        hit.row = event->row;
        hit.velocity = event->velocity;
        hit.probability = event->probability;
        hit.durationTicks = event->durationTicks;
        current.push_back(hit);
    }

    pattern.clear(true);
    pattern.setGeometry(numerator, denominator, bars);
    const int newBarTicks = pattern.getBarTicks();

    std::vector<ParkedHit> stillParked;
    auto place = [&](const ParkedHit& hit)
    {
        if (hit.bar >= bars || hit.offset >= newBarTicks)
            return false;
        const int tick = hit.bar * newBarTicks + hit.offset;
        if (pattern.findEvent(hit.row, tick) >= 0)
            return true; // a live hit already owns this spot
        return pattern.addEvent(hit.row, tick, hit.velocity, hit.probability, hit.durationTicks);
    };

    for (const auto& hit : current)
        if (!place(hit))
            stillParked.push_back(hit);
    for (const auto& hit : parked)
        if (!place(hit))
            stillParked.push_back(hit);

    parked = std::move(stillParked);
    return true;
}

bool duplicateBars(GgdEventPattern& pattern)
{
    if (!pattern.isActive())
        return false;

    const int bars = pattern.getBars();
    const int numerator = pattern.getNumerator();
    const int denominator = pattern.getDenominator();
    if (bars * 2 > maxBarsFor(numerator, denominator)
        || pattern.getEventCount() * 2 > GGD_MAX_EVENTS_PER_PATTERN)
        return false;

    const int length = pattern.getLengthTicks();
    std::vector<GgdSequenceEvent> events;
    events.reserve(static_cast<size_t>(pattern.getEventCount()));
    for (int i = 0; i < pattern.getEventCount(); ++i)
        events.push_back(*pattern.getEvent(i));

    pattern.setGeometry(numerator, denominator, bars * 2);
    for (const auto& event : events)
        pattern.addEvent(event.row, event.tick + length, event.velocity,
                         event.probability, event.durationTicks);
    return true;
}
}
