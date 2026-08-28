#include "GgdDrumEditor.h"
#include "GgdDrumGridBeta.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr juce::uint32 bg          = 0xff0d1013;
constexpr juce::uint32 panel       = 0xff14191e;
constexpr juce::uint32 panelRaised = 0xff1a2026;
constexpr juce::uint32 border      = 0xff2d363f;
constexpr juce::uint32 text        = 0xffedf2f5;
constexpr juce::uint32 muted       = 0xff8d99a5;
constexpr juce::uint32 accent      = 0xff70d6c1;
constexpr juce::uint32 accent2     = 0xff3f9f90;

juce::Colour c(juce::uint32 value) { return juce::Colour(value); }

int activeStorageRowCount(int canonicalCount)
{
    return juce::jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, canonicalCount);
}

int storageRowForCanonical(int canonicalRow, int canonicalCount)
{
    return (SEQ_MAX_ROWS - activeStorageRowCount(canonicalCount)) + canonicalRow;
}

int ticksPerBarForMeter(int numerator, int denominator)
{
    return juce::jmax(1, GGD_EVENT_PPQ * numerator * 4 / denominator);
}

int stepsPerBarForLegacyMeter(int numerator, int denominator)
{
    return juce::jmax(1, numerator * 16 / denominator);
}
}

