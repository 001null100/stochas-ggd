#include "GgdDrumEditor.h"
#include "GgdDrumGridBeta.h"

void GgdDrumEditor::initialiseBeta3Ui()
{
    if (beta3UiInitialised)
        return;
    beta3UiInitialised = true;

    exportMidiButton.setMouseClickGrabsKeyboardFocus(false);
    exportMidiButton.setTooltip("Export the current pattern as high-resolution MIDI using the active GGD kit map");
    exportMidiButton.onClick = [this] { exportCurrentPatternMidi(); };
    addAndMakeVisible(exportMidiButton);

    auto prep = [this](juce::TextButton& button, const juce::String& tooltip)
    {
        button.setMouseClickGrabsKeyboardFocus(false);
        button.setClickingTogglesState(false);
        button.setTooltip(tooltip);
        addAndMakeVisible(button);
    };

    prep(ghostButton, "Set selected hits to ghost-note velocity (35)");
    prep(accentButton, "Set selected hits to accent velocity (120)");
    prep(probability25Button, "Set selected hit probability to 25%");
    prep(probability50Button, "Set selected hit probability to 50%");
    prep(probability75Button, "Set selected hit probability to 75%");
    prep(probability100Button, "Set selected hit probability to 100%");
    prep(flamButton, "Create a quieter 1/64 grace hit before each selected hit");
    prep(doubleButton, "Create an independent 1/32 follow-up hit after each selected hit");

    ghostButton.onClick = [this] { if (grid) grid->setSelectedVelocity(35); };
    accentButton.onClick = [this] { if (grid) grid->setSelectedVelocity(120); };
    probability25Button.onClick = [this] { if (grid) grid->setSelectedProbability(25); };
    probability50Button.onClick = [this] { if (grid) grid->setSelectedProbability(50); };
    probability75Button.onClick = [this] { if (grid) grid->setSelectedProbability(75); };
    probability100Button.onClick = [this] { if (grid) grid->setSelectedProbability(100); };
    flamButton.onClick = [this] { if (grid) grid->createFlamFromSelection(); };
    doubleButton.onClick = [this] { if (grid) grid->createDoubleFromSelection(); };

    // Copy/Paste already forms a natural chained phrase-repeat workflow: copy
    // once, paste after the current selection, and each new copy remains
    // selected for the next V. Keep permanent buttons for transforms that are
    // genuinely slower to reproduce through direct editing or shortcuts.
    auto transform = [](juce::TextButton& button,
                        const juce::String& text,
                        const juce::String& tooltip)
    {
        button.setButtonText(text);
        button.setTooltip(tooltip);
        button.setMouseClickGrabsKeyboardFocus(false);
    };

    transform(selectAllButton, "Rows",
              "Select every hit on the instrument rows represented by the current selection");
    transform(copyButton, "Fill",
              "Fill missing current-grid subdivisions between selected endpoints on each row");
    transform(pasteButton, "Mirror",
              "Mirror selected hit timing around the selection's temporal centre");
    transform(velocityDownButton, "Ramp+",
              "Create a rising velocity ramp across the selected hits");
    transform(velocityUpButton, "Ramp-",
              "Create a falling velocity ramp across the selected hits");
    transform(timingEarlierButton, "Dyn-",
              "Compress velocity differences around the selection's average velocity");
    transform(timingResetButton, "Thin",
              "Remove every second selected hit on each instrument row");
    transform(timingLaterButton, "Dyn+",
              "Expand velocity differences around the selection's average velocity");

    selectAllButton.onClick = [this] { if (grid) grid->selectRowsContainingSelection(); };
    copyButton.onClick = [this] { if (grid) grid->fillSelectionGapsSelectNew(); };
    pasteButton.onClick = [this] { if (grid) grid->mirrorSelectedTiming(); };
    velocityDownButton.onClick = [this] { if (grid) grid->rampSelectedVelocity(true); };
    velocityUpButton.onClick = [this] { if (grid) grid->rampSelectedVelocity(false); };
    timingEarlierButton.onClick = [this] { if (grid) grid->scaleSelectedVelocityRange(0.65f); };
    timingResetButton.onClick = [this] { if (grid) grid->thinSelection(); };
    timingLaterButton.onClick = [this] { if (grid) grid->scaleSelectedVelocityRange(1.45f); };

    probabilityLabel.setVisible(false);
    probabilitySelector.setVisible(false);

    updateSelectionPropertyControls();
}

