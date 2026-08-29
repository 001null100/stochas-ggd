#include "GgdDrumEditor.h"
#include "GgdDrumGridFinal.h"

#include <cmath>
#include <functional>
#include <utility>

namespace
{
class GgdSettingsOverlayFinal final : public juce::Component
{
public:
    using ChangeCallback = std::function<void(int,
                                              bool, bool, bool,
                                              bool, bool, bool,
                                              bool, bool, bool, bool, bool,
                                              int, int)>;
    using CloseCallback = std::function<void()>;

    GgdSettingsOverlayFinal(int themeIndex,
                            bool follow,
                            bool smooth,
                            bool playheadGlow,
                            bool autoFine,
                            bool shiftHover,
                            bool audition,
                            bool playedEffects,
                            bool ripple,
                            bool laneFlash,
                            bool velocityReactive,
                            bool reduceMotion,
                            int effectIntensityPercent,
                            int effectDecayMs,
                            ChangeCallback change,
                            CloseCallback close)
        : onChange(std::move(change)), onClose(std::move(close))
    {
        setOpaque(false);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(true);

        themeLabel.setText("Theme", juce::dontSendNotification);
        addAndMakeVisible(themeLabel);
        for (int i = 0; i < static_cast<int>(GgdThemeId::count); ++i)
            themeSelector.addItem(ggdThemeName(ggdThemeFromIndex(i)), i + 1);
        themeSelector.setSelectedId(themeIndex + 1, juce::dontSendNotification);
        themeSelector.setMouseClickGrabsKeyboardFocus(false);
        themeSelector.onChange = [this] { emitChange(); };
        addAndMakeVisible(themeSelector);

        setupToggle(followToggle,
                    "Centre-follow when the timeline exceeds the canvas", follow);
        setupToggle(smoothToggle, "Smooth playhead interpolation", smooth);
        setupToggle(playheadGlowToggle, "Playhead forward glow", playheadGlow);

        setupToggle(autoFineToggle, "Use a finer grid automatically at high zoom", autoFine);
        setupToggle(shiftHoverToggle, "Hold Shift over a hit to inspect velocity", shiftHover);
        setupToggle(auditionToggle, "Click articulation names to audition", audition);

        setupToggle(playedEffectsToggle, "Played-hit feedback", playedEffects);
        setupToggle(rippleToggle, "Expanding hit ripple", ripple);
        setupToggle(laneFlashToggle, "Lane flash", laneFlash);
        setupToggle(velocityReactiveToggle, "Velocity-reactive intensity", velocityReactive);
        setupToggle(reduceMotionToggle, "Reduce Motion", reduceMotion);

        intensityLabel.setText("Effect strength", juce::dontSendNotification);
        addAndMakeVisible(intensityLabel);
        intensitySlider.setRange(0.0, 150.0, 1.0);
        intensitySlider.setValue(effectIntensityPercent, juce::dontSendNotification);
        intensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        intensitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 24);
        intensitySlider.setTextValueSuffix(" %");
        intensitySlider.onValueChange = [this] { emitChange(); };
        addAndMakeVisible(intensitySlider);

        decayLabel.setText("Effect decay", juce::dontSendNotification);
        addAndMakeVisible(decayLabel);
        decaySlider.setRange(100.0, 1400.0, 10.0);
        decaySlider.setValue(effectDecayMs, juce::dontSendNotification);
        decaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        decaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
        decaySlider.setTextValueSuffix(" ms");
        decaySlider.onValueChange = [this] { emitChange(); };
        addAndMakeVisible(decaySlider);

        doneButton.setButtonText("Done");
        doneButton.setMouseClickGrabsKeyboardFocus(false);
        doneButton.onClick = [this]
        {
            if (onClose)
                onClose();
        };
        addAndMakeVisible(doneButton);