GgdDrumEditor::GgdDrumEditor(SeqAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    maps = GgdKitMapLibrary::loadBuiltInMaps();
    canonicalRows = GgdKitMapLibrary::buildCanonicalRows(maps);

    configureLookAndFeel();
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(1040, 560, 2400, 1600);
    setSize(1480, 820);

    initialiseDrumState();

    grid = std::make_unique<GgdDrumGrid>(
        processor,
        canonicalRows,
        [this] { performUndo(); },
        [this] { performRedo(); },
        [this] { publishModelChange(); },
        [this](float scale) { refreshZoomControls(scale); },
        [this](GgdDrumGrid::ToolMode) { updateContextStrip(); },
        [this] { updateContextStrip(); });
    grid->setMap(maps.isEmpty() ? nullptr : &maps.getReference(activeMapIndex));
    grid->setMeter(timeSigNumerator, timeSigDenominator, activeBars);
    grid->setSnapTicks(gridTicks);

    gridViewport.setViewedComponent(grid.get(), false);
    gridViewport.setScrollBarsShown(true, true);
    gridViewport.setScrollBarThickness(10);
    gridViewport.setScrollOnDragEnabled(false);
    addAndMakeVisible(gridViewport);

    libraryBrowser = std::make_unique<GgdLibraryBrowser>();
    libraryBrowser->setGrooveOpenCallback(
        [this](const juce::File& file) { requestLoadGroove(file); });
    libraryBrowser->setPatternOpenCallback(
        [this](const juce::File& file) { requestLoadPattern(file); });
    libraryBrowser->setSavePatternCallback(
        [this] { saveCurrentPatternToLibrary(); });
    addAndMakeVisible(*libraryBrowser);

    productLabel.setText("STOCHAS GGD  BETA 1", juce::dontSendNotification);
    productLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    productLabel.setColour(juce::Label::textColourId, c(text));
    addAndMakeVisible(productLabel);

    transportStatus.setColour(juce::Label::textColourId, c(accent));
    transportStatus.setFont(10.5f);
    transportStatus.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(transportStatus);

    kitSelector.setTooltip("Destination GGD mapping");
    kitSelector.setMouseClickGrabsKeyboardFocus(false);
    for (int i = 0; i < maps.size(); ++i)
        kitSelector.addItem(maps.getReference(i).library, i + 1);
    kitSelector.onChange = [this]
    {
        if (kitSelector.getSelectedId() > 0)
            setActiveMap(kitSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(kitSelector);

    for (int i = 0; i < SEQ_MAX_PATTERNS; ++i)
        patternSelector.addItem("Pattern " + juce::String(i + 1), i + 1);
    patternSelector.setMouseClickGrabsKeyboardFocus(false);
    patternSelector.onChange = [this]
    {
        const int selected = patternSelector.getSelectedId() - 1;
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        if (selected < 0 || selected >= SEQ_MAX_PATTERNS
            || selected == layer->getCurrentPattern())
            return;

        layer->setCurrentPattern(selected);
        loadGeometryFromCurrentPattern();
        publishModelChange(false);
        if (libraryBrowser)
            libraryBrowser->clearLoaded();
        if (grid)
        {
            grid->setMeter(timeSigNumerator, timeSigDenominator, activeBars);
            grid->patternChanged();
            grid->grabKeyboardFocus();
        }
        resetHistoryForCurrentPattern(false);
        refreshControlsFromModel();
    };
    addAndMakeVisible(patternSelector);

    patternName.setSelectAllWhenFocused(true);
    patternName.setTooltip("Current internal pattern name");
    auto commitName = [this]
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const auto desired = patternName.getText().trim().substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
        if (desired.isEmpty())
            return;
        if (desired != juce::String(layer->getPatternName(pattern)))
        {
            layer->setPatternName(desired.toRawUTF8(), pattern);
            publishModelChange();
            refreshPatternSelectorLabels();
        }
    };
    patternName.onReturnKey = commitName;
    patternName.onFocusLost = commitName;
    addAndMakeVisible(patternName);

    patternActionsButton.setMouseClickGrabsKeyboardFocus(false);
    patternActionsButton.onClick = [this] { showPatternActions(); };
    addAndMakeVisible(patternActionsButton);

    importMidiButton.setMouseClickGrabsKeyboardFocus(false);
    importMidiButton.onClick = [this] { chooseMidiFile(); };
    addAndMakeVisible(importMidiButton);

    drawModeButton.setClickingTogglesState(false);
    drawModeButton.setMouseClickGrabsKeyboardFocus(false);
    drawModeButton.onClick = [this]
    {
        if (grid) grid->setToolMode(GgdDrumGrid::ToolMode::draw);
        updateContextStrip();
    };
    addAndMakeVisible(drawModeButton);

    selectModeButton.setClickingTogglesState(false);
    selectModeButton.setMouseClickGrabsKeyboardFocus(false);
    selectModeButton.onClick = [this]
    {
        if (grid) grid->setToolMode(GgdDrumGrid::ToolMode::select);
        updateContextStrip();
    };
    addAndMakeVisible(selectModeButton);

    undoButton.setMouseClickGrabsKeyboardFocus(false);
    undoButton.onClick = [this] { performUndo(); };
    addAndMakeVisible(undoButton);
    redoButton.setMouseClickGrabsKeyboardFocus(false);
    redoButton.onClick = [this] { performRedo(); };
    addAndMakeVisible(redoButton);

    clearButton.setMouseClickGrabsKeyboardFocus(false);
    clearButton.onClick = [this] { clearCurrentPattern(); };
    addAndMakeVisible(clearButton);

    meterLabel.setText("METER", juce::dontSendNotification);
    meterLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(meterLabel);
    for (int i = 1; i <= 32; ++i)
        numeratorSelector.addItem(juce::String(i), i);
    numeratorSelector.setMouseClickGrabsKeyboardFocus(false);
    numeratorSelector.onChange = [this]
    {
        if (numeratorSelector.getSelectedId() > 0)
        {
            timeSigNumerator = numeratorSelector.getSelectedId();
            applyPatternGeometry();
        }
    };
    addAndMakeVisible(numeratorSelector);

    meterSlash.setText("/", juce::dontSendNotification);
    meterSlash.setColour(juce::Label::textColourId, c(muted));
    meterSlash.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(meterSlash);

    static constexpr int denominators[] = { 2, 4, 8, 16 };
    for (int i = 0; i < 4; ++i)
        denominatorSelector.addItem(juce::String(denominators[i]), i + 1);
    denominatorSelector.setMouseClickGrabsKeyboardFocus(false);
    denominatorSelector.onChange = [this]
    {
        const int id = denominatorSelector.getSelectedId();
        if (id > 0)
        {
            static constexpr int values[] = { 2, 4, 8, 16 };
            timeSigDenominator = values[id - 1];
            applyPatternGeometry();
        }
    };
    addAndMakeVisible(denominatorSelector);

    barsLabel.setText("BARS", juce::dontSendNotification);
    barsLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(barsLabel);
    barsEditor.setInputRestrictions(2, "0123456789");
    barsEditor.setJustification(juce::Justification::centred);
    barsEditor.onReturnKey = [this] { commitBarCountEditor(); };
    barsEditor.onFocusLost = [this] { commitBarCountEditor(); };
    addAndMakeVisible(barsEditor);

    gridLabel.setText("GRID", juce::dontSendNotification);
    gridLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(gridLabel);
    gridSelector.addItem("1/16", 1);
    gridSelector.addItem("1/32", 2);
    gridSelector.addItem("1/8T", 3);
    gridSelector.addItem("1/16T", 4);
    gridSelector.setMouseClickGrabsKeyboardFocus(false);
    gridSelector.onChange = [this]
    {
        switch (gridSelector.getSelectedId())
        {
            case 2: setGridTicks(GGD_TICKS_PER_32ND); break;
            case 3: setGridTicks(GGD_TICKS_PER_8TH_TRIPLET); break;
            case 4: setGridTicks(GGD_TICKS_PER_16TH_TRIPLET); break;
            default: setGridTicks(GGD_TICKS_PER_16TH); break;
        }
    };
    addAndMakeVisible(gridSelector);

    zoomLabel.setText("ZOOM", juce::dontSendNotification);
    zoomLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(zoomLabel);
    zoomSlider.setRange(0.5, 4.0, 0.25);
    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.onValueChange = [this]
    {
        if (grid)
            grid->setZoomScale(static_cast<float>(zoomSlider.getValue()));
    };
    addAndMakeVisible(zoomSlider);
    zoomValueLabel.setColour(juce::Label::textColourId, c(muted));
    zoomValueLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(zoomValueLabel);
    fitZoomButton.setMouseClickGrabsKeyboardFocus(false);
    fitZoomButton.onClick = [this] { if (grid) grid->resetZoom(); };
    addAndMakeVisible(fitZoomButton);

    selectionStatusLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(selectionStatusLabel);
    hintLabel.setColour(juce::Label::textColourId, c(muted));
    hintLabel.setFont(10.5f);
    addAndMakeVisible(hintLabel);

    auto prep = [this](juce::TextButton& button)
    {
        button.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(button);
    };
    prep(selectAllButton); prep(copyButton); prep(pasteButton);
    prep(velocityDownButton); prep(velocityUpButton);
    prep(timingEarlierButton); prep(timingResetButton); prep(timingLaterButton);
    prep(humanizeButton); prep(deleteSelectionButton);

    selectAllButton.onClick = [this] { if (grid) grid->selectAll(); };
    copyButton.onClick = [this] { if (grid) grid->copySelectionToClipboard(); };
    pasteButton.onClick = [this] { if (grid) grid->pasteClipboard(); };
    velocityDownButton.onClick = [this] { if (grid) grid->adjustSelectedVelocityBy(-5); };
    velocityUpButton.onClick = [this] { if (grid) grid->adjustSelectedVelocityBy(5); };
    timingEarlierButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(-10); };
    timingResetButton.onClick = [this] { if (grid) grid->quantizeSelected(); };
    timingLaterButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(10); };
    humanizeButton.onClick = [this] { if (grid) grid->humanizeSelected(); };
    deleteSelectionButton.onClick = [this] { if (grid) grid->deleteSelected(); };

    initialiseCleanPatternFingerprints();
    resetHistoryForCurrentPattern(false);
    refreshControlsFromModel();
    updateContextStrip();
    startTimerHz(60);
}

