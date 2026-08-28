#pragma once

#include "GgdKitMap.h"
#include "GgdPatternFile.h"

struct GgdMidiExportResult
{
    bool ok = false;
    int writtenHits = 0;
    int unmappedHits = 0;
    juce::String error;

    juce::String summary() const;
};

class GgdMidiExporter
{
public:
    static GgdMidiExportResult writeFile(const juce::File& file,
                                         const GgdPatternSnapshot& pattern,
                                         const GgdKitMap& map,
                                         int midiChannel = 1);
};
