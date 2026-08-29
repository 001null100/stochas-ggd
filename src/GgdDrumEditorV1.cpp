#include "GgdDrumEditor.h"
#include "GgdDrumGridV1.h"

#include <functional>
#include <utility>

namespace
{
class GgdSettingsOverlayV1 final : public juce::Component
{
public:
    using ChangeCallback = std::function<void(int, bool, bool, bool, bool, bool, bool)>;
    using CloseCallback = std::function<void()>;

    GgdSettingsOverlayV1(int themeIndex,
                         bool follow,
                         bool smooth,
                         bool glow,
                         bool autoFine,
                         bool shiftHoverVelocity,
                         bool articulationAudition,
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

        followToggle.setButtonText("Follow playhead when timeline exceeds the visible canvas");
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

        shiftHoverToggle.setButtonText("Hold Shift over a hit to inspect velocity");
        shiftHoverToggle.setToggleState(shiftHoverVelocity, juce::dontSendNotification);
        shiftHoverToggle.setMouseClickGrabsKeyboardFocus(false);
        shiftHoverToggle.onClick = [this] { emitChange(); };
        addAndMakeVisible(shiftHoverToggle);

        auditionToggle.setButtonText("Click articulation names to audition the mapped note");
        auditionToggle.setToggleState(articulationAudition, juce::dontSendNotification);
        auditionToggle.setMouseClickGrabsKeyboardFocus(false);
        auditionToggle.onClick = [this] { emitChange(); };
        addAndMakeVisible(auditionToggle);

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
        content.removeFromTop(50);
        content.removeFromTop(10);
        drawSection(g, content.removeFromTop(22), "PLAYBACK & PLAYHEAD");
        content.removeFromTop(98);
        content.removeFromTop(12);
        drawSection(g, content.removeFromTop(22), "EDITING & AUDITION");
    }

    void resized() override
    {
        auto area = panelBounds().reduced(28, 22);
        area.removeFromTop(42);
        area.removeFromTop(22);

        auto themeRow = area.removeFromTop(40);
        themeLabel.setBounds(themeRow.removeFromLeft(150));
        themeSelector.setBounds(themeRow.removeFromRight(260).reduced(0, 4));

        area.removeFromTop(20);
        area.removeFromTop(22);
        followToggle.setBounds(area.removeFromTop(32));
        smoothToggle.setBounds(area.removeFromTop(32));
        glowToggle.setBounds(area.removeFromTop(32));

        area.removeFromTop(22);
        area.removeFromTop(22);
        autoFineToggle.setBounds(area.removeFromTop(32));
        shiftHoverToggle.setBounds(area.removeFromTop(32));
        auditionToggle.setBounds(area.removeFromTop(32));

        auto footer = panelBounds().reduced(24, 18);
        doneButton.setBounds(footer.removeFromBottom(34).removeFromRight(92));
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
    juce::ToggleButton glowToggle;
    juce::ToggleButton autoFineToggle;
    juce::ToggleButton shiftHoverToggle;
    juce::ToggleButton auditionToggle;
    juce::TextButton doneButton;
    ChangeCallback onChange;
    CloseCallback onClose;

    juce::Rectangle<int> panelBounds() const
    {
        const int width = juce::jmin(640, juce::jmax(470, getWidth() - 80));
        const int height = juce::jmin(500, juce::jmax(430, getHeight() - 80));
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
                     autoFineToggle.getToggleState(),
                     shiftHoverToggle.getToggleState(),
                     auditionToggle.getToggleState());
        }
        repaint();
    }
};
}

