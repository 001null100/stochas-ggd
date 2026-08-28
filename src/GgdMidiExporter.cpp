#include "GgdMidiExporter.h"

#include <algorithm>

juce::String GgdMidiExportResult::summary() const
{
    if (!ok)
        return error;

    juce::String text = "Exported " + juce::String(writtenHits)
                      + (writtenHits == 1 ? " hit" : " hits");
    if (unmappedHits > 0)
        text += " | skipped " + juce::String(unmappedHits) + " unmapped";
    return text;
}

GgdMidiExportResult GgdMidiExporter::writeFile(const juce::File& requestedFile,
                                               const GgdPatternSnapshot& pattern,
                                               const GgdKitMap& map,
                                               int midiChannel)
{
    GgdMidiExportResult result;

    auto file = requestedFile;
    if (!file.hasFileExtension("mid;midi"))
        file = file.withFileExtension(".mid");

    if (file == juce::File())
    {
        result.error = "No output file selected.";
        return result;
    }

    const int channel = juce::jlimit(1, 16, midiChannel);
    juce::MidiMessageSequence sequence;

    auto trackName = juce::MidiMessage::trackNameEvent(
        pattern.name.isNotEmpty() ? pattern.name : juce::String("Stochas GGD Pattern"));
    trackName.setTimeStamp(0.0);
    sequence.addEvent(trackName);

    auto timeSignature = juce::MidiMessage::timeSignatureMetaEvent(
        juce::jmax(1, pattern.numerator),
        juce::jmax(1, pattern.denominator));
    timeSignature.setTimeStamp(0.0);
    sequence.addEvent(timeSignature);

    for (const auto& hit : pattern.hits)
    {
        const auto* articulation = map.findArticulation(hit.semanticId);
        const auto* binding = articulation != nullptr ? articulation->primaryNoteBinding() : nullptr;
        if (binding == nullptr || binding->midi < 0 || binding->midi > 127)
        {
            ++result.unmappedHits;
            continue;
        }

        const int tick = juce::jmax(0, hit.tick);
        const int duration = juce::jmax(1, hit.durationTicks);
        const auto velocity = static_cast<juce::uint8>(juce::jlimit(1, 127, hit.velocity));

        auto on = juce::MidiMessage::noteOn(channel, binding->midi, velocity);
        on.setTimeStamp(static_cast<double>(tick));
        sequence.addEvent(on);

        auto off = juce::MidiMessage::noteOff(channel, binding->midi);
        off.setTimeStamp(static_cast<double>(tick + duration));
        sequence.addEvent(off);
    }

    sequence.updateMatchedPairs();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(pattern.ppq > 0 ? pattern.ppq : GGD_EVENT_PPQ);
    midi.addTrack(sequence);

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream stream(file);
    if (!stream.openedOk())
    {
        result.error = "Could not open the MIDI file for writing: " + file.getFullPathName();
        return result;
    }

    if (!midi.writeTo(stream))
    {
        result.error = "JUCE could not write the MIDI file.";
        return result;
    }

    stream.flush();
    result.ok = true;
    result.writtenHits = static_cast<int>(pattern.hits.size()) - result.unmappedHits;
    return result;
}
