#pragma once

#include <cstdint>

// Maps the host's musical timeline onto a pattern's own timeline.
//
// The engine and the editor's playhead share these functions so the position
// the user sees is computed exactly the way playback computes it.
//
// Two anchoring models are supported:
//
//  * Host bars (default): each pattern bar starts on a host bar line. The
//    pattern bar index is the host bar number modulo the pattern's bar count,
//    and the position inside the bar comes from the host's own bar start. A
//    time-signature change in the host therefore never shifts the pattern
//    against the bar grid. When the host bar is longer than the pattern bar
//    the pattern rests until the next host bar; when it is shorter, the
//    pattern bar is cut at the host bar line.
//
//  * Free: the pattern loops continuously at its own length from song start,
//    independent of host bars (polymeter).
namespace GgdTimeline
{
constexpr int ticksPerQuarter = 960;

enum class SyncMode : int
{
    hostBars = 0,
    free = 1
};

struct HostPosition
{
    double ppq = 0.0;
    double bpm = 120.0;
    bool hasBarStart = false;
    double barStartPpq = 0.0;
    bool hasBarNumber = false;
    std::int64_t barNumber = 0;
    bool hasTimeSignature = false;
    int numerator = 4;
    int denominator = 4;
};

struct PatternShape
{
    int barTicks = 4 * ticksPerQuarter;
    int bars = 1;

    int lengthTicks() const { return barTicks * bars; }
    double barQuarters() const { return static_cast<double>(barTicks) / ticksPerQuarter; }
};

struct Anchor
{
    bool barAnchored = false;

    // Host-bar anchoring: a reference bar and the host bar length.
    double barStartPpq = 0.0;
    std::int64_t barNumber = 0;
    double barQuarters = 4.0;

    // Free anchoring: where pattern tick 0 sits and how fast ticks advance.
    double originPpq = 0.0;
    double ticksPerQuarterScaled = ticksPerQuarter;
};

// A host range that maps linearly onto a contiguous range of pattern ticks.
struct Segment
{
    double ppqStart = 0.0;
    double ppqEnd = 0.0;
    double tickStart = 0.0;
    double ticksPerQuarterScaled = ticksPerQuarter;
    // Host bar number (bar anchoring) or loop count (free anchoring). Used to
    // derive deterministic per-occurrence randomness.
    std::int64_t cycle = 0;

    double tickEnd() const { return tickStart + (ppqEnd - ppqStart) * ticksPerQuarterScaled; }
};

double barQuartersForMeter(int numerator, int denominator);
bool isUsableMeter(int numerator, int denominator);

// Resolves the anchor for one processing block.
//
// speed scales the pattern tempo (1 = normal). Bar anchoring needs a normal
// speed and a song-start origin; anything else falls back to free anchoring.
Anchor resolveAnchor(SyncMode mode,
                     const HostPosition& host,
                     const PatternShape& shape,
                     double originPpq = 0.0,
                     double speed = 1.0);

// Splits the half-open host range [ppqStart, ppqEnd) into pattern segments.
// Rest regions (host bar longer than the pattern bar) produce no segment.
// Returns the number of segments written.
int segmentsForRange(const Anchor& anchor,
                     const PatternShape& shape,
                     double ppqStart,
                     double ppqEnd,
                     Segment* output,
                     int maxSegments);

// Pattern tick at a host position, or a negative value while the pattern is
// resting between the end of a short pattern bar and the next host bar.
double tickAt(const Anchor& anchor, const PatternShape& shape, double ppq);
}
