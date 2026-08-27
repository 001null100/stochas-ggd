#pragma once

#include <JuceHeader.h>
#include "GgdKitMap.h"

struct GgdImportedCell
{
    int canonicalRow = -1;
    int step = 0;
    int velocity = 100;
    int offset = 0;
    int retriggerLength = 0;
};

struct GgdMidiImportResult
{
    bool ok = false;
    juce::String error;
    juce::String fileName;

    int sourceMapIndex = -1;
    juce::String sourceMapName;
    float sourceConfidence = 0.0f;

    int numerator = 4;
    int denominator = 4;
    bool meterSupported = true;
    int bars = 1;

    int totalNotes = 0;
    int mappedNotes = 0;
    int unresolvedNotes = 0;
    int collisions = 0;
    int truncatedNotes = 0;
    juce::Array<int> unresolvedPitches;
    std::vector<GgdImportedCell> cells;

    juce::String summary() const;
};

class GgdMidiImporter
{
public:
    // forcedSourceMap: -1 = auto detect, otherwise index into maps.
    static GgdMidiImportResult parseFile(
        const juce::File& file,
        const juce::Array<GgdKitMap>& maps,
        const juce::Array<GgdCanonicalRow>& canonicalRows,
        int forcedSourceMap,
        int maxBars);
};