void GgdDrumEditor::initialiseV1Ui()
{
    if (v1UiInitialised)
        return;
    v1UiInitialised = true;

    if (appearanceSettings)
    {
        shiftHoverVelocityInspector =
            appearanceSettings->getIntValue("shiftHoverVelocityInspector", 1) != 0;
        articulationAudition =
            appearanceSettings->getIntValue("articulationAudition", 1) != 0;
    }

    if (grid)
    {
        grid->setInteractionPreferences(shiftHoverVelocityInspector, articulationAudition);
        grid->setSelectionCallback([this] { handleGridSelectionChangedV1(); });
        grid->setUndoRedoCallbacks([this] { performUndoV1(); },
                                   [this] { performRedoV1(); });
        if (auto* v1 = dynamic_cast<GgdDrumGridV1*>(grid.get()))
            v1->refreshActiveMapLayout();
    }

    undoButton.onClick = [this] { performUndoV1(); };
    redoButton.onClick = [this] { performRedoV1(); };

    importMidiButton.setButtonText("...");
    importMidiButton.setTooltip("Settings");
    importMidiButton.onClick = [this] { showSettingsDialogV1(); };

    exportMidiButton.setButtonText("Drag MIDI");
    exportMidiButton.setTooltip(
        "Drag the current pattern directly into Bitwig or another target that accepts MIDI files");
    exportMidiButton.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    exportMidiButton.onClick = [this]
    {
        hintLabel.setText("Drag MIDI into Bitwig | Pattern menu keeps file export as fallback",
                          juce::dontSendNotification);
    };
    exportMidiButton.onBeginDrag = [this](juce::Component* source)
    {
        beginMidiDragV1(source);
    };

    patternActionsButton.onClick = [this] { showPatternActionsV1(); };

    kitSelector.onChange = [this]
    {
        if (kitSelector.getSelectedId() <= 0)
            return;
        setActiveMap(kitSelector.getSelectedId() - 1);
        if (auto* v1 = dynamic_cast<GgdDrumGridV1*>(grid.get()))
            v1->refreshActiveMapLayout();
    };

    productLabel.setTooltip("Stochas GGD  |  1.0 RC1");
    rememberCurrentSelectionV1();
    applyV1InteractionPreferences(false);
}

void GgdDrumEditor::applyV1InteractionPreferences(bool persist)
{
    if (grid)
        grid->setInteractionPreferences(shiftHoverVelocityInspector, articulationAudition);

    if (persist && appearanceSettings)
    {
        appearanceSettings->setValue(
            "shiftHoverVelocityInspector", shiftHoverVelocityInspector ? 1 : 0);
        appearanceSettings->setValue(
            "articulationAudition", articulationAudition ? 1 : 0);
        appearanceSettings->saveIfNeeded();
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

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    auto change = [safe](int themeIndex,
                         bool follow,
                         bool smooth,
                         bool glow,
                         bool autoFine,
                         bool shiftHover,
                         bool audition)
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

    settingsOverlay = std::make_unique<GgdSettingsOverlayV1>(
        ggdThemeIndex(ggdCurrentTheme()),
        followPlayhead,
        smoothPlayhead,
        playheadGlow,
        autoFineGrid,
        shiftHoverVelocityInspector,
        articulationAudition,
        std::move(change),
        std::move(close));

    addAndMakeVisible(*settingsOverlay);
    settingsOverlay->setBounds(getLocalBounds());
    settingsOverlay->toFront(true);
    settingsOverlay->enterModalState(true);
    settingsOverlay->grabKeyboardFocus();
}

void GgdDrumEditor::rememberCurrentSelectionV1()
{
    if (!grid)
        return;

    const auto fingerprint = GgdPatternFile::fingerprint(captureCurrentPattern());
    selectionByFingerprint[fingerprint] = grid->getSelectionCoordinates();

    // Selection history only needs to cover the editor's short undo window.
    // Avoid letting long sessions turn this convenience map into permanent baggage.
    if (selectionByFingerprint.size() > maxHistoryDepth * 4)
    {
        const auto current = selectionByFingerprint[fingerprint];
        selectionByFingerprint.clear();
        selectionByFingerprint[fingerprint] = current;
    }
}

void GgdDrumEditor::handleGridSelectionChangedV1()
{
    if (!restoringHistory)
        rememberCurrentSelectionV1();
    updateContextStrip();
}

void GgdDrumEditor::performUndoV1()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (undoHistory.empty() || now - lastUndoMs < 45.0)
        return;

    std::vector<std::pair<int, int>> currentSelection;
    if (grid)
        currentSelection = grid->getSelectionCoordinates();

    const auto currentFingerprint = GgdPatternFile::fingerprint(captureCurrentPattern());
    selectionByFingerprint[currentFingerprint] = currentSelection;

    const auto targetFingerprint = GgdPatternFile::fingerprint(undoHistory.back());
    auto targetSelection = currentSelection;
    if (const auto found = selectionByFingerprint.find(targetFingerprint);
        found != selectionByFingerprint.end())
        targetSelection = found->second;

    performUndo();

    if (grid)
        grid->restoreSelectionCoordinates(targetSelection);
    rememberCurrentSelectionV1();
    updateSelectionPropertyControls();
}

