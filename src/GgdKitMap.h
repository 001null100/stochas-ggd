#pragma once

#include <JuceHeader.h>

struct GgdMidiBinding
{
    juce::String kind;
    int midi = -1;
    juce::String noteName;
    juce::String role;
};

struct GgdArticulation
{
    juce::String semanticId;
    juce::String label;
    juce::String kind;
    juce::Array<GgdMidiBinding> bindings;

    const GgdMidiBinding* primaryNoteBinding() const;
};

struct GgdGroup
{
    juce::String id;
    juce::String label;
    juce::Array<GgdArticulation> articulations;
};

struct GgdKitMap
{
    juce::String id;
    juce::String vendor;
    juce::String library;
    juce::String variant;
    juce::Array<GgdGroup> groups;

    const GgdArticulation* findArticulation(const juce::String& semanticId) const;
};

struct GgdCanonicalRow
{
    juce::String semanticId;
    juce::String groupId;
    juce::String groupLabel;
    juce::String defaultLabel;
};

class GgdKitMapLibrary
{
public:
    static juce::Array<GgdKitMap> loadBuiltInMaps();
    static juce::Array<GgdCanonicalRow> buildCanonicalRows(const juce::Array<GgdKitMap>& maps);
    static int findCanonicalRow(const juce::Array<GgdCanonicalRow>& rows,
                                const juce::String& semanticId);

private:
    static bool parseMap(const juce::String& json, GgdKitMap& outMap, juce::String& error);
};
