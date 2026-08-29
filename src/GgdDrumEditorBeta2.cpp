#include "GgdDrumEditor.h"
#include "GgdDrumGridBeta.h"

// Beta 1 already contains the event-aware editor behavior we want to retain.
// Compile it unchanged under private compatibility entry points for the shell
// methods Beta 2 restores to the Alpha 10 behavior.
#define paint paintLegacy
#define resized resizedLegacy
#define keyPressed keyPressedLegacy
#define timerCallback timerCallbackLegacy
#include "GgdDrumEditorBeta1.cpp"
#undef timerCallback
#undef keyPressed
#undef resized
#undef paint

namespace
{
constexpr float defaultZoomScale = 1.25f;
constexpr float detail32ZoomThreshold = 3.5f;

juce::Colour ui(GgdThemeRole role)
{
    return ggdThemeColour(role);
}
}

void GgdDrumEditor::initialiseBeta2Ui()
{
    if (beta2UiInitialised)
        return;
    beta2UiInitialised = true;

    // Beta 1 accidentally exposed the engine transition as a UI rewrite. Put
    // the established Alpha 10 shell back and leave the event engine underneath.
    productLabel.setText("STOCHAS GGD", juce::dontSendNotification);
    productLabel.setFont(juce::Font(17.0f, juce::Font::bold));

    transportStatus.setText("READY", juce::dontSendNotification);
    transportStatus.setFont(juce::Font(10.5f, juce::Font::bold));
    transportStatus.setJustificationType(juce::Justification::centred);

    gridLabel.setVisible(false);
    gridSelector.setVisible(false);

    tripletModeButton.setClickingTogglesState(true);
    tripletModeButton.setMouseClickGrabsKeyboardFocus(false);
    tripletModeButton.setToggleState(tripletMode, juce::dontSendNotification);
    tripletModeButton.setTooltip(
        "Triplet grid mode. Normal zoom uses 1/8T; high zoom automatically uses 1/16T.");
    tripletModeButton.onClick = [this]
    {
        tripletMode = tripletModeButton.getToggleState();
        updateGridResolutionForZoom(grid ? grid->getZoomScale() : defaultZoomScale);
        if (grid)
            grid->grabKeyboardFocus();
    };
    addAndMakeVisible(tripletModeButton);

    zoomSlider.setSkewFactorFromMidPoint(defaultZoomScale);
    zoomSlider.setDoubleClickReturnValue(true, defaultZoomScale);
    zoomSlider.setTooltip(
        "Timeline zoom. Grid density changes automatically: 1/16 to 1/32, or 1/8T to 1/16T in Triplet mode.");

    if (libraryBrowser)
    {
        libraryBrowser->setVisible(true);
        libraryBrowser->toFront(false);
    }

    // The browser needs a real reserved column. Do not allow the host to restore
    // the plug-in to Beta 1's too-small minimum where controls and browser fight.
    setResizeLimits(1320, 640, 2200, 1500);
    if (getWidth() < 1320 || getHeight() < 640)
        setSize(juce::jmax(1420, getWidth()), juce::jmax(820, getHeight()));

    updateGridResolutionForZoom(grid ? grid->getZoomScale() : defaultZoomScale);
    initialiseBeta3Ui();
    initialiseBeta4Ui();
    resized();
}

juce::String GgdDrumEditor::currentGridText() const
{
    if (grid == nullptr)
        return tripletMode ? "1/8T" : "1/16";

    switch (grid->getSnapTicks())
    {
        case GGD_TICKS_PER_32ND: return "1/32";
        case GGD_TICKS_PER_8TH_TRIPLET: return "1/8T";
        case GGD_TICKS_PER_16TH_TRIPLET: return "1/16T";
        default: return "1/16";
    }
}

void GgdDrumEditor::updateGridResolutionForZoom(float scale)
{
    const bool fine = scale >= detail32ZoomThreshold;
    const int desired = tripletMode
        ? (fine ? GGD_TICKS_PER_16TH_TRIPLET : GGD_TICKS_PER_8TH_TRIPLET)
        : (fine ? GGD_TICKS_PER_32ND : GGD_TICKS_PER_16TH);

    gridTicks = desired;
    if (grid && grid->getSnapTicks() != desired)
        grid->setSnapTicks(desired);
}

void GgdDrumEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui(GgdThemeRole::background));

    juce::ColourGradient header(
        ui(GgdThemeRole::panelRaised), 0.0f, 0.0f,
        ui(GgdThemeRole::panel), 0.0f, static_cast<float>(topAreaHeight), false);
    g.setGradientFill(header);
    g.fillRect(0, 0, getWidth(), topAreaHeight);

    g.setColour(ui(GgdThemeRole::borderSoft).withAlpha(0.72f));
    g.drawHorizontalLine(55, 12.0f, static_cast<float>(getWidth() - 12));

    g.setColour(ui(GgdThemeRole::accentSecondary).withAlpha(0.70f));
    g.fillRect(0, topAreaHeight - 2, getWidth(), 2);

    const int editorWidth = juce::jmax(780, getWidth() - browserWidth);
    const int stripY = getHeight() - bottomAreaHeight;
    g.setColour(ui(GgdThemeRole::panel));
    g.fillRect(0, stripY, editorWidth, bottomAreaHeight);

    g.setColour(ui(GgdThemeRole::panelRaised).withAlpha(0.55f));
    g.fillRoundedRectangle(7.0f, static_cast<float>(stripY + 4),
                           static_cast<float>(editorWidth - 14), 29.0f, 5.0f);
    g.fillRoundedRectangle(7.0f, static_cast<float>(stripY + 38),
                           static_cast<float>(editorWidth - 14), 29.0f, 5.0f);

    g.setColour(ui(GgdThemeRole::border).withAlpha(0.90f));
    g.drawHorizontalLine(stripY, 0.0f, static_cast<float>(editorWidth));

    g.setColour(ui(GgdThemeRole::borderStrong).withAlpha(0.58f));
    g.fillRect(editorWidth - 1, topAreaHeight, 1,
               juce::jmax(0, getHeight() - topAreaHeight));
    g.setColour(ui(GgdThemeRole::accentSecondary).withAlpha(0.22f));
    g.fillRect(editorWidth, topAreaHeight, 2,
               juce::jmax(0, getHeight() - topAreaHeight));
}

void GgdDrumEditor::resized()
{
    const int pad = 12;
    const int gap = 7;
    const int editorWidth = juce::jmax(780, getWidth() - browserWidth);

    auto first = juce::Rectangle<int>(pad, 7, editorWidth - pad * 2, 43);
    productLabel.setBounds(first.removeFromLeft(142));
    first.removeFromLeft(3);
    transportStatus.setBounds(first.removeFromLeft(72).reduced(0, 8));
    first.removeFromLeft(gap + 2);
    kitSelector.setBounds(first.removeFromLeft(180).reduced(0, 5));
    first.removeFromLeft(gap);
    patternSelector.setBounds(first.removeFromLeft(156).reduced(0, 5));
    first.removeFromLeft(gap);
    patternActionsButton.setBounds(first.removeFromLeft(76).reduced(0, 5));
    first.removeFromLeft(gap);
    exportMidiButton.setBounds(first.removeFromLeft(88).reduced(0, 5));
    first.removeFromLeft(gap);

    // Beta 5 moves appearance into a compact settings menu and gives the freed
    // space to pattern names. The old Import MIDI component is repurposed as
    // that settings control; groove import remains in the dedicated browser.
    auto settingsArea = first.removeFromRight(34);
    importMidiButton.setBounds(settingsArea.reduced(0, 5));
    first.removeFromRight(gap);
    patternName.setBounds(first.reduced(0, 5));
    themeSelector.setBounds({});
    themeSelector.setVisible(false);

    auto second = juce::Rectangle<int>(pad, 60, editorWidth - pad * 2, 43);

    auto actions = second.removeFromRight(181);
    clearButton.setBounds(actions.removeFromRight(56).reduced(0, 5));
    actions.removeFromRight(5);
    redoButton.setBounds(actions.removeFromRight(57).reduced(0, 5));
    actions.removeFromRight(5);
    undoButton.setBounds(actions.removeFromRight(58).reduced(0, 5));
    second.removeFromRight(10);

    drawModeButton.setBounds(second.removeFromLeft(55).reduced(0, 5));
    second.removeFromLeft(4);
    selectModeButton.setBounds(second.removeFromLeft(62).reduced(0, 5));
    second.removeFromLeft(10);

    meterLabel.setBounds(second.removeFromLeft(43));
    numeratorSelector.setBounds(second.removeFromLeft(48).reduced(0, 5));
    meterSlash.setBounds(second.removeFromLeft(15));
    denominatorSelector.setBounds(second.removeFromLeft(55).reduced(0, 5));
    second.removeFromLeft(9);

    barsLabel.setBounds(second.removeFromLeft(38));
    barsEditor.setBounds(second.removeFromLeft(52).reduced(0, 5));
    second.removeFromLeft(8);

    tripletModeButton.setBounds(second.removeFromLeft(72).reduced(0, 5));
    second.removeFromLeft(8);

    zoomLabel.setBounds(second.removeFromLeft(36));
    fitZoomButton.setBounds(second.removeFromRight(54).reduced(0, 5));
    second.removeFromRight(4);
    zoomValueLabel.setBounds(second.removeFromRight(96));
    second.removeFromRight(5);
    zoomSlider.setBounds(second.reduced(0, 7));

    gridLabel.setBounds({});
    gridSelector.setBounds({});
    gridLabel.setVisible(false);
    gridSelector.setVisible(false);
    probabilityLabel.setBounds({});
    probabilitySelector.setBounds({});

    const int contentHeight = getHeight() - topAreaHeight;
    gridViewport.setBounds(0, topAreaHeight, editorWidth,
                           juce::jmax(1, contentHeight - bottomAreaHeight));

    if (libraryBrowser)
    {
        libraryBrowser->setBounds(editorWidth, topAreaHeight,
                                  juce::jmax(1, getWidth() - editorWidth),
                                  juce::jmax(1, contentHeight));
        libraryBrowser->setVisible(true);
        libraryBrowser->toFront(false);
    }

    auto strip = juce::Rectangle<int>(8, getHeight() - bottomAreaHeight + 3,
                                      editorWidth - 16, bottomAreaHeight - 6);
    auto rowOne = strip.removeFromTop(30);
    strip.removeFromTop(4);
    auto rowTwo = strip.removeFromTop(30);

    selectionStatusLabel.setBounds(rowOne.removeFromLeft(132));
    rowOne.removeFromLeft(4);
    auto placeOne = [&](juce::TextButton& button, int width)
    {
        button.setBounds(rowOne.removeFromLeft(width).reduced(0, 3));
        rowOne.removeFromLeft(4);
    };
    placeOne(selectAllButton, 42);
    placeOne(copyButton, 48);
    placeOne(pasteButton, 50);
    placeOne(velocityDownButton, 50);
    placeOne(velocityUpButton, 50);
    placeOne(ghostButton, 52);
    placeOne(accentButton, 54);
    placeOne(probability25Button, 38);
    placeOne(probability50Button, 38);
    placeOne(probability75Button, 38);
    placeOne(probability100Button, 42);
    placeOne(deleteSelectionButton, 54);

    auto placeTwo = [&](juce::TextButton& button, int width)
    {
        button.setBounds(rowTwo.removeFromLeft(width).reduced(0, 3));
        rowTwo.removeFromLeft(4);
    };
    placeTwo(timingEarlierButton, 58);
    placeTwo(timingResetButton, 66);
    placeTwo(timingLaterButton, 52);
    placeTwo(humanizeButton, 68);
    placeTwo(flamButton, 48);
    placeTwo(doubleButton, 56);
    hintLabel.setBounds(rowTwo.reduced(5, 0));

    if (grid)
        grid->refreshSize();
}

