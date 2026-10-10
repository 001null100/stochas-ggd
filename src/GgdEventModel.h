#pragma once

#include "Constants.h"

#include <cstdint>

// Musical positions are stored independently of any editor grid. 960 ticks per quarter note exactly represents the straight and
// triplet divisions we care about (1/32 = 120, 1/8T = 320, 1/16T = 160) while
// retaining enough resolution for imported performance timing.
constexpr int GGD_EVENT_PPQ = 960;
constexpr int GGD_TICKS_PER_16TH = GGD_EVENT_PPQ / 4;
constexpr int GGD_TICKS_PER_32ND = GGD_EVENT_PPQ / 8;
constexpr int GGD_TICKS_PER_8TH_TRIPLET = GGD_EVENT_PPQ / 3;
constexpr int GGD_TICKS_PER_16TH_TRIPLET = GGD_EVENT_PPQ / 6;
constexpr int GGD_DEFAULT_EVENT_DURATION_TICKS = GGD_TICKS_PER_32ND;
constexpr int GGD_MAX_EVENTS_PER_PATTERN = 4096;
constexpr int GGD_MAX_PATTERN_BARS = 64;
// 1024 sixteenths: 64 bars of 4/4.
constexpr int GGD_MAX_PATTERN_TICKS = 1024 * GGD_TICKS_PER_16TH;
constexpr int GGD_MAX_PARKED_HITS = 2048;

struct GgdSequenceEvent
{
    std::int32_t tick = 0;
    std::uint16_t durationTicks = GGD_DEFAULT_EVENT_DURATION_TICKS;
    std::uint8_t row = 0;
    std::uint8_t velocity = 100;
    std::int8_t probability = SEQ_PROB_ON;
    std::uint8_t reserved = 0;
};

// A hit hidden by a meter or bar-count change. It is kept in musical
// coordinates (bar, offset from the bar line) so it can come back when the
// pattern grows again. Parked hits are saved with the project but never play.
struct GgdParkedHit
{
    std::int32_t offset = 0;
    std::int16_t bar = 0;
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

    // Removes every hit. keepGeometry = false also resets the meter, the bar
    // count and the parked hits.
    void clear(bool keepGeometry = true);
    void activate(int numerator = 4, int denominator = 4, int bars = 1);
    bool isActive() const { return active; }

    int getNumerator() const { return numerator; }
    int getDenominator() const { return denominator; }
    int getBars() const { return bars; }
    void setGeometry(int newNumerator, int newDenominator, int newBars);

    int getLengthTicks() const;
    int getBarTicks() const { return barTicksFor(numerator, denominator); }

    // Ticks in one bar of numerator/denominator at GGD_EVENT_PPQ.
    static int barTicksFor(int numerator, int denominator);
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

    // Hits hidden by a meter or bar change (see GgdPatternOps).
    int getParkedCount() const { return parkedCount; }
    const GgdParkedHit* getParked(int index) const;
    bool addParked(const GgdParkedHit& hit); // false when the store is full
    void clearParked() { parkedCount = 0; }

private:
    bool active = false;
    std::int16_t numerator = 4;
    std::int16_t denominator = 4;
    std::int16_t bars = 1;
    std::int16_t reservedGeometry = 0;
    std::int32_t eventCount = 0;
    std::int32_t parkedCount = 0;
    GgdSequenceEvent events[GGD_MAX_EVENTS_PER_PATTERN];
    GgdParkedHit parked[GGD_MAX_PARKED_HITS];

    int insertionIndex(int row, int tick) const;
};
