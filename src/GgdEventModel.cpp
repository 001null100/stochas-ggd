#include "GgdEventModel.h"

#include <algorithm>
#include <cstring>

namespace
{
bool eventLess(const GgdSequenceEvent& a, const GgdSequenceEvent& b)
{
    if (a.tick != b.tick)
        return a.tick < b.tick;
    return a.row < b.row;
}
}

GgdEventPattern::GgdEventPattern()
{
    std::memset(events, 0, sizeof(events));
}

void GgdEventPattern::clear(bool keepGeometry)
{
    eventCount = 0;
    std::memset(events, 0, sizeof(events));

    if (!keepGeometry)
    {
        numerator = 4;
        denominator = 4;
        bars = 1;
        active = false;
    }
}

void GgdEventPattern::activate(int newNumerator, int newDenominator, int newBars)
{
    active = true;
    setGeometry(newNumerator, newDenominator, newBars);
}

void GgdEventPattern::setGeometry(int newNumerator, int newDenominator, int newBars)
{
    numerator = static_cast<std::int16_t>(std::max(1, std::min(32, newNumerator)));

    switch (newDenominator)
    {
        case 2:
        case 4:
        case 8:
        case 16:
            denominator = static_cast<std::int16_t>(newDenominator);
            break;
        default:
            denominator = 4;
            break;
    }

    bars = static_cast<std::int16_t>(std::max(1, std::min(64, newBars)));
    active = true;

    const int length = getLengthTicks();
    int write = 0;
    for (int read = 0; read < eventCount; ++read)
    {
        if (events[read].tick < 0 || events[read].tick >= length)
            continue;
        if (write != read)
            events[write] = events[read];
        ++write;
    }
    eventCount = write;
}

int GgdEventPattern::getLengthTicks() const
{
    const std::int64_t ticksPerBar = static_cast<std::int64_t>(GGD_EVENT_PPQ)
                                   * static_cast<std::int64_t>(numerator)
                                   * 4
                                   / std::max(1, static_cast<int>(denominator));
    return static_cast<int>(std::max<std::int64_t>(1, ticksPerBar * bars));
}

const GgdSequenceEvent* GgdEventPattern::getEvent(int index) const
{
    return index >= 0 && index < eventCount ? &events[index] : nullptr;
}

GgdSequenceEvent* GgdEventPattern::getEvent(int index)
{
    return index >= 0 && index < eventCount ? &events[index] : nullptr;
}

int GgdEventPattern::findEvent(int row, int tick) const
{
    const int start = insertionIndex(row, tick);
    if (start >= 0 && start < eventCount
        && events[start].tick == tick
        && events[start].row == row)
        return start;
    return -1;
}

const GgdSequenceEvent* GgdEventPattern::findEventPtr(int row, int tick) const
{
    const int index = findEvent(row, tick);
    return index >= 0 ? &events[index] : nullptr;
}

GgdSequenceEvent* GgdEventPattern::findEventPtr(int row, int tick)
{
    const int index = findEvent(row, tick);
    return index >= 0 ? &events[index] : nullptr;
}

int GgdEventPattern::insertionIndex(int row, int tick) const
{
    int lo = 0;
    int hi = eventCount;
    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;
        const auto& event = events[mid];
        if (event.tick < tick || (event.tick == tick && event.row < row))
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

bool GgdEventPattern::addEvent(int row,
                               int tick,
                               int velocity,
                               int probability,
                               int durationTicks)
{
    if (row < 0 || row >= SEQ_MAX_ROWS || tick < 0 || tick >= getLengthTicks())
        return false;

    active = true;
    velocity = std::max(1, std::min(127, velocity));
    probability = std::max(0, std::min(100, probability));
    durationTicks = std::max(1, std::min(65535, durationTicks));

    const int existing = findEvent(row, tick);
    if (existing >= 0)
    {
        auto& event = events[existing];
        event.velocity = static_cast<std::uint8_t>(velocity);
        event.probability = static_cast<std::int8_t>(probability);
        event.durationTicks = static_cast<std::uint16_t>(durationTicks);
        return true;
    }

    if (eventCount >= GGD_MAX_EVENTS_PER_PATTERN)
        return false;

    const int insert = insertionIndex(row, tick);
    for (int i = eventCount; i > insert; --i)
        events[i] = events[i - 1];

    auto& event = events[insert];
    event.tick = tick;
    event.row = static_cast<std::uint8_t>(row);
    event.velocity = static_cast<std::uint8_t>(velocity);
    event.probability = static_cast<std::int8_t>(probability);
    event.durationTicks = static_cast<std::uint16_t>(durationTicks);
    event.reserved = 0;
    ++eventCount;
    return true;
}

bool GgdEventPattern::removeEvent(int row, int tick)
{
    const int index = findEvent(row, tick);
    if (index < 0)
        return false;

    for (int i = index; i + 1 < eventCount; ++i)
        events[i] = events[i + 1];
    --eventCount;
    if (eventCount >= 0)
        events[eventCount] = {};
    return true;
}

bool GgdEventPattern::moveEvent(int row, int tick, int newRow, int newTick)
{
    const int index = findEvent(row, tick);
    if (index < 0)
        return false;
    if (newRow < 0 || newRow >= SEQ_MAX_ROWS || newTick < 0 || newTick >= getLengthTicks())
        return false;
    if ((newRow != row || newTick != tick) && findEvent(newRow, newTick) >= 0)
        return false;

    const auto event = events[index];
    removeEvent(row, tick);
    return addEvent(newRow, newTick,
                    event.velocity,
                    event.probability,
                    event.durationTicks);
}

bool GgdEventPattern::updateEvent(int row,
                                  int tick,
                                  int velocity,
                                  int probability,
                                  int durationTicks)
{
    auto* event = findEventPtr(row, tick);
    if (event == nullptr)
        return false;

    event->velocity = static_cast<std::uint8_t>(std::max(1, std::min(127, velocity)));
    event->probability = static_cast<std::int8_t>(std::max(0, std::min(100, probability)));
    event->durationTicks = static_cast<std::uint16_t>(
        std::max(1, std::min(65535, durationTicks)));
    return true;
}

void GgdEventPattern::sortEvents()
{
    std::sort(events, events + eventCount, eventLess);
}
