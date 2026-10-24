#include "GgdProject.h"
#include "Persist.h"

namespace
{
const char* drumMachineId = "custom.drum-machine";
}

GgdKitMap GgdProject::makeDrumMachineMap(const GgdDrumMachineKit& pads, bool allPads)
{
    GgdKitMap map;
    map.id = drumMachineId;
    map.vendor = "Custom";
    map.library = "Drum machine";
    map.variant = "Project pads";

    GgdGroup group;
    group.id = "pads";
    group.label = "Pads";
    const int count = allPads ? GGD_DRUM_MACHINE_MAX_PADS : pads.getPadCount();
    for (int pad = 0; pad < count; ++pad)
    {
        GgdArticulation articulation;
        articulation.semanticId = GgdDrumMachineKit::semanticId(pad);
        articulation.label = allPads ? GgdDrumMachineKit::defaultName(pad) : pads.getDisplayName(pad);
        articulation.kind = "hit";

        GgdMidiBinding binding;
        binding.kind = "note";
        binding.role = "primary";
        binding.midi = pads.getNote(pad);
        binding.noteName = GgdDrumMachineKit::noteName(binding.midi);
        articulation.bindings.add(binding);
        group.articulations.add(articulation);
    }
    map.groups.add(group);
    return map;
}

GgdProject::GgdProject()
    : maps(GgdKitMapLibrary::loadBuiltInMaps())
{
    // The built-in GGD rows come first and keep their storage rows; the
    // Drum machine pads are appended after them.
    builtInRows = GgdKitMapLibrary::buildCanonicalRows(maps).size();
    drumMachineIndex = maps.size();
    maps.add(makeDrumMachineMap(GgdDrumMachineKit(), true));
    canonicalRows = GgdKitMapLibrary::buildCanonicalRows(maps);
}

int GgdProject::storageRowForCanonical(int canonicalRow) const
{
    if (canonicalRow < 0 || canonicalRow >= canonicalRows.size())
        return -1;
    const int builtInBase = SEQ_MAX_ROWS - builtInRows;
    if (canonicalRow < builtInRows)
        return builtInBase + canonicalRow;
    const int padRows = canonicalRows.size() - builtInRows;
    return builtInBase - padRows + (canonicalRow - builtInRows);
}

int GgdProject::canonicalRowForStorage(int storageRow) const
{
    const int builtInBase = SEQ_MAX_ROWS - builtInRows;
    const int padRows = canonicalRows.size() - builtInRows;
    if (storageRow >= builtInBase && storageRow < SEQ_MAX_ROWS)
        return storageRow - builtInBase;
    if (storageRow >= builtInBase - padRows && storageRow < builtInBase)
        return builtInRows + storageRow - (builtInBase - padRows);
    return -1;
}

int GgdProject::findCanonicalRow(const juce::String& semanticId) const
{
    return GgdKitMapLibrary::findCanonicalRow(canonicalRows, semanticId);
}

juce::String GgdProject::persistenceToken(const GgdKitMap& map) const
{
    if (map.id == drumMachineId) return "dm";
    if (map.id.containsIgnoreCase(".piv.")) return "piv";
    if (map.id.containsIgnoreCase(".pv.")) return "pv";
    if (map.id.containsIgnoreCase("modern")) return "mm";
    return map.id.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ")
        .replaceCharacter(' ', '-').toLowerCase();
}

int GgdProject::mapIndexFromTag(const juce::String& layerName) const
{
    if (!SeqPersist::isGgdLayerTag(layerName))
        return 0;
    const auto token = layerName.fromFirstOccurrenceOf(":", false, false)
                                .upToFirstOccurrenceOf(":", false, false).trim();
    for (int i = 0; i < maps.size(); ++i)
        if (persistenceToken(maps.getReference(i)) == token)
            return i;
    return 0;
}

int GgdProject::getActiveMapIndex(const SequenceData& data) const
{
    if (maps.isEmpty())
        return -1;
    return juce::jlimit(0, maps.size() - 1,
                        mapIndexFromTag(juce::String::fromUTF8(data.getLayer(drumLayer)->getLayerName())));
}

GgdKitMap GgdProject::kitForIndex(const SequenceData& data, int mapIndex) const
{
    if (maps.isEmpty())
        return {};
    mapIndex = juce::jlimit(0, maps.size() - 1, mapIndex);
    if (isDrumMachine(mapIndex))
        return makeDrumMachineMap(data.getDrumMachine(), false);
    return maps.getReference(mapIndex);
}

