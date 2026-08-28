#include "GgdDrumGridBeta.h"

#include <cmath>

int GgdDrumGrid::getSelectedProbability() const
{
    if (selection.empty())
        return -1;

    const int first = snapshotEvent(selection.front()).probability;
    for (size_t i = 1; i < selection.size(); ++i)
        if (snapshotEvent(selection[i]).probability != first)
            return -1;
    return first;
}

void GgdDrumGrid::setSelectedVelocity(int velocity)
{
    if (selection.empty())
        return;

    auto* p = pattern();
    if (p == nullptr)
        return;

    const int desired = juce::jlimit(1, 127, velocity);
    bool changed = false;
    for (const auto& ref : selection)
    {
        if (auto* event = p->findEventPtr(storageRowForCanonical(ref.row), ref.tick))
        {
            if (event->velocity != desired)
            {
                event->velocity = static_cast<std::uint8_t>(desired);
                changed = true;
            }
        }
    }

    if (changed)
        publishChange();
}

void GgdDrumGrid::setSelectedProbability(int probability)
{
    if (selection.empty())
        return;

    auto* p = pattern();
    if (p == nullptr)
        return;

    const int desired = juce::jlimit(0, 100, probability);
    bool changed = false;
    for (const auto& ref : selection)
    {
        if (auto* event = p->findEventPtr(storageRowForCanonical(ref.row), ref.tick))
        {
            if (event->probability != desired)
            {
                event->probability = static_cast<std::int8_t>(desired);
                changed = true;
            }
        }
    }

    if (changed)
        publishChange();
}

void GgdDrumGrid::duplicateSelectionWithOffset(int deltaTicks, float velocityScale)
{
    if (selection.empty() || deltaTicks == 0)
        return;

    const int length = patternLengthTicks();
    std::vector<EventState> source;
    source.reserve(selection.size());
    for (const auto& ref : selection)
        source.push_back(snapshotEvent(ref));

    std::vector<EventRef> created;
    created.reserve(source.size());
    for (auto state : source)
    {
        const int targetTick = state.ref.tick + deltaTicks;
        if (targetTick < 0 || targetTick >= length || eventOccupied(state.ref.row, targetTick))
            continue;

        state.velocity = juce::jlimit(
            1, 127,
            static_cast<int>(std::lround(static_cast<float>(state.velocity) * velocityScale)));
        if (writeEvent(state, state.ref.row, targetTick))
            created.push_back({ state.ref.row, targetTick });
    }

    if (created.empty())
        return;

    for (const auto& ref : created)
        addSelection(ref);
    publishChange();
}

void GgdDrumGrid::createFlamFromSelection()
{
    // 60 ticks is 1/64 at the 960 PPQ event resolution: ~31 ms at 120 BPM.
    // A lower grace velocity keeps this useful across snares/toms without
    // forcing a particular GGD articulation or tempo-dependent millisecond mode.
    duplicateSelectionWithOffset(-(GGD_EVENT_PPQ / 16), 0.64f);
}

void GgdDrumGrid::createDoubleFromSelection()
{
    // Add an independent 1/32 follow-up hit. The event engine means this is a
    // real hit with its own velocity/probability rather than a retrigger flag.
    duplicateSelectionWithOffset(GGD_TICKS_PER_32ND, 0.94f);
}