GgdDrumEditor::~GgdDrumEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
    gridViewport.setViewedComponent(nullptr, false);
}

void GgdDrumEditor::configureLookAndFeel()
{
    lookAndFeel.setColour(juce::ResizableWindow::backgroundColourId, c(bg));
    lookAndFeel.setColour(juce::TextButton::buttonColourId, c(panelRaised));
    lookAndFeel.setColour(juce::TextButton::buttonOnColourId, c(accent2));
    lookAndFeel.setColour(juce::TextButton::textColourOffId, c(text));
    lookAndFeel.setColour(juce::TextButton::textColourOnId, c(text));
    lookAndFeel.setColour(juce::ComboBox::backgroundColourId, c(panel));
    lookAndFeel.setColour(juce::ComboBox::outlineColourId, c(border));
    lookAndFeel.setColour(juce::ComboBox::textColourId, c(text));
    lookAndFeel.setColour(juce::TextEditor::backgroundColourId, c(panel));
    lookAndFeel.setColour(juce::TextEditor::outlineColourId, c(border));
    lookAndFeel.setColour(juce::TextEditor::focusedOutlineColourId, c(accent2));
    lookAndFeel.setColour(juce::TextEditor::textColourId, c(text));
    lookAndFeel.setColour(juce::Slider::trackColourId, c(border));
    lookAndFeel.setColour(juce::Slider::thumbColourId, c(accent));
}

