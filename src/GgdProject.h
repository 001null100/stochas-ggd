#pragma once

#include "GgdKitMap.h"
#include "SequenceData.h"

// The GGD view of a project: the built-in kit maps, the stable canonical row
// table shared by every kit, and the rules that keep layer 0 of a
// SequenceData configured as a GGD drum layer.
//
// Patterns store hits on "storage rows". Canonical row r (the r-th semantic
// articulation across all built-in maps, in a stable order) lives on storage
// row SEQ_MAX_ROWS - canonicalCount + r. The active kit map only decides
// which destination MIDI note each storage row plays.
class GgdProject
{
public:
    static constexpr int drumLayer = 0;

    GgdProject();

    const juce::Array<GgdKitMap>& getMaps() const { return maps; }
    const juce::Array<GgdCanonicalRow>& getCanonicalRows() const { return canonicalRows; }

    int storageRowForCanonical(int canonicalRow) const;
    int canonicalRowForStorage(int storageRow) const;
    int findCanonicalRow(const juce::String& semanticId) const;

    int getActiveMapIndex(const SequenceData& data) const;
    const GgdKitMap* getActiveMap(const SequenceData& data) const;

    // Configures the drum layer for GGD playback: activates every pattern,
    // passes incoming MIDI through, binds destination notes for the active
    // kit and stamps the kit tag that is saved with the project.
    void normalise(SequenceData& data) const;

    // Switches the destination kit. Patterns are untouched; only note
    // bindings and the saved kit tag change.
    void setActiveMap(SequenceData& data, int mapIndex) const;

    // Keeps the inherited step count in line with the current pattern so
    // builds that still read it see a sensible value.
    static void syncLegacyStepCount(SequenceLayer& layer);

    juce::String persistenceToken(const GgdKitMap& map) const;

private:
    juce::Array<GgdKitMap> maps;
    juce::Array<GgdCanonicalRow> canonicalRows;

    int mapIndexFromTag(const juce::String& layerName) const;
    void applyBindings(SequenceLayer& layer, int mapIndex) const;
};