bool GgdDrumEditor::keyPressed(const juce::KeyPress& key)
{
    if (grid && grid->hasKeyboardFocus(true))
        return grid->keyPressed(key);
    return false;
}

void GgdDrumEditor::timerCallback()
{
    initialiseBeta2Ui();
    initialiseBeta3Ui();
    initialiseBeta4Ui();
    if (!grid)
        return;

    const bool textEntryActive =
        patternName.hasKeyboardFocus(true) || barsEditor.hasKeyboardFocus(true);
    grid->pollFallbackShortcuts(!textEntryActive && grid->hasKeyboardFocus(true));

    const int play = processor.mNotifier.getPlayPosition(0);
    grid->setPlayPosition(play);

    if (processor.mNotifier.doesUINeedUpdate())
    {
        refreshControlsFromModel();
        grid->refreshSize();
    }

    const float zoom = grid->getZoomScale();
    updateGridResolutionForZoom(zoom);
    zoomSlider.setValue(zoom, juce::dontSendNotification);
    zoomValueLabel.setText(
        juce::String(static_cast<int>(std::round(zoom * 100.0f)))
            + "% | " + currentGridText(),
        juce::dontSendNotification);

    tripletModeButton.setToggleState(tripletMode, juce::dontSendNotification);

    if (play >= 0)
    {
        transportStatus.setColour(juce::Label::textColourId,
                                  ui(GgdThemeRole::accent));
        transportStatus.setText("PLAY", juce::dontSendNotification);
    }
    else
    {
        transportStatus.setColour(juce::Label::textColourId,
                                  ui(GgdThemeRole::muted));
        transportStatus.setText("READY", juce::dontSendNotification);
    }

    undoButton.setEnabled(!undoHistory.empty());
    redoButton.setEnabled(!redoHistory.empty());
    updateContextStrip();
    updateSelectionPropertyControls();

    grid->repaint();
}
