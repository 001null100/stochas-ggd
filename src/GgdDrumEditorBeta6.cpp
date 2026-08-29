#include "GgdDrumEditor.h"
#include "GgdDrumGridBeta.h"

#include <cmath>
#include <functional>

namespace
{
class GgdSettingsOverlay final : public juce::Component
{
public:
    using ChangeCallback = std::function<void(int, bool, bool, bool, int, bool)>;
    using CloseCallback = std::function<void()>;

    GgdSettingsOverlay(int themeIndex,
                       bool follow,
                       bool smooth,
                       bool glow,
                       int marginPercent,
                       bool autoFine,
                       ChangeCallback change,
                       CloseCallback close)
        : onChange(std::move(change)), onClose(std::move(close))
    {
        setOpaque(false);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(true);

        for (int i = 0; i < static_cast<int>(GgdThemeId::count); ++i)
            themeSelector.addItem(ggdThemeName(ggdThemeFromIndex(i)), i + 1);
        themeSelector.setSelectedId(themeIndex + 1, juce::dontSendNotification);
        themeSelector.setMouseClickGrabsKeyboardFocus(false);
        themeSelector.onChange = [this] { emitChange(); };
        addAndMakeVisible(themeSelector);

        followToggle.setButtonText("Follow playhead while playing");
        followToggle.setToggleState(follow, juce::dontSendNotification);
        followToggle.setMouseClickGrabsKeyboardFocus(false);
        followToggle.onClick = [this]
        {
            marginSlider.setEnabled(followToggle.getToggleState());
            emitChange();
        };
        addAndMakeVisible(followToggle);

        smoothToggle.setButtonText("Smooth playhead interpolation");
        smoothToggle.setToggleState(smooth, juce::dontSendNotification);
        smoothToggle.setMouseClickGrabsKeyboardFocus(false);
        smoothToggle.onClick = [this] { emitChange(); };
        addAndMakeVisible(smoothToggle);

        glowToggle.setButtonText("Playhead forward glow");
        glowToggle.setToggleState(glow, juce::dontSendNotification);
        glowToggle.setMouseClickGrabsKeyboardFocus(false);
        glowToggle.onClick = [this] { emitChange(); };
        addAndMakeVisible(glowToggle);

        marginSlider.setRange(0.0, 30.0, 1.0);
        marginSlider.setValue(marginPercent, juce::dontSendNotification);
        marginSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        marginSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 24);
        marginSlider.setTextValueSuffix(" %");
        marginSlider.setEnabled(follow);
        marginSlider.onValueChange = [this] { emitChange(); };
        addAndMakeVisible(marginSlider);

        autoFineToggle.setButtonText("Automatically use finer grid at high zoom");
        autoFineToggle.setToggleState(autoFine, juce::dontSendNotification);
        autoFineToggle.setMouseClickGrabsKeyboardFocus(false);
        autoFineToggle.onClick = [this] { emitChange(); };
        addAndMakeVisible(autoFineToggle);

