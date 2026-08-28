#include "GgdPatternFile.h"

#include <algorithm>
#include <cmath>

namespace
{
int activeStorageRowCount(int canonicalCount)
{
    return juce::jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, canonicalCount);
}

int storageRowForCanonical(int canonicalRow, int canonicalCount)
{
    const int rowCount = activeStorageRowCount(canonicalCount);
    return (SEQ_MAX_ROWS - rowCount) + canonicalRow;
}

int canonicalRowForStorage(int storageRow, int canonicalCount)
{
    const int first = SEQ_MAX_ROWS - activeStorageRowCount(canonicalCount);
    const int canonical = storageRow - first;
    return canonical >= 0 && canonical < canonicalCount ? canonical : -1;
}

juce::var hitToVar(const GgdPatternHit& hit)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("semantic", hit.semanticId);
    object->setProperty("tick", hit.tick);
    object->setProperty("duration", hit.durationTicks);
    object->setProperty("velocity", hit.velocity);
    object->setProperty("probability", hit.probability);
    return juce::var(object);
}

bool varToHitV2(const juce::var& value, GgdPatternHit& hit)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    hit.semanticId = object->getProperty("semantic").toString().trim();
    hit.tick = juce::jmax(0, static_cast<int>(object->getProperty("tick")));
    hit.durationTicks = juce::jlimit(
        1, 65535, static_cast<int>(object->getProperty("duration")));
    hit.velocity = juce::jlimit(1, 127, static_cast<int>(object->getProperty("velocity")));
    hit.probability = juce::jlimit(0, 100, static_cast<int>(object->getProperty("probability")));
    return hit.semanticId.isNotEmpty();
}

void appendLegacyHit(const juce::var& value, std::vector<GgdPatternHit>& output)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return;

    const auto semantic = object->getProperty("semantic").toString().trim();
    const int step = static_cast<int>(object->getProperty("step"));
    if (semantic.isEmpty() || step < 0)
        return;

    const int velocity = juce::jlimit(
        1, 127, static_cast<int>(object->getProperty("velocity")));
    const int probability = juce::jlimit(
        0, 100, static_cast<int>(object->getProperty("probability")));
    const int length = juce::jlimit(
        -SEQ_MAX_RETRIGGER + 1, 127, static_cast<int>(object->getProperty("length")));
    const int offset = juce::jlimit(
        -50, 50, static_cast<int>(object->getProperty("offset")));
    const int baseTick = step * GGD_TICKS_PER_16TH;

    if (length < 0)
    {
        const int triggers = juce::jmax(2, -length + 1);
        for (int i = 0; i < triggers; ++i)
        {
            GgdPatternHit hit;
            hit.semanticId = semantic;
            hit.tick = baseTick + static_cast<int>(std::llround(
                static_cast<double>(GGD_TICKS_PER_16TH) * i / triggers));
            hit.durationTicks = juce::jmax(1, GGD_TICKS_PER_16TH / (triggers * 2));
            hit.velocity = velocity;
            hit.probability = probability;
            output.push_back(std::move(hit));
        }
        return;
    }

    GgdPatternHit hit;
    hit.semanticId = semantic;
    hit.tick = juce::jmax(0, baseTick + static_cast<int>(std::llround(
        static_cast<double>(offset) * GGD_TICKS_PER_16TH / 100.0)));
    hit.durationTicks = juce::jlimit(
        1, 65535, (length + 1) * GGD_TICKS_PER_16TH);
    hit.velocity = velocity;
    hit.probability = probability;
    output.push_back(std::move(hit));
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
    auto* events = layer.getEventPattern(pattern);
    if (!events->isActive())
    {
        const int fallbackNumerator = numerator > 0 ? numerator : 4;
        const int fallbackDenominator = denominator > 0 ? denominator : 4;
        const int fallbackBars = bars > 0 ? bars : 1;
        layer.migrateLegacyPatternToEvents(
            pattern, fallbackNumerator, fallbackDenominator, fallbackBars);
        events = layer.getEventPattern(pattern);
    }

    GgdPatternSnapshot snapshot;
    snapshot.name = layer.getPatternName(pattern);
    snapshot.numerator = events->getNumerator();
    snapshot.denominator = events->getDenominator();
    snapshot.bars = events->getBars();
    snapshot.ppq = GGD_EVENT_PPQ;

    if (numerator > 0)
        snapshot.numerator = numerator;
    if (denominator > 0)
        snapshot.denominator = denominator;
    if (bars > 0)
        snapshot.bars = bars;

    snapshot.hits.reserve(static_cast<size_t>(events->getEventCount()));
    for (int index = 0; index < events->getEventCount(); ++index)
    {
        const auto* event = events->getEvent(index);
        if (event == nullptr)
            continue;

        const int canonicalRow = canonicalRowForStorage(event->row, canonicalRows.size());
        if (canonicalRow < 0)
            continue;

        GgdPatternHit hit;
        hit.semanticId = canonicalRows.getReference(canonicalRow).semanticId;
        hit.tick = event->tick;
        hit.durationTicks = event->durationTicks;
        hit.velocity = event->velocity;
        hit.probability = event->probability;
        snapshot.hits.push_back(std::move(hit));
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

    auto* events = layer->getEventPattern(pattern);
    events->activate(snapshot.numerator, snapshot.denominator, snapshot.bars);
    const int lengthTicks = events->getLengthTicks();

    for (const auto& hit : snapshot.hits)
    {
        const int canonicalRow = GgdKitMapLibrary::findCanonicalRow(canonicalRows, hit.semanticId);
        if (canonicalRow < 0 || hit.tick < 0 || hit.tick >= lengthTicks)
            continue;

        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
        events->addEvent(storageRow,
                         hit.tick,
                         juce::jlimit(1, 127, hit.velocity),
                         juce::jlimit(0, 100, hit.probability),
                         juce::jlimit(1, 65535, hit.durationTicks));
    }

    const auto name = snapshot.name.substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(name.toRawUTF8(), pattern);
}

