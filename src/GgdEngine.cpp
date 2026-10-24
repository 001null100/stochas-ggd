#include "GgdEngine.h"
#include "Scale.h"

#include <algorithm>
#include <cmath>

namespace
{
// The UI and scheduler both rely on 960 PPQ; keep the timeline in lockstep.
static_assert(GgdTimeline::ticksPerQuarter == GGD_EVENT_PPQ,
              "GgdTimeline and the event model must agree on PPQ");

// Swing and humanised timing never move a hit by more than half a 1/16.
constexpr int maxTimingShiftTicks = GGD_TICKS_PER_16TH / 2;

// A newly started or relocated transport also picks up hits that sit a hair
// before the reported start position (hosts report bar lines as doubles).
constexpr double startCaptureQuarters = 2.0 / GGD_EVENT_PPQ;

std::uint64_t mix64(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

// Stateless per-occurrence randomness: the same seed, bar/loop, tick and row
// always produce the same probability and humanisation decisions, no matter
// where playback started or how the host split its blocks.
class OccurrenceRandom
{
public:
    OccurrenceRandom(std::uint64_t seed, std::int64_t cycle, int tick, int row)
        : state(mix64(seed
                      ^ mix64(static_cast<std::uint64_t>(cycle) * 0x100000001b3ULL)
                      ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tick)) << 9)
                      ^ static_cast<std::uint64_t>(row & 0x1ff)))
    {
    }

    // Uniform value in [0, size).
    int next(int size)
    {
        state = mix64(state);
        return size > 0 ? static_cast<int>(state % static_cast<std::uint64_t>(size)) : 0;
    }

    // Uniform value in [-range, range].
    int nextSigned(int range)
    {
        return range > 0 ? next(range * 2 + 1) - range : 0;
    }

private:
    std::uint64_t state;
};

// Swing delays every second 1/16 of the bar by up to half a 1/16.
int swingShiftTicks(int tick, int barTicks, int swingPercent)
{
    if (swingPercent == 0 || barTicks <= 0 || (tick % GGD_TICKS_PER_16TH) != 0)
        return 0;
    const int sixteenthInBar = (tick % barTicks) / GGD_TICKS_PER_16TH;
    if ((sixteenthInBar & 1) == 0)
        return 0;
    return swingPercent * GGD_TICKS_PER_16TH / 100;
}

int firstEventAtOrAfter(const GgdEventPattern& pattern, int tick)
{
    int lo = 0;
    int hi = pattern.getEventCount();
    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (pattern.getEvent(mid)->tick < tick)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}
}

GgdEngine::GgdEngine()
{
    publishStopped();
}

int GgdEngine::getPlayingPattern(const SequenceData& data) const
{
    return juce::jlimit(0, SEQ_MAX_PATTERNS - 1,
                        overridePattern.get(data.getLayer(layer)->getCurrentPattern()));
}

bool GgdEngine::isMuted(const SequenceData& data) const
{
    const int assigned = data.getLayer(layer)->getMuted()
        ? SEQMIDI_VALUE_MUTE_MUTE : SEQMIDI_VALUE_MUTE_UNMUTE;
    return overrideMute.get(assigned) == SEQMIDI_VALUE_MUTE_MUTE;
}