void GgdDrumEditor::performRedoV1()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (redoHistory.empty() || now - lastRedoMs < 45.0)
        return;

    std::vector<std::pair<int, int>> currentSelection;
    if (grid)
        currentSelection = grid->getSelectionCoordinates();

    const auto currentFingerprint = GgdPatternFile::fingerprint(captureCurrentPattern());
    selectionByFingerprint[currentFingerprint] = currentSelection;

    const auto targetFingerprint = GgdPatternFile::fingerprint(redoHistory.back());
    auto targetSelection = currentSelection;
    if (const auto found = selectionByFingerprint.find(targetFingerprint);
        found != selectionByFingerprint.end())
        targetSelection = found->second;

    performRedo();

    if (grid)
        grid->restoreSelectionCoordinates(targetSelection);
    rememberCurrentSelectionV1();
    updateSelectionPropertyControls();
}

void GgdDrumEditor::beginMidiDragV1(juce::Component* source)
{
    if (source == nullptr || maps.isEmpty())
        return;

    const auto snapshot = captureCurrentPattern();
    const auto activeMap = maps.getReference(activeMapIndex);

    auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("Stochas GGD MIDI Drag");
    const auto directoryResult = tempRoot.createDirectory();
    if (directoryResult.failed())
    {
        hintLabel.setText("Could not create temporary MIDI drag folder",
                          juce::dontSendNotification);
        return;
    }

    if (midiDragTempFile.existsAsFile())
        midiDragTempFile.deleteFile();

    const auto baseName = juce::File::createLegalFileName(
        snapshot.name.isNotEmpty() && snapshot.name != SEQ_DEFAULT_PAT_NAME
            ? snapshot.name : juce::String("Stochas GGD Pattern"));
    const auto uniqueName = baseName + "-"
        + juce::String(juce::Time::getMillisecondCounter()) + ".mid";
    const auto tempFile = tempRoot.getChildFile(uniqueName);

    const auto result = GgdMidiExporter::writeFile(tempFile, snapshot, activeMap);
    if (!result.ok)
    {
        hintLabel.setText("MIDI drag failed: " + result.error,
                          juce::dontSendNotification);
        tempFile.deleteFile();
        return;
    }

    midiDragTempFile = tempFile;
    juce::StringArray files;
    files.add(tempFile.getFullPathName());

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles(
        files, false, source,
        [safe, tempFile]
        {
            tempFile.deleteFile();
            if (auto* self = safe.getComponent())
                self->midiDragTempFile = juce::File();
        });

    if (!started)
    {
        tempFile.deleteFile();
        midiDragTempFile = juce::File();
        hintLabel.setText(
            "Native MIDI drag could not start | use Pattern > Export MIDI file as fallback",
            juce::dontSendNotification);
        return;
    }

    hintLabel.setText("Drop the MIDI clip into Bitwig",
                      juce::dontSendNotification);
}

void GgdDrumEditor::showPatternActionsV1()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Duplicate to empty slot");
    menu.addItem(2, "Save to pattern library");
    menu.addItem(3, "Export MIDI file...");
    menu.addSeparator();
    menu.addItem(4, "Clear current pattern");

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(patternActionsButton),
        [safe](int result)
        {
            if (auto* self = safe.getComponent())
            {
                if (result == 1) self->duplicateCurrentPatternSlot();
                else if (result == 2) self->saveCurrentPatternToLibrary();
                else if (result == 3) self->exportCurrentPatternMidi();
                else if (result == 4) self->clearCurrentPattern();
            }
        });
}
