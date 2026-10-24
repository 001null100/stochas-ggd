#pragma once

#include "SequenceData.h"
#include "GgdPlayhead.h"
#include "GgdTimeline.h"

#include <atomic>
#include <cstdint>

// Real-time scheduler for one layer of GGD event patterns.
//
// Each block maps the host range onto the pattern timeline (GgdTimeline),
// schedules every event whose time falls inside the block and hands the
// resulting note-on/note-off pairs back through a small fixed-size queue.
// Scheduling windows are chained from one block to the next, so tempo
// automation or small host position jitter can never play a hit twice or skip
// it. Nothing here allocates or locks.
class GgdEngine
{
public:
    struct BlockContext
    {
        GgdTimeline::HostPosition host;
        double sampleRate = 44100.0;
        int numSamples = 0;
        GgdTimeline::SyncMode syncMode = GgdTimeline::SyncMode::hostBars;
        std::uint64_t randomSeed = 0;
        double wallMs = 0.0;
    };

    struct MidiOut
    {
        int samplePosition = 0;
        int note = 0;
        int velocity = 0; // 0 = note-off
        int channel = 1;
    };

    struct PlayedEvent
    {
        int pattern = -1;
        int row = -1;
        int tick = -1;
        int velocity = 0;
        double dueMs = 0.0;
    };

    GgdEngine();

    void setLayer(int layerIndex) { layer = layerIndex; }

    // Schedules this block. Returns false if the note queue overflowed.
    bool processBlock(const SequenceData& data, const BlockContext& context);

    // Pops the next MIDI message due inside the block. Note-offs are always
    // delivered before note-ons so a retriggered note is never cut short.
    bool popMidiEvent(int numSamples, MidiOut& out);

    // Moves queued events forward by one block.
    void finishBlock(int numSamples);

    // Transport stopped: queued note-ons are dropped, pending note-offs are
    // flushed immediately and transport continuity is forgotten.
    void playbackStopped();

    int getPlayingPattern(const SequenceData& data) const;
    bool isMuted(const SequenceData& data) const;

    // 1/16 step inside the playing pattern, or -1 while stopped/resting.
    int getCurrentStepPosition() const { return currentStep; }

    void setAutomationParameterValue(int paramId, int value);
    void resetOverrides();

    // UI feedback: events that were actually scheduled after mute and
    // probability decisions. Single producer (audio), single consumer (UI).
    bool popPlayedEvent(PlayedEvent& event);
    void clearPlayedEvents();

    bool readPlayhead(GgdPlayheadSnapshot& snapshot) const { return playhead.load(snapshot); }

private:
    // Automation overrides may be written by the host on the main thread
    // while the audio thread reads them.
    struct Override
    {
        std::atomic<bool> active { false };
        std::atomic<int> value { 0 };
        void set(int newValue)
        {
            value.store(newValue, std::memory_order_relaxed);
            active.store(true, std::memory_order_release);
        }
        void clear() { active.store(false, std::memory_order_release); }
        int get(int fallback) const
        {
            return active.load(std::memory_order_acquire) ? value.load(std::memory_order_relaxed)
                                                          : fallback;
        }
    };

    struct QueuedEvent
    {
        int samples = -1; // -1 = free slot
        std::int8_t note = -1;
        std::int8_t velocity = -1;
        std::int8_t channel = -1;
        QueuedEvent* noteOff = nullptr;

        void clear()
        {
            samples = -1;
            note = -1;
            velocity = -1;
            channel = -1;
            noteOff = nullptr;
        }
    };

    static constexpr int queueSize = 256;
    static constexpr int playedQueueSize = 256;
    static constexpr int maxSegmentsPerBlock = 16;

    int layer = 0;

    Override overridePattern;
    Override overrideMute;
    Override overrideSpeed;
    Override overrideTranspose;
    Override overrideSwing;
    Override overrideDuty;
    Override overrideVelocityVariance;
    Override overrideLengthVariance;
    Override overridePositionVariance;
    Override overrideChannel;

    QueuedEvent queue[queueSize];
    int pendingNoteOns = 0;
    int pendingNoteOffs = 0;

    bool haveWindowEnd = false;
    double windowEndPpq = 0.0;
    int lastPattern = -1;
    int currentStep = -1;

    juce::AbstractFifo playedFifo { playedQueueSize };
    PlayedEvent played[playedQueueSize] {};

    GgdSeqLockValue<GgdPlayheadSnapshot> playhead;

    bool queueNote(int startSamples, int note, int velocity, int channel, int lengthSamples,
                   bool combineOverlaps);
    void dropPendingNoteOns(bool flushNoteOffs);
    void notifyPlayed(int pattern, int row, int tick, int velocity, double dueMs);
    void publishStopped();
};