        doneButton.setButtonText("Done");
        doneButton.setMouseClickGrabsKeyboardFocus(false);
        doneButton.onClick = [this]
        {
            if (onClose)
                onClose();
        };
        addAndMakeVisible(doneButton);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.58f));

        const auto panel = panelBounds().toFloat();
        g.setColour(ggdThemeColour(GgdThemeRole::panelRaised));
        g.fillRoundedRectangle(panel, 12.0f);
        g.setColour(ggdThemeColour(GgdThemeRole::borderStrong).withAlpha(0.82f));
        g.drawRoundedRectangle(panel, 12.0f, 1.2f);

        auto content = panelBounds().reduced(28, 22);
        g.setColour(ggdThemeColour(GgdThemeRole::text));
        g.setFont(juce::Font(20.0f, juce::Font::bold));
        g.drawText("Settings", content.removeFromTop(30),
                   juce::Justification::centredLeft, false);
        content.removeFromTop(12);

        drawSection(g, content.removeFromTop(22), "APPEARANCE");
        content.removeFromTop(54);
        content.removeFromTop(10);
        drawSection(g, content.removeFromTop(22), "PLAYBACK & PLAYHEAD");
        content.removeFromTop(151);
        content.removeFromTop(10);
        drawSection(g, content.removeFromTop(22), "EDITING");
    }

    void resized() override
    {
        auto area = panelBounds().reduced(28, 22);
        area.removeFromTop(42);
        area.removeFromTop(22);

        auto themeRow = area.removeFromTop(40);
        themeSelector.setBounds(themeRow.removeFromRight(250).reduced(0, 4));
        area.removeFromTop(20);
        area.removeFromTop(22);

        followToggle.setBounds(area.removeFromTop(31));
        smoothToggle.setBounds(area.removeFromTop(31));
        glowToggle.setBounds(area.removeFromTop(31));

        auto marginRow = area.removeFromTop(40);
        marginSlider.setBounds(marginRow.removeFromRight(280).reduced(0, 5));

        area.removeFromTop(20);
        area.removeFromTop(22);
        autoFineToggle.setBounds(area.removeFromTop(32));

        auto footer = panelBounds().reduced(24, 18);
        doneButton.setBounds(footer.removeFromBottom(34).removeFromRight(92));
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
    juce::ComboBox themeSelector;
    juce::ToggleButton followToggle;
    juce::ToggleButton smoothToggle;
    juce::ToggleButton glowToggle;
    juce::Slider marginSlider;
    juce::ToggleButton autoFineToggle;
    juce::TextButton doneButton;
    ChangeCallback onChange;
    CloseCallback onClose;

    juce::Rectangle<int> panelBounds() const
    {
        const int width = juce::jmin(580, juce::jmax(420, getWidth() - 80));
        const int height = juce::jmin(455, juce::jmax(390, getHeight() - 80));
        return juce::Rectangle<int>(width, height).withCentre(getLocalBounds().getCentre());
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
                     glowToggle.getToggleState(),
                     static_cast<int>(std::lround(marginSlider.getValue())),
                     autoFineToggle.getToggleState());
        }
        repaint();
    }
};
}

void GgdDrumEditor::initialiseBeta6Ui()
{
    if (beta6UiInitialised)
        return;
    beta6UiInitialised = true;

    if (appearanceSettings)
    {
        followPlayhead = appearanceSettings->getIntValue("followPlayhead", 1) != 0;
        smoothPlayhead = appearanceSettings->getIntValue("smoothPlayhead", 1) != 0;
        playheadGlow = appearanceSettings->getIntValue("playheadGlow", 1) != 0;
        autoFineGrid = appearanceSettings->getIntValue("autoFineGrid", 1) != 0;
        followMarginPercent = juce::jlimit(
            0, 30, appearanceSettings->getIntValue("followMarginPercent", 8));
    }

    importMidiButton.setButtonText("...");
    importMidiButton.setTooltip("Settings");
    importMidiButton.onClick = [this] { showSettingsDialog(); };

    productLabel.setTooltip("Stochas GGD  |  Beta 6");
    applyBeta6Preferences(false);
}

void GgdDrumEditor::applyBeta6Preferences(bool persist)
{
    followMarginPercent = juce::jlimit(0, 30, followMarginPercent);

    if (grid)
        grid->setPlayheadPresentation(smoothPlayhead, playheadGlow);

    // The original 60 Hz timer is adequate for normal editor housekeeping.
    // Smooth playhead mode doubles the visual cadence; JUCE/OS repaint
    // coalescing still caps actual draws to the display where appropriate.
    startTimerHz(smoothPlayhead ? 120 : 60);

    if (!followPlayhead)
    {
        followScrollTargetX = -1;
        lastFollowPlayheadX = -1.0f;
    }

    updateGridResolutionForZoom(grid ? grid->getZoomScale() : 1.25f);

    if (persist && appearanceSettings)
    {
        appearanceSettings->setValue("followPlayhead", followPlayhead ? 1 : 0);
        appearanceSettings->setValue("smoothPlayhead", smoothPlayhead ? 1 : 0);
        appearanceSettings->setValue("playheadGlow", playheadGlow ? 1 : 0);
        appearanceSettings->setValue("autoFineGrid", autoFineGrid ? 1 : 0);
        appearanceSettings->setValue("followMarginPercent", followMarginPercent);
        appearanceSettings->saveIfNeeded();
    }

    repaint();
}