void GgdDrumEditor::paint(juce::Graphics& g)
{
    g.fillAll(c(bg));
    g.setColour(c(panel));
    g.fillRect(0, 0, getWidth(), topAreaHeight);
    g.setColour(c(border));
    g.drawHorizontalLine(topAreaHeight - 1, 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(getHeight() - bottomAreaHeight, 0.0f,
                         static_cast<float>(getWidth() - browserWidth));
}

void GgdDrumEditor::resized()
{
    const int editorWidth = juce::jmax(500, getWidth() - browserWidth);
    auto top = juce::Rectangle<int>(0, 0, editorWidth, topAreaHeight).reduced(8, 6);
    auto first = top.removeFromTop(45);
    top.removeFromTop(5);
    auto second = top.removeFromTop(42);

    productLabel.setBounds(first.removeFromLeft(170));
    first.removeFromLeft(5);
    kitSelector.setBounds(first.removeFromLeft(185).reduced(0, 5));
    first.removeFromLeft(5);
    patternSelector.setBounds(first.removeFromLeft(118).reduced(0, 5));
    first.removeFromLeft(5);
    patternName.setBounds(first.removeFromLeft(155).reduced(0, 5));
    first.removeFromLeft(5);
    patternActionsButton.setBounds(first.removeFromLeft(72).reduced(0, 5));
    first.removeFromLeft(5);
    importMidiButton.setBounds(first.removeFromLeft(88).reduced(0, 5));
    first.removeFromLeft(8);
    drawModeButton.setBounds(first.removeFromLeft(58).reduced(0, 5));
    first.removeFromLeft(3);
    selectModeButton.setBounds(first.removeFromLeft(62).reduced(0, 5));
    first.removeFromLeft(6);
    undoButton.setBounds(first.removeFromLeft(55).reduced(0, 5));
    first.removeFromLeft(3);
    redoButton.setBounds(first.removeFromLeft(55).reduced(0, 5));
    first.removeFromLeft(3);
    clearButton.setBounds(first.removeFromLeft(52).reduced(0, 5));
    transportStatus.setBounds(first);

    meterLabel.setBounds(second.removeFromLeft(48));
    numeratorSelector.setBounds(second.removeFromLeft(56).reduced(0, 4));
    meterSlash.setBounds(second.removeFromLeft(18));
    denominatorSelector.setBounds(second.removeFromLeft(58).reduced(0, 4));
    second.removeFromLeft(12);
    barsLabel.setBounds(second.removeFromLeft(38));
    barsEditor.setBounds(second.removeFromLeft(46).reduced(0, 4));
    second.removeFromLeft(12);
    gridLabel.setBounds(second.removeFromLeft(35));
    gridSelector.setBounds(second.removeFromLeft(72).reduced(0, 4));
    second.removeFromLeft(14);
    zoomLabel.setBounds(second.removeFromLeft(42));
    zoomSlider.setBounds(second.removeFromLeft(190).reduced(0, 6));
    zoomValueLabel.setBounds(second.removeFromLeft(82));
    fitZoomButton.setBounds(second.removeFromLeft(58).reduced(0, 4));

    const int contentHeight = getHeight() - topAreaHeight;
    gridViewport.setBounds(0, topAreaHeight, editorWidth,
                           contentHeight - bottomAreaHeight);
    if (libraryBrowser)
        libraryBrowser->setBounds(editorWidth, topAreaHeight,
                                  getWidth() - editorWidth, contentHeight);

    auto strip = juce::Rectangle<int>(8, getHeight() - bottomAreaHeight + 2,
                                      editorWidth - 16, bottomAreaHeight - 4);
    selectionStatusLabel.setBounds(strip.removeFromLeft(110));
    auto place = [&](juce::TextButton& b, int width)
    {
        b.setBounds(strip.removeFromLeft(width).reduced(2, 3));
    };
    place(selectAllButton, 45); place(copyButton, 50); place(pasteButton, 52);
    place(velocityDownButton, 52); place(velocityUpButton, 52);
    place(timingEarlierButton, 58); place(timingResetButton, 66);
    place(timingLaterButton, 52); place(humanizeButton, 68);
    place(deleteSelectionButton, 54);
    hintLabel.setBounds(strip);

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
    if (!grid)
        return;

    grid->pollFallbackShortcuts(grid->hasKeyboardFocus(true));
    const int play = processor.mNotifier.getPlayPosition(0);
    grid->setPlayPosition(play);
    transportStatus.setText(play >= 0 ? "PLAY" : "READY", juce::dontSendNotification);

    if (processor.mNotifier.doesUINeedUpdate())
        refreshControlsFromModel();

    undoButton.setEnabled(!undoHistory.empty());
    redoButton.setEnabled(!redoHistory.empty());
}

void GgdDrumEditor::initialiseDrumState()
{
    if (maps.isEmpty() || canonicalRows.isEmpty())
        return;

    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const juce::String oldLayerName(layer->getLayerName());
    const bool configured = oldLayerName.startsWith("GGD:")
                         || oldLayerName.startsWith("GGD2:")
                         || oldLayerName.startsWith("GGD3:")
                         || oldLayerName.startsWith("GGD4:");

    bool betaEventsPresent = false;
    for (int p = 0; p < SEQ_MAX_PATTERNS; ++p)
        betaEventsPresent |= layer->getEventPattern(p)->isActive();

    if (!configured && !betaEventsPresent)
    {
        seq->clearLayer(0);
        layer = seq->getLayer(0);
    }

    activeMapIndex = configured ? mapIndexFromLayerName(oldLayerName) : 0;
    activeMapIndex = juce::jlimit(0, maps.size() - 1, activeMapIndex);

    int legacyNumerator = 4;
    int legacyDenominator = 4;
    if (oldLayerName.startsWith("GGD3:"))
        parseMeterFromLayerName(oldLayerName, legacyNumerator, legacyDenominator);
    const int legacyStepsPerBar = stepsPerBarForLegacyMeter(legacyNumerator, legacyDenominator);
    const int legacyBars = juce::jmax(
        1, (layer->getNumSteps() + legacyStepsPerBar - 1) / legacyStepsPerBar);

    for (int p = 0; p < SEQ_MAX_PATTERNS; ++p)
    {
        auto* events = layer->getEventPattern(p);
        if (events->isActive())
            continue;
        if (layer->legacyPatternHasData(p))
            layer->migrateLegacyPatternToEvents(p, legacyNumerator, legacyDenominator, legacyBars);
        else
            events->activate(legacyNumerator, legacyDenominator, legacyBars);
    }

    seq->setMidiPassthru(SEQ_MIDI_PASSTHRU_ALL);
    seq->setMidiRespond(SEQCTL_MIDI_RESPOND_NO);
    layer->setNoteSource(true);
    layer->setMonoMode(false);
    layer->setMaxRows(activeStorageRowCount(canonicalRows.size()));
    layer->setMaxPoly(juce::jmin(SEQ_DEFAULT_MAX_POLY, layer->getMaxRows()));
    layer->setClockDivider(SEQ_DEFAULT_CLOCK_DIV);

    loadGeometryFromCurrentPattern();
    applyActiveMapBindings(false);
    updatePersistenceTag();
    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
}

void GgdDrumEditor::applyActiveMapBindings(bool publish)
{
    if (maps.isEmpty())
        return;

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const auto& activeMap = maps.getReference(activeMapIndex);

    for (int row = 0; row < canonicalRows.size(); ++row)
    {
        const int storage = storageRowForCanonical(row, canonicalRows.size());
        const auto& canonical = canonicalRows.getReference(row);
        int midi = -1;
        juce::String label = canonical.defaultLabel;
        if (const auto* articulation = activeMap.findArticulation(canonical.semanticId))
        {
            if (articulation->label.isNotEmpty())
                label = articulation->label;
            if (const auto* binding = articulation->primaryNoteBinding())
                midi = binding->midi;
        }
        layer->setNote(storage, static_cast<int8_t>(midi), true);
        layer->setNoteName(storage, label.substring(0, SEQ_MAX_NOTELABEL_LEN - 1).toRawUTF8());
    }

    layer->setNoteSource(true);
    updatePersistenceTag();
    if (publish)
        publishModelChange(false);
}

void GgdDrumEditor::loadGeometryFromCurrentPattern()
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    auto* events = layer->getEventPattern(layer->getCurrentPattern());
    if (!events->isActive())
        events->activate(4, 4, 1);

    timeSigNumerator = events->getNumerator();
    timeSigDenominator = events->getDenominator();
    activeBars = events->getBars();
    layer->setNumSteps(juce::jlimit(
        1, SEQ_MAX_STEPS,
        (events->getLengthTicks() + GGD_TICKS_PER_16TH - 1) / GGD_TICKS_PER_16TH));
}