bool GgdEngine::processBlock(const SequenceData& data, const BlockContext& context)
{
    const auto* layerData = data.getLayer(layer);
    const int patternIndex = getPlayingPattern(data);
    const auto* pattern = layerData->getEventPattern(patternIndex);

    if (context.numSamples <= 0 || context.sampleRate <= 0.0 || context.host.bpm <= 0.0
        || pattern == nullptr || !pattern->isActive() || pattern->getLengthTicks() <= 0)
    {
        currentStep = -1;
        haveWindowEnd = false;
        publishStopped();
        return true;
    }

    const double samplesPerQuarter = context.sampleRate * 60.0 / context.host.bpm;
    const double blockQuarters = static_cast<double>(context.numSamples) / samplesPerQuarter;
    const double blockStart = context.host.ppq;
    const double blockEnd = blockStart + blockQuarters;

    // Chain this block's scheduling window onto the previous one. A host
    // position that drifted slightly (tempo automation, rounding) continues
    // seamlessly; anything larger is a relocation (loop, seek).
    double windowStart = blockStart - startCaptureQuarters;
    if (haveWindowEnd)
    {
        const double tolerance = std::max(0.05, blockQuarters * 2.0);
        if (std::abs(blockStart - windowEndPpq) <= tolerance)
            windowStart = windowEndPpq;
        else
            dropPendingNoteOns(false);
    }
    lastPattern = patternIndex;

    // Nothing plays before song start (host pre-roll reports negative
    // positions); a block that crosses zero schedules from zero onwards.
    windowStart = std::max(windowStart, 0.0);

    const double speed = std::max(
        1.0 / 16.0,
        static_cast<double>(overrideSpeed.get(layerData->getClockDivider()))
            / static_cast<double>(SEQ_CLOCK_DENOM));

    GgdTimeline::PatternShape shape;
    shape.barTicks = pattern->getBarTicks();
    shape.bars = pattern->getBars();
    const auto anchor = GgdTimeline::resolveAnchor(context.syncMode, context.host, shape, 0.0, speed);
    const bool muted = isMuted(data);

    GgdPlayheadSnapshot snapshot;
    snapshot.playing = true;
    snapshot.muted = muted;
    snapshot.pattern = patternIndex;
    snapshot.hostMeterKnown = context.host.hasTimeSignature;
    snapshot.hostNumerator = context.host.numerator;
    snapshot.hostDenominator = context.host.denominator;
    snapshot.anchor = anchor;
    snapshot.shape = shape;
    snapshot.ppq = blockStart;
    snapshot.quartersPerMs = context.host.bpm / 60000.0;
    snapshot.wallMs = context.wallMs;
    snapshot.blockMs = 1000.0 * context.numSamples / context.sampleRate;
    playhead.store(snapshot);

    const double tickNow = GgdTimeline::tickAt(anchor, shape, blockStart);
    currentStep = tickNow >= 0.0 ? static_cast<int>(tickNow) / GGD_TICKS_PER_16TH : -1;

    if (windowStart >= blockEnd)
    {
        // The host reported a position behind what was already scheduled.
        haveWindowEnd = true;
        return true;
    }

    haveWindowEnd = true;
    windowEndPpq = blockEnd;

    if (muted)
        return true;

    const int swing = juce::jlimit(SEQ_MIN_SWING, SEQ_MAX_SWING,
                                   overrideSwing.get(data.getSwing()));
    const int velocityVariance = juce::jlimit(0, SEQ_MAX_HUMAN_VELOCITY,
        overrideVelocityVariance.get(layerData->getHumanVelocity()));
    const int lengthVariance = juce::jlimit(0, SEQ_MAX_HUMAN_LENGTH,
        overrideLengthVariance.get(layerData->getHumanLength()));
    const int positionVariance = juce::jlimit(0, SEQ_MAX_HUMAN_POSITION,
        overridePositionVariance.get(layerData->getHumanPosition()));
    const double duty = juce::jlimit(
        0.01, 2.0, static_cast<double>(overrideDuty.get(layerData->getDutyCycle())) / 100.0);
    const int channel = juce::jlimit(1, 16, overrideChannel.get(layerData->getMidiChannel()));
    const bool combineOverlaps = layerData->isCombineMode();

    GgdTimeline::Segment segments[maxSegmentsPerBlock];
    const int segmentCount = GgdTimeline::segmentsForRange(
        anchor, shape, windowStart, blockEnd, segments, maxSegmentsPerBlock);

    bool success = true;
    for (int s = 0; s < segmentCount; ++s)
    {
        const auto& segment = segments[s];
        const double tickStart = segment.tickStart;
        const double tickEnd = segment.tickEnd();
        const int searchFrom = static_cast<int>(std::floor(tickStart)) - maxTimingShiftTicks;

        for (int index = firstEventAtOrAfter(*pattern, searchFrom);
             index < pattern->getEventCount(); ++index)
        {
            const auto* event = pattern->getEvent(index);
            if (static_cast<double>(event->tick) >= tickEnd + maxTimingShiftTicks)
                break;
            if (event->probability <= 0)
                continue;

            const double effectiveTick = static_cast<double>(
                event->tick + swingShiftTicks(event->tick, shape.barTicks, swing));
            if (effectiveTick < tickStart || effectiveTick >= tickEnd)
                continue;

            OccurrenceRandom random(context.randomSeed, segment.cycle, event->tick, event->row);
            const int probability = juce::jlimit(0, 100, static_cast<int>(event->probability));
            if (probability < 100 && random.next(100) >= probability)
                continue;

            const int note = layerData->getCurNote(event->row);
            if (note < 0)
                continue;

            int velocity = juce::jlimit(1, 127, static_cast<int>(event->velocity));
            if (velocityVariance > 0)
                velocity = juce::jlimit(1, 127, velocity + random.nextSigned(velocityVariance) * 127 / 100);

            const double durationQuarters = static_cast<double>(event->durationTicks)
                                          / segment.ticksPerQuarterScaled;
            int lengthSamples = juce::jmax(1, static_cast<int>(std::llround(
                durationQuarters * samplesPerQuarter * duty)));
            if (lengthVariance > 0)
            {
                const int reduction = 1 + random.next(lengthVariance);
                lengthSamples = juce::jmax(1, lengthSamples - lengthSamples * reduction / 100);
            }

            const double eventPpq = segment.ppqStart
                                  + (effectiveTick - tickStart) / segment.ticksPerQuarterScaled;
            double startSamples = std::floor((eventPpq - blockStart) * samplesPerQuarter);
            if (positionVariance > 0)
            {
                const double shiftQuarters = static_cast<double>(random.nextSigned(positionVariance))
                                           * GGD_TICKS_PER_16TH / 100.0
                                           / segment.ticksPerQuarterScaled;
                startSamples += std::floor(shiftQuarters * samplesPerQuarter);
            }
            const int start = juce::jmax(0, static_cast<int>(startSamples));

            if (!queueNote(start, note, velocity, channel, lengthSamples, combineOverlaps))
            {
                success = false;
                continue;
            }

            notifyPlayed(patternIndex, event->row, event->tick, velocity,
                         context.wallMs + 1000.0 * start / context.sampleRate);
        }
    }

    return success;
}