void GgdDrumEditor::showSettingsDialog()
{
    if (settingsOverlay)
    {
        settingsOverlay->toFront(true);
        settingsOverlay->grabKeyboardFocus();
        return;
    }

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    auto change = [safe](int themeIndex,
                         bool follow,
                         bool smooth,
                         bool glow,
                         int margin,
                         bool autoFine)
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
        self->followMarginPercent = margin;
        self->autoFineGrid = autoFine;
        self->applyBeta6Preferences(true);

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

    settingsOverlay = std::make_unique<GgdSettingsOverlay>(
        ggdThemeIndex(ggdCurrentTheme()),
        followPlayhead,
        smoothPlayhead,
        playheadGlow,
        followMarginPercent,
        autoFineGrid,
        std::move(change),
        std::move(close));

    addAndMakeVisible(*settingsOverlay);
    settingsOverlay->setBounds(getLocalBounds());
    settingsOverlay->toFront(true);
    settingsOverlay->enterModalState(true);
    settingsOverlay->grabKeyboardFocus();
}

void GgdDrumEditor::closeSettingsDialog()
{
    if (!settingsOverlay)
        return;

    if (settingsOverlay->isCurrentlyModal())
        settingsOverlay->exitModalState(0);
    settingsOverlay.reset();

    if (grid)
        grid->grabKeyboardFocus();
}

void GgdDrumEditor::updatePlayheadFollow(int playPosition)
{
    if (!followPlayhead || playPosition < 0 || grid == nullptr)
    {
        followScrollTargetX = -1;
        lastFollowPlayheadX = -1.0f;
        return;
    }

    const float playX = grid->getPlayheadContentX();
    if (playX < 0.0f)
        return;

    const int viewX = gridViewport.getViewPositionX();
    const int viewWidth = gridViewport.getViewWidth();
    const int inset = grid->getTimelineInsetPixels();
    const int timelineWidth = juce::jmax(1, viewWidth - inset);
    const int margin = static_cast<int>(std::round(
        timelineWidth * static_cast<float>(followMarginPercent) / 100.0f));

    const float visibleLeft = static_cast<float>(viewX + inset);
    const float visibleRight = static_cast<float>(viewX + viewWidth);
    const float leftTrigger = visibleLeft + margin;
    const float rightTrigger = visibleRight - margin;

    const bool wrapped = lastFollowPlayheadX >= 0.0f
                      && playX + static_cast<float>(timelineWidth) * 0.35f < lastFollowPlayheadX;

    if (wrapped)
    {
        followScrollTargetX = 0;
    }
    else if (playX > rightTrigger)
    {
        // Once the playhead reaches the right follow edge, move the viewport
        // continuously so the cursor rides that edge instead of page-jumping.
        followScrollTargetX = static_cast<int>(std::round(
            playX - static_cast<float>(viewWidth - margin)));
    }
    else if (playX < leftTrigger && viewX > 0)
    {
        followScrollTargetX = static_cast<int>(std::round(
            playX - static_cast<float>(inset + margin)));
    }

    lastFollowPlayheadX = playX;

    if (followScrollTargetX < 0)
        return;

    const int maxViewX = juce::jmax(0, grid->getWidth() - viewWidth);
    followScrollTargetX = juce::jlimit(0, maxViewX, followScrollTargetX);

    const int current = gridViewport.getViewPositionX();
    const int difference = followScrollTargetX - current;
    if (std::abs(difference) <= 1)
    {
        gridViewport.setViewPosition(followScrollTargetX,
                                     gridViewport.getViewPositionY());
        followScrollTargetX = -1;
        return;
    }

    const float response = wrapped ? 0.50f : 0.28f;
    int next = current + static_cast<int>(std::round(difference * response));
    if (next == current)
        next += difference > 0 ? 1 : -1;

    gridViewport.setViewPosition(
        juce::jlimit(0, maxViewX, next),
        gridViewport.getViewPositionY());
}