void GgdDrumEditor::applyPatternGeometry(bool publish)
{
    timeSigNumerator = juce::jlimit(1, 32, timeSigNumerator);
    if (timeSigDenominator != 2 && timeSigDenominator != 4
        && timeSigDenominator != 8 && timeSigDenominator != 16)
        timeSigDenominator = 4;

    const int barTicks = ticksPerBarForMeter(timeSigNumerator, timeSigDenominator);
    const int maxBars = juce::jmax(1, maxPatternTicks / barTicks);
    activeBars = juce::jlimit(1, maxBars, activeBars);

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    auto* events = layer->getEventPattern(layer->getCurrentPattern());
    events->setGeometry(timeSigNumerator, timeSigDenominator, activeBars);
    layer->setNumSteps(juce::jlimit(
        1, SEQ_MAX_STEPS,
        (events->getLengthTicks() + GGD_TICKS_PER_16TH - 1) / GGD_TICKS_PER_16TH));

    if (!barsEditor.hasKeyboardFocus(true))
        barsEditor.setText(juce::String(activeBars), false);
    if (grid)
        grid->setMeter(timeSigNumerator, timeSigDenominator, activeBars);
    if (publish)
        publishModelChange();
}

void GgdDrumEditor::commitBarCountEditor()
{
    const int requested = barsEditor.getText().getIntValue();
    activeBars = juce::jmax(1, requested);
    applyPatternGeometry();
}

void GgdDrumEditor::setGridTicks(int ticks)
{
    gridTicks = juce::jmax(1, ticks);
    if (grid)
    {
        grid->setSnapTicks(gridTicks);
        grid->grabKeyboardFocus();
    }
}

void GgdDrumEditor::refreshControlsFromModel()
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const int pattern = layer->getCurrentPattern();
    const auto* events = layer->getEventPattern(pattern);
    if (events && events->isActive())
    {
        timeSigNumerator = events->getNumerator();
        timeSigDenominator = events->getDenominator();
        activeBars = events->getBars();
    }

    kitSelector.setSelectedId(activeMapIndex + 1, juce::dontSendNotification);
    patternSelector.setSelectedId(pattern + 1, juce::dontSendNotification);
    numeratorSelector.setSelectedId(timeSigNumerator, juce::dontSendNotification);
    int denomId = timeSigDenominator == 2 ? 1 : timeSigDenominator == 4 ? 2
                : timeSigDenominator == 8 ? 3 : 4;
    denominatorSelector.setSelectedId(denomId, juce::dontSendNotification);
    if (!barsEditor.hasKeyboardFocus(true))
        barsEditor.setText(juce::String(activeBars), false);
    if (!patternName.hasKeyboardFocus(true))
        patternName.setText(layer->getPatternName(pattern), false);

    const int gridId = gridTicks == GGD_TICKS_PER_32ND ? 2
                     : gridTicks == GGD_TICKS_PER_8TH_TRIPLET ? 3
                     : gridTicks == GGD_TICKS_PER_16TH_TRIPLET ? 4 : 1;
    gridSelector.setSelectedId(gridId, juce::dontSendNotification);
    refreshPatternSelectorLabels();
    refreshZoomControls(grid ? grid->getZoomScale() : 1.25f);
    updateContextStrip();
}

void GgdDrumEditor::refreshPatternSelectorLabels()
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    for (int i = 0; i < SEQ_MAX_PATTERNS; ++i)
    {
        const auto name = juce::String(layer->getPatternName(i));
        patternSelector.changeItemText(i + 1,
            name.isNotEmpty() && name != SEQ_DEFAULT_PAT_NAME
                ? juce::String(i + 1) + "  " + name
                : "Pattern " + juce::String(i + 1));
    }
}

void GgdDrumEditor::refreshZoomControls(float scale)
{
    zoomSlider.setValue(scale, juce::dontSendNotification);
    zoomValueLabel.setText(
        juce::String(static_cast<int>(std::round(scale * 100.0f))) + "%",
        juce::dontSendNotification);
}

void GgdDrumEditor::setActiveMap(int index)
{
    if (maps.isEmpty())
        return;
    activeMapIndex = juce::jlimit(0, maps.size() - 1, index);
    applyActiveMapBindings(true);
    if (grid)
    {
        grid->setMap(&maps.getReference(activeMapIndex));
        grid->grabKeyboardFocus();
    }
}