        refreshColours();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.62f));

        const auto panel = panelBounds().toFloat();
        g.setColour(ggdThemeColour(GgdThemeRole::panelRaised));
        g.fillRoundedRectangle(panel, 14.0f);
        g.setColour(ggdThemeColour(GgdThemeRole::borderStrong).withAlpha(0.88f));
        g.drawRoundedRectangle(panel, 14.0f, 1.2f);

        auto inner = panelBounds().reduced(28, 20);
        g.setColour(ggdThemeColour(GgdThemeRole::text));
        g.setFont(juce::Font(21.0f, juce::Font::bold));
        g.drawText("Stochas GGD Settings", inner.removeFromTop(28),
                   juce::Justification::centredLeft, false);
        g.setColour(ggdThemeColour(GgdThemeRole::muted));
        g.setFont(10.5f);
        g.drawText("Editor preferences are local to this workstation. SEQ ON/OFF is saved with the project.",
                   inner.removeFromTop(18), juce::Justification::centredLeft, false);

        inner.removeFromTop(10);
        inner.removeFromBottom(44);
        const int gap = 16;
        auto left = inner.removeFromLeft((inner.getWidth() - gap) / 2);
        inner.removeFromLeft(gap);
        auto right = inner;

        drawCard(g, left.toFloat());
        drawCard(g, right.toFloat());

        auto leftText = left.reduced(18, 14);
        drawSection(g, leftText.removeFromTop(20), "APPEARANCE");
        leftText.removeFromTop(48);
        leftText.removeFromTop(10);
        drawSection(g, leftText.removeFromTop(20), "PLAYBACK & PLAYHEAD");
        leftText.removeFromTop(92);
        leftText.removeFromTop(8);
        drawSection(g, leftText.removeFromTop(20), "EDITING & AUDITION");

        auto rightText = right.reduced(18, 14);
        drawSection(g, rightText.removeFromTop(20), "PLAYED NOTE EFFECTS");
        rightText.removeFromTop(230);
        rightText.removeFromTop(10);
        g.setColour(ggdThemeColour(GgdThemeRole::borderSoft).withAlpha(0.72f));
        g.fillRect(rightText.removeFromTop(1));
        rightText.removeFromTop(10);
        g.setColour(ggdThemeColour(GgdThemeRole::muted));
        g.setFont(10.5f);
        g.drawFittedText(
            "Effects react only to notes the sequencer actually schedules. Muted and probability-skipped hits stay dark. Reduce Motion keeps the bloom and lane feedback static.",
            rightText.removeFromTop(64), juce::Justification::topLeft, 3);
    }

    void resized() override
    {
        auto inner = panelBounds().reduced(28, 20);
        inner.removeFromTop(56);
        inner.removeFromBottom(44);
        const int gap = 16;
        auto left = inner.removeFromLeft((inner.getWidth() - gap) / 2).reduced(18, 14);
        inner.removeFromLeft(gap);
        auto right = inner.reduced(18, 14);

        left.removeFromTop(20);
        auto themeRow = left.removeFromTop(42);
        themeLabel.setBounds(themeRow.removeFromLeft(92));
        themeSelector.setBounds(themeRow.reduced(0, 4));

        left.removeFromTop(16);
        left.removeFromTop(20);
        followToggle.setBounds(left.removeFromTop(30));
        smoothToggle.setBounds(left.removeFromTop(30));
        playheadGlowToggle.setBounds(left.removeFromTop(30));

        left.removeFromTop(10);
        left.removeFromTop(20);
        autoFineToggle.setBounds(left.removeFromTop(30));
        shiftHoverToggle.setBounds(left.removeFromTop(30));
        auditionToggle.setBounds(left.removeFromTop(30));

        right.removeFromTop(20);
        playedEffectsToggle.setBounds(right.removeFromTop(30));
        rippleToggle.setBounds(right.removeFromTop(30));
        laneFlashToggle.setBounds(right.removeFromTop(30));
        velocityReactiveToggle.setBounds(right.removeFromTop(30));
        reduceMotionToggle.setBounds(right.removeFromTop(30));
        right.removeFromTop(8);

        auto intensityRow = right.removeFromTop(36);
        intensityLabel.setBounds(intensityRow.removeFromLeft(104));
        intensitySlider.setBounds(intensityRow.reduced(0, 3));
        auto decayRow = right.removeFromTop(36);
        decayLabel.setBounds(decayRow.removeFromLeft(104));
        decaySlider.setBounds(decayRow.reduced(0, 3));

        auto footer = panelBounds().reduced(24, 16);
        doneButton.setBounds(footer.removeFromBottom(34).removeFromRight(96));
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!panelBounds().contains(e.getPosition()) && onClose)
            onClose();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose)
                onClose();
            return true;
        }
        return false;
    }