GgdKitMap GgdProject::getActiveKit(const SequenceData& data) const
{
    return kitForIndex(data, getActiveMapIndex(data));
}

void GgdProject::applyBindings(SequenceData& data, int mapIndex) const
{
    if (mapIndex < 0 || mapIndex >= maps.size())
        return;

    auto& layer = *data.getLayer(drumLayer);
    const auto map = kitForIndex(data, mapIndex);
    for (int row = 0; row < canonicalRows.size(); ++row)
    {
        const auto& canonical = canonicalRows.getReference(row);
        int midi = -1;
        juce::String label = canonical.defaultLabel;
        if (const auto* articulation = map.findArticulation(canonical.semanticId))
        {
            if (articulation->label.isNotEmpty())
                label = articulation->label;
            if (const auto* binding = articulation->primaryNoteBinding())
                midi = binding->midi;
        }

        const int storage = storageRowForCanonical(row);
        layer.setNote(storage, static_cast<int8_t>(midi), true);
        // The inherited per-row label is display data; cut it on a character boundary.
        while (label.getNumBytesAsUTF8() > static_cast<size_t>(SEQ_MAX_NOTELABEL_LEN - 1))
            label = label.dropLastCharacters(1);
        layer.setNoteName(storage, label.toRawUTF8());
    }

    layer.setNoteSource(true);
    layer.setLayerName(("GGD4:" + persistenceToken(map)).toRawUTF8());
}

void GgdProject::syncLegacyStepCount(SequenceLayer& layer)
{
    const auto* pattern = layer.getEventPattern(layer.getCurrentPattern());
    const int length = pattern->isActive() ? pattern->getLengthTicks() : GGD_EVENT_PPQ * 4;
    layer.setNumSteps(juce::jlimit(SEQ_MIN_STEPS, SEQ_MAX_STEPS,
                                   (length + GGD_TICKS_PER_16TH - 1) / GGD_TICKS_PER_16TH));
}

void GgdProject::normalise(SequenceData& data) const
{
    if (maps.isEmpty() || canonicalRows.isEmpty())
        return;

    auto* layer = data.getLayer(drumLayer);
    const auto tag = juce::String::fromUTF8(layer->getLayerName());
    const bool tagged = SeqPersist::isGgdLayerTag(tag);

    bool anyPatternActive = false;
    for (int p = 0; p < SEQ_MAX_PATTERNS; ++p)
        anyPatternActive |= layer->getEventPattern(p)->isActive();

    // A layer that never belonged to Stochas GGD (fresh instance or a plain
    // Stochas project) starts from a clean drum layer.
    if (!tagged && !anyPatternActive)
        data.clearLayer(drumLayer);

    const int mapIndex = tagged ? mapIndexFromTag(tag) : 0;

    int numerator = 4;
    int denominator = 4;
    SeqPersist::parseLegacyMeter(tag, numerator, denominator);
    const int stepsPerBar = juce::jmax(1, numerator * 16 / denominator);
    const int bars = juce::jlimit(1, 64, (layer->getNumSteps() + stepsPerBar - 1) / stepsPerBar);

    for (int p = 0; p < SEQ_MAX_PATTERNS; ++p)
    {
        auto* pattern = layer->getEventPattern(p);
        if (!pattern->isActive())
            pattern->activate(tagged ? numerator : 4, tagged ? denominator : 4, tagged ? bars : 1);
    }

    data.setMidiPassthru(SEQ_MIDI_PASSTHRU_ALL);
    data.setMidiRespond(SEQ_MIDI_RESPOND_NO);
    data.setAutoPlayMode(SEQ_PLAYMODE_AUTO);
    layer->setMonoMode(false);
    layer->setMaxRows(juce::jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, canonicalRows.size()));
    layer->setMaxPoly(juce::jmin(SEQ_DEFAULT_MAX_POLY, layer->getMaxRows()));
    layer->setClockDivider(SEQ_DEFAULT_CLOCK_DIV);

    applyBindings(data, mapIndex);
    syncLegacyStepCount(*layer);
}

void GgdProject::setActiveMap(SequenceData& data, int mapIndex) const
{
    if (maps.isEmpty())
        return;
    applyBindings(data, juce::jlimit(0, maps.size() - 1, mapIndex));
}