GgdPatternSnapshot GgdDrumEditor::capturePattern(int pattern) const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    return GgdPatternFile::capture(*layer, canonicalRows, pattern);
}

GgdPatternSnapshot GgdDrumEditor::captureCurrentPattern() const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    return capturePattern(layer->getCurrentPattern());
}

void GgdDrumEditor::recordCommittedPatternEdit()
{
    if (restoringHistory)
        return;

    auto current = captureCurrentPattern();
    if (!lastCommittedSnapshot)
    {
        lastCommittedSnapshot = current;
        return;
    }

    if (GgdPatternFile::fingerprint(current)
        == GgdPatternFile::fingerprint(*lastCommittedSnapshot))
        return;

    undoHistory.push_back(*lastCommittedSnapshot);
    while (undoHistory.size() > maxHistoryDepth)
        undoHistory.pop_front();
    redoHistory.clear();
    lastCommittedSnapshot = std::move(current);
}

void GgdDrumEditor::resetHistoryForCurrentPattern(bool markClean)
{
    undoHistory.clear();
    redoHistory.clear();
    lastCommittedSnapshot = captureCurrentPattern();
    if (markClean)
        markCurrentPatternClean();
}

void GgdDrumEditor::restorePatternSnapshot(const GgdPatternSnapshot& snapshot, bool publish)
{
    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const int patternIndex = layer->getCurrentPattern();
    GgdPatternFile::restore(snapshot, *seq, canonicalRows, 0, patternIndex);
    loadGeometryFromCurrentPattern();
    if (publish)
        publishModelChange(false);
    if (grid)
    {
        grid->setMeter(timeSigNumerator, timeSigDenominator, activeBars);
        grid->patternChanged();
    }
    refreshControlsFromModel();
}

void GgdDrumEditor::performUndo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (undoHistory.empty() || now - lastUndoMs < 45.0)
        return;
    lastUndoMs = now;

    auto current = captureCurrentPattern();
    auto target = undoHistory.back();
    undoHistory.pop_back();
    redoHistory.push_back(std::move(current));
    while (redoHistory.size() > maxHistoryDepth)
        redoHistory.pop_front();

    restoringHistory = true;
    restorePatternSnapshot(target, true);
    restoringHistory = false;
    lastCommittedSnapshot = target;
    if (grid) grid->grabKeyboardFocus();
}

void GgdDrumEditor::performRedo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (redoHistory.empty() || now - lastRedoMs < 45.0)
        return;
    lastRedoMs = now;

    auto current = captureCurrentPattern();
    auto target = redoHistory.back();
    redoHistory.pop_back();
    undoHistory.push_back(std::move(current));
    while (undoHistory.size() > maxHistoryDepth)
        undoHistory.pop_front();

    restoringHistory = true;
    restorePatternSnapshot(target, true);
    restoringHistory = false;
    lastCommittedSnapshot = target;
    if (grid) grid->grabKeyboardFocus();
}

void GgdDrumEditor::initialiseCleanPatternFingerprints()
{
    for (int i = 0; i < SEQ_MAX_PATTERNS; ++i)
    {
        cleanPatternFingerprints[static_cast<size_t>(i)] =
            GgdPatternFile::fingerprint(capturePattern(i));
        cleanPatternFingerprintValid[static_cast<size_t>(i)] = true;
    }
}

void GgdDrumEditor::markCurrentPatternClean()
{
    const int p = processor.mData.getUISeqData()->getLayer(0)->getCurrentPattern();
    cleanPatternFingerprints[static_cast<size_t>(p)] =
        GgdPatternFile::fingerprint(capturePattern(p));
    cleanPatternFingerprintValid[static_cast<size_t>(p)] = true;
}

bool GgdDrumEditor::currentPatternHasChanges() const
{
    const int p = processor.mData.getUISeqData()->getLayer(0)->getCurrentPattern();
    if (!cleanPatternFingerprintValid[static_cast<size_t>(p)])
        return true;
    return cleanPatternFingerprints[static_cast<size_t>(p)]
        != GgdPatternFile::fingerprint(capturePattern(p));
}

void GgdDrumEditor::chooseMidiFile()
{
    midiFileChooser = std::make_unique<juce::FileChooser>(
        "Import GGD MIDI groove", juce::File(), "*.mid;*.midi");
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    midiFileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& chooser)
        {
            if (auto* self = safe.getComponent())
            {
                const auto file = chooser.getResult();
                if (file.existsAsFile())
                    self->importMidiFile(file);
            }
        });
}

void GgdDrumEditor::importMidiFile(const juce::File& file)
{
    if (maps.isEmpty())
        return;
    const auto result = GgdMidiImporter::parseFile(
        file, maps.getReference(activeMapIndex), canonicalRows, maxPatternTicks);
    if (!result.ok)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("MIDI import failed")
                .withMessage(result.error)
                .withButton("OK"), nullptr);
        return;
    }

    requestPatternReplacement("Load groove " + file.getFileNameWithoutExtension(),
        [this, result, file] { applyMidiImport(result, file); });
}

