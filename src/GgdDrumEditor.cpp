#include "GgdDrumEditor.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr juce::uint32 bg       = 0xff101316;
constexpr juce::uint32 panel    = 0xff171b1f;
constexpr juce::uint32 panel2   = 0xff1c2126;
constexpr juce::uint32 border   = 0xff2a3138;
constexpr juce::uint32 text     = 0xffe6ebef;
constexpr juce::uint32 muted    = 0xff8e9aa5;
constexpr juce::uint32 accent   = 0xff70d6c1;
constexpr juce::uint32 accent2  = 0xff449f91;
constexpr juce::uint32 warning  = 0xffe4b66a;

juce::Colour c(juce::uint32 value) { return juce::Colour(value); }
}

class GgdDrumGrid : public juce::Component
{
public:
    GgdDrumGrid(SeqAudioProcessor& p,
                const juce::Array<GgdCanonicalRow>& canonical)
        : processor(p), canonicalRows(canonical)
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);
    }

    void setMap(const GgdKitMap* newMap)
    {
        map = newMap;
        rebuildLayout();
        repaint();
    }

    void refreshSize()
    {
        rebuildLayout();
        repaint();
    }

    void setPlayPosition(int newPosition)
    {
        if (playPosition == newPosition)
            return;
        playPosition = newPosition;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(c(bg));

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();
        const int pattern = layer->getCurrentPattern();

        // Step ruler.
        g.setColour(c(panel));
        g.fillRect(0, 0, getWidth(), rulerHeight);
        g.setColour(c(border));
        g.drawHorizontalLine(rulerHeight - 1, 0.0f, static_cast<float>(getWidth()));

        g.setFont(12.0f);
        g.setColour(c(muted));
        g.drawText("ARTICULATION", 18, 0, nameWidth - 24, rulerHeight,
                   juce::Justification::centredLeft, false);

        for (int step = 0; step < numSteps; ++step)
        {
            const int x = nameWidth + step * stepWidth;
            if (step % 16 == 0)
            {
                const int bar = step / 16 + 1;
                g.setColour(c(text));
                g.setFont(juce::Font(12.0f, juce::Font::bold));
                g.drawText(juce::String(bar), x + 4, 3, stepWidth * 4 - 8, 18,
                           juce::Justification::centredLeft, false);
            }

            if (step % 4 == 0)
            {
                g.setColour(c(muted));
                g.setFont(10.5f);
                const int beat = (step / 4) % 4 + 1;
                g.drawText(juce::String(beat), x, 21, stepWidth, 18,
                           juce::Justification::centred, false);
            }
        }

        // Playback column, underneath hits.
        if (playPosition >= 0 && playPosition < numSteps)
        {
            const int x = nameWidth + playPosition * stepWidth;
            g.setColour(c(accent).withAlpha(0.08f));
            g.fillRect(x, rulerHeight, stepWidth, getHeight() - rulerHeight);
        }

        for (int i = 0; i < static_cast<int>(layout.size()); ++i)
        {
            const auto& item = layout[static_cast<size_t>(i)];
            if (item.header)
            {
                g.setColour(c(panel));
                g.fillRect(0, item.y, getWidth(), item.height);
                g.setColour(c(border));
                g.drawHorizontalLine(item.y + item.height - 1, 0.0f,
                                     static_cast<float>(getWidth()));
                g.setColour(c(muted));
                g.setFont(juce::Font(10.5f, juce::Font::bold));
                g.drawText(item.groupLabel.toUpperCase(), 18, item.y,
                           nameWidth - 24, item.height,
                           juce::Justification::centredLeft, false);
                continue;
            }

            const bool alternate = (i % 2) != 0;
            g.setColour(alternate ? c(panel2).withAlpha(0.42f) : c(bg));
            g.fillRect(0, item.y, getWidth(), item.height);

            g.setColour(c(border).withAlpha(0.55f));
            g.drawHorizontalLine(item.y + item.height - 1, 0.0f,
                                 static_cast<float>(getWidth()));

            g.setColour(c(text));
            g.setFont(12.5f);
            g.drawText(item.label, 18, item.y + 1, nameWidth - 86,
                       item.height - 2, juce::Justification::centredLeft, true);

            g.setColour(c(muted));
            g.setFont(10.5f);
            g.drawText(item.noteName, nameWidth - 70, item.y + 1, 52,
                       item.height - 2, juce::Justification::centredRight, false);

            for (int step = 0; step < numSteps; ++step)
            {
                const int x = nameWidth + step * stepWidth;

                if (step % 16 == 0)
                    g.setColour(c(border).brighter(0.28f));
                else if (step % 4 == 0)
                    g.setColour(c(border));
                else
                    g.setColour(c(border).withAlpha(0.38f));

                g.drawVerticalLine(x, static_cast<float>(item.y),
                                   static_cast<float>(item.y + item.height));

                const int prob = layer->getProb(item.canonicalRow, step, pattern);
                if (prob < 0)
                    continue;

                int velocity = layer->getVel(item.canonicalRow, step, pattern);
                velocity = juce::jlimit(1, 127, velocity <= 0 ? 1 : velocity);
                const float v = static_cast<float>(velocity) / 127.0f;

                const float hitW = std::min(20.0f, static_cast<float>(stepWidth) - 10.0f);
                const float hitH = 7.0f + 13.0f * v;
                const float cx = static_cast<float>(x) + stepWidth * 0.5f;
                const float cy = static_cast<float>(item.y) + item.height * 0.5f;
                juce::Rectangle<float> hit(cx - hitW * 0.5f, cy - hitH * 0.5f, hitW, hitH);

                g.setColour(c(accent).withAlpha(0.48f + 0.48f * v));
                g.fillRoundedRectangle(hit, 4.0f);

                const int offset = layer->getOffset(item.canonicalRow, step, pattern);
                if (offset != 0)
                {
                    const float markerX = cx + (static_cast<float>(offset) / 50.0f)
                                                  * (stepWidth * 0.30f);
                    g.setColour(c(text).withAlpha(0.88f));
                    g.drawVerticalLine(static_cast<int>(std::round(markerX)),
                                       cy - hitH * 0.32f, cy + hitH * 0.32f);
                }
            }

            const int endX = nameWidth + numSteps * stepWidth;
            g.setColour(c(border));
            g.drawVerticalLine(endX, static_cast<float>(item.y),
                               static_cast<float>(item.y + item.height));
        }

        g.setColour(c(border));
        g.drawVerticalLine(nameWidth, 0.0f, static_cast<float>(getHeight()));
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragMode = DragMode::none;
        lastPaintRow = -1;
        lastPaintStep = -1;

        int canonicalRow = -1;
        int step = -1;
        if (!cellAt(e.position, canonicalRow, step))
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const bool isOn = layer->getProb(canonicalRow, step, pattern) >= 0;

        dragRow = canonicalRow;
        dragStep = step;
        dragStart = e.position;

        if (e.mods.isShiftDown() && isOn)
        {
            dragMode = DragMode::velocity;
            dragStartValue = juce::jlimit(1, 127,
                static_cast<int>(layer->getVel(canonicalRow, step, pattern)));
            return;
        }

        if (e.mods.isAltDown() && isOn)
        {
            dragMode = DragMode::timing;
            dragStartValue = layer->getOffset(canonicalRow, step, pattern);
            return;
        }

        if (e.mods.isRightButtonDown())
        {
            dragMode = DragMode::erase;
            setCell(canonicalRow, step, false);
        }
        else
        {
            dragMode = isOn ? DragMode::erase : DragMode::paint;
            setCell(canonicalRow, step, !isOn);
        }

        lastPaintRow = canonicalRow;
        lastPaintStep = step;
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();

        if (dragMode == DragMode::velocity && dragRow >= 0)
        {
            const int delta = static_cast<int>((dragStart.y - e.position.y) * 1.5f);
            const int velocity = juce::jlimit(1, 127, dragStartValue + delta);
            layer->setVel(dragRow, dragStep, static_cast<int8_t>(velocity), pattern);
            repaint();
            return;
        }

        if (dragMode == DragMode::timing && dragRow >= 0)
        {
            const int delta = static_cast<int>((e.position.x - dragStart.x) * 1.8f);
            const int offset = juce::jlimit(-50, 50, dragStartValue + delta);
            layer->setOffset(dragRow, dragStep, static_cast<int8_t>(offset), pattern);
            repaint();
            return;
        }

        if (dragMode != DragMode::paint && dragMode != DragMode::erase)
            return;

        int canonicalRow = -1;
        int step = -1;
        if (!cellAt(e.position, canonicalRow, step))
            return;

        if (canonicalRow == lastPaintRow && step == lastPaintStep)
            return;

        setCell(canonicalRow, step, dragMode == DragMode::paint);
        lastPaintRow = canonicalRow;
        lastPaintStep = step;
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragMode != DragMode::none)
            publishChange();

        dragMode = DragMode::none;
    }

    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override
    {
        int canonicalRow = -1;
        int step = -1;
        if (!cellAt(e.position, canonicalRow, step))
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        if (layer->getProb(canonicalRow, step, pattern) < 0)
            return;

        int velocity = layer->getVel(canonicalRow, step, pattern);
        velocity += static_cast<int>(std::round(wheel.deltaY * 24.0f));
        velocity = juce::jlimit(1, 127, velocity);
        layer->setVel(canonicalRow, step, static_cast<int8_t>(velocity), pattern);
        publishChange();
        repaint();
    }

