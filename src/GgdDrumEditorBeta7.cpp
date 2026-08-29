#include "GgdDrumEditor.h"
#include "GgdDrumGridBeta.h"

#include <functional>

// Keep the rest of Beta 6 intact, but retain its replaced entry points under
// compatibility names so Beta 7 can change the settings surface and follow
// policy without copying the playback/presentation implementation.
#define initialiseBeta6Ui initialiseBeta6UiLegacy
#define showSettingsDialog showSettingsDialogLegacy
#define updatePlayheadFollow updatePlayheadFollowLegacy
#include "GgdDrumEditorBeta6.cpp"
#undef updatePlayheadFollow
#undef showSettingsDialog
#undef initialiseBeta6Ui

namespace
{
class GgdSettingsOverlayBeta7 final : public juce::Component
{
public:
    using ChangeCallback = std::function<void(int, bool, bool, bool, bool)>;
    using CloseCallback = std::function<void()>;

    GgdSettingsOverlayBeta7(int themeIndex,
                            bool follow,
                            bool smooth,
                            bool glow,
                            bool autoFine,
                            ChangeCallback change,
                            CloseCallback close)
        : onChange(std::move(change)), onClose(std::move(close))
    {
        setOpaque(false);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(true);

        themeLabel.setText("Theme", juce::dontSendNotification);
        themeLabel.setColour(juce::Label::textColourId, ggdThemeColour(GgdThemeRole::text));
        addAndMakeVisible(themeLabel);

        for (int i = 0; i < static_cast<int>(GgdThemeId::count); ++i)
            themeSelector.addItem(ggdThemeName(ggdThemeFromIndex(i)), i + 1);
        themeSelector.setSelectedId(themeIndex + 1, juce::dontSendNotification);
        themeSelector.setMouseClickGrabsKeyboardFocus(false);
        themeSelector.onChange = [this] { emitChange(); };
        addAndMakeVisible(themeSelector);

        followToggle.setButtonText("Lock playhead to timeline centre while playing");
        followToggle.setToggleState(follow, juce::dontSendNotification);
        followToggle.setMouseClickGrabsKeyboardFocus(false);
        followToggle.onClick = [this] { emitChange(); };
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
        content.removeFromTop(98);
        content.removeFromTop(12);
        drawSection(g, content.removeFromTop(22), "EDITING");
    }

    void resized() override
    {
        auto area = panelBounds().reduced(28, 22);
        area.removeFromTop(42);
        area.removeFromTop(22);

        auto themeRow = area.removeFromTop(40);
        themeLabel.setBounds(themeRow.removeFromLeft(150));
        themeSelector.setBounds(themeRow.removeFromRight(250).reduced(0, 4));

        area.removeFromTop(20);
        area.removeFromTop(22);
        followToggle.setBounds(area.removeFromTop(32));
        smoothToggle.setBounds(area.removeFromTop(32));
        glowToggle.setBounds(area.removeFromTop(32));

        area.removeFromTop(22);
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
    juce::Label themeLabel;
    juce::ComboBox themeSelector;
    juce::ToggleButton followToggle;
    juce::ToggleButton smoothToggle;
    juce::ToggleButton glowToggle;
    juce::ToggleButton autoFineToggle;
    juce::TextButton doneButton;
    ChangeCallback onChange;
    CloseCallback onClose;

    juce::Rectangle<int> panelBounds() const
    {
        const int width = juce::jmin(590, juce::jmax(430, getWidth() - 80));
        const int height = juce::jmin(400, juce::jmax(350, getHeight() - 80));
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
    }

    importMidiButton.setButtonText("...");
    importMidiButton.setTooltip("Settings");
    importMidiButton.onClick = [this] { showSettingsDialog(); };

    // Restore the explicit TextEditor focus setup used by the proven Alpha UI.
    // This prevents host/editor focus handoff from intermittently turning Bars
    // into a field that looks editable but does not receive typed digits.
    barsEditor.setWantsKeyboardFocus(true);
    barsEditor.setMouseClickGrabsKeyboardFocus(true);
    barsEditor.setMultiLine(false);
    barsEditor.setReturnKeyStartsNewLine(false);
    barsEditor.setReadOnly(false);
    barsEditor.setInputRestrictions(4, "0123456789");
    barsEditor.setSelectAllWhenFocused(true);
    barsEditor.onReturnKey = [this]
    {
        commitBarCountEditor();
        barsEditor.giveAwayKeyboardFocus();
        if (grid)
            grid->grabKeyboardFocus();
    };

    productLabel.setTooltip("Stochas GGD  |  Beta 7");
    applyBeta6Preferences(false);
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

    settingsOverlay = std::make_unique<GgdSettingsOverlayBeta7>(
        ggdThemeIndex(ggdCurrentTheme()),
        followPlayhead,
        smoothPlayhead,
        playheadGlow,
        autoFineGrid,
        std::move(change),
        std::move(close));

    addAndMakeVisible(*settingsOverlay);
    settingsOverlay->setBounds(getLocalBounds());
    settingsOverlay->toFront(true);
    settingsOverlay->enterModalState(true);
    settingsOverlay->grabKeyboardFocus();
}

void GgdDrumEditor::updatePlayheadFollow(int playPosition)
{
    if (!followPlayhead || playPosition < 0 || grid == nullptr)
    {
        followScrollTargetX = -1;
        lastFollowPlayheadX = -1.0f;

        if (grid != nullptr && followBaseGridWidth >= 0)
        {
            followBaseGridWidth = -1;
            grid->refreshSize();
        }
        return;
    }

    const float playX = grid->getPlayheadContentX();
    if (playX < 0.0f)
        return;

    const int viewWidth = gridViewport.getViewWidth();
    const int inset = grid->getTimelineInsetPixels();
    const int timelineWidth = juce::jmax(1, viewWidth - inset);
    const int centreOffset = inset + timelineWidth / 2;

    if (followBaseGridWidth < 0)
        followBaseGridWidth = grid->getWidth();

    // Give the content a temporary trailing runway while playback is active so
    // the final half-screen of a pattern can remain genuinely centred instead
    // of being forced against the viewport's right boundary.
    const int paddedWidth = followBaseGridWidth + centreOffset + 12;
    if (grid->getWidth() < paddedWidth)
        grid->setSize(paddedWidth, grid->getHeight());

    const int maxViewX = juce::jmax(0, grid->getWidth() - viewWidth);
    const int target = juce::jlimit(
        0, maxViewX,
        static_cast<int>(std::lround(playX)) - centreOffset);

    followScrollTargetX = target;
    lastFollowPlayheadX = playX;

    // Centre-lock is intentional rather than an eased chase. At the pattern
    // start the viewport naturally clamps at zero; once enough timeline exists
    // to the left, the playhead remains fixed at the visual centre.
    gridViewport.setViewPosition(target, gridViewport.getViewPositionY());
}
