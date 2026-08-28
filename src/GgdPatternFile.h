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

class GgdPatternFile
{
public:
    static constexpr const char* extension = ".sggdp";

    static GgdPatternSnapshot capture(SequenceLayer& layer,
                                      const juce::Array<GgdCanonicalRow>& canonicalRows,
                                      int pattern,
                                      int numerator = 0,
                                      int denominator = 0,
                                      int bars = 0);

    static void restore(const GgdPatternSnapshot& snapshot,
                        SequenceData& sequence,
                        const juce::Array<GgdCanonicalRow>& canonicalRows,
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
