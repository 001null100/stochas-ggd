#pragma once

#include "GgdUiTheme.h"

#include <functional>
#include <memory>

// Folder (inside the user's application data folder) that holds the editor
// preference files. Tools can redirect it before the first editor opens.
juce::String ggdSettingsFolderName();
void ggdSetSettingsFolderName(const juce::String& name);

// Editor preferences. They belong to this computer, not to a project, and are
// shared by every open Stochas GGD window.
struct GgdPreferences
{
    int theme = 0;
    bool colourByInstrument = true;

    bool followPlayhead = true;
    bool smoothPlayhead = true;
    bool playheadGlow = true;

    bool autoFineGrid = true;
    bool shiftHoverVelocity = true;
    bool audition = true;

    bool playedEffects = true;
    bool playedRipple = true;
    bool playedLaneFlash = true;
    bool velocityReactive = true;
    bool reduceMotion = false;
    int effectIntensityPercent = 100;
    int effectDecayMs = 420;

    // Window layout, remembered per computer.
    bool showLibrary = true;
    int gridTicks = 240; // 1/16
    float zoom = 1.25f;
    int editorWidth = 1480;
    int editorHeight = 860;

    bool operator==(const GgdPreferences& other) const;
    bool operator!=(const GgdPreferences& other) const { return !(*this == other); }
};

// The preferences file, shared by all editor instances in the process.
// Listeners are told (asynchronously) whenever any instance saves.
class GgdPreferenceStore final : public juce::ChangeBroadcaster
{
public:
    GgdPreferenceStore();
    ~GgdPreferenceStore() override;

    GgdPreferences load() const;
    void save(const GgdPreferences& preferences);

private:
    std::unique_ptr<juce::PropertiesFile> file;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdPreferenceStore)
};

// Modal settings sheet drawn over the editor. Every change is applied
// immediately through onChange; Done, Escape or a click outside closes it.
class GgdSettingsPanel final : public juce::Component
{
public:
    GgdSettingsPanel(const GgdPreferences& current,
                     std::function<void(const GgdPreferences&)> onChange,
                     std::function<void()> onClose);
    ~GgdSettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // The card that holds the controls, in local coordinates.
    juce::Rectangle<int> getCardBounds() const;

private:
    class ThemeSwatch;

    GgdPreferences prefs;
    std::function<void(const GgdPreferences&)> changed;
    std::function<void()> closed;

    juce::OwnedArray<ThemeSwatch> swatches;
    juce::ToggleButton colourToggle;
    juce::ToggleButton followToggle;
    juce::ToggleButton smoothToggle;
    juce::ToggleButton glowToggle;
    juce::ToggleButton autoFineToggle;
    juce::ToggleButton shiftHoverToggle;
    juce::ToggleButton auditionToggle;
    juce::ToggleButton effectsToggle;
    juce::ToggleButton rippleToggle;
    juce::ToggleButton laneFlashToggle;
    juce::ToggleButton velocityToggle;
    juce::ToggleButton reduceMotionToggle;
    juce::Label intensityLabel;
    juce::Slider intensitySlider;
    juce::Label decayLabel;
    juce::Slider decaySlider;
    GgdButton resetButton { "Restore defaults" };
    GgdButton doneButton { "Done" };
    GgdButton closeButton { {}, GgdIcon::close };
    juce::Rectangle<int> feedbackNoteArea;

    void setupToggle(juce::ToggleButton& button, const juce::String& text, const juce::String& tooltip,
                     bool GgdPreferences::*field);
    void refreshControls();
    void updateEnablement();
    void emit();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdSettingsPanel)
};