juce::String GgdPatternFile::serialise(const GgdPatternSnapshot& snapshot)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format", "stochas-ggd-pattern");
    root->setProperty("version", 2);
    root->setProperty("ppq", GGD_EVENT_PPQ);
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
    if (version != 1 && version != 2)
    {
        error = "Unsupported Stochas GGD pattern version: " + juce::String(version);
        return false;
    }

    GgdPatternSnapshot loaded;
    loaded.name = object->getProperty("name").toString();
    loaded.numerator = juce::jlimit(1, SEQ_MAX_STEPS_PER_MEASURE,
                                    static_cast<int>(object->getProperty("numerator")));
    loaded.denominator = static_cast<int>(object->getProperty("denominator"));
    if (loaded.denominator != 2 && loaded.denominator != 4
        && loaded.denominator != 8 && loaded.denominator != 16)
        loaded.denominator = 4;
    loaded.bars = juce::jlimit(1, 64, static_cast<int>(object->getProperty("bars")));
    loaded.ppq = GGD_EVENT_PPQ;

    const auto hitValues = object->getProperty("hits");
    if (const auto* array = hitValues.getArray())
    {
        loaded.hits.reserve(static_cast<size_t>(array->size()));
        for (const auto& value : *array)
        {
            if (version == 2)
            {
                GgdPatternHit hit;
                if (varToHitV2(value, hit))
                    loaded.hits.push_back(std::move(hit));
            }
            else
            {
                appendLegacyHit(value, loaded.hits);
            }
        }
    }

    const int patternLength = static_cast<int>(
        static_cast<std::int64_t>(GGD_EVENT_PPQ) * loaded.numerator * 4
        / loaded.denominator * loaded.bars);
    loaded.hits.erase(
        std::remove_if(loaded.hits.begin(), loaded.hits.end(),
                       [patternLength](const GgdPatternHit& hit)
                       {
                           return hit.tick < 0 || hit.tick >= patternLength;
                       }),
        loaded.hits.end());

    snapshot = std::move(loaded);
    return true;
}