void GgdDrumEditor::applyMidiImport(const GgdMidiImportResult& result,
                                    const juce::File& sourceFile)
{
    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const int patternIndex = layer->getCurrentPattern();
    seq->clearPattern(0, patternIndex);
    auto* events = layer->getEventPattern(patternIndex);
    events->activate(result.numerator, result.denominator, result.bars);

    for (const auto& imported : result.events)
    {
        if (imported.canonicalRow < 0 || imported.canonicalRow >= canonicalRows.size())
            continue;
        events->addEvent(
            storageRowForCanonical(imported.canonicalRow, canonicalRows.size()),
            imported.tick, imported.velocity, SEQ_PROB_ON, imported.durationTicks);
    }

    const auto importedName = result.fileName.substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(importedName.toRawUTF8(), patternIndex);
    loadGeometryFromCurrentPattern();
    publishModelChange();
    markCurrentPatternClean();
    resetHistoryForCurrentPattern(false);
    if (libraryBrowser)
        libraryBrowser->setLoadedGroove(sourceFile);
    if (grid)
    {
        grid->setMeter(timeSigNumerator, timeSigDenominator, activeBars);
        grid->patternChanged();
        grid->grabKeyboardFocus();
    }
    hintLabel.setText(result.summary(), juce::dontSendNotification);
    refreshControlsFromModel();
}

void GgdDrumEditor::requestLoadGroove(const juce::File& file)
{
    importMidiFile(file);
}

void GgdDrumEditor::requestLoadPattern(const juce::File& file)
{
    GgdPatternSnapshot snapshot;
    juce::String error;
    if (!GgdPatternFile::read(file, snapshot, error))
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Pattern load failed").withMessage(error).withButton("OK"), nullptr);
        return;
    }
    requestPatternReplacement("Load pattern " + file.getFileNameWithoutExtension(),
        [this, file, snapshot] { applyPatternFile(file, snapshot); });
}

void GgdDrumEditor::applyPatternFile(const juce::File& file,
                                     const GgdPatternSnapshot& snapshot)
{
    restorePatternSnapshot(snapshot, true);
    markCurrentPatternClean();
    resetHistoryForCurrentPattern(false);
    if (libraryBrowser)
        libraryBrowser->setLoadedPattern(file);
    if (grid) grid->grabKeyboardFocus();
    hintLabel.setText("Loaded pattern: " + snapshot.name, juce::dontSendNotification);
}

void GgdDrumEditor::saveCurrentPatternToLibrary()
{
    if (!libraryBrowser)
        return;
    const auto root = libraryBrowser->getPatternRoot();
    if (!root.isDirectory())
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("Choose a pattern folder")
                .withMessage("Open the Patterns tab and choose a library folder first.")
                .withButton("OK"), nullptr);
        return;
    }

    auto snapshot = captureCurrentPattern();
    auto suggested = root.getChildFile(
        juce::File::createLegalFileName(snapshot.name.isNotEmpty() ? snapshot.name : "Pattern"))
        .withFileExtension(GgdPatternFile::extension);
    patternSaveChooser = std::make_unique<juce::FileChooser>(
        "Save Stochas GGD pattern", suggested, "*.sggdp");
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    patternSaveChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe, snapshot](const juce::FileChooser& chooser)
        {
            if (auto* self = safe.getComponent())
            {
                auto file = chooser.getResult();
                if (file == juce::File())
                    return;
                juce::String error;
                if (!GgdPatternFile::write(file, snapshot, error))
                {
                    juce::AlertWindow::showAsync(
                        juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
                            .withTitle("Pattern save failed").withMessage(error).withButton("OK"), nullptr);
                    return;
                }
                if (!file.hasFileExtension(GgdPatternFile::extension))
                    file = file.withFileExtension(GgdPatternFile::extension);
                self->markCurrentPatternClean();
                self->libraryBrowser->refresh();
                self->libraryBrowser->setLoadedPattern(file);
            }
        });
}

void GgdDrumEditor::requestPatternReplacement(const juce::String& description,
                                              std::function<void()> replacement)
{
    if (!currentPatternHasChanges())
    {
        replacement();
        return;
    }

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Replace edited pattern?")
            .withMessage(description + " will replace unsaved edits in the current pattern.")
            .withButton("Replace").withButton("Cancel"),
        [safe, replacement = std::move(replacement)](int result)
        {
            if (result == 1 && safe.getComponent() != nullptr)
                replacement();
        });
}

void GgdDrumEditor::showPatternActions()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Duplicate to empty slot");
    menu.addItem(2, "Save to pattern library");
    menu.addSeparator();
    menu.addItem(3, "Clear current pattern");
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(patternActionsButton),
        [safe](int result)
        {
            if (auto* self = safe.getComponent())
            {
                if (result == 1) self->duplicateCurrentPatternSlot();
                else if (result == 2) self->saveCurrentPatternToLibrary();
                else if (result == 3) self->clearCurrentPattern();
            }
        });
}

bool GgdDrumEditor::currentPatternSlotIsEmpty(int pattern) const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const auto* events = layer->getEventPattern(pattern);
    return (events == nullptr || events->getEventCount() == 0)
        && juce::String(layer->getPatternName(pattern)) == SEQ_DEFAULT_PAT_NAME;
}

