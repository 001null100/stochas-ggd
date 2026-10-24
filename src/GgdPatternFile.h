#pragma once

#include "GgdKitMap.h"
#include "SequenceData.h"

#include <cstdint>
#include <vector>

struct GgdPatternHit
{
    juce::String semanticId;
    int tick = 0;
    int durationTicks = GGD_DEFAULT_EVENT_DURATION_TICKS;
    int velocity = 100;
    int probability = SEQ_PROB_ON;
};

struct GgdPatternSnapshot
{
    juce::String name;
    int numerator = 4;
    int denominator = 4;
    int bars = 1;
    int ppq = GGD_EVENT_PPQ;
    std::vector<GgdPatternHit> hits;
};

class GgdProject;

class GgdPatternFile
{
public:
    static constexpr const char* extension = ".sggdp";

    // Semantic snapshot of one pattern slot (hits keyed by articulation id).
    static GgdPatternSnapshot capture(const SequenceLayer& layer,
                                      const GgdProject& project,
                                      int pattern);

    // Replaces a pattern slot with a snapshot. Hits for articulations that
    // are unknown to every built-in kit are skipped.
    static void restore(const GgdPatternSnapshot& snapshot,
                        SequenceData& sequence,
                        const GgdProject& project,
                        int layerIndex,
                        int pattern);

    static bool write(const juce::File& file,
                      const GgdPatternSnapshot& snapshot,
                      juce::String& error);

    static bool read(const juce::File& file,
                     GgdPatternSnapshot& snapshot,
                     juce::String& error);

    static juce::String serialise(const GgdPatternSnapshot& snapshot);
    static std::uint64_t fingerprint(const GgdPatternSnapshot& snapshot);
};
