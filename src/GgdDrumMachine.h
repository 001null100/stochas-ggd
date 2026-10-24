#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <cstring>

// The "Drum machine" kit: up to 32 pads, each with a MIDI note and a name the
// user chooses. It is saved with the project. Pads are addressed by their
// semantic id "pad.01" ... "pad.32", so patterns stay portable between
// projects that number their pads the same way.
//
// Plain data (no pointers) because it lives inside SequenceData, which is
// copied between the UI and audio buffers.
constexpr int GGD_DRUM_MACHINE_MAX_PADS = 32;
constexpr int GGD_DRUM_MACHINE_DEFAULT_PADS = 16;
constexpr int GGD_DRUM_MACHINE_FIRST_NOTE = 36; // C1 when middle C is C3, as in Bitwig and Ableton
constexpr int GGD_PAD_NAME_BYTES = 32;          // UTF-8 including the terminator

struct GgdDrumMachineKit
{
    GgdDrumMachineKit() { reset(); }

    // 16 pads on C1-D#2 (MIDI 36-51), default names.
    void reset()
    {
        padCount = GGD_DRUM_MACHINE_DEFAULT_PADS;
        for (int pad = 0; pad < GGD_DRUM_MACHINE_MAX_PADS; ++pad)
            notes[pad] = static_cast<std::int8_t>(defaultNote(pad));
        std::memset(names, 0, sizeof(names));
    }

    int getPadCount() const { return juce::jlimit(1, GGD_DRUM_MACHINE_MAX_PADS, static_cast<int>(padCount)); }
    void setPadCount(int count) { padCount = juce::jlimit(1, GGD_DRUM_MACHINE_MAX_PADS, count); }

    int getNote(int pad) const { return isPad(pad) ? notes[pad] : -1; }
    void setNote(int pad, int note)
    {
        if (isPad(pad))
            notes[pad] = static_cast<std::int8_t>(juce::jlimit(0, 127, note));
    }

    // The name the user typed, empty when the pad uses its default name.
    juce::String getName(int pad) const
    {
        return isPad(pad) ? juce::String::fromUTF8(names[pad]) : juce::String();
    }

    void setName(int pad, const juce::String& name)
    {
        if (!isPad(pad))
            return;
        auto text = name.trim();
        while (text.getNumBytesAsUTF8() > static_cast<size_t>(GGD_PAD_NAME_BYTES - 1))
            text = text.dropLastCharacters(1);
        std::memset(names[pad], 0, sizeof(names[pad]));
        text.copyToUTF8(names[pad], sizeof(names[pad]));
    }

    juce::String getDisplayName(int pad) const
    {
        const auto name = getName(pad);
        return name.isNotEmpty() ? name : defaultName(pad);
    }

    bool operator==(const GgdDrumMachineKit& other) const
    {
        return padCount == other.padCount && std::memcmp(notes, other.notes, sizeof(notes)) == 0
            && std::memcmp(names, other.names, sizeof(names)) == 0;
    }
    bool operator!=(const GgdDrumMachineKit& other) const { return !(*this == other); }

    static bool isPad(int pad) { return pad >= 0 && pad < GGD_DRUM_MACHINE_MAX_PADS; }
    static int defaultNote(int pad) { return juce::jmin(127, GGD_DRUM_MACHINE_FIRST_NOTE + pad); }
    static juce::String defaultName(int pad) { return "Pad " + juce::String(pad + 1); }
    static juce::String semanticId(int pad) { return "pad." + juce::String(pad + 1).paddedLeft('0', 2); }

    // Index of a "pad.NN" id, or -1.
    static int padForSemanticId(const juce::String& id)
    {
        if (!id.startsWith("pad."))
            return -1;
        const int number = id.fromFirstOccurrenceOf(".", false, false).getIntValue();
        return number >= 1 && number <= GGD_DRUM_MACHINE_MAX_PADS ? number - 1 : -1;
    }

    // "C1" for 36: middle C (60) is C3, the naming Bitwig and Ableton use.
    static juce::String noteName(int note)
    {
        return note >= 0 && note <= 127 ? juce::MidiMessage::getMidiNoteName(note, true, true, 3) : juce::String("-");
    }

private:
    std::int32_t padCount = GGD_DRUM_MACHINE_DEFAULT_PADS;
    std::int8_t notes[GGD_DRUM_MACHINE_MAX_PADS] {};
    char names[GGD_DRUM_MACHINE_MAX_PADS][GGD_PAD_NAME_BYTES] {};
};