private:
    struct LayoutItem
    {
        bool header = false;
        int y = 0;
        int height = 0;
        int canonicalRow = -1;
        juce::String groupLabel;
        juce::String label;
        juce::String noteName;
    };

    enum class DragMode { none, paint, erase, velocity, timing };

    SeqAudioProcessor& processor;
    const juce::Array<GgdCanonicalRow>& canonicalRows;
    const GgdKitMap* map = nullptr;
    std::vector<LayoutItem> layout;

    int playPosition = -1;
    DragMode dragMode = DragMode::none;
    int dragRow = -1;
    int dragStep = -1;
    int dragStartValue = 0;
    juce::Point<float> dragStart;
    int lastPaintRow = -1;
    int lastPaintStep = -1;

    static constexpr int nameWidth = 238;
    static constexpr int stepWidth = 38;
    static constexpr int rulerHeight = 42;
    static constexpr int groupHeight = 25;
    static constexpr int rowHeight = 34;

    void rebuildLayout()
    {
        layout.clear();
        int y = rulerHeight;

        if (map != nullptr)
        {
            for (const auto& group : map->groups)
            {
                LayoutItem headerItem;
                headerItem.header = true;
                headerItem.y = y;
                headerItem.height = groupHeight;
                headerItem.groupLabel = group.label;
                layout.push_back(headerItem);
                y += groupHeight;

                for (const auto& articulation : group.articulations)
                {
                    const int canonical = GgdKitMapLibrary::findCanonicalRow(
                        canonicalRows, articulation.semanticId);
                    if (canonical < 0)
                        continue;

                    LayoutItem row;
                    row.y = y;
                    row.height = rowHeight;
                    row.canonicalRow = canonical;
                    row.label = articulation.label;
                    if (const auto* binding = articulation.primaryNoteBinding())
                        row.noteName = binding->noteName;
                    else
                        row.noteName = "—";
                    layout.push_back(row);
                    y += rowHeight;
                }
            }
        }

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int width = std::max(840, nameWidth + layer->getNumSteps() * stepWidth + 24);
        setSize(width, std::max(320, y + 18));
    }

    bool cellAt(juce::Point<float> p, int& canonicalRow, int& step) const
    {
        if (p.x < nameWidth || p.y < rulerHeight)
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        step = static_cast<int>((p.x - nameWidth) / stepWidth);
        if (step < 0 || step >= layer->getNumSteps())
            return false;

        for (const auto& item : layout)
        {
            if (!item.header && p.y >= item.y && p.y < item.y + item.height)
            {
                canonicalRow = item.canonicalRow;
                return true;
            }
        }

        return false;
    }

    void setCell(int row, int step, bool on)
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();

        if (!on)
        {
            layer->clearCell(row, step);
            return;
        }

        layer->setProb(row, step, SEQ_PROB_ON, pattern);
        if (layer->getVel(row, step, pattern) <= 0)
            layer->setVel(row, step, 100, pattern);
    }

    void publishChange()
    {
        processor.mData.swap();
        processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    }
};