private:
    juce::Label themeLabel;
    juce::ComboBox themeSelector;
    juce::ToggleButton followToggle;
    juce::ToggleButton smoothToggle;
    juce::ToggleButton playheadGlowToggle;
    juce::ToggleButton autoFineToggle;
    juce::ToggleButton shiftHoverToggle;
    juce::ToggleButton auditionToggle;
    juce::ToggleButton playedEffectsToggle;
    juce::ToggleButton rippleToggle;
    juce::ToggleButton laneFlashToggle;
    juce::ToggleButton velocityReactiveToggle;
    juce::ToggleButton reduceMotionToggle;
    juce::Label intensityLabel;
    juce::Slider intensitySlider;
    juce::Label decayLabel;
    juce::Slider decaySlider;
    juce::TextButton doneButton;
    ChangeCallback onChange;
    CloseCallback onClose;

    void setupToggle(juce::ToggleButton& button,
                     const juce::String& text,
                     bool state)
    {
        button.setButtonText(text);
        button.setToggleState(state, juce::dontSendNotification);
        button.setMouseClickGrabsKeyboardFocus(false);
        button.onClick = [this] { emitChange(); };
        addAndMakeVisible(button);
    }

    void refreshColours()
    {
        const auto text = ggdThemeColour(GgdThemeRole::text);
        const auto muted = ggdThemeColour(GgdThemeRole::muted);
        themeLabel.setColour(juce::Label::textColourId, text);
        intensityLabel.setColour(juce::Label::textColourId, muted);
        decayLabel.setColour(juce::Label::textColourId, muted);
    }

    juce::Rectangle<int> panelBounds() const
    {
        const int width = juce::jmin(880, juce::jmax(760, getWidth() - 100));
        const int height = juce::jmin(590, juce::jmax(520, getHeight() - 60));
        return juce::Rectangle<int>(width, height).withCentre(getLocalBounds().getCentre());
    }

    void drawCard(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour(ggdThemeColour(GgdThemeRole::panel).withAlpha(0.76f));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(ggdThemeColour(GgdThemeRole::borderSoft).withAlpha(0.76f));
        g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
    }

    void drawSection(juce::Graphics& g,
                     juce::Rectangle<int> bounds,
                     const juce::String& title)
    {
        g.setColour(ggdThemeColour(GgdThemeRole::accentSecondary));
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText(title, bounds, juce::Justification::centredLeft, false);
    }

    void emitChange()
    {
        if (onChange)
        {
            onChange(themeSelector.getSelectedId() - 1,
                     followToggle.getToggleState(),
                     smoothToggle.getToggleState(),
                     playheadGlowToggle.getToggleState(),
                     autoFineToggle.getToggleState(),
                     shiftHoverToggle.getToggleState(),
                     auditionToggle.getToggleState(),
                     playedEffectsToggle.getToggleState(),
                     rippleToggle.getToggleState(),
                     laneFlashToggle.getToggleState(),
                     velocityReactiveToggle.getToggleState(),
                     reduceMotionToggle.getToggleState(),
                     static_cast<int>(std::lround(intensitySlider.getValue())),
                     static_cast<int>(std::lround(decaySlider.getValue())));
        }
        repaint();
    }
};

bool prefBool(juce::PropertiesFile* prefs, const char* key, bool fallback)
{
    return prefs != nullptr ? prefs->getIntValue(key, fallback ? 1 : 0) != 0 : fallback;
}

int prefInt(juce::PropertiesFile* prefs, const char* key, int fallback, int lo, int hi)
{
    return juce::jlimit(lo, hi,
        prefs != nullptr ? prefs->getIntValue(key, fallback) : fallback);
}
}

void GgdDrumEditor::initialiseV1Ui()
{
    if (v1UiInitialised)
        return;

    initialiseV1UiLegacy();

    // Clear remains available in Pattern, so the scarce top-bar slot becomes
    // the project-persistent sequencer output switch needed after MIDI drag-out.
    clearButton.setToggleable(true);
    clearButton.setClickingTogglesState(false);
    clearButton.setTooltip(
        "Enable or mute Stochas-generated notes. Incoming Bitwig MIDI always passes through.");

    auto syncOutputButton = [this]
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const bool enabled = layer != nullptr && !layer->getMuted();
        clearButton.setToggleState(enabled, juce::dontSendNotification);
        clearButton.setButtonText(enabled ? "SEQ ON" : "SEQ OFF");
    };
    syncOutputButton();

    clearButton.onClick = [this]
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        if (layer == nullptr)
            return;

        const bool enable = layer->getMuted();
        layer->setMuted(!enable);
        publishModelChange(false);

        auto* updated = processor.mData.getUISeqData()->getLayer(0);
        const bool nowEnabled = updated != nullptr && !updated->getMuted();
        clearButton.setToggleState(nowEnabled, juce::dontSendNotification);
        clearButton.setButtonText(nowEnabled ? "SEQ ON" : "SEQ OFF");
        hintLabel.setText(
            nowEnabled
                ? "Sequencer output enabled"
                : "Sequencer output muted | Bitwig/timeline MIDI still passes through",
            juce::dontSendNotification);
        if (grid)
            grid->grabKeyboardFocus();
    };

    importMidiButton.setButtonText("...");
    importMidiButton.setTooltip("Settings, playback feedback and editor preferences");
    importMidiButton.onClick = [this] { showSettingsDialogV1(); };

    productLabel.setTooltip("Stochas GGD  |  1.0.1");
    applyV1InteractionPreferences(false);
}