void GgdDrumEditor::duplicateCurrentPatternSlot()
{
    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const int source = layer->getCurrentPattern();
    int target = -1;
    for (int offset = 1; offset < SEQ_MAX_PATTERNS; ++offset)
    {
        const int candidate = (source + offset) % SEQ_MAX_PATTERNS;
        if (currentPatternSlotIsEmpty(candidate))
        {
            target = candidate;
            break;
        }
    }
    if (target < 0)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("No empty pattern slot")
                .withMessage("All eight pattern slots contain data.")
                .withButton("OK"), nullptr);
        return;
    }

    seq->copyPatternData(0, target, 0, source);
    auto copiedName = juce::String(layer->getPatternName(source));
    if (copiedName == SEQ_DEFAULT_PAT_NAME)
        copiedName = "Pattern " + juce::String(source + 1);
    copiedName = (copiedName + " Copy").substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(copiedName.toRawUTF8(), target);
    publishModelChange(false);
    cleanPatternFingerprintValid[static_cast<size_t>(target)] = false;
    refreshPatternSelectorLabels();
}

void GgdDrumEditor::clearCurrentPattern()
{
    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const int patternIndex = layer->getCurrentPattern();
    const int num = timeSigNumerator;
    const int den = timeSigDenominator;
    const int bars = activeBars;
    seq->clearPattern(0, patternIndex);
    layer->getEventPattern(patternIndex)->activate(num, den, bars);
    publishModelChange();
    if (libraryBrowser) libraryBrowser->clearLoaded();
    if (grid) { grid->patternChanged(); grid->grabKeyboardFocus(); }
    refreshControlsFromModel();
}

void GgdDrumEditor::updateContextStrip()
{
    if (!grid)
        return;
    const bool select = grid->getToolMode() == GgdDrumGrid::ToolMode::select;
    const int selected = grid->getSelectedCount();
    drawModeButton.setToggleState(!select, juce::dontSendNotification);
    selectModeButton.setToggleState(select, juce::dontSendNotification);

    selectionStatusLabel.setText(select
        ? juce::String(selected) + (selected == 1 ? " hit" : " hits") : "Draw mode",
        juce::dontSendNotification);

    selectAllButton.setVisible(select); copyButton.setVisible(select && selected > 0);
    pasteButton.setVisible(select); velocityDownButton.setVisible(select && selected > 0);
    velocityUpButton.setVisible(select && selected > 0);
    timingEarlierButton.setVisible(select && selected > 0);
    timingResetButton.setVisible(select && selected > 0);
    timingLaterButton.setVisible(select && selected > 0);
    humanizeButton.setVisible(select && selected > 0);
    deleteSelectionButton.setVisible(select && selected > 0);

    if (!select)
        hintLabel.setText("D draw | S select | Shift-drag velocity | Alt-drag free timing | Ctrl-wheel zoom",
                          juce::dontSendNotification);
    else if (selected == 0)
        hintLabel.setText("A all | C copy | V paste | arrows move by grid | Alt+arrows duplicate | Z undo",
                          juce::dontSendNotification);
    else
        hintLabel.setText("Drag move | Alt-drag duplicate | arrows nudge | Quantize snaps to selected grid",
                          juce::dontSendNotification);
}

void GgdDrumEditor::publishModelChange(bool recordHistory)
{
    if (recordHistory)
        recordCommittedPatternEdit();
    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
}

void GgdDrumEditor::updatePersistenceTag()
{
    if (maps.isEmpty())
        return;
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const auto tag = "GGD4:" + mapPersistenceToken(maps.getReference(activeMapIndex));
    layer->setLayerName(tag.toRawUTF8());
}

juce::String GgdDrumEditor::mapPersistenceToken(const GgdKitMap& map) const
{
    if (map.id.containsIgnoreCase(".piv.")) return "piv";
    if (map.id.containsIgnoreCase(".pv.")) return "pv";
    if (map.id.containsIgnoreCase("modern")) return "mm";
    return map.id.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ")
        .replaceCharacter(' ', '-').toLowerCase();
}

int GgdDrumEditor::mapIndexFromLayerName(const juce::String& layerName) const
{
    if (!layerName.startsWith("GGD:") && !layerName.startsWith("GGD2:")
        && !layerName.startsWith("GGD3:") && !layerName.startsWith("GGD4:"))
        return 0;
    const auto remainder = layerName.fromFirstOccurrenceOf(":", false, false);
    const auto token = remainder.upToFirstOccurrenceOf(":", false, false).trim();
    for (int i = 0; i < maps.size(); ++i)
        if (mapPersistenceToken(maps.getReference(i)) == token)
            return i;
    return 0;
}

bool GgdDrumEditor::parseMeterFromLayerName(const juce::String& layerName,
                                            int& numerator,
                                            int& denominator) const
{
    if (!layerName.startsWith("GGD3:"))
        return false;
    auto remainder = layerName.fromFirstOccurrenceOf(":", false, false);
    remainder = remainder.fromFirstOccurrenceOf(":", false, false);
    const auto numeratorText = remainder.upToFirstOccurrenceOf("/", false, false);
    const auto denominatorText = remainder.fromFirstOccurrenceOf("/", false, false);
    const int n = numeratorText.getIntValue();
    const int d = denominatorText.getIntValue();
    if (n < 1 || n > 32 || (d != 4 && d != 8 && d != 16))
        return false;
    numerator = n;
    denominator = d;
    return true;
}