GgdDrumEditor::GgdDrumEditor(SeqAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    configureLookAndFeel();
    setLookAndFeel(&lookAndFeel);

    maps = GgdKitMapLibrary::loadBuiltInMaps();
    canonicalRows = GgdKitMapLibrary::buildCanonicalRows(maps);

    grid = std::make_unique<GgdDrumGrid>(processor, canonicalRows);
    gridViewport.setViewedComponent(grid.get(), false);
    gridViewport.setScrollBarsShown(true, true);
    gridViewport.setScrollBarThickness(11);
    addAndMakeVisible(gridViewport);

    productLabel.setText("STOCHAS GGD", juce::dontSendNotification);
    productLabel.setFont(juce::Font(17.0f, juce::Font::bold));
    productLabel.setColour(juce::Label::textColourId, c(text));
    addAndMakeVisible(productLabel);

    transportStatus.setText("Ready", juce::dontSendNotification);
    transportStatus.setFont(juce::Font(11.5f));
    transportStatus.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(transportStatus);

    kitSelector.setTooltip("Destination GGD mapping");
    for (int i = 0; i < maps.size(); ++i)
        kitSelector.addItem(maps.getReference(i).library, i + 1);
    kitSelector.onChange = [this]
    {
        if (kitSelector.getSelectedId() > 0)
            setActiveMap(kitSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(kitSelector);

    patternSelector.setTooltip("Stochas pattern slot");
    for (int i = 1; i <= SEQ_MAX_PATTERNS; ++i)
        patternSelector.addItem("Pattern " + juce::String(i), i);
    patternSelector.onChange = [this]
    {
        const int selected = patternSelector.getSelectedId();
        if (selected <= 0)
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        layer->setCurrentPattern(selected - 1);
        publishModelChange();
        refreshControlsFromModel();
        grid->repaint();
    };
    addAndMakeVisible(patternSelector);

    barsSelector.setTooltip("Pattern length in 4/4 bars");
    for (int bars = 1; bars <= 4; ++bars)
        barsSelector.addItem(juce::String(bars) + (bars == 1 ? " bar" : " bars"), bars);
    barsSelector.onChange = [this]
    {
        const int bars = barsSelector.getSelectedId();
        if (bars <= 0)
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        layer->setNumSteps(bars * 16);
        layer->setStepsPerMeasure(16);
        layer->setClockDivider(SEQ_DEFAULT_CLOCK_DIV);
        publishModelChange();
        grid->refreshSize();
    };
    addAndMakeVisible(barsSelector);

    patternName.setMultiLine(false);
    patternName.setReturnKeyStartsNewLine(false);
    patternName.setInputRestrictions(SEQ_PATTERN_NAME_MAXLEN - 1);
    patternName.setTextToShowWhenEmpty("Pattern name", c(muted));
    patternName.onReturnKey = [this]
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        layer->setPatternName(patternName.getText().toRawUTF8());
        publishModelChange();
        patternName.giveAwayKeyboardFocus();
    };
    patternName.onFocusLost = [this]
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        if (patternName.getText() != juce::String(layer->getPatternName()))
        {
            layer->setPatternName(patternName.getText().toRawUTF8());
            publishModelChange();
        }
    };
    addAndMakeVisible(patternName);

    undoButton.setTooltip("Undo the most recent sequence edit");
    undoButton.onClick = [this]
    {
        processor.mData.undo();
        refreshControlsFromModel();
        grid->refreshSize();
    };
    addAndMakeVisible(undoButton);

    clearButton.setTooltip("Clear the current pattern");
    clearButton.onClick = [this] { clearCurrentPattern(); };
    addAndMakeVisible(clearButton);

    hintLabel.setText("Click/drag: paint  •  Right-drag: erase  •  Shift-drag: velocity  •  Alt-drag: timing  •  Wheel: velocity",
                      juce::dontSendNotification);
    hintLabel.setFont(juce::Font(11.0f));
    hintLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(hintLabel);

    initialiseDrumState();
    refreshControlsFromModel();
    if (!maps.isEmpty())
        grid->setMap(&maps.getReference(activeMapIndex));

    setResizable(true, true);
    setResizeLimits(760, 480, 1600, 1200);
    setSize(1080, 720);
    startTimerHz(30);
}

