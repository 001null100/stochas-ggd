#include "GgdPatternFile.h"

#include <algorithm>

namespace
{
int storageRowForCanonical(int canonicalRow, int canonicalCount)
{
    const int rowCount = juce::jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, canonicalCount);
    return (SEQ_MAX_ROWS - rowCount) + canonicalRow;
}

juce::var hitToVar(const GgdPatternHit& hit)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("semantic", hit.semanticId);
    object->setProperty("step", hit.step);
    object->setProperty("velocity", hit.velocity);
    object->setProperty("probability", hit.probability);
    object->setProperty("length", hit.retriggerLength);
    object->setProperty("offset", hit.offset);
    return juce::var(object);
}

bool varToHit(const juce::var& value, GgdPatternHit& hit)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    hit.semanticId = object->getProperty("semantic").toString().trim();
    hit.step = static_cast<int>(object->getProperty("step"));
    hit.velocity = juce::jlimit(1, 127, static_cast<int>(object->getProperty("velocity")));
    hit.probability = juce::jlimit(0, 100, static_cast<int>(object->getProperty("probability")));
    hit.retriggerLength = juce::jlimit(-SEQ_MAX_RETRIGGER + 1,
                                       127,
                                       static_cast<int>(object->getProperty("length")));
    hit.offset = juce::jlimit(-50, 50, static_cast<int>(object->getProperty("offset")));
    return hit.semanticId.isNotEmpty() && hit.step >= 0;
}
}

GgdPatternSnapshot GgdPatternFile::capture(
    SequenceLayer& layer,
    const juce::Array<GgdCanonicalRow>& canonicalRows,
    int pattern,
    int numerator,
    int denominator,
    int bars)
{
    GgdPatternSnapshot snapshot;
    snapshot.name = layer.getPatternName(pattern);
    snapshot.numerator = juce::jmax(1, numerator);
    snapshot.denominator = denominator;
    snapshot.bars = juce::jmax(1, bars);

    const int numSteps = layer.getNumSteps();
    for (int canonicalRow = 0; canonicalRow < canonicalRows.size(); ++canonicalRow)
    {
        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
        const auto semantic = canonicalRows.getReference(canonicalRow).semanticId;

        for (int step = 0; step < numSteps; ++step)
        {
            const int probability = layer.getProb(storageRow, step, pattern);
            if (probability < 0)
                continue;

            GgdPatternHit hit;
            hit.semanticId = semantic;
            hit.step = step;
            hit.velocity = juce::jlimit(1, 127,
                static_cast<int>(layer.getVel(storageRow, step, pattern)));
            hit.probability = juce::jlimit(0, 100, probability);
            hit.retriggerLength = layer.getLength(storageRow, step, pattern);
            hit.offset = layer.getOffset(storageRow, step, pattern);
            snapshot.hits.push_back(std::move(hit));
        }
    }

    return snapshot;
}

void GgdPatternFile::restore(
    const GgdPatternSnapshot& snapshot,
    SequenceData& sequence,
    const juce::Array<GgdCanonicalRow>& canonicalRows,
    int layerIndex,
    int pattern)
{
    auto* layer = sequence.getLayer(layerIndex);
    sequence.clearPattern(layerIndex, pattern);

    for (const auto& hit : snapshot.hits)
    {
        const int canonicalRow = GgdKitMapLibrary::findCanonicalRow(canonicalRows, hit.semanticId);
        if (canonicalRow < 0 || hit.step < 0 || hit.step >= layer->getNumSteps())
            continue;

        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
        layer->setProb(storageRow, hit.step,
                       static_cast<int8_t>(juce::jlimit(0, 100, hit.probability)), pattern);
        layer->setVel(storageRow, hit.step,
                      static_cast<int8_t>(juce::jlimit(1, 127, hit.velocity)), pattern);
        layer->setLength(storageRow, hit.step,
                         static_cast<int8_t>(hit.retriggerLength), pattern);
        layer->setOffset(storageRow, hit.step,
                         static_cast<int8_t>(juce::jlimit(-50, 50, hit.offset)), pattern);
    }

    const auto name = snapshot.name.substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(name.toRawUTF8(), pattern);
}

juce::String GgdPatternFile::serialise(const GgdPatternSnapshot& snapshot)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format", "stochas-ggd-pattern");
    root->setProperty("version", 1);
    root->setProperty("name", snapshot.name);
    root->setProperty("numerator", snapshot.numerator);
    root->setProperty("denominator", snapshot.denominator);
    root->setProperty("bars", snapshot.bars);

    juce::Array<juce::var> hits;
    hits.ensureStorageAllocated(static_cast<int>(snapshot.hits.size()));
    for (const auto& hit : snapshot.hits)
        hits.add(hitToVar(hit));
    root->setProperty("hits", juce::var(hits));

    return juce::JSON::toString(juce::var(root), true);
}

std::uint64_t GgdPatternFile::fingerprint(const GgdPatternSnapshot& snapshot)
{
    return static_cast<std::uint64_t>(serialise(snapshot).hashCode64());
}

bool GgdPatternFile::write(const juce::File& file,
                           const GgdPatternSnapshot& snapshot,
                           juce::String& error)
{
    error.clear();
    auto target = file;
    if (!target.hasFileExtension(extension))
        target = target.withFileExtension(extension);

    const auto parent = target.getParentDirectory();
    if (!parent.exists() && !parent.createDirectory())
    {
        error = "Could not create pattern directory: " + parent.getFullPathName();
        return false;
    }

    if (!target.replaceWithText(serialise(snapshot), false, false, "\n"))
    {
        error = "Could not write pattern file: " + target.getFullPathName();
        return false;
    }

    return true;
}

bool GgdPatternFile::read(const juce::File& file,
                          GgdPatternSnapshot& snapshot,
                          juce::String& error)
{
    error.clear();
    if (!file.existsAsFile())
    {
        error = "Pattern file does not exist.";
        return false;
    }

    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr
        || object->getProperty("format").toString() != "stochas-ggd-pattern")
    {
        error = "This is not a Stochas GGD pattern file.";
        return false;
    }

    const int version = static_cast<int>(object->getProperty("version"));
    if (version != 1)
    {
        error = "Unsupported Stochas GGD pattern version: " + juce::String(version);
        return false;
    }

    GgdPatternSnapshot loaded;
    loaded.name = object->getProperty("name").toString();
    loaded.numerator = juce::jlimit(1, SEQ_MAX_STEPS_PER_MEASURE,
                                    static_cast<int>(object->getProperty("numerator")));
    loaded.denominator = static_cast<int>(object->getProperty("denominator"));
    if (loaded.denominator != 4 && loaded.denominator != 8 && loaded.denominator != 16)
        loaded.denominator = 4;
    loaded.bars = juce::jmax(1, static_cast<int>(object->getProperty("bars")));

    const auto hitValues = object->getProperty("hits");
    if (const auto* array = hitValues.getArray())
    {
        loaded.hits.reserve(static_cast<size_t>(array->size()));
        for (const auto& value : *array)
        {
            GgdPatternHit hit;
            if (varToHit(value, hit))
                loaded.hits.push_back(std::move(hit));
        }
    }

    snapshot = std::move(loaded);
    return true;
}
