#include "GgdDrumEditor.h"
#include "GgdDrumGridBeta.h"

// Beta 1 already contains the event-aware editor behavior we want to retain.
// Compile it unchanged under private compatibility entry points for the shell
// methods Beta 2 restores to the Alpha 10 behavior.
#define paint paintLegacy
#define resized resizedLegacy
#define timerCallback timerCallbackLegacy
#include "GgdDrumEditorBeta1.cpp"
#undef timerCallback
#undef resized
#undef paint

namespace
{
constexpr float defaultZoomScale = 1.25f;
constexpr float detail32ZoomThreshold = 3.5f;
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
    transportStatus.setColour(juce::Label::textColourId, c(muted));
    transportStatus.setColour(juce::Label::backgroundColourId, c(panelRaised));

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
    setResizeLimits(1320, 600, 2200, 1500);
    if (getWidth() < 1320 || getHeight() < 600)
        setSize(juce::jmax(1420, getWidth()), juce::jmax(820, getHeight()));

    updateGridResolutionForZoom(grid ? grid->getZoomScale() : defaultZoomScale);
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
    g.fillAll(c(bg));

    juce::ColourGradient header(
        c(panelRaised), 0.0f, 0.0f,
        c(panel), 0.0f, static_cast<float>(topAreaHeight), false);
    g.setGradientFill(header);
    g.fillRect(0, 0, getWidth(), topAreaHeight);

    g.setColour(c(accent2).withAlpha(0.44f));
    g.fillRect(0, topAreaHeight - 2, getWidth(), 2);

    g.setColour(c(panel));
    g.fillRect(0, getHeight() - bottomAreaHeight, getWidth(), bottomAreaHeight);
    g.setColour(c(border).withAlpha(0.90f));
    g.drawHorizontalLine(getHeight() - bottomAreaHeight, 0.0f,
                         static_cast<float>(getWidth()));

    g.setColour(c(border).withAlpha(0.44f));
    g.drawHorizontalLine(55, 12.0f, static_cast<float>(getWidth() - 12));

    // Make the browser's ownership of the right column unambiguous.
    const int editorWidth = juce::jmax(780, getWidth() - browserWidth);
    g.setColour(c(accent2).withAlpha(0.32f));
    g.fillRect(editorWidth - 1, topAreaHeight, 1,
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
    patternSelector.setBounds(first.removeFromLeft(126).reduced(0, 5));
    first.removeFromLeft(gap);
    patternActionsButton.setBounds(first.removeFromLeft(76).reduced(0, 5));
    first.removeFromLeft(gap);
    importMidiButton.setBounds(first.removeFromLeft(86).reduced(0, 5));
    first.removeFromLeft(gap);
    patternName.setBounds(first.reduced(0, 5));

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

    // Keep Beta 1's explicit resolution widgets out of the layout. Resolution
    // is now a zoom-derived property with one Triplet-mode toggle.
    gridLabel.setBounds({});
    gridSelector.setBounds({});
    gridLabel.setVisible(false);
    gridSelector.setVisible(false);

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

    auto strip = juce::Rectangle<int>(8, getHeight() - bottomAreaHeight + 2,
                                      editorWidth - 16, bottomAreaHeight - 4);
    selectionStatusLabel.setBounds(strip.removeFromLeft(110));
    strip.removeFromLeft(5);

    auto place = [&](juce::TextButton& button, int width)
    {
        button.setBounds(strip.removeFromLeft(width).reduced(0, 4));
        strip.removeFromLeft(4);
    };
    place(selectAllButton, 42);
    place(copyButton, 48);
    place(pasteButton, 50);
    place(velocityDownButton, 50);
    place(velocityUpButton, 50);
    place(timingEarlierButton, 58);
    place(timingResetButton, 66);
    place(timingLaterButton, 52);
    place(humanizeButton, 68);
    place(deleteSelectionButton, 54);
    hintLabel.setBounds(strip);

    if (grid)
        grid->refreshSize();
}

void GgdDrumEditor::timerCallback()
{
    initialiseBeta2Ui();
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
        transportStatus.setColour(juce::Label::textColourId, c(accent));
        transportStatus.setText("PLAY", juce::dontSendNotification);
    }
    else
    {
        transportStatus.setColour(juce::Label::textColourId, c(muted));
        transportStatus.setText("READY", juce::dontSendNotification);
    }

    undoButton.setEnabled(!undoHistory.empty());
    redoButton.setEnabled(!redoHistory.empty());
    updateContextStrip();

    // The interpolated playhead advances between notifier steps, so it needs a
    // repaint every timer frame even when the host notification did not change.
    grid->repaint();
}