GgdDrumEditor::~GgdDrumEditor()
{
    stopTimer();
    gridViewport.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
}

void GgdDrumEditor::configureLookAndFeel()
{
    lookAndFeel.setColour(juce::ComboBox::backgroundColourId, c(panel2));
    lookAndFeel.setColour(juce::ComboBox::textColourId, c(text));
    lookAndFeel.setColour(juce::ComboBox::outlineColourId, c(border));
    lookAndFeel.setColour(juce::ComboBox::arrowColourId, c(muted));
    lookAndFeel.setColour(juce::PopupMenu::backgroundColourId, c(panel2));
    lookAndFeel.setColour(juce::PopupMenu::textColourId, c(text));
    lookAndFeel.setColour(juce::PopupMenu::highlightedBackgroundColourId, c(accent2));
    lookAndFeel.setColour(juce::PopupMenu::highlightedTextColourId, c(text));
    lookAndFeel.setColour(juce::TextEditor::backgroundColourId, c(panel2));
    lookAndFeel.setColour(juce::TextEditor::textColourId, c(text));
    lookAndFeel.setColour(juce::TextEditor::outlineColourId, c(border));
    lookAndFeel.setColour(juce::TextEditor::focusedOutlineColourId, c(accent2));
    lookAndFeel.setColour(juce::TextButton::buttonColourId, c(panel2));
    lookAndFeel.setColour(juce::TextButton::buttonOnColourId, c(accent2));
    lookAndFeel.setColour(juce::TextButton::textColourOffId, c(text));
    lookAndFeel.setColour(juce::TextButton::textColourOnId, c(text));
    lookAndFeel.setColour(juce::ScrollBar::thumbColourId, c(border).brighter(0.32f));
    lookAndFeel.setColour(juce::ScrollBar::trackColourId, c(bg));
}

