#pragma once

#include <JuceHeader.h>
#include <array>

// Shared appearance roles for the editor, timeline and library browser.  Keep
// timing hierarchy in semantic roles rather than deriving everything from one
// border colour so every theme preserves the same readability guarantees.
enum class GgdThemeRole
{
    background,
    outside,
    panel,
    panelRaised,
    panelSoft,
    borderSoft,
    border,
    borderStrong,
    text,
    muted,
    accent,
    accentSecondary,
    warm,
    danger,
    barLine,
    beatLine,
    subdivisionLine,
    fineSubdivisionLine,
    groupFill,
    groupLine,
    rowAlternate,
    selection,
    playhead,
    count
};

enum class GgdThemeId
{
    graphite = 0,
    midnight,
    ember,
    contrast,
    count
};

juce::Colour ggdThemeColour(GgdThemeRole role);
juce::String ggdThemeName(GgdThemeId theme);
GgdThemeId ggdThemeFromIndex(int index);
int ggdThemeIndex(GgdThemeId theme);
GgdThemeId ggdCurrentTheme();
void ggdSetCurrentTheme(GgdThemeId theme);

// Maps the original Beta palette constants into semantic roles.  This lets
// older presentation code become theme-aware without coupling it to palette
// storage or duplicating colour tables.
juce::Colour ggdLegacyThemeColour(juce::uint32 original);

class GgdLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GgdLookAndFeel();
    void syncThemeColours();

    void drawButtonBackground(juce::Graphics&,
                              juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics&,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox&) override;
    void drawLinearSlider(juce::Graphics&,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle,
                          juce::Slider&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};
