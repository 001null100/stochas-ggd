#include "GgdMidiImporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <limits>
#include <set>

namespace
{
struct RawNote
{
    double quarter = 0.0;
    int note = 0;
    int velocity = 100;
};

struct ReverseMap
{
    std::array<juce::String, 128> semantic;
    std::array<bool, 128> primary{};
};

ReverseMap buildReverseMap(const GgdKitMap& map)
{
    ReverseMap result;

    for (const auto& group : map.groups)
    {
        for (const auto& articulation : group.articulations)
        {
            for (const auto& binding : articulation.bindings)
            {
                if (!binding.kind.equalsIgnoreCase("note") || binding.midi < 0 || binding.midi > 127)
                    continue;

                const bool isPrimary = binding.role.equalsIgnoreCase("primary");
                auto& current = result.semantic[static_cast<size_t>(binding.midi)];
                auto& currentPrimary = result.primary[static_cast<size_t>(binding.midi)];

                if (current.isEmpty() || (isPrimary && !currentPrimary))
                {
                    current = articulation.semanticId;
                    currentPrimary = isPrimary;
                }
            }
        }
    }

    return result;
}

bool isSupportedMeter(int numerator, int denominator)
{
    return numerator >= 1 && numerator <= 16
        && (denominator == 4 || denominator == 8 || denominator == 16);
}

int stepsPerBar(int numerator, int denominator)
{
    return juce::jmax(1, numerator * 16 / denominator);
}
}

juce::String GgdMidiImportResult::summary() const
{
    if (!ok)
        return error;

    juce::String text;
    text << "Imported " << mappedNotes << "/" << totalNotes << " notes using "
         << sourceMapName << " (" << juce::String(sourceConfidence * 100.0f, 0) << "% coverage)";

    if (unresolvedNotes > 0)
    {
        text << ". Unresolved: " << unresolvedNotes << " note" << (unresolvedNotes == 1 ? "" : "s") << " [";
        for (int i = 0; i < unresolvedPitches.size(); ++i)
        {
            if (i != 0)
                text << ", ";
            text << unresolvedPitches.getUnchecked(i);
        }
        text << "]";
    }

    if (collisions > 0)
        text << ". " << collisions << " same-cell flam/retrigger collision" << (collisions == 1 ? "" : "s") << " compacted";

    if (truncatedNotes > 0)
        text << ". " << truncatedNotes << " note" << (truncatedNotes == 1 ? "" : "s") << " beyond the " << bars << "-bar limit skipped";

    if (!meterSupported)
        text << ". Meter metadata is not supported by the editor";

    return text;
}