void GgdDrumEditor::paint(juce::Graphics& g)
{
    g.fillAll(c(bg));

    g.setColour(c(panel));
    g.fillRect(0, 0, getWidth(), 72);
    g.setColour(c(border));
    g.drawHorizontalLine(71, 0.0f, static_cast<float>(getWidth()));

    g.setColour(c(panel));
    g.fillRect(0, getHeight() - 34, getWidth(), 34);
    g.setColour(c(border));
    g.drawHorizontalLine(getHeight() - 34, 0.0f, static_cast<float>(getWidth()));
}

void GgdDrumEditor::resized()
{
    const int pad = 12;
    auto top = juce::Rectangle<int>(pad, 9, getWidth() - pad * 2, 52);

    productLabel.setBounds(top.removeFromLeft(142));
    transportStatus.setBounds(top.removeFromLeft(76));
    top.removeFromLeft(8);

    kitSelector.setBounds(top.removeFromLeft(210).reduced(0, 7));
    top.removeFromLeft(8);
    patternSelector.setBounds(top.removeFromLeft(116).reduced(0, 7));
    top.removeFromLeft(8);
    barsSelector.setBounds(top.removeFromLeft(92).reduced(0, 7));
    top.removeFromLeft(8);

    const int buttonWidth = 62;
    clearButton.setBounds(top.removeFromRight(buttonWidth).reduced(0, 7));
    top.removeFromRight(6);
    undoButton.setBounds(top.removeFromRight(buttonWidth).reduced(0, 7));
    top.removeFromRight(8);
    patternName.setBounds(top.reduced(0, 7));

    gridViewport.setBounds(0, 72, getWidth(), getHeight() - 106);
    hintLabel.setBounds(14, getHeight() - 33, getWidth() - 28, 32);
}

void GgdDrumEditor::timerCallback()
{
    const int pos = processor.mNotifier.getPlayPosition(0);
    grid->setPlayPosition(pos);

    if (pos >= 0)
    {
        transportStatus.setColour(juce::Label::textColourId, c(accent));
        transportStatus.setText("PLAY  " + juce::String(pos + 1), juce::dontSendNotification);
    }
    else
    {
        transportStatus.setColour(juce::Label::textColourId, c(muted));
        transportStatus.setText("Ready", juce::dontSendNotification);
    }

    if (processor.mNotifier.doesUINeedUpdate())
    {
        refreshControlsFromModel();
        grid->refreshSize();
    }
}