void GgdDrumEditor::applyV1InteractionPreferences(bool persist)
{
    applyV1InteractionPreferencesLegacy(persist);

    const bool effects = prefBool(appearanceSettings.get(), "playedHitEffects", true);
    const bool ripple = prefBool(appearanceSettings.get(), "playedHitRipple", true);
    const bool laneFlash = prefBool(appearanceSettings.get(), "playedLaneFlash", true);
    const bool velocityReactive = prefBool(
        appearanceSettings.get(), "playedVelocityReactive", true);
    const bool reduceMotion = prefBool(appearanceSettings.get(), "reduceMotion", false);
    const int intensity = prefInt(
        appearanceSettings.get(), "playedEffectIntensity", 100, 0, 150);
    const int decayMs = prefInt(
        appearanceSettings.get(), "playedEffectDecayMs", 420, 100, 1400);

    if (auto* finalGrid = dynamic_cast<GgdDrumGridFinal*>(grid.get()))
    {
        finalGrid->setPlaybackEffects(
            effects,
            ripple,
            laneFlash,
            velocityReactive,
            reduceMotion,
            static_cast<float>(intensity) / 100.0f,
            decayMs);
    }
}

void GgdDrumEditor::showSettingsDialogV1()
{
    if (settingsOverlay)
    {
        settingsOverlay->toFront(true);
        settingsOverlay->grabKeyboardFocus();
        return;
    }

    auto* prefs = appearanceSettings.get();
    const bool effects = prefBool(prefs, "playedHitEffects", true);
    const bool ripple = prefBool(prefs, "playedHitRipple", true);
    const bool laneFlash = prefBool(prefs, "playedLaneFlash", true);
    const bool velocityReactive = prefBool(prefs, "playedVelocityReactive", true);
    const bool reduceMotion = prefBool(prefs, "reduceMotion", false);
    const int intensity = prefInt(prefs, "playedEffectIntensity", 100, 0, 150);
    const int decayMs = prefInt(prefs, "playedEffectDecayMs", 420, 100, 1400);

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    auto change = [safe](int themeIndex,
                         bool follow,
                         bool smooth,
                         bool glow,
                         bool autoFine,
                         bool shiftHover,
                         bool audition,
                         bool playedEffects,
                         bool playedRipple,
                         bool playedLaneFlash,
                         bool velocityReactiveEffects,
                         bool shouldReduceMotion,
                         int effectIntensity,
                         int effectDecay)
    {
        auto* self = safe.getComponent();
        if (self == nullptr)
            return;

        const auto theme = ggdThemeFromIndex(themeIndex);
        if (theme != ggdCurrentTheme())
            self->applyBeta4Theme(theme, true);

        self->followPlayhead = follow;
        self->smoothPlayhead = smooth;
        self->playheadGlow = glow;
        self->autoFineGrid = autoFine;
        self->shiftHoverVelocityInspector = shiftHover;
        self->articulationAudition = audition;

        if (self->appearanceSettings)
        {
            auto* settings = self->appearanceSettings.get();
            settings->setValue("playedHitEffects", playedEffects ? 1 : 0);
            settings->setValue("playedHitRipple", playedRipple ? 1 : 0);
            settings->setValue("playedLaneFlash", playedLaneFlash ? 1 : 0);
            settings->setValue(
                "playedVelocityReactive", velocityReactiveEffects ? 1 : 0);
            settings->setValue("reduceMotion", shouldReduceMotion ? 1 : 0);
            settings->setValue(
                "playedEffectIntensity", juce::jlimit(0, 150, effectIntensity));
            settings->setValue(
                "playedEffectDecayMs", juce::jlimit(100, 1400, effectDecay));
            settings->saveIfNeeded();
        }

        self->applyBeta6Preferences(true);
        self->applyV1InteractionPreferences(true);

        if (self->settingsOverlay)
            self->settingsOverlay->repaint();
    };

    auto close = [safe]
    {
        juce::MessageManager::callAsync([safe]
        {
            if (auto* self = safe.getComponent())
                self->closeSettingsDialog();
        });
    };

    settingsOverlay = std::make_unique<GgdSettingsOverlayFinal>(
        ggdThemeIndex(ggdCurrentTheme()),
        followPlayhead,
        smoothPlayhead,
        playheadGlow,
        autoFineGrid,
        shiftHoverVelocityInspector,
        articulationAudition,
        effects,
        ripple,
        laneFlash,
        velocityReactive,
        reduceMotion,
        intensity,
        decayMs,
        std::move(change),
        std::move(close));

    addAndMakeVisible(*settingsOverlay);
    settingsOverlay->setBounds(getLocalBounds());
    settingsOverlay->toFront(true);
    settingsOverlay->enterModalState(true);
    settingsOverlay->grabKeyboardFocus();
}
