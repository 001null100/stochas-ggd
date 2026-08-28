#include "GgdMidiImporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
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

struct PitchTranslation
{
    int destinationMidi = -1;
    bool fallback = false;
};

enum class DestinationKit
{
    pv,
    piv,
    modernMassive,
    unknown
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

DestinationKit destinationKitFor(const GgdKitMap& map)
{
    if (map.id.containsIgnoreCase(".piv."))
        return DestinationKit::piv;
    if (map.id.containsIgnoreCase(".pv."))
        return DestinationKit::pv;
    if (map.id.containsIgnoreCase("modern"))
        return DestinationKit::modernMassive;
    return DestinationKit::unknown;
}

bool isSupportedMeter(int numerator, int denominator)
{
    return numerator >= 1 && numerator <= 32
        && (denominator == 2 || denominator == 4 || denominator == 8 || denominator == 16);
}

PitchTranslation translatePv(int source)
{
    switch (source)
    {
        case 10: return { 43, false };
        case 24: return { 44, false };
        case 36: return { 24, false };
        case 37: return { 30, false };
        case 38: return { 26, false };
        case 41: return { 35, false };
        case 42: return { 42, false };
        case 43: return { 34, false };
        case 44: return { 48, false };
        case 46: return { 46, false };
        case 48: return { 33, false };
        case 49: return { 52, false };
        case 50: return { 33, false };
        case 51: return { 62, false };
        case 52: return { 77, false };
        case 53: return { 61, false };
        case 54: return { 44, false };
        case 55: return { 65, false };
        case 57: return { 54, false };
        case 58: return { 45, false };
        case 62: return { 44, false };
        case 64: return { 73, false };
        case 70: return { 47, false };
        case 74: return { 56, false };
        case 25: return { 45, true };
        case 39: return { 26, true };
        case 59: return { 62, true };
        case 95: return { 55, true };
        default: return {};
    }
}

PitchTranslation translatePiv(int source)
{
    switch (source)
    {
        case 10: return { 43, false };
        case 24: return { 44, false };
        case 36: return { 24, false };
        case 37: return { 30, false };
        case 38: return { 26, false };
        case 41: return { 36, false };
        case 42: return { 42, false };
        case 43: return { 36, false };
        case 44: return { 48, false };
        case 46: return { 46, false };
        case 48: return { 34, false };
        case 49: return { 52, false };
        case 50: return { 33, false };
        case 51: return { 62, false };
        case 52: return { 65, false };
        case 53: return { 61, false };
        case 54: return { 44, false };
        case 55: return { 73, false };
        case 57: return { 54, false };
        case 58: return { 45, false };
        case 62: return { 44, false };
        case 64: return { 73, false };
        case 70: return { 47, false };
        case 74: return { 65, false };
        case 25: return { 45, true };
        case 39: return { 28, true };
        case 59: return { 62, true };
        case 95: return { 55, true };
        default: return {};
    }
}

PitchTranslation translateModernMassive(int source)
{
    switch (source)
    {
        case 10: return { 47, false };
        case 24: return { 48, false };
        case 25: return { 53, false };
        case 36: return { 24, false };
        case 37: return { 30, false };
        case 38: return { 26, false };
        case 39: return { 28, false };
        case 41: return { 39, false };
        case 42: return { 45, false };
        case 43: return { 37, false };
        case 44: return { 43, false };
        case 46: return { 54, false };
        case 48: return { 37, false };
        case 49: return { 62, false };
        case 50: return { 33, false };
        case 51: return { 72, false };
        case 52: return { 76, false };
        case 53: return { 74, false };
        case 54: return { 47, false };
        case 55: return { 83, false };
        case 57: return { 67, false };
        case 58: return { 54, false };
        case 59: return { 73, false };
        case 62: return { 45, false };
        case 64: return { 83, false };
        case 70: return { 56, false };
        case 74: return { 76, false };
        case 95: return { 69, false };
        default: return {};
    }
}

PitchTranslation translatePitch(DestinationKit kit, int source)
{
    if (source == 85)
        return {};

    switch (kit)
    {
        case DestinationKit::pv:            return translatePv(source);
        case DestinationKit::piv:           return translatePiv(source);
        case DestinationKit::modernMassive: return translateModernMassive(source);
        default:                            return {};
    }
}
}