bool GgdEngine::queueNote(int startSamples,
                          int note,
                          int velocity,
                          int channel,
                          int lengthSamples,
                          bool combineOverlaps)
{
    note = juce::jlimit(0, 127, note + overrideTranspose.get(0));
    const auto note8 = static_cast<std::int8_t>(note);
    const auto channel8 = static_cast<std::int8_t>(channel);

    // A note that is still sounding when this one starts is either extended
    // (combine mode) or ended just before the retrigger.
    for (auto& queued : queue)
    {
        if (queued.samples > startSamples && queued.velocity == 0
            && queued.note == note8 && queued.channel == channel8)
        {
            if (combineOverlaps)
            {
                queued.samples = startSamples + lengthSamples;
                return true;
            }
            queued.samples = startSamples > 0 ? startSamples - 1 : 0;
            break;
        }
    }

    QueuedEvent* on = nullptr;
    QueuedEvent* off = nullptr;
    for (auto& slot : queue)
    {
        if (slot.samples != -1)
            continue;
        if (on == nullptr)
            on = &slot;
        else
        {
            off = &slot;
            break;
        }
    }
    if (on == nullptr || off == nullptr)
        return false;

    on->samples = startSamples;
    on->note = note8;
    on->velocity = static_cast<std::int8_t>(juce::jlimit(1, 127, velocity));
    on->channel = channel8;
    on->noteOff = off;

    off->samples = startSamples + juce::jmax(1, lengthSamples);
    off->note = note8;
    off->velocity = 0;
    off->channel = channel8;
    off->noteOff = nullptr;

    ++pendingNoteOns;
    ++pendingNoteOffs;
    return true;
}

bool GgdEngine::popMidiEvent(int numSamples, MidiOut& out)
{
    if (pendingNoteOffs > 0)
    {
        for (auto& queued : queue)
        {
            if (queued.samples != -1 && queued.velocity == 0 && queued.samples < numSamples)
            {
                out.samplePosition = queued.samples;
                out.note = queued.note;
                out.velocity = 0;
                out.channel = queued.channel;
                queued.clear();
                --pendingNoteOffs;
                return true;
            }
        }
    }

    if (pendingNoteOns > 0)
    {
        for (auto& queued : queue)
        {
            if (queued.samples != -1 && queued.velocity > 0 && queued.samples < numSamples)
            {
                out.samplePosition = queued.samples;
                out.note = queued.note;
                out.velocity = queued.velocity;
                out.channel = queued.channel;
                queued.clear();
                --pendingNoteOns;
                return true;
            }
        }
    }

    return false;
}

void GgdEngine::finishBlock(int numSamples)
{
    for (auto& queued : queue)
        if (queued.samples != -1)
            queued.samples = juce::jmax(0, queued.samples - numSamples);
}

