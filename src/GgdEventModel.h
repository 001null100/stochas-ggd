#pragma once

#include "Constants.h"

#include <cstdint>

// Musical positions in the beta event engine are stored independently of any
// editor grid. 960 ticks per quarter note exactly represents the straight and
// triplet divisions we care about (1/32 = 120, 1/8T = 320, 1/16T = 160) while
// retaining enough resolution for imported performance timing.
constexpr int GGD_EVENT_PPQ = 960;
constexpr int GGD_TICKS_PER_16TH = GGD_EVENT_PPQ / 4;
constexpr int GGD_TICKS_PER_32ND = GGD_EVENT_PPQ / 8;
constexpr int GGD_TICKS_PER_8TH_TRIPLET = GGD_EVENT_PPQ / 3;
constexpr int GGD_TICKS_PER_16TH_TRIPLET = GGD_EVENT_PPQ / 6;
constexpr int GGD_DEFAULT_EVENT_DURATION_TICKS = GGD_TICKS_PER_32ND;
constexpr int GGD_MAX_EVENTS_PER_PATTERN = 4096;

struct GgdSequenceEvent
{
    std::int32_t tick = 0;
    std::uint16_t durationTicks = GGD_DEFAULT_EVENT_DURATION_TICKS;
    std::uint8_t row = 0;
    std::uint8_t velocity = 100;
    std::int8_t probability = SEQ_PROB_ON;
    std::uint8_t reserved = 0;
};

// Deliberately pointer-free. SequenceData is copied between UI/audio buffers,
// so event patterns need the same value semantics as the inherited cell matrix.
class GgdEventPattern
{
public:
    GgdEventPattern();

    void clear(bool keepGeometry = true);
    void activate(int numerator = 4, int denominator = 4, int bars = 1);
    bool isActive() const { return active; }

    int getNumerator() const { return numerator; }
    int getDenominator() const { return denominator; }
    int getBars() const { return bars; }
    void setGeometry(int newNumerator, int newDenominator, int newBars);

    int getLengthTicks() const;
    int getEventCount() const { return eventCount; }
    const GgdSequenceEvent* getEvent(int index) const;
    GgdSequenceEvent* getEvent(int index);

    int findEvent(int row, int tick) const;
    const GgdSequenceEvent* findEventPtr(int row, int tick) const;
    GgdSequenceEvent* findEventPtr(int row, int tick);

    bool addEvent(int row,
                  int tick,
                  int velocity = 100,
                  int probability = SEQ_PROB_ON,
                  int durationTicks = GGD_DEFAULT_EVENT_DURATION_TICKS);
    bool removeEvent(int row, int tick);
    bool moveEvent(int row, int tick, int newRow, int newTick);
    bool updateEvent(int row,
                     int tick,
                     int velocity,
                     int probability,
                     int durationTicks);

    // Re-sorts after a batch operation which directly edited event objects.
    void sortEvents();

private:
    bool active = false;
    std::int16_t numerator = 4;
    std::int16_t denominator = 4;
    std::int16_t bars = 1;
    std::int16_t reservedGeometry = 0;
    std::int32_t eventCount = 0;
    GgdSequenceEvent events[GGD_MAX_EVENTS_PER_PATTERN];

    int insertionIndex(int row, int tick) const;
};
