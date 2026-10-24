#pragma once

#include "GgdDrumMachine.h"
#include "GgdUiTheme.h"

#include <functional>

// Sheet for the project's Drum machine kit: how many pads there are, and the
// name and MIDI note of each. Every change is applied immediately through
// onChange; Done, Escape or a click outside closes it.
class GgdPadEditor final : public juce::Component
{
public:
    GgdPadEditor(const GgdDrumMachineKit& pads,
                 std::function<void(const GgdDrumMachineKit&)> onChange,
                 std::function<void(int)> onAudition,
                 std::function<void()> onClose);
    ~GgdPadEditor() override;

    // Puts the cursor in a pad's name field (and scrolls it into view).
    void focusPad(int pad);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    juce::Rectangle<int> getCardBounds() const;

private:
    class PadRow;

    GgdDrumMachineKit pads;
    std::function<void(const GgdDrumMachineKit&)> changed;
    std::function<void(int)> audition;
    std::function<void()> closed;

    juce::ComboBox countBox;
    GgdButton resetButton { "Reset to C1 - D#2" };
    GgdButton doneButton { "Done" };
    GgdButton closeButton { {}, GgdIcon::close };
    juce::Viewport viewport;
    juce::Component list;
    juce::OwnedArray<PadRow> rows;
    juce::Rectangle<int> headingArea;

    void rebuildRows();
    void layoutRows();
    void emit();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdPadEditor)
};
