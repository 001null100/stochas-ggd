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

    // Reserved for a future exact-value property editor. Beta 3 keeps the fast
    // probability presets visible and avoids another tiny dropdown in the strip.
    probabilityLabel.setVisible(false);
    probabilitySelector.setVisible(false);

    updateSelectionPropertyControls();
}

void GgdDrumEditor::updateSelectionPropertyControls()
{
    if (!grid)
        return;

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