GgdMidiImportResult GgdMidiImporter::parseFile(
    const juce::File& file,
    const juce::Array<GgdKitMap>& maps,
    const juce::Array<GgdCanonicalRow>& canonicalRows,
    int forcedSourceMap,
    int maxBars)
{
    GgdMidiImportResult result;
    result.fileName = file.getFileNameWithoutExtension();

    if (!file.existsAsFile())
    {
        result.error = "The selected MIDI file does not exist.";
        return result;
    }

    if (maps.isEmpty() || canonicalRows.isEmpty())
    {
        result.error = "No drum mappings are available.";
        return result;
    }

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
    {
        result.error = "Could not open the MIDI file.";
        return result;
    }

    juce::MidiFile midi;
    if (!midi.readFrom(stream))
    {
        result.error = "The file is not a readable Standard MIDI File.";
        return result;
    }

    const int ppq = midi.getTimeFormat();
    if (ppq <= 0)
    {
        result.error = "SMPTE-timed MIDI files are not supported yet. Use a PPQ MIDI file.";
        return result;
    }

    std::vector<RawNote> rawNotes;
    double earliestMeterTick = std::numeric_limits<double>::max();
    bool foundMeter = false;
    bool meterChanges = false;
    int firstNumerator = 4;
    int firstDenominator = 4;

    for (int trackIndex = 0; trackIndex < midi.getNumTracks(); ++trackIndex)
    {
        const auto* track = midi.getTrack(trackIndex);
        if (track == nullptr)
            continue;

        for (int eventIndex = 0; eventIndex < track->getNumEvents(); ++eventIndex)
        {
            const auto* holder = track->getEventPointer(eventIndex);
            if (holder == nullptr)
                continue;

            const auto& message = holder->message;
            if (message.isNoteOn())
            {
                RawNote note;
                note.quarter = message.getTimeStamp() / static_cast<double>(ppq);
                note.note = message.getNoteNumber();
                note.velocity = juce::jlimit(1, 127, message.getVelocity());
                rawNotes.push_back(note);
            }
            else if (message.isTimeSignatureMetaEvent())
            {
                int numerator = 4;
                int denominator = 4;
                message.getTimeSignatureInfo(numerator, denominator);

                if (!foundMeter || message.getTimeStamp() < earliestMeterTick)
                {
                    if (foundMeter && (numerator != firstNumerator || denominator != firstDenominator))
                        meterChanges = true;
                    foundMeter = true;
                    earliestMeterTick = message.getTimeStamp();
                    firstNumerator = numerator;
                    firstDenominator = denominator;
                }
                else if (numerator != firstNumerator || denominator != firstDenominator)
                {
                    meterChanges = true;
                }
            }
        }
    }

    if (rawNotes.empty())
    {
        result.error = "The MIDI file contains no note-on events.";
        return result;
    }

    std::sort(rawNotes.begin(), rawNotes.end(), [](const RawNote& a, const RawNote& b)
    {
        if (a.quarter != b.quarter)
            return a.quarter < b.quarter;
        return a.note < b.note;
    });

    result.totalNotes = static_cast<int>(rawNotes.size());
    result.numerator = firstNumerator;
    result.denominator = firstDenominator;
    result.meterSupported = isSupportedMeter(result.numerator, result.denominator) && !meterChanges;

    std::vector<ReverseMap> reverseMaps;
    reverseMaps.reserve(static_cast<size_t>(maps.size()));
    for (const auto& map : maps)
        reverseMaps.push_back(buildReverseMap(map));

    auto scoreMap = [&](int mapIndex)
    {
        int mapped = 0;
        std::set<int> distinct;
        const auto& reverse = reverseMaps[static_cast<size_t>(mapIndex)];
        for (const auto& note : rawNotes)
        {
            if (note.note >= 0 && note.note <= 127
                && reverse.semantic[static_cast<size_t>(note.note)].isNotEmpty())
            {
                ++mapped;
                distinct.insert(note.note);
            }
        }
        return std::pair<int, int>(mapped, static_cast<int>(distinct.size()));
    };

    if (forcedSourceMap >= 0 && forcedSourceMap < maps.size())
    {
        result.sourceMapIndex = forcedSourceMap;
    }
    else
    {
        std::pair<int, int> bestScore { -1, -1 };
        for (int mapIndex = 0; mapIndex < maps.size(); ++mapIndex)
        {
            const auto score = scoreMap(mapIndex);
            if (score > bestScore)
            {
                bestScore = score;
                result.sourceMapIndex = mapIndex;
            }
        }
    }

    if (result.sourceMapIndex < 0)
    {
        result.error = "Could not choose a source mapping profile.";
        return result;
    }

    const auto sourceScore = scoreMap(result.sourceMapIndex);
    result.sourceMapName = maps.getReference(result.sourceMapIndex).library;
    result.sourceConfidence = result.totalNotes > 0
        ? static_cast<float>(sourceScore.first) / static_cast<float>(result.totalNotes)
        : 0.0f;

    if (!result.meterSupported)
    {
        if (meterChanges)
            result.error = "This MIDI file changes time signature mid-pattern. Meter changes are not supported yet.";
        else
            result.error = "This MIDI file uses an unsupported time signature: "
                         + juce::String(result.numerator) + "/" + juce::String(result.denominator) + ".";
        return result;
    }

    const double barLengthQuarters = static_cast<double>(result.numerator) * 4.0
                                   / static_cast<double>(result.denominator);
    const double latestQuarter = rawNotes.back().quarter;
    const int requestedBars = juce::jmax(1, static_cast<int>(std::floor(latestQuarter / barLengthQuarters)) + 1);
    result.bars = juce::jlimit(1, juce::jmax(1, maxBars), requestedBars);
    const int totalSteps = result.bars * stepsPerBar(result.numerator, result.denominator);

    const auto& reverse = reverseMaps[static_cast<size_t>(result.sourceMapIndex)];
    std::set<int> unresolved;
    std::map<std::pair<int, int>, GgdImportedCell> cells;

    for (const auto& raw : rawNotes)
    {
        const auto semantic = reverse.semantic[static_cast<size_t>(raw.note)];
        if (semantic.isEmpty())
        {
            ++result.unresolvedNotes;
            unresolved.insert(raw.note);
            continue;
        }

        const int canonicalRow = GgdKitMapLibrary::findCanonicalRow(canonicalRows, semantic);
        if (canonicalRow < 0)
        {
            ++result.unresolvedNotes;
            unresolved.insert(raw.note);
            continue;
        }

        const double stepPosition = raw.quarter * 4.0;
        const int step = static_cast<int>(std::floor(stepPosition + 0.5));
        if (step < 0 || step >= totalSteps)
        {
            ++result.truncatedNotes;
            continue;
        }

        const int offset = juce::jlimit(-50, 50,
            static_cast<int>(std::round((stepPosition - static_cast<double>(step)) * 100.0)));
        const auto key = std::make_pair(canonicalRow, step);
        auto found = cells.find(key);

        if (found == cells.end())
        {
            GgdImportedCell cell;
            cell.canonicalRow = canonicalRow;
            cell.step = step;
            cell.velocity = raw.velocity;
            cell.offset = offset;
            cells.emplace(key, cell);
        }
        else
        {
            ++result.collisions;
            auto& cell = found->second;
            cell.velocity = juce::jmax(cell.velocity, raw.velocity);
            cell.offset = 0;
            cell.retriggerLength = cell.retriggerLength >= 0
                ? -1
                : juce::jmax(-3, cell.retriggerLength - 1);
        }

        ++result.mappedNotes;
    }

    for (int pitch : unresolved)
        result.unresolvedPitches.add(pitch);

    result.cells.reserve(cells.size());
    for (const auto& pair : cells)
        result.cells.push_back(pair.second);

    if (result.mappedNotes == 0)
    {
        result.error = "No notes could be mapped with source profile '" + result.sourceMapName
                     + "'. Choose another source profile and try again.";
        return result;
    }

    result.ok = true;
    return result;
}
