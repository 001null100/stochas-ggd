#include "GgdDrumEditor.h"
#include "GgdDrumGridBeta.h"

namespace
{
juce::PropertiesFile::Options appearanceOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Stochas GGD Appearance";
    options.filenameSuffix = "settings";
    options.folderName = "Stochas GGD";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    return options;
}
}

void GgdDrumEditor::initialiseBeta4Ui()
{
    if (beta4UiInitialised)
        return;
    beta4UiInitialised = true;

    appearanceSettings = std::make_unique<juce::PropertiesFile>(appearanceOptions());
    const int savedTheme = appearanceSettings->getIntValue("theme", 0);
    ggdSetCurrentTheme(ggdThemeFromIndex(savedTheme));

    themeSelector.clear(juce::dontSendNotification);
    for (int i = 0; i < static_cast<int>(GgdThemeId::count); ++i)
    {
        const auto theme = ggdThemeFromIndex(i);
        themeSelector.addItem(ggdThemeName(theme), i + 1);
    }
    themeSelector.setSelectedId(ggdThemeIndex(ggdCurrentTheme()) + 1,
                                juce::dontSendNotification);
    themeSelector.setMouseClickGrabsKeyboardFocus(false);
    themeSelector.setTooltip("UI theme. All themes preserve the same bar, beat, subdivision and instrument-group hierarchy.");
    themeSelector.onChange = [this]
    {
        const int index = themeSelector.getSelectedId() - 1;
        if (index >= 0)
            applyBeta4Theme(ggdThemeFromIndex(index), true);
        if (grid)
            grid->grabKeyboardFocus();
    };
    addAndMakeVisible(themeSelector);

    productLabel.setText("STOCHAS GGD", juce::dontSendNotification);
    productLabel.setFont(juce::Font(17.5f, juce::Font::bold));
    productLabel.setTooltip("Stochas GGD  |  Beta 4");

    // Keep controls tactile without becoming visually noisy. The custom look
    // and feel handles hover/down/toggle states; these tooltips clarify the few
    // less-obvious performance actions without adding permanent labels.
    drawModeButton.setTooltip("Draw hits (D)");
    selectModeButton.setTooltip("Select and transform hits (S)");
    timingResetButton.setTooltip("Quantize selected hits to the visible grid");
    humanizeButton.setTooltip("Humanize selected timing and velocity");

    applyBeta4Theme(ggdCurrentTheme(), false);
    resized();
}

void GgdDrumEditor::applyBeta4Theme(GgdThemeId theme, bool persist)
{
    ggdSetCurrentTheme(theme);
    lookAndFeel.syncThemeColours();

    const auto text = ggdThemeColour(GgdThemeRole::text);
    const auto muted = ggdThemeColour(GgdThemeRole::muted);
    const auto accent = ggdThemeColour(GgdThemeRole::accent);
    const auto panelRaised = ggdThemeColour(GgdThemeRole::panelRaised);

    productLabel.setColour(juce::Label::textColourId, text);
    meterLabel.setColour(juce::Label::textColourId, muted);
    meterSlash.setColour(juce::Label::textColourId, muted);
    barsLabel.setColour(juce::Label::textColourId, muted);
    zoomLabel.setColour(juce::Label::textColourId, muted);
    zoomValueLabel.setColour(juce::Label::textColourId, muted);
    selectionStatusLabel.setColour(juce::Label::textColourId, muted);
    hintLabel.setColour(juce::Label::textColourId, muted);
    transportStatus.setColour(juce::Label::backgroundColourId, panelRaised);
    transportStatus.setColour(juce::Label::outlineColourId,
                              ggdThemeColour(GgdThemeRole::borderSoft));
    transportStatus.setColour(juce::Label::textColourId,
                              processor.mNotifier.getPlayPosition(0) >= 0 ? accent : muted);

    gridViewport.setColour(juce::ScrollBar::backgroundColourId,
                           ggdThemeColour(GgdThemeRole::outside));
    gridViewport.setColour(juce::ScrollBar::thumbColourId,
                           ggdThemeColour(GgdThemeRole::borderStrong));

    if (persist && appearanceSettings)
    {
        appearanceSettings->setValue("theme", ggdThemeIndex(theme));
        appearanceSettings->saveIfNeeded();
    }

    // The palette is global to the plugin UI, so a single theme switch updates
    // all paint paths without rebuilding model state or touching the scheduler.
    sendLookAndFeelChange();
    if (grid)
        grid->repaint();
    if (libraryBrowser)
        libraryBrowser->themeChanged();
    repaint();
}