juce::String GgdMidiImportResult::summary() const
{
    if (!ok)
        return error;

    juce::String text;
    text << formatName << " | "
         << mappedNotes << "/" << totalNotes << " notes mapped | "
         << bars << (bars == 1 ? " bar" : " bars") << " | "
         << numerator << "/" << denominator;

    if (fallbackNotes > 0)
        text << " | " << fallbackNotes << " fallback" << (fallbackNotes == 1 ? "" : "s");

    if (unresolvedNotes > 0)
    {
        text << " | unresolved " << unresolvedNotes << " [";
        for (int i = 0; i < unresolvedPitches.size(); ++i)
        {
            if (i != 0)
                text << ", ";
            text << unresolvedPitches.getUnchecked(i);
        }
        text << "]";
    }

    if (collisions > 0)
        text << " | " << collisions << " exact collision" << (collisions == 1 ? "" : "s");

    if (truncatedNotes > 0)
        text << " | " << truncatedNotes << " truncated";

    return text;
}

GgdMidiImportResult GgdMidiImporter::parseFile(
    const juce::File& file,
    const GgdKitMap& destinationMap,
    const juce::Array<GgdCanonicalRow>& canonicalRows,
    int maxPatternTicks)
{
    GgdMidiImportResult result;
    result.fileName = file.getFileNameWithoutExtension();

    if (!file.existsAsFile())
    {
        result.error = "The selected MIDI file does not exist.";
        return result;
    }

    if (canonicalRows.isEmpty())
    {
        result.error = "No drum articulations are available.";
        return result;
    }

    const auto destinationKit = destinationKitFor(destinationMap);
    if (destinationKit == DestinationKit::unknown)
    {
        result.error = "The selected destination kit does not have a GGD Groove Player translation table.";
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
        result.error = "SMPTE-timed MIDI files are not supported. Use a PPQ MIDI file.";
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
                note.velocity = juce::jlimit(1, 127, static_cast<int>(message.getVelocity()));
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
        if (a.note != b.note)
            return a.note < b.note;
        return a.velocity < b.velocity;
    });

    result.totalNotes = static_cast<int>(rawNotes.size());
    result.numerator = firstNumerator;
    result.denominator = firstDenominator;
    result.meterSupported = isSupportedMeter(result.numerator, result.denominator) && !meterChanges;

    if (!result.meterSupported)
    {
        if (meterChanges)
            result.error = "This MIDI file changes time signature inside the groove. Meter changes are not supported yet.";
        else
            result.error = "Unsupported time signature: "
                         + juce::String(result.numerator) + "/" + juce::String(result.denominator) + ".";
        return result;
    }

    const int ticksPerBar = GGD_EVENT_PPQ * result.numerator * 4 / result.denominator;
    if (ticksPerBar <= 0 || ticksPerBar > maxPatternTicks)
    {
        result.error = "The selected time signature is wider than the engine capacity.";
        return result;
    }

    const int maxBars = juce::jmax(1, maxPatternTicks / ticksPerBar);
    const double barLengthQuarters = static_cast<double>(result.numerator) * 4.0
                                   / static_cast<double>(result.denominator);
    const double latestQuarter = rawNotes.back().quarter;
    const int requestedBars = juce::jmax(
        1, static_cast<int>(std::floor(latestQuarter / barLengthQuarters)) + 1);
    result.bars = juce::jmin(requestedBars, maxBars);
    const int totalTicks = result.bars * ticksPerBar;

    const auto reverse = buildReverseMap(destinationMap);
    std::set<int> unresolved;
    std::map<std::pair<int, int>, GgdImportedEvent> uniqueEvents;

    for (const auto& raw : rawNotes)
    {
        const auto translation = translatePitch(destinationKit, raw.note);
        if (translation.destinationMidi < 0 || translation.destinationMidi > 127)
        {
            ++result.unresolvedNotes;
            unresolved.insert(raw.note);
            continue;
        }

        const auto semantic = reverse.semantic[static_cast<size_t>(translation.destinationMidi)];
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

        const int tick = static_cast<int>(std::llround(raw.quarter * GGD_EVENT_PPQ));
        if (tick < 0 || tick >= totalTicks)
        {
            ++result.truncatedNotes;
            continue;
        }

        if (translation.fallback)
            ++result.fallbackNotes;

        const auto key = std::make_pair(canonicalRow, tick);
        auto found = uniqueEvents.find(key);
        if (found == uniqueEvents.end())
        {
            GgdImportedEvent event;
            event.canonicalRow = canonicalRow;
            event.tick = tick;
            event.velocity = raw.velocity;
            uniqueEvents.emplace(key, event);
        }
        else
        {
            ++result.collisions;
            found->second.velocity = juce::jmax(found->second.velocity, raw.velocity);
        }

        ++result.mappedNotes;
    }

    for (int pitch : unresolved)
        result.unresolvedPitches.add(pitch);

    result.events.reserve(uniqueEvents.size());
    for (const auto& pair : uniqueEvents)
        result.events.push_back(pair.second);

    if (result.mappedNotes == 0)
    {
        result.error = "No notes matched the GGD Groove Player mapping for "
                     + destinationMap.library + ".";
        return result;
    }

    result.ok = true;
    return result;
}
