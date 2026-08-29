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
    themeSelector.setVisible(false);

    // The standalone Import MIDI button duplicated the Grooves browser. Reuse
    // that stable control slot as a compact settings menu instead of adding more
    // permanent chrome to the top bar.
    importMidiButton.setButtonText("...");
    importMidiButton.setMouseClickGrabsKeyboardFocus(false);
    importMidiButton.setTooltip("Settings");
    importMidiButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addSectionHeader("Theme");
        for (int i = 0; i < static_cast<int>(GgdThemeId::count); ++i)
        {
            const auto theme = ggdThemeFromIndex(i);
            menu.addItem(100 + i, ggdThemeName(theme), true,
                         theme == ggdCurrentTheme());
        }

        auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(importMidiButton),
            [safe](int result)
            {
                if (auto* self = safe.getComponent())
                {
                    if (result >= 100 && result < 100 + static_cast<int>(GgdThemeId::count))
                        self->applyBeta4Theme(ggdThemeFromIndex(result - 100), true);
                    if (self->grid)
                        self->grid->grabKeyboardFocus();
                }
            });
    };

    // One click selects the entire bar count. Typing immediately replaces the
    // old value instead of requiring a manual Ctrl+A or backspace first.
    barsEditor.setSelectAllWhenFocused(true);

    productLabel.setText("STOCHAS GGD", juce::dontSendNotification);
    productLabel.setFont(juce::Font(17.5f, juce::Font::bold));
    productLabel.setTooltip("Stochas GGD  |  Beta 5");

    setResizeLimits(1400, 640, 2200, 1500);
    if (getWidth() < 1400)
        setSize(juce::jmax(1480, getWidth()), juce::jmax(820, getHeight()));

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

    sendLookAndFeelChange();
    if (grid)
        grid->repaint();
    if (libraryBrowser)
        libraryBrowser->themeChanged();
    repaint();
}