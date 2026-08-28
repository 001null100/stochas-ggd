#pragma once

#include <JuceHeader.h>
#include <vector>
#include "GgdKitMap.h"
#include "GgdEventModel.h"

struct GgdImportedEvent
{
    int canonicalRow = -1;
    int tick = 0;
    int durationTicks = GGD_DEFAULT_EVENT_DURATION_TICKS;
    int velocity = 100;
};

struct GgdMidiImportResult
{
    bool ok = false;
    juce::String error;
    juce::String fileName;
    juce::String formatName { "GGD Groove Player" };

    int numerator = 4;
    int denominator = 4;
    bool meterSupported = true;
    int bars = 1;

    int totalNotes = 0;
    int mappedNotes = 0;
    int fallbackNotes = 0;
    int unresolvedNotes = 0;
    int collisions = 0;
    int truncatedNotes = 0;
    juce::Array<int> unresolvedPitches;
    std::vector<GgdImportedEvent> events;

    juce::String summary() const;
};

class GgdMidiImporter
{
public:
    static GgdMidiImportResult parseFile(
        const juce::File& file,
        const GgdKitMap& destinationMap,
        const juce::Array<GgdCanonicalRow>& canonicalRows,
        int maxPatternTicks);
};
