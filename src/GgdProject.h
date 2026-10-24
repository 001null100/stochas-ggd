#pragma once

#include "GgdKitMap.h"
#include "SequenceData.h"

// The GGD view of a project: the kits (the built-in GGD maps plus the
// project's own Drum machine kit), the stable canonical row table shared by
// every kit, and the rules that keep layer 0 of a SequenceData configured as
// a drum layer.
//
// Patterns store hits on "storage rows". The articulations of the built-in
// GGD maps (canonical rows 0 .. builtInRows-1) live on storage rows
// SEQ_MAX_ROWS - builtInRows + r, as they always have. The 32 Drum machine
// pads follow in the canonical table and sit on the storage rows just below
// that block, so adding them moved nothing that existing projects saved. The
// active kit only decides which destination MIDI note each storage row plays.
class GgdProject
{
public:
    static constexpr int drumLayer = 0;

    GgdProject();

    // Kit templates. The Drum machine entry holds default pads; use
    // kitForIndex() / getActiveKit() for the project's own names and notes.
    const juce::Array<GgdKitMap>& getMaps() const { return maps; }
    const juce::Array<GgdCanonicalRow>& getCanonicalRows() const { return canonicalRows; }

    int storageRowForCanonical(int canonicalRow) const;
    int canonicalRowForStorage(int storageRow) const;
    int findCanonicalRow(const juce::String& semanticId) const;

    int getDrumMachineIndex() const { return drumMachineIndex; }
    bool isDrumMachine(int mapIndex) const { return mapIndex == drumMachineIndex; }

    int getActiveMapIndex(const SequenceData& data) const;

    // The kit as this project plays it (Drum machine pads named and tuned
    // from the project).
    GgdKitMap kitForIndex(const SequenceData& data, int mapIndex) const;
    GgdKitMap getActiveKit(const SequenceData& data) const;

    // Configures the drum layer for playback: activates every pattern,
    // passes incoming MIDI through, binds destination notes for the active
    // kit and stamps the kit tag that is saved with the project.
    void normalise(SequenceData& data) const;

    // Switches the destination kit (or re-applies it after its pads changed).
    // Patterns are untouched; only note bindings and the saved kit tag change.
    void setActiveMap(SequenceData& data, int mapIndex) const;

    // Keeps the inherited step count in line with the current pattern so
    // builds that still read it see a sensible value.
    static void syncLegacyStepCount(SequenceLayer& layer);

    juce::String persistenceToken(const GgdKitMap& map) const;

    // A Drum machine kit map for the given pads.
    static GgdKitMap makeDrumMachineMap(const GgdDrumMachineKit& pads, bool allPads);

private:
    juce::Array<GgdKitMap> maps;
    juce::Array<GgdCanonicalRow> canonicalRows;
    int builtInRows = 0;
    int drumMachineIndex = -1;

    int mapIndexFromTag(const juce::String& layerName) const;
    void applyBindings(SequenceData& data, int mapIndex) const;
};