void GgdDrumEditor::initialiseDrumState()
{
    if (maps.isEmpty() || canonicalRows.isEmpty())
        return;

    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const juce::String oldLayerName(layer->getLayerName());
    const bool alreadyConfigured = oldLayerName.startsWith("GGD:");

    if (!alreadyConfigured)
    {
        seq->clearLayer(0);
        layer = seq->getLayer(0);
        layer->setNumSteps(16);
        layer->setStepsPerMeasure(16);
        layer->setClockDivider(SEQ_DEFAULT_CLOCK_DIV);
        layer->setMidiChannel(1);
        layer->setCurrentPattern(0);

        for (int i = 1; i < SEQ_MAX_LAYERS; ++i)
            seq->getLayer(i)->setMuted(true);
    }

    activeMapIndex = alreadyConfigured ? mapIndexFromLayerName(oldLayerName) : 0;
    activeMapIndex = juce::jlimit(0, maps.size() - 1, activeMapIndex);

    layer->setNoteSource(true);
    layer->setMonoMode(false);
    layer->setMaxRows(juce::jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, canonicalRows.size()));
    layer->setMaxPoly(juce::jmin(SEQ_DEFAULT_MAX_POLY, layer->getMaxRows()));

    applyActiveMapBindings(false);
    publishModelChange();
}

void GgdDrumEditor::applyActiveMapBindings(bool publish)
{
    if (maps.isEmpty())
        return;

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const auto& map = maps.getReference(activeMapIndex);

    const int rowCount = juce::jmin(canonicalRows.size(), SEQ_MAX_ROWS);
    layer->setMaxRows(juce::jmax(SEQ_MIN_ROWS, rowCount));
    layer->setNoteSource(true);

    for (int row = 0; row < rowCount; ++row)
    {
        const auto& canonical = canonicalRows.getReference(row);
        const auto* articulation = map.findArticulation(canonical.semanticId);
        const auto* binding = articulation != nullptr ? articulation->primaryNoteBinding() : nullptr;

        // StochaEngine treats MIDI note -1 as a deliberately silent row. This
        // lets semantic hits survive kit changes even when a kit lacks an articulation.
        const int midi = binding != nullptr ? binding->midi : SEQ_NOTE_OFF;
        layer->setNote(row, static_cast<int8_t>(midi), true);

        const auto displayName = (articulation != nullptr ? articulation->label
                                                          : canonical.defaultLabel)
                                     .substring(0, SEQ_MAX_NOTELABEL_LEN - 1);
        layer->setNoteName(row, displayName.toRawUTF8());
    }

    layer->setLayerName(("GGD:" + mapPersistenceToken(map)).toRawUTF8());

    if (publish)
        publishModelChange();
}

void GgdDrumEditor::refreshControlsFromModel()
{
    if (maps.isEmpty())
        return;

    auto* layer = processor.mData.getUISeqData()->getLayer(0);

    const int mapFromState = mapIndexFromLayerName(layer->getLayerName());
    if (mapFromState >= 0 && mapFromState < maps.size())
        activeMapIndex = mapFromState;

    kitSelector.setSelectedId(activeMapIndex + 1, juce::dontSendNotification);
    patternSelector.setSelectedId(layer->getCurrentPattern() + 1, juce::dontSendNotification);
    patternName.setText(layer->getPatternName(), false);

    const int bars = juce::jlimit(1, 4, (layer->getNumSteps() + 15) / 16);
    barsSelector.setSelectedId(bars, juce::dontSendNotification);

    grid->setMap(&maps.getReference(activeMapIndex));
}

void GgdDrumEditor::setActiveMap(int index)
{
    if (index < 0 || index >= maps.size() || index == activeMapIndex)
        return;

    activeMapIndex = index;
    applyActiveMapBindings(true);
    grid->setMap(&maps.getReference(activeMapIndex));
}

void GgdDrumEditor::publishModelChange()
{
    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
}

void GgdDrumEditor::clearCurrentPattern()
{
    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    seq->clearPattern(0, layer->getCurrentPattern());
    publishModelChange();
    grid->repaint();
}

juce::String GgdDrumEditor::mapPersistenceToken(const GgdKitMap& map) const
{
    if (map.id.containsIgnoreCase(".pv."))
        return "pv";
    if (map.id.containsIgnoreCase(".piv."))
        return "piv";
    if (map.id.containsIgnoreCase("modern"))
        return "mm";
    return juce::String(activeMapIndex);
}

int GgdDrumEditor::mapIndexFromLayerName(const juce::String& layerName) const
{
    if (!layerName.startsWith("GGD:"))
        return 0;

    const auto token = layerName.fromFirstOccurrenceOf("GGD:", false, false).trim();
    for (int i = 0; i < maps.size(); ++i)
        if (mapPersistenceToken(maps.getReference(i)) == token)
            return i;

    return 0;
}