void GgdEngine::dropPendingNoteOns(bool flushNoteOffs)
{
    for (auto& queued : queue)
    {
        if (queued.samples != -1 && queued.velocity > 0)
        {
            if (queued.noteOff != nullptr)
            {
                queued.noteOff->clear();
                --pendingNoteOffs;
            }
            queued.clear();
        }
    }
    pendingNoteOns = 0;

    if (flushNoteOffs)
        for (auto& queued : queue)
            if (queued.samples != -1)
                queued.samples = 0;
}

void GgdEngine::playbackStopped()
{
    dropPendingNoteOns(true);
    haveWindowEnd = false;
    lastPattern = -1;
    currentStep = -1;
    resetOverrides();
    publishStopped();
}

void GgdEngine::publishStopped()
{
    GgdPlayheadSnapshot snapshot;
    snapshot.playing = false;
    playhead.store(snapshot);
}

void GgdEngine::notifyPlayed(int pattern, int row, int tick, int velocity, double dueMs)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    playedFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        auto& event = played[start1];
        event.pattern = pattern;
        event.row = row;
        event.tick = tick;
        event.velocity = velocity;
        event.dueMs = dueMs;
    }
    playedFifo.finishedWrite(size1);
}

bool GgdEngine::popPlayedEvent(PlayedEvent& event)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    playedFifo.prepareToRead(1, start1, size1, start2, size2);
    if (size1 <= 0)
        return false;
    event = played[start1];
    playedFifo.finishedRead(size1);
    return true;
}

void GgdEngine::clearPlayedEvents()
{
    PlayedEvent discard;
    while (popPlayedEvent(discard))
    {
    }
}

void GgdEngine::resetOverrides()
{
    overridePattern.clear();
    overrideMute.clear();
    overrideSpeed.clear();
    overrideTranspose.clear();
    overrideSwing.clear();
    overrideDuty.clear();
    overrideVelocityVariance.clear();
    overrideLengthVariance.clear();
    overridePositionVariance.clear();
    overrideChannel.clear();
}

void GgdEngine::setAutomationParameterValue(int paramId, int value)
{
    Override* target = nullptr;
    switch (paramId)
    {
        case SEQ_AUT_CLOCKDIV: target = &overrideSpeed; break;
        case SEQ_AUT_NOTE_LENGTH: target = &overrideDuty; break;
        case SEQ_AUT_POS_VARIANCE: target = &overridePositionVariance; break;
        case SEQ_AUT_VELO_VARIANCE: target = &overrideVelocityVariance; break;
        case SEQ_AUT_LENGTH_VARIANCE: target = &overrideLengthVariance; break;
        case SEQ_AUT_MUTED: target = &overrideMute; break;
        case SEQ_AUT_OUTPUT_CHANNEL: target = &overrideChannel; break;
        case SEQ_AUT_CURRENT_PATTERN: target = &overridePattern; break;
        case SEQ_AUT_TRANSPOSE: target = &overrideTranspose; break;
        case SEQ_AUT_GLOBAL_SWING: target = &overrideSwing; break;
        default:
            // Steps, steps per measure, max poly and poly bias only applied to
            // the inherited cell sequencer. The parameters stay registered so
            // existing host automation keeps its slots.
            return;
    }

    if (value == SEQ_AUT_DEFAULT_VALUE_DESIG)
    {
        target->clear();
        return;
    }

    switch (paramId)
    {
        case SEQ_AUT_CLOCKDIV:
        {
            int numerator = SEQ_DEFAULT_CLOCK_DIV;
            SeqScale::getClockDividerTextAndId(juce::jlimit(0, SEQ_NUM_CLOCK_DIVS - 1, value), &numerator);
            target->set(juce::jlimit(SEQ_MIN_CLOCK_DIV, SEQ_MAX_CLOCK_DIV, numerator));
            break;
        }
        case SEQ_AUT_MUTED:
            target->set(value != 0 ? SEQMIDI_VALUE_MUTE_MUTE : SEQMIDI_VALUE_MUTE_UNMUTE);
            break;
        case SEQ_AUT_TRANSPOSE:
        {
            int semitones = 0;
            SeqScale::getTransposeSemitones(
                juce::jlimit(SEQMIDI_VALUE_TRANSPOSE_MIN, SEQMIDI_VALUE_TRANSPOSE_MAX, value), &semitones);
            target->set(semitones);
            break;
        }
        case SEQ_AUT_CURRENT_PATTERN:
            target->set(juce::jlimit(0, SEQ_MAX_PATTERNS - 1, value));
            break;
        default:
            target->set(value);
            break;
    }
}
