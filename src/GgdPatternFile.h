#pragma once

#include "GgdKitMap.h"
#include "SequenceData.h"

#include <cstdint>
#include <vector>

struct GgdPatternHit
{
    juce::String semanticId;
    int step = 0;
    int velocity = 100;
    int probability = SEQ_PROB_ON;
    int retriggerLength = 0;
    int offset = 0;
};

struct GgdPatternSnapshot
{
    juce::String name;
    int numerator = 4;
    int denominator = 4;
    int bars = 1;
    std::vector<GgdPatternHit> hits;
};

class GgdPatternFile
{
public:
    static constexpr const char* extension = ".sggdp";

    static GgdPatternSnapshot capture(SequenceLayer& layer,
                                      const juce::Array<GgdCanonicalRow>& canonicalRows,
                                      int pattern,
                                      int numerator,
                                      int denominator,
                                      int bars);

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