void GgdDrumEditor::updateSelectionPropertyControls()
{
    if (!grid)
        return;

    if (auto* layer = processor.mData.getUISeqData()->getLayer(0))
    {
        const int pattern = layer->getCurrentPattern();
        const auto name = juce::String(layer->getPatternName(pattern));
        const auto caption = name.isNotEmpty() && name != SEQ_DEFAULT_PAT_NAME
            ? juce::String(pattern + 1) + "  " + name
            : "Pattern " + juce::String(pattern + 1);
        if (patternSelector.getText() != caption)
        {
            patternSelector.changeItemText(pattern + 1, caption);
            patternSelector.setSelectedId(pattern + 1, juce::dontSendNotification);
        }
    }

    const bool selectMode = grid->getToolMode() == GgdDrumGrid::ToolMode::select;
    const int selected = grid->getSelectedCount();
    const bool active = selectMode && selected > 0;

    ghostButton.setVisible(active);
    accentButton.setVisible(active);
    probability25Button.setVisible(active);
    probability50Button.setVisible(active);
    probability75Button.setVisible(active);
    probability100Button.setVisible(active);
    flamButton.setVisible(active);
    doubleButton.setVisible(active);

    // updateContextStrip() runs immediately before this function in the editor
    // timer and still knows these components by their historic roles. Make the
    // current transform strip authoritative after that compatibility pass.
    selectAllButton.setVisible(active);
    copyButton.setVisible(active);
    pasteButton.setVisible(active);
    velocityDownButton.setVisible(active);
    velocityUpButton.setVisible(active);
    timingEarlierButton.setVisible(active);
    timingResetButton.setVisible(active);
    timingLaterButton.setVisible(active);

    const int probability = active ? grid->getSelectedProbability() : -1;
    probability25Button.setToggleState(probability == 25, juce::dontSendNotification);
    probability50Button.setToggleState(probability == 50, juce::dontSendNotification);
    probability75Button.setToggleState(probability == 75, juce::dontSendNotification);
    probability100Button.setToggleState(probability == 100, juce::dontSendNotification);

    if (active)
    {
        juce::String status = juce::String(selected) + (selected == 1 ? " hit" : " hits");
        status += probability >= 0 ? "  P" + juce::String(probability) : "  Pmix";
        selectionStatusLabel.setText(status, juce::dontSendNotification);
        hintLabel.setText(
            "Shift hover velocity | Shift-drag edit | Alt-drag timing | C copy, V chained paste",
            juce::dontSendNotification);
    }
    else if (selectMode)
    {
        hintLabel.setText(
            "Shift-click add/remove | Shift-hover velocity | A all | C copy | V paste | arrows move",
            juce::dontSendNotification);
    }
}

void GgdDrumEditor::exportCurrentPatternMidi()
{
    if (maps.isEmpty())
        return;

    const auto snapshot = captureCurrentPattern();
    const auto activeMap = maps.getReference(activeMapIndex);

    juce::File root = libraryBrowser ? libraryBrowser->getPatternRoot() : juce::File();
    if (!root.isDirectory())
        root = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    const auto baseName = juce::File::createLegalFileName(
        snapshot.name.isNotEmpty() && snapshot.name != SEQ_DEFAULT_PAT_NAME
            ? snapshot.name : juce::String("Stochas GGD Pattern"));
    const auto suggested = root.getChildFile(baseName).withFileExtension(".mid");

    midiExportChooser = std::make_unique<juce::FileChooser>(
        "Export Stochas GGD pattern as MIDI", suggested, "*.mid;*.midi");

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    midiExportChooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe, snapshot, activeMap](const juce::FileChooser& chooser)
        {
            auto* self = safe.getComponent();
            if (self == nullptr)
                return;

            const auto file = chooser.getResult();
            if (file == juce::File())
                return;

            const auto result = GgdMidiExporter::writeFile(file, snapshot, activeMap);
            if (!result.ok)
            {
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::WarningIcon)
                        .withTitle("MIDI export failed")
                        .withMessage(result.error)
                        .withButton("OK"), nullptr);
                return;
            }

            self->hintLabel.setText(
                result.summary() + " | probability stays in the Stochas GGD pattern",
                juce::dontSendNotification);
        });
}
