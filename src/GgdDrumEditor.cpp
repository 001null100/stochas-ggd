#include "GgdDrumEditor.h"
#include "GgdMidiExporter.h"
#include "GgdMidiImporter.h"
#include "GgdPatternFile.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::Colour tc(GgdThemeRole role)
{
    return ggdThemeColour(role);
}

double nowMs()
{
    return juce::Time::getMillisecondCounterHiRes();
}

struct GridChoice
{
    int ticks;
    const char* name;
};

constexpr GridChoice gridChoices[] = {
    { GGD_EVENT_PPQ / 2, "1/8" },
    { GGD_TICKS_PER_16TH, "1/16" },
    { GGD_TICKS_PER_32ND, "1/32" },
    { GGD_TICKS_PER_8TH_TRIPLET, "1/8T" },
    { GGD_TICKS_PER_16TH_TRIPLET, "1/16T" },
    { GGD_EVENT_PPQ / 12, "1/32T" },
};

juce::String gridName(int ticks)
{
    for (const auto& choice : gridChoices)
        if (choice.ticks == ticks)
            return choice.name;
    return "1/16";
}

// The grid used when zoomed in far enough for "finer grid" to kick in.
int finerGrid(int ticks)
{
    if (ticks == GGD_TICKS_PER_32ND || ticks == GGD_EVENT_PPQ / 12)
        return ticks;
    return ticks / 2;
}

constexpr float fineGridZoom = 3.5f;
constexpr int denominators[] = { 2, 4, 8, 16 };
constexpr int chanceValues[] = { 25, 50, 75, 100 };

juce::String nameOf(const SequenceLayer& layer, int slot)
{
    return juce::String::fromUTF8(layer.getPatternName(slot));
}

bool isDefaultName(const juce::String& name)
{
    return name.trim().isEmpty() || name == SEQ_DEFAULT_PAT_NAME;
}

juce::String displayName(const SequenceLayer& layer, int slot)
{
    const auto name = nameOf(layer, slot);
    return isDefaultName(name) ? "Pattern " + juce::String(slot + 1) : name;
}

// Longest prefix of text whose UTF-8 form fits a pattern name buffer.
juce::String fitPatternName(juce::String text)
{
    text = text.trim();
    while (text.getNumBytesAsUTF8() > SEQ_PATTERN_NAME_MAXLEN - 1)
        text = text.dropLastCharacters(1);
    return text;
}

bool sameEvent(const GgdSequenceEvent& a, const GgdSequenceEvent& b)
{
    return a.tick == b.tick && a.row == b.row && a.velocity == b.velocity
        && a.probability == b.probability && a.durationTicks == b.durationTicks;
}

bool sameParked(const GgdParkedHit& a, const GgdParkedHit& b)
{
    return a.bar == b.bar && a.offset == b.offset && a.row == b.row && a.velocity == b.velocity
        && a.probability == b.probability && a.durationTicks == b.durationTicks;
}

bool sameParkedList(const std::vector<GgdParkedHit>& a, const std::vector<GgdParkedHit>& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), sameParked);
}

bool parkedMatches(const GgdEventPattern& pattern, const std::vector<GgdParkedHit>& hits)
{
    if (pattern.getParkedCount() != static_cast<int>(hits.size()))
        return false;
    for (int i = 0; i < pattern.getParkedCount(); ++i)
        if (!sameParked(*pattern.getParked(i), hits[static_cast<size_t>(i)]))
            return false;
    return true;
}

juce::String plural(int count, const juce::String& singular, const juce::String& pluralForm)
{
    return juce::String(count) + " " + (count == 1 ? singular : pluralForm);
}
}

//==============================================================================
// Eight pattern slots as one segmented control. The current slot is filled,
// slots with content carry a dot and the slot the engine is playing gets a
// playhead-coloured bar.
class GgdDrumEditor::SlotStrip final : public juce::Component
{
public:
    std::function<void(int)> onSelect;
    std::function<void(int)> onMenu;

    SlotStrip()
    {
        for (int i = 0; i < SEQ_MAX_PATTERNS; ++i)
            addAndMakeVisible(slots.add(new Slot(*this, i)));
    }

    void setState(int newCurrent, int newPlaying, const std::array<bool, SEQ_MAX_PATTERNS>& content)
    {
        const bool changed = current != newCurrent || playing != newPlaying || filled != content;
        current = newCurrent;
        playing = newPlaying;
        filled = content;
        if (changed)
            repaintAll();
    }

    void setSlotTooltip(int slot, const juce::String& text)
    {
        if (auto* s = slots[slot])
            s->setTooltip(text);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(tc(GgdThemeRole::panelRaised));
        g.fillRoundedRectangle(bounds, 6.0f);
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        g.setColour(tc(GgdThemeRole::border));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const float width = static_cast<float>(area.getWidth()) / SEQ_MAX_PATTERNS;
        for (int i = 0; i < slots.size(); ++i)
        {
            const int x0 = juce::roundToInt(width * i);
            const int x1 = juce::roundToInt(width * (i + 1));
            slots[i]->setBounds(x0, 0, x1 - x0, area.getHeight());
        }
    }

private:
    class Slot final : public juce::Button
    {
    public:
        Slot(SlotStrip& strip, int slotIndex)
            : juce::Button(juce::String(slotIndex + 1)), owner(strip), index(slotIndex)
        {
            setMouseClickGrabsKeyboardFocus(false);
            setWantsKeyboardFocus(false);
            setTriggeredOnMouseDown(true);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (owner.onMenu)
                    owner.onMenu(index);
                return;
            }
            juce::Button::mouseDown(e);
        }

        void clicked() override
        {
            if (owner.onSelect)
                owner.onSelect(index);
        }

        void paintButton(juce::Graphics& g, bool highlighted, bool down) override
        {
            const bool first = index == 0;
            const bool last = index == SEQ_MAX_PATTERNS - 1;
            const bool isCurrent = owner.current == index;
            const bool hasContent = owner.filled[static_cast<size_t>(index)];
            const bool isPlaying = owner.playing == index;
            auto bounds = getLocalBounds().toFloat().reduced(first ? 0.5f : 0.0f, 0.5f);
            if (last)
                bounds.removeFromRight(0.5f);

            juce::Path shape;
            shape.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 6.0f, 6.0f,
                                      first, last, first, last);

            if (isCurrent)
            {
                g.setColour(tc(GgdThemeRole::accent).withAlpha(down ? 0.8f : 0.92f));
                g.fillPath(shape);
            }
            else if (highlighted || down)
            {
                g.setColour(tc(GgdThemeRole::panelSoft).withAlpha(down ? 1.0f : 0.85f));
                g.fillPath(shape);
            }

            if (!first && !isCurrent && owner.current != index - 1)
            {
                g.setColour(tc(GgdThemeRole::borderSoft));
                g.fillRect(bounds.getX(), bounds.getY() + 6.0f, 1.0f, bounds.getHeight() - 12.0f);
            }

            const auto textColour = isCurrent ? tc(GgdThemeRole::accentText)
                                  : hasContent ? tc(GgdThemeRole::text)
                                               : tc(GgdThemeRole::faint);
            g.setColour(textColour);
            g.setFont(isCurrent ? GgdFonts::bold(12.5f) : GgdFonts::semibold(12.5f));
            g.drawText(getButtonText(), bounds.withTrimmedBottom(5.0f), juce::Justification::centred, false);

            if (isPlaying)
            {
                g.setColour(tc(GgdThemeRole::playhead));
                g.fillRoundedRectangle(juce::Rectangle<float>(bounds.getWidth() * 0.5f, 2.5f)
                                           .withCentre({ bounds.getCentreX(), bounds.getBottom() - 6.0f }),
                                       1.25f);
            }
            else if (hasContent)
            {
                g.setColour(isCurrent ? tc(GgdThemeRole::accentText).withAlpha(0.7f)
                                      : tc(GgdThemeRole::accent).withAlpha(0.85f));
                g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre({ bounds.getCentreX(),
                                                                              bounds.getBottom() - 6.5f }));
            }
        }

    private:
        SlotStrip& owner;
        int index;
    };

    juce::OwnedArray<Slot> slots;
    int current = 0;
    int playing = -1;
    std::array<bool, SEQ_MAX_PATTERNS> filled {};

    void repaintAll()
    {
        repaint();
        for (auto* slot : slots)
            slot->repaint();
    }
};

//==============================================================================
// Bar count: [-] value [+]. Click the value to type, scroll to change.
class GgdDrumEditor::BarsStepper final : public juce::Component,
                                         public juce::SettableTooltipClient,
                                         private juce::Timer
{
public:
    std::function<void(int)> onChange;
    std::function<void()> onEditingFinished;

    BarsStepper()
    {
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setRepaintsOnMouseActivity(true);

        editor.setJustification(juce::Justification::centred);
        editor.setInputRestrictions(2, "0123456789");
        editor.setSelectAllWhenFocused(true);
        editor.setFont(GgdFonts::semibold(13.0f));
        // TextEditor delivers these asynchronously. Only an explicit Return
        // or Escape hands focus back; when focus moved elsewhere it stays.
        editor.onReturnKey = [this]
        {
            commitText();
            returnFocus();
        };
        editor.onEscapeKey = [this]
        {
            endEditing();
            returnFocus();
        };
        editor.onFocusLost = [this] { commitText(); };
        addChildComponent(editor);
    }

    void setValue(int newValue, int newMaximum)
    {
        newMaximum = juce::jmax(1, newMaximum);
        newValue = juce::jlimit(1, newMaximum, newValue);
        if (newValue != value || newMaximum != maximum)
        {
            value = newValue;
            maximum = newMaximum;
            repaint();
        }
    }

    bool isEditing() const { return editor.isVisible(); }

    // Applies a value being typed right away, before something else (a slot
    // switch) changes what the value would apply to.
    void finishEditing() { commitText(); }
    void cancelEditing() { endEditing(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(tc(GgdThemeRole::panelRaised));
        g.fillRoundedRectangle(bounds, 6.0f);

        const int hover = isMouseOver(true) ? zoneAt(getMouseXYRelative().x) : 0;
        auto minusArea = bounds.withWidth(zoneWidth);
        auto plusArea = bounds.withLeft(bounds.getRight() - zoneWidth);
        auto paintZone = [&](juce::Rectangle<float> area, int zone, bool enabled, bool plus)
        {
            if (hover == zone && enabled)
            {
                juce::Path path;
                path.addRoundedRectangle(area.getX(), area.getY(), area.getWidth(), area.getHeight(), 6.0f, 6.0f,
                                         !plus, plus, !plus, plus);
                g.setColour(tc(GgdThemeRole::panelSoft));
                g.fillPath(path);
            }
            const auto colour = enabled ? (hover == zone ? tc(GgdThemeRole::text) : tc(GgdThemeRole::muted))
                                        : tc(GgdThemeRole::faint).withAlpha(0.5f);
            g.setColour(colour);
            const auto c = area.getCentre();
            g.fillRoundedRectangle(juce::Rectangle<float>(9.0f, 1.6f).withCentre(c), 0.8f);
            if (plus)
                g.fillRoundedRectangle(juce::Rectangle<float>(1.6f, 9.0f).withCentre(c), 0.8f);
        };
        paintZone(minusArea, -1, value > 1, false);
        paintZone(plusArea, 1, value < maximum, true);

        g.setColour(tc(GgdThemeRole::borderSoft));
        g.fillRect(minusArea.getRight(), bounds.getY() + 6.0f, 1.0f, bounds.getHeight() - 12.0f);
        g.fillRect(plusArea.getX() - 1.0f, bounds.getY() + 6.0f, 1.0f, bounds.getHeight() - 12.0f);

        g.setColour(hover == 0 && isMouseOver(true) ? tc(GgdThemeRole::accent).withAlpha(0.7f)
                                                    : tc(GgdThemeRole::border));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        if (!editor.isVisible())
        {
            auto centre = bounds.reduced(zoneWidth, 0.0f);
            g.setColour(tc(GgdThemeRole::text));
            g.setFont(GgdFonts::semibold(12.5f));
            g.drawText(juce::String(value) + (value == 1 ? " bar" : " bars"), centre, juce::Justification::centred,
                       false);
        }
    }

    void resized() override
    {
        editor.setBounds(getLocalBounds().reduced(static_cast<int>(zoneWidth) + 2, 3));
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int zone = zoneAt(e.x);
        if (zone == 0)
        {
            beginEditing();
            return;
        }
        repeatDirection = zone;
        step(zone);
        startTimer(420);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        stopTimer();
        repeatDirection = 0;
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        wheelAccumulator += wheel.deltaY;
        while (std::abs(wheelAccumulator) >= 0.12f)
        {
            step(wheelAccumulator > 0.0f ? 1 : -1);
            wheelAccumulator -= wheelAccumulator > 0.0f ? 0.12f : -0.12f;
        }
    }

private:
    static constexpr float zoneWidth = 26.0f;
    juce::TextEditor editor;
    int value = 1;
    int maximum = GGD_MAX_PATTERN_BARS;
    int repeatDirection = 0;
    float wheelAccumulator = 0.0f;

    int zoneAt(int x) const
    {
        if (x < zoneWidth)
            return -1;
        if (x > getWidth() - zoneWidth)
            return 1;
        return 0;
    }

    void step(int direction)
    {
        const int next = juce::jlimit(1, maximum, value + direction);
        if (next == value)
            return;
        value = next;
        repaint();
        if (onChange)
            onChange(value);
    }

    void timerCallback() override
    {
        if (repeatDirection == 0 || !isMouseButtonDown())
        {
            stopTimer();
            return;
        }
        step(repeatDirection);
        startTimer(70);
    }

    void beginEditing()
    {
        editor.setColour(juce::TextEditor::backgroundColourId, tc(GgdThemeRole::panel));
        editor.setColour(juce::TextEditor::textColourId, tc(GgdThemeRole::text));
        editor.setText(juce::String(value), false);
        editor.setVisible(true);
        editor.grabKeyboardFocus();
        editor.selectAll();
        repaint();
    }

    void commitText()
    {
        if (!editor.isVisible())
            return;
        const int typed = editor.getText().getIntValue();
        endEditing();
        if (typed >= 1)
        {
            const int next = juce::jlimit(1, maximum, typed);
            if (next != value)
            {
                value = next;
                repaint();
                if (onChange)
                    onChange(value);
            }
        }
    }

    void endEditing()
    {
        if (!editor.isVisible())
            return;
        editor.setVisible(false);
        repaint();
    }

    void returnFocus()
    {
        if (onEditingFinished)
            onEditingFinished();
    }
};

//==============================================================================
// Play state, pattern position and the host meter.
class GgdDrumEditor::TransportDisplay final : public juce::Component,
                                              public juce::SettableTooltipClient
{
public:
    struct State
    {
        bool playing = false;
        bool resting = false;
        bool muted = false;
        juce::String position;
        juce::String hostMeter;
        bool mismatch = false;

        bool operator==(const State& o) const
        {
            return playing == o.playing && resting == o.resting && muted == o.muted && position == o.position
                && hostMeter == o.hostMeter && mismatch == o.mismatch;
        }
    };

    void setState(const State& next)
    {
        if (next == state)
            return;
        state = next;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(tc(GgdThemeRole::outside).withAlpha(0.7f));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(tc(GgdThemeRole::borderSoft));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        auto content = bounds.reduced(10.0f, 0.0f);
        auto icon = content.removeFromLeft(12.0f).withSizeKeepingCentre(10.0f, 10.0f);
        if (state.playing)
        {
            juce::Path play;
            play.addTriangle(icon.getX() + 1.0f, icon.getY(), icon.getX() + 1.0f, icon.getBottom(), icon.getRight(),
                             icon.getCentreY());
            g.setColour(state.muted ? tc(GgdThemeRole::warning) : tc(GgdThemeRole::success));
            g.fillPath(play);
        }
        else
        {
            g.setColour(tc(GgdThemeRole::faint));
            g.fillRoundedRectangle(icon.reduced(1.0f), 1.5f);
        }
        content.removeFromLeft(8.0f);

        if (state.hostMeter.isNotEmpty())
        {
            g.setFont(GgdFonts::medium(11.5f));
            const float width = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), state.hostMeter)
                              + (state.mismatch ? 18.0f : 0.0f) + 4.0f;
            auto meter = content.removeFromRight(width);
            const auto colour = state.mismatch ? tc(GgdThemeRole::warning) : tc(GgdThemeRole::muted);
            if (state.mismatch)
                ggdDrawIcon(g, GgdIcon::warning, meter.removeFromLeft(14.0f).withSizeKeepingCentre(13.0f, 13.0f),
                            colour, 1.4f);
            g.setColour(colour);
            g.drawText(state.hostMeter, meter, juce::Justification::centredRight, false);
            content.removeFromRight(6.0f);
        }

        if (state.playing && !state.resting)
        {
            g.setColour(tc(GgdThemeRole::text));
            g.setFont(GgdFonts::semibold(14.0f));
            g.drawText(state.position, content, juce::Justification::centredLeft, false);
            if (state.muted)
            {
                const float used = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), state.position) + 8.0f;
                g.setColour(tc(GgdThemeRole::warning));
                g.setFont(GgdFonts::semibold(10.5f));
                g.drawText("SEQ OFF", content.withTrimmedLeft(used), juce::Justification::centredLeft, false);
            }
        }
        else
        {
            g.setColour(tc(GgdThemeRole::muted));
            g.setFont(GgdFonts::medium(12.5f));
            g.drawText(state.playing ? "Waiting for bar" : "Stopped", content, juce::Justification::centredLeft,
                       false);
        }
    }

private:
    State state;
};

//==============================================================================
// Starts an external drag of a MIDI file once the pointer moves a little.
class GgdDrumEditor::MidiDragButton final : public GgdButton
{
public:
    using GgdButton::GgdButton;

    std::function<void(juce::Component*)> onBeginDrag;

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStarted = false;
        GgdButton::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragStarted && e.getDistanceFromDragStart() >= 5)
        {
            dragStarted = true;
            if (onBeginDrag)
                onBeginDrag(this);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!dragStarted)
            GgdButton::mouseUp(e);
        else
            setState(juce::Button::buttonNormal);
        dragStarted = false;
    }

private:
    bool dragStarted = false;
};

//==============================================================================
bool GgdDrumEditor::PatternState::sameContent(const PatternState& other) const
{
    return numerator == other.numerator && denominator == other.denominator && bars == other.bars
        && name == other.name && events.size() == other.events.size()
        && std::equal(events.begin(), events.end(), other.events.begin(), sameEvent)
        && sameParkedList(parked, other.parked);
}

//==============================================================================
GgdDrumEditor::GgdDrumEditor(SeqAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p), project(p.getProject())
{
    prefs = preferenceStore->load();
    appliedTheme = prefs.theme;
    ggdSetCurrentTheme(ggdThemeFromIndex(prefs.theme));
    lookAndFeel.syncThemeColours();
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);

    // Timeline ----------------------------------------------------------------
    GgdDrumGrid::Callbacks callbacks;
    callbacks.onEdit = [this] { commitEdit(); };
    callbacks.onSelectionChanged = [this] { handleSelectionChanged(); };
    callbacks.onToolChanged = [this](GgdDrumGrid::ToolMode)
    {
        refreshHeader();
        refreshContextStrip();
    };
    callbacks.onZoomChanged = [this](float zoom)
    {
        prefs.zoom = zoom;
        grid->setSnapTicks(effectiveGridTicks());
        refreshZoomControls();
        preferenceStore->save(prefs);
    };
    callbacks.onUndo = [this] { undo(); };
    callbacks.onRedo = [this] { redo(); };
    callbacks.onStatus = [this](const juce::String& text) { showStatus(text); };
    callbacks.onEditRow = [this](int canonicalRow)
    {
        if (!project.isDrumMachine(project.getActiveMapIndex(ui())))
            return;
        const auto& rows = project.getCanonicalRows();
        if (juce::isPositiveAndBelow(canonicalRow, rows.size()))
            openPadEditor(GgdDrumMachineKit::padForSemanticId(rows.getReference(canonicalRow).semanticId));
    };
    grid = std::make_unique<GgdDrumGrid>(processor, project, std::move(callbacks));

    viewport.setViewedComponent(grid.get(), false);
    viewport.setScrollBarsShown(true, true);
    viewport.setScrollBarThickness(10);
    viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    viewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(viewport);

    // Header, row 1 -----------------------------------------------------------
    slotStrip = std::make_unique<SlotStrip>();
    slotStrip->onSelect = [this](int slot) { selectPatternSlot(slot); };
    slotStrip->onMenu = [this](int slot) { showSlotMenu(slot); };
    addAndMakeVisible(*slotStrip);

    patternName.setFont(GgdFonts::medium(13.0f));
    patternName.setJustification(juce::Justification::centredLeft);
    patternName.setIndents(10, 0);
    patternName.setSelectAllWhenFocused(true);
    patternName.setTooltip("Pattern name, used when you save, export or drag the pattern");
    patternName.onReturnKey = [this]
    {
        renamePattern();
        focusGrid();
    };
    patternName.onEscapeKey = [this]
    {
        const auto name = nameOf(layer(), nameFieldSlot >= 0 ? nameFieldSlot : currentSlot());
        patternName.setText(isDefaultName(name) ? juce::String() : name, false);
        focusGrid();
    };
    patternName.onFocusLost = [this] { renamePattern(); };
    addAndMakeVisible(patternName);

    patternMenuButton.setTooltip("Pattern actions: duplicate, double length, save, import, export and clear");
    patternMenuButton.onClick = [this] { showPatternMenu(); };
    addAndMakeVisible(patternMenuButton);

    transport = std::make_unique<TransportDisplay>();
    addAndMakeVisible(*transport);

    outputButton.setTooltip("Turns the notes Stochas GGD plays on or off. MIDI coming from the host always "
                            "passes through. Saved with the project.");
    outputButton.onClick = [this] { toggleOutput(); };
    addAndMakeVisible(outputButton);

    dragButton = std::make_unique<MidiDragButton>("Drag MIDI", GgdIcon::drag);
    dragButton->setTooltip("Drag the current pattern onto a track in your DAW as a MIDI clip");
    dragButton->setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    dragButton->onClick = [this]
    {
        showStatus("Drag the Drag MIDI button onto a track to drop the pattern as a MIDI clip");
    };
    dragButton->onBeginDrag = [this](juce::Component* source) { beginMidiDrag(source); };
    addAndMakeVisible(*dragButton);

    libraryButton.setStyle(GgdButton::Style::quiet);
    libraryButton.setTooltip("Show or hide the groove and pattern library");
    libraryButton.onClick = [this]
    {
        auto next = prefs;
        next.showLibrary = !next.showLibrary;
        applyPreferences(next, true);
    };
    addAndMakeVisible(libraryButton);

    settingsButton.setStyle(GgdButton::Style::quiet);
    settingsButton.setTooltip("Settings");
    settingsButton.onClick = [this] { openSettings(); };
    addAndMakeVisible(settingsButton);

    // Header, row 2 -----------------------------------------------------------
    drawButton.setSegment(false, true);
    selectButton.setSegment(true, false);
    drawButton.setTooltip("Draw: click to add hits, drag to paint, right-drag to erase " + shortcut("D"));
    selectButton.setTooltip("Select: drag a box to select hits, then move, copy or transform them "
                            + shortcut("S"));
    drawButton.onClick = [this]
    {
        grid->setToolMode(GgdDrumGrid::ToolMode::draw);
        focusGrid();
    };
    selectButton.onClick = [this]
    {
        grid->setToolMode(GgdDrumGrid::ToolMode::select);
        focusGrid();
    };
    addAndMakeVisible(drawButton);
    addAndMakeVisible(selectButton);

    undoButton.setSegment(false, true);
    redoButton.setSegment(true, false);
    undoButton.setTooltip("Undo " + shortcut("Ctrl+Z"));
    redoButton.setTooltip("Redo " + shortcut("Ctrl+Shift+Z"));
    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);

    for (int i = 1; i <= 32; ++i)
        numeratorBox.addItem(juce::String(i), i);
    for (const int d : denominators)
        denominatorBox.addItem(juce::String(d), d);
    numeratorBox.setTooltip("Beats per bar");
    denominatorBox.setTooltip("Beat unit");
    numeratorBox.onChange = [this]
    {
        if (numeratorBox.getSelectedId() > 0)
            setMeter(numeratorBox.getSelectedId(), currentPattern().getDenominator());
    };
    denominatorBox.onChange = [this]
    {
        if (denominatorBox.getSelectedId() > 0)
            setMeter(currentPattern().getNumerator(), denominatorBox.getSelectedId());
    };

    barsStepper = std::make_unique<BarsStepper>();
    barsStepper->setTooltip("Pattern length in bars. Click the number to type it, or scroll.");
    barsStepper->onChange = [this](int bars) { setBars(bars); };
    barsStepper->onEditingFinished = [this] { focusGrid(); };
    addAndMakeVisible(*barsStepper);

    parkedButton.setStyle(GgdButton::Style::quiet);
    parkedButton.onClick = [this]
    {
        juce::PopupMenu menu;
        const int count = currentPattern().getParkedCount();
        menu.addSectionHeader(plural(count, "hidden hit", "hidden hits"));
        menu.addItem(1, "Discard hidden hits");
        auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
        menu.setLookAndFeel(&lookAndFeel);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(parkedButton),
                           [safe](int result)
                           {
                               if (auto* self = safe.getComponent(); self != nullptr && result == 1)
                                   self->discardParkedHits();
                           });
    };
    addChildComponent(parkedButton);

    for (int i = 0; i < static_cast<int>(std::size(gridChoices)); ++i)
        gridBox.addItem(juce::String("Grid ") + gridChoices[i].name, i + 1);
    gridBox.setTooltip("Grid: new hits, moves and Quantize snap to this division");
    gridBox.onChange = [this]
    {
        const int index = gridBox.getSelectedId() - 1;
        if (index >= 0 && index < static_cast<int>(std::size(gridChoices)))
            setBaseGrid(gridChoices[index].ticks);
    };

    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange(GgdDrumGrid::minZoomScale, GgdDrumGrid::maxZoomScale, 0.0);
    zoomSlider.setSkewFactorFromMidPoint(GgdDrumGrid::defaultZoomScale);
    zoomSlider.setDoubleClickReturnValue(true, GgdDrumGrid::defaultZoomScale);
    zoomSlider.setMouseClickGrabsKeyboardFocus(false);
    zoomSlider.setWantsKeyboardFocus(false);
    zoomSlider.setTooltip("Timeline zoom (Ctrl + mouse wheel over the timeline)");
    zoomSlider.onValueChange = [this] { grid->setZoomScale(static_cast<float>(zoomSlider.getValue())); };
    addAndMakeVisible(zoomSlider);

    zoomValueButton.setStyle(GgdButton::Style::quiet);
    zoomValueButton.setTooltip("Reset zoom to 125%");
    zoomValueButton.onClick = [this]
    {
        grid->resetZoom();
        focusGrid();
    };
    addAndMakeVisible(zoomValueButton);

    fitButton.setStyle(GgdButton::Style::quiet);
    fitButton.setTooltip("Fit the whole pattern in view");
    fitButton.onClick = [this]
    {
        grid->zoomToFit();
        focusGrid();
    };
    addAndMakeVisible(fitButton);

    for (int i = 0; i < project.getMaps().size(); ++i)
    {
        if (project.isDrumMachine(i))
            kitBox.addSeparator();
        kitBox.addItem(project.getMaps().getReference(i).library, i + 1);
    }
    kitBox.setTooltip("Destination kit: decides which note each row plays. Drum machine has pads you name and "
                      "tune yourself (C1 - D#2 to start).");

    padsButton.setStyle(GgdButton::Style::quiet);
    padsButton.setTooltip("Name the Drum machine pads and pick their notes (or double-click a row name)");
    padsButton.onClick = [this] { openPadEditor(); };
    addChildComponent(padsButton);
    kitBox.onChange = [this]
    {
        if (kitBox.getSelectedId() > 0)
            setKit(kitBox.getSelectedId() - 1);
    };

    syncBox.addItem("Sync: Host bars", 1);
    syncBox.addItem("Sync: Free run", 2);
    syncBox.setTooltip("Host bars: every pattern bar starts on a host bar line, so changing the host time "
                       "signature never shifts the groove. Free run: the pattern loops on its own from the "
                       "song start (polymeter).");
    syncBox.onChange = [this]
    {
        setSyncMode(syncBox.getSelectedId() == 2 ? GgdTimeline::SyncMode::free : GgdTimeline::SyncMode::hostBars);
    };

    for (auto* box : { &numeratorBox, &denominatorBox, &gridBox, &kitBox, &syncBox })
    {
        box->setWantsKeyboardFocus(false);
        box->setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(*box);
    }

    // Context strip -----------------------------------------------------------
    ghostButton.setSegment(false, true);
    accentButton.setSegment(true, false);
    ghostButton.setTooltip("Set the selected hits to ghost-note velocity (35)");
    accentButton.setTooltip("Set the selected hits to accent velocity (120)");
    ghostButton.onClick = [this] { grid->setSelectedVelocity(35); };
    accentButton.onClick = [this] { grid->setSelectedVelocity(120); };

    for (size_t i = 0; i < chanceButtons.size(); ++i)
    {
        auto& button = chanceButtons[i];
        const int value = chanceValues[i];
        button.setButtonText(juce::String(value) + "%");
        button.setSegment(i > 0, i + 1 < chanceButtons.size());
        button.setTooltip("Play the selected hits with " + juce::String(value) + "% probability");
        button.onClick = [this, value] { grid->setSelectedProbability(value); };
    }

    quantizeButton.setSegment(false, true);
    humanizeButton.setSegment(true, false);
    quantizeButton.setTooltip("Snap the selected hits to the grid " + shortcut("Q"));
    humanizeButton.setTooltip("Nudge timing and velocity of the selected hits slightly");
    quantizeButton.onClick = [this] { grid->quantizeSelection(); };
    humanizeButton.onClick = [this] { grid->humanizeSelection(); };

    flamButton.setSegment(false, true);
    doubleButton.setSegment(true, false);
    flamButton.setTooltip("Add a quieter grace hit a 1/64 before each selected hit");
    doubleButton.setTooltip("Add a follow-up hit a 1/32 after each selected hit");
    flamButton.onClick = [this] { grid->createFlam(); };
    doubleButton.onClick = [this] { grid->createDouble(); };

    moreButton.setIcon(GgdIcon::menu);
    moreButton.setTooltip("More transforms: velocity ramps, dynamics, fill, mirror, thin");
    moreButton.onClick = [this] { showMoreMenu(); };

    deleteButton.setStyle(GgdButton::Style::danger);
    deleteButton.setTooltip("Delete the selected hits " + shortcut("Del"));
    deleteButton.onClick = [this] { grid->deleteSelection(); };

    for (auto* button : { &ghostButton, &accentButton, &quantizeButton, &humanizeButton, &flamButton,
                          &doubleButton, &moreButton, &deleteButton })
        addChildComponent(*button);
    for (auto& button : chanceButtons)
        addChildComponent(button);

    // Library -----------------------------------------------------------------
    browser = std::make_unique<GgdLibraryBrowser>();
    browser->setGrooveOpenCallback([this](const juce::File& file) { loadGroove(file); });
    browser->setPatternOpenCallback([this](const juce::File& file) { loadPatternFile(file); });
    browser->setSavePatternCallback([this] { savePatternToLibrary(); });
    addChildComponent(*browser);

    preferenceStore->addChangeListener(this);

    // State -------------------------------------------------------------------
    reloadKit(false);
    grid->setZoomScale(prefs.zoom);
    applyPreferences(prefs, false);
    ensureHistory(currentSlot());

    setResizable(true, true);
    setResizeLimits(1200, 660, 4096, 2600);
    setSize(juce::jlimit(1200, 4096, prefs.editorWidth), juce::jlimit(660, 2600, prefs.editorHeight));

    // Components created before they were parented initialised themselves
    // with the default look and feel; let them pick up ours.
    sendLookAndFeelChange();
    refreshAll();

    vblank = std::make_unique<juce::VBlankAttachment>(this, [this] { renderFrame(); });
    startTimerHz(10);
}

GgdDrumEditor::~GgdDrumEditor()
{
    stopTimer();
    vblank.reset();
    preferenceStore->removeChangeListener(this);
    settingsPanel.reset();
    padEditor.reset();
    viewport.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
}

juce::String GgdDrumEditor::shortcut(const juce::String& keys)
{
    const bool mac = (juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0;
    auto text = keys;
    if (mac)
        text = text.replace("Ctrl+", juce::String::fromUTF8("\xe2\x8c\x98")).replace("Shift+", juce::String::fromUTF8("\xe2\x87\xa7"));
    return "(" + text + ")";
}

//==============================================================================
void GgdDrumEditor::timerCallback()
{
    // Fallback for hosts or windows that stop delivering display callbacks.
    if (nowMs() - lastFrameMs > 100.0)
        renderFrame();
}

void GgdDrumEditor::renderFrame()
{
    const double now = nowMs();
    lastFrameMs = now;

    if (processor.mNotifier.doesUINeedUpdate())
        reloadFromProcessor();

    if (!focusGivenToGrid && isShowing() && !overlayOpen())
    {
        focusGrid();
        focusGivenToGrid = grid->hasKeyboardFocus(true);
    }

    const auto* focused = juce::Component::getCurrentlyFocusedComponent();
    const bool typing = dynamic_cast<const juce::TextEditor*>(focused) != nullptr;
    grid->pollKeyboard(!typing && !overlayOpen() && grid->hasKeyboardFocus(true));

    grid->updatePlayback();
    updateTransport();
    updatePlayheadFollow();

    if (statusText.isNotEmpty() && now > statusUntilMs)
    {
        statusText.clear();
        repaint(contextArea);
    }
}

void GgdDrumEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // Another window (or this one) saved preferences. Layout choices stay
    // per window; everything from the settings sheet is shared.
    auto loaded = preferenceStore->load();
    loaded.zoom = prefs.zoom;
    loaded.gridTicks = prefs.gridTicks;
    loaded.showLibrary = prefs.showLibrary;
    loaded.editorWidth = prefs.editorWidth;
    loaded.editorHeight = prefs.editorHeight;
    if (loaded != prefs || loaded.theme != appliedTheme)
        applyPreferences(loaded, false);
}

void GgdDrumEditor::reloadFromProcessor()
{
    // The host replaced the project (preset load, undo in the host...).
    barsStepper->cancelEditing();
    nameFieldSlot = -1;
    resetHistories();
    // The sheet would edit pads the new project does not have.
    padEditor.reset();
    reloadKit(false);
    grid->patternGeometryChanged();
    ensureHistory(currentSlot());
    if (browser != nullptr)
        browser->clearLoaded();
    refreshAll();
}

void GgdDrumEditor::updateTransport()
{
    GgdPlayheadSnapshot snapshot;
    const bool have = processor.readPlayhead(snapshot);
    const bool playing = have && snapshot.playing;

    if (playing && snapshot.hostMeterKnown
        && (snapshot.hostNumerator != hostNumerator || snapshot.hostDenominator != hostDenominator
            || !hostMeterKnown))
    {
        hostMeterKnown = true;
        hostNumerator = snapshot.hostNumerator;
        hostDenominator = snapshot.hostDenominator;
    }
    transportPlaying = playing;

    TransportDisplay::State state;
    state.playing = playing;
    state.muted = playing ? snapshot.muted : layer().getMuted();

    if (playing)
    {
        const double tick = snapshot.tickAt(nowMs());
        const auto* playingPattern = layer().getEventPattern(juce::jlimit(0, SEQ_MAX_PATTERNS - 1, snapshot.pattern));
        if (tick >= 0.0 && playingPattern != nullptr)
        {
            const int barTicks = juce::jmax(1, playingPattern->getBarTicks());
            const int beatTicks = juce::jmax(1, GGD_EVENT_PPQ * 4 / playingPattern->getDenominator());
            const int whole = static_cast<int>(tick);
            state.position = juce::String(whole / barTicks + 1) + "." + juce::String((whole % barTicks) / beatTicks + 1);
        }
        else
        {
            state.resting = true;
        }
    }

    const auto& pattern = currentPattern();
    const bool barsSync = ui().getSyncMode() == GgdTimeline::SyncMode::hostBars;
    if (hostMeterKnown)
    {
        state.hostMeter = "Host " + juce::String(hostNumerator) + "/" + juce::String(hostDenominator);
        const double hostBar = GgdTimeline::barQuartersForMeter(hostNumerator, hostDenominator);
        const double patternBar = static_cast<double>(pattern.getBarTicks()) / GGD_EVENT_PPQ;
        state.mismatch = barsSync && std::abs(hostBar - patternBar) > 1.0e-6;
    }

    transport->setState(state);

    juce::String tip;
    if (state.mismatch)
        tip << "The host bar is " << hostNumerator << "/" << hostDenominator << " but this pattern is "
            << pattern.getNumerator() << "/" << pattern.getDenominator()
            << ". With Sync: Host bars every pattern bar still starts on a host bar line: "
               "shorter bars rest until the next host bar, longer bars are cut short. "
               "Choose Sync: Free run for polymeter loops.";
    else if (playing)
        tip = "Bar.beat of the playing pattern";
    else
        tip = "Follows the host transport";
    if (transport->getTooltip() != tip)
        transport->setTooltip(tip);

    std::array<bool, SEQ_MAX_PATTERNS> content {};
    for (int i = 0; i < SEQ_MAX_PATTERNS; ++i)
        content[static_cast<size_t>(i)] = slotHasContent(i);
    playingSlot = playing ? snapshot.pattern : -1;
    slotStrip->setState(currentSlot(), playingSlot, content);
}

void GgdDrumEditor::updatePlayheadFollow()
{
    const int viewWidth = viewport.getViewWidth();
    const int inset = grid->getTimelineInsetPixels();
    const int timelineWidth = juce::jmax(1, viewWidth - inset);
    const bool overflow = grid->getTimelineContentWidthPixels() > static_cast<float>(timelineWidth) + 1.0f;
    const bool following = prefs.followPlayhead && transportPlaying && overflow;

    if (!following)
    {
        if (followPadded)
        {
            grid->setTrailingPadding(0);
            followPadded = false;
        }
        followSetViewX = -1;
        return;
    }

    // Pad the end so the playhead can reach the centre on the last bar.
    if (!followPadded)
    {
        grid->setTrailingPadding(timelineWidth / 2 + 12);
        followPadded = true;
    }

    const int currentX = viewport.getViewPositionX();
    if (followSetViewX >= 0 && currentX != followSetViewX)
        followPausedUntilMs = nowMs() + 2500.0; // the user scrolled: let them look around
    followSetViewX = -1;

    const float playX = grid->getPlayheadContentX();
    if (playX < 0.0f || grid->isMouseButtonDown() || nowMs() < followPausedUntilMs)
        return;

    const int centre = inset + timelineWidth / 2;
    const int maxX = juce::jmax(0, grid->getWidth() - viewWidth);
    const int target = juce::jlimit(0, maxX, juce::roundToInt(playX) - centre);
    if (target != currentX)
        viewport.setViewPosition(target, viewport.getViewPositionY());
    followSetViewX = viewport.getViewPositionX();
}

//==============================================================================
void GgdDrumEditor::applyPreferences(const GgdPreferences& next, bool save)
{
    const bool themeChanged = next.theme != appliedTheme;
    const bool libraryChanged = next.showLibrary != prefs.showLibrary;
    prefs = next;

    if (themeChanged)
    {
        appliedTheme = prefs.theme;
        ggdSetCurrentTheme(ggdThemeFromIndex(prefs.theme));
        lookAndFeel.syncThemeColours();
        patternName.setColour(juce::TextEditor::backgroundColourId, tc(GgdThemeRole::panelRaised));
        patternName.applyColourToAllText(tc(GgdThemeRole::text));
        sendLookAndFeelChange();
        if (browser != nullptr)
            browser->themeChanged();
    }
    patternName.setColour(juce::TextEditor::backgroundColourId, tc(GgdThemeRole::panelRaised));
    patternName.setTextToShowWhenEmpty("Name this pattern", tc(GgdThemeRole::faint));

    GgdDrumGrid::Preferences gp;
    gp.smoothPlayhead = prefs.smoothPlayhead;
    gp.playheadGlow = prefs.playheadGlow;
    gp.shiftHoverVelocity = prefs.shiftHoverVelocity;
    gp.audition = prefs.audition;
    gp.colourByInstrument = prefs.colourByInstrument;
    gp.playedEffects = prefs.playedEffects;
    gp.playedRipple = prefs.playedRipple;
    gp.playedLaneFlash = prefs.playedLaneFlash;
    gp.velocityReactive = prefs.velocityReactive;
    gp.reduceMotion = prefs.reduceMotion;
    gp.effectIntensity = static_cast<float>(prefs.effectIntensityPercent) / 100.0f;
    gp.effectDecayMs = prefs.effectDecayMs;
    grid->setPreferences(gp);
    grid->setSnapTicks(effectiveGridTicks());

    if (browser != nullptr)
        browser->setVisible(prefs.showLibrary);
    if (libraryChanged)
        resized();

    if (save)
        preferenceStore->save(prefs);

    refreshAll();
    repaint();
}

int GgdDrumEditor::effectiveGridTicks() const
{
    const bool fine = prefs.autoFineGrid && grid != nullptr && grid->getZoomScale() >= fineGridZoom;
    return fine ? finerGrid(prefs.gridTicks) : prefs.gridTicks;
}

void GgdDrumEditor::setBaseGrid(int ticks)
{
    if (prefs.gridTicks != ticks)
    {
        prefs.gridTicks = ticks;
        preferenceStore->save(prefs);
    }
    grid->setSnapTicks(effectiveGridTicks());
    refreshHeader();
    focusGrid();
}

//==============================================================================
GgdDrumEditor::PatternState GgdDrumEditor::capture(int slot) const
{
    PatternState state;
    const auto* pattern = layer().getEventPattern(slot);
    state.numerator = pattern->getNumerator();
    state.denominator = pattern->getDenominator();
    state.bars = pattern->getBars();
    state.events.reserve(static_cast<size_t>(pattern->getEventCount()));
    for (int i = 0; i < pattern->getEventCount(); ++i)
        state.events.push_back(*pattern->getEvent(i));
    state.name = nameOf(layer(), slot);
    for (int i = 0; i < pattern->getParkedCount(); ++i)
        state.parked.push_back(*pattern->getParked(i));

    const auto& slotHistory = history[static_cast<size_t>(slot)];
    state.source = slotHistory.source;
    state.sourceIsPattern = slotHistory.sourceIsPattern;
    if (slot == currentSlot() && grid != nullptr)
        state.selection = grid->getSelectionCoordinates();
    return state;
}

bool GgdDrumEditor::liveMatches(const PatternState& state, int slot) const
{
    const auto* pattern = layer().getEventPattern(slot);
    if (pattern->getNumerator() != state.numerator || pattern->getDenominator() != state.denominator
        || pattern->getBars() != state.bars || pattern->getEventCount() != static_cast<int>(state.events.size())
        || nameOf(layer(), slot) != state.name
        || !parkedMatches(*pattern, state.parked))
        return false;

    for (int i = 0; i < pattern->getEventCount(); ++i)
        if (!sameEvent(*pattern->getEvent(i), state.events[static_cast<size_t>(i)]))
            return false;
    return true;
}

void GgdDrumEditor::restore(int slot, const PatternState& state)
{
    auto* pattern = layer().getEventPattern(slot);
    pattern->clear(false);
    pattern->activate(state.numerator, state.denominator, state.bars);
    for (const auto& event : state.events)
        pattern->addEvent(event.row, event.tick, event.velocity, event.probability, event.durationTicks);
    for (const auto& hit : state.parked)
        pattern->addParked(hit);
    layer().setPatternName(state.name.toRawUTF8(), slot);

    auto& slotHistory = history[static_cast<size_t>(slot)];
    slotHistory.source = state.source;
    slotHistory.sourceIsPattern = state.sourceIsPattern;

    if (slot == currentSlot())
        GgdProject::syncLegacyStepCount(layer());
}

void GgdDrumEditor::ensureHistory(int slot)
{
    auto& slotHistory = history[static_cast<size_t>(slot)];
    if (!slotHistory.valid)
    {
        slotHistory.committed = capture(slot);
        slotHistory.valid = true;
    }
}

void GgdDrumEditor::resetHistories()
{
    for (auto& slotHistory : history)
        slotHistory = SlotHistory();
}

void GgdDrumEditor::commitEdit()
{
    commitSlotEdit(currentSlot());
}

void GgdDrumEditor::commitSlotEdit(int slot)
{
    if (slot == currentSlot())
        GgdProject::syncLegacyStepCount(layer());
    processor.publishEdit(true);

    auto& slotHistory = history[static_cast<size_t>(slot)];
    auto present = capture(slot);
    if (!slotHistory.valid)
    {
        slotHistory.committed = std::move(present);
        slotHistory.valid = true;
    }
    else if (!present.sameContent(slotHistory.committed))
    {
        slotHistory.undo.push_back(std::move(slotHistory.committed));
        while (slotHistory.undo.size() > maxHistory)
            slotHistory.undo.pop_front();
        slotHistory.redo.clear();
        slotHistory.committed = std::move(present);
    }
    else
    {
        slotHistory.committed.selection = std::move(present.selection);
    }

    refreshAll();
}

void GgdDrumEditor::handleSelectionChanged()
{
    if (!restoringHistory)
    {
        const int slot = currentSlot();
        auto& slotHistory = history[static_cast<size_t>(slot)];
        // Remember the selection that belongs to the committed content, so
        // undo can bring back what was selected before an edit.
        if (slotHistory.valid && liveMatches(slotHistory.committed, slot))
            slotHistory.committed.selection = grid->getSelectionCoordinates();
    }
    refreshContextStrip();
}

void GgdDrumEditor::applyHistoryState(int slot, const PatternState& state)
{
    restoringHistory = true;
    restore(slot, state);
    processor.publishEdit(true);
    grid->patternGeometryChanged();
    grid->restoreSelectionCoordinates(state.selection);
    restoringHistory = false;
    updateBrowserHighlight();
    refreshAll();
}

void GgdDrumEditor::undo()
{
    const int slot = currentSlot();
    auto& slotHistory = history[static_cast<size_t>(slot)];
    if (slotHistory.valid && !liveMatches(slotHistory.committed, slot))
        commitSlotEdit(slot); // keep anything not yet recorded redoable
    if (slotHistory.undo.empty())
    {
        showStatus("Nothing to undo in this pattern");
        return;
    }

    slotHistory.redo.push_back(capture(slot));
    auto target = std::move(slotHistory.undo.back());
    slotHistory.undo.pop_back();
    applyHistoryState(slot, target);
    slotHistory.committed = std::move(target);
    refreshHistoryButtons();
}

void GgdDrumEditor::redo()
{
    const int slot = currentSlot();
    auto& slotHistory = history[static_cast<size_t>(slot)];
    if (slotHistory.redo.empty())
    {
        showStatus("Nothing to redo");
        return;
    }

    slotHistory.undo.push_back(capture(slot));
    auto target = std::move(slotHistory.redo.back());
    slotHistory.redo.pop_back();
    applyHistoryState(slot, target);
    slotHistory.committed = std::move(target);
    refreshHistoryButtons();
}

//==============================================================================
void GgdDrumEditor::selectPatternSlot(int slot)
{
    slot = juce::jlimit(0, SEQ_MAX_PATTERNS - 1, slot);
    if (slot == currentSlot())
    {
        focusGrid();
        return;
    }

    // Keep the outgoing selection for when we come back.
    const int outgoing = currentSlot();
    auto& outgoingHistory = history[static_cast<size_t>(outgoing)];
    if (outgoingHistory.valid && liveMatches(outgoingHistory.committed, outgoing))
        outgoingHistory.committed.selection = grid->getSelectionCoordinates();

    // Typed but not yet committed values belong to the outgoing slot.
    barsStepper->finishEditing();
    if (patternName.hasKeyboardFocus(true))
        renamePattern();

    layer().setCurrentPattern(slot);
    GgdProject::syncLegacyStepCount(layer());
    processor.publishEdit(true);
    ensureHistory(slot);

    restoringHistory = true;
    grid->patternGeometryChanged();
    grid->restoreSelectionCoordinates(history[static_cast<size_t>(slot)].committed.selection);
    restoringHistory = false;

    // Messages about the previous slot no longer apply.
    statusText.clear();
    updateBrowserHighlight();
    refreshAll();
    repaint(contextArea);
    focusGrid();
}

void GgdDrumEditor::setMeter(int numerator, int denominator)
{
    auto& pattern = currentPattern();
    const auto change = GgdPatternOps::changeGeometry(pattern, numerator, denominator, pattern.getBars());
    if (change.changed)
    {
        grid->patternGeometryChanged();
        commitEdit();
        reportGeometryChange(change);
    }
    refreshHeader();
    focusGrid();
}

void GgdDrumEditor::setBars(int bars)
{
    auto& pattern = currentPattern();
    const auto change = GgdPatternOps::changeGeometry(pattern, pattern.getNumerator(), pattern.getDenominator(), bars);
    if (change.changed)
    {
        grid->patternGeometryChanged();
        commitEdit();
        reportGeometryChange(change);
    }
    refreshHeader();
}

void GgdDrumEditor::reportGeometryChange(const GgdPatternOps::GeometryChange& change)
{
    const auto& pattern = currentPattern();
    juce::String text;
    if (change.hidden > 0)
        text << plural(change.hidden, "hit doesn't", "hits don't") << " fit "
             << pattern.getNumerator() << "/" << pattern.getDenominator() << " x "
             << plural(pattern.getBars(), "bar", "bars") << " and "
             << (change.hidden == 1 ? "is" : "are") << " hidden. They come back if the pattern grows again.";
    else if (change.restored > 0)
        text << plural(change.restored, "hidden hit is", "hidden hits are") << " back.";
    if (change.dropped > 0)
        text << (text.isEmpty() ? "" : " ") << plural(change.dropped, "hit", "hits")
             << " could not be kept (too many hidden hits). Undo brings them back.";
    if (text.isNotEmpty())
        showStatus(text);
}

void GgdDrumEditor::discardParkedHits()
{
    auto& pattern = currentPattern();
    if (pattern.getParkedCount() == 0)
        return;
    const int count = pattern.getParkedCount();
    pattern.clearParked();
    commitEdit();
    showStatus(plural(count, "hidden hit", "hidden hits") + " discarded");
}

void GgdDrumEditor::renamePattern()
{
    // The field edits the slot whose name it shows. Its focus-loss callback
    // arrives asynchronously, possibly after a slot switch.
    const int slot = nameFieldSlot >= 0 ? nameFieldSlot : currentSlot();
    auto desired = fitPatternName(patternName.getText());
    if (desired.isEmpty())
        desired = SEQ_DEFAULT_PAT_NAME;
    if (desired == nameOf(layer(), slot))
    {
        refreshHeader();
        return;
    }
    ensureHistory(slot);
    layer().setPatternName(desired.toRawUTF8(), slot);
    commitSlotEdit(slot);
}

void GgdDrumEditor::clearPattern()
{
    const int slot = currentSlot();
    auto& slotHistory = history[static_cast<size_t>(slot)];
    currentPattern().clear(true);
    currentPattern().clearParked();
    slotHistory.source = juce::File();
    grid->patternReplaced();
    commitEdit();
    updateBrowserHighlight();
    showStatus("Pattern cleared. Undo brings it back.");
}

void GgdDrumEditor::doublePatternLength()
{
    int discarded = 0;
    if (!GgdPatternOps::duplicateBars(currentPattern(), &discarded))
    {
        showStatus("The pattern can't double: it would be longer than " + juce::String(GGD_MAX_PATTERN_BARS)
                   + " bars of 4/4");
        return;
    }
    grid->patternGeometryChanged();
    commitEdit();
    juce::String text = "Doubled to " + plural(currentPattern().getBars(), "bar", "bars") + ".";
    if (discarded > 0)
        text << " " << plural(discarded, "hidden hit", "hidden hits") << " from those bars "
             << (discarded == 1 ? "was" : "were") << " replaced by the repeat.";
    showStatus(text);
}

void GgdDrumEditor::duplicateToEmptySlot()
{
    const int source = currentSlot();
    int target = -1;
    for (int offset = 1; offset < SEQ_MAX_PATTERNS && target < 0; ++offset)
    {
        const int candidate = (source + offset) % SEQ_MAX_PATTERNS;
        if (!slotHasContent(candidate))
            target = candidate;
    }
    if (target < 0)
    {
        showStatus("All 8 slots hold patterns. Right-click a slot to clear it first.");
        return;
    }
    copyPatternToSlot(target);
}

void GgdDrumEditor::copyPatternToSlot(int target)
{
    const int source = currentSlot();
    if (target == source)
        return;

    ensureHistory(target);
    ui().copyPatternData(GgdProject::drumLayer, target, GgdProject::drumLayer, source);
    const auto name = fitPatternName(displayName(layer(), source) + " copy");
    layer().setPatternName(name.toRawUTF8(), target);
    auto& targetHistory = history[static_cast<size_t>(target)];
    targetHistory.source = juce::File();
    commitSlotEdit(target);
    showStatus("Copied to slot " + juce::String(target + 1) + ". Click it to edit the copy.");
}

void GgdDrumEditor::clearSlot(int slot)
{
    if (slot == currentSlot())
    {
        clearPattern();
        return;
    }
    ensureHistory(slot);
    auto* pattern = layer().getEventPattern(slot);
    pattern->clear(true);
    pattern->clearParked();
    layer().setPatternName(SEQ_DEFAULT_PAT_NAME, slot);
    auto& slotHistory = history[static_cast<size_t>(slot)];
    slotHistory.source = juce::File();
    commitSlotEdit(slot);
    showStatus("Slot " + juce::String(slot + 1) + " cleared. Open it and undo to bring it back.");
}

void GgdDrumEditor::toggleOutput()
{
    layer().setMuted(!layer().getMuted());
    processor.publishEdit(true);
    refreshHeader();
    showStatus(layer().getMuted() ? "Sequencer output off. MIDI from the host still passes through."
                                  : "Sequencer output on");
    focusGrid();
}

void GgdDrumEditor::setKit(int index)
{
    if (index == project.getActiveMapIndex(ui()))
        return;
    project.setActiveMap(ui(), index);
    processor.publishEdit(true);
    reloadKit(false);
    refreshAll();
    if (project.isDrumMachine(index))
        showStatus("Rows now play your Drum machine pads. Click Pads (or double-click a row name) to name them.");
    else
        showStatus("Rows now play " + activeKit.library + " notes");
    focusGrid();
}

void GgdDrumEditor::reloadKit(bool keepSelection)
{
    activeKit = project.getActiveKit(ui());
    grid->setKitMap(&activeKit, keepSelection);
}

void GgdDrumEditor::applyPadChanges(const GgdDrumMachineKit& pads)
{
    if (ui().getDrumMachine() == pads)
        return;
    ui().getDrumMachine() = pads;
    const int active = project.getActiveMapIndex(ui());
    if (project.isDrumMachine(active))
        project.setActiveMap(ui(), active); // re-binds the pad notes and row names
    processor.publishEdit(true);
    reloadKit(true);
    refreshHeader();
}

void GgdDrumEditor::auditionPad(int pad)
{
    if (!juce::isPositiveAndBelow(pad, ui().getDrumMachine().getPadCount()))
        return;
    const int canonical = project.findCanonicalRow(GgdDrumMachineKit::semanticId(pad));
    if (canonical < 0)
        return;
    const int storage = project.storageRowForCanonical(canonical);
    if (layer().getCurNote(storage) < 0)
        return;
    processor.mIncomingData.addToFifo(SEQ_MIDI_NOTEON, GgdProject::drumLayer, storage);
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    juce::Timer::callAfterDelay(180, [safe, storage]
    {
        if (auto* self = safe.getComponent())
            self->processor.mIncomingData.addToFifo(SEQ_MIDI_NOTEOFF, GgdProject::drumLayer, storage);
    });
}

void GgdDrumEditor::setSyncMode(GgdTimeline::SyncMode mode)
{
    if (ui().getSyncMode() != mode)
    {
        ui().setSyncMode(mode);
        processor.publishEdit(true);
        showStatus(mode == GgdTimeline::SyncMode::hostBars
                       ? "Sync: every pattern bar starts on a host bar line"
                       : "Sync: the pattern loops on its own from the song start");
    }
    refreshHeader();
    focusGrid();
}

void GgdDrumEditor::showPatternMenu()
{
    const auto& pattern = currentPattern();
    const bool hasHits = pattern.getEventCount() > 0;
    const bool canDouble = pattern.getBars() * 2
                        <= GgdPatternOps::maxBarsFor(pattern.getNumerator(), pattern.getDenominator());

    juce::PopupMenu menu;
    menu.addSectionHeader(displayName(layer(), currentSlot()));
    menu.addItem(1, "Duplicate to an empty slot");
    menu.addItem(2, "Double length (repeat the bars)", canDouble);
    menu.addSeparator();
    menu.addItem(3, "Save to pattern library...");
    menu.addItem(4, "Import MIDI groove...");
    menu.addItem(5, "Export MIDI file...", hasHits);
    menu.addSeparator();
    menu.addItem(6, "Clear pattern", hasHits || pattern.getParkedCount() > 0);

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    menu.setLookAndFeel(&lookAndFeel);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(patternMenuButton).withMinimumWidth(230),
                       [safe](int result)
                       {
                           auto* self = safe.getComponent();
                           if (self == nullptr)
                               return;
                           switch (result)
                           {
                               case 1: self->duplicateToEmptySlot(); break;
                               case 2: self->doublePatternLength(); break;
                               case 3: self->savePatternToLibrary(); break;
                               case 4: self->chooseMidiToImport(); break;
                               case 5: self->exportMidiFile(); break;
                               case 6: self->clearPattern(); break;
                               default: break;
                           }
                           self->focusGrid();
                       });
}

void GgdDrumEditor::showMoreMenu()
{
    const int count = grid->getSelectedCount();
    juce::PopupMenu menu;
    menu.addSectionHeader("Velocity");
    menu.addItem(1, "Ramp up", count >= 2);
    menu.addItem(2, "Ramp down", count >= 2);
    menu.addItem(3, "Compress dynamics", count >= 2);
    menu.addItem(4, "Expand dynamics", count >= 2);
    menu.addSectionHeader("Timing");
    menu.addItem(5, "Flam (grace hit before)", count >= 1);
    menu.addItem(6, "Double (hit after)", count >= 1);
    menu.addItem(7, "Humanize", count >= 1);
    menu.addSectionHeader("Shape");
    menu.addItem(8, "Fill the gaps on the grid", count >= 2);
    menu.addItem(9, "Mirror timing", count >= 2);
    menu.addItem(10, "Thin out (remove every other hit)", count >= 2);
    menu.addItem(11, "Select the whole rows", count >= 1);

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    menu.setLookAndFeel(&lookAndFeel);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(moreButton).withMinimumWidth(220),
                       [safe](int result)
                       {
                           auto* self = safe.getComponent();
                           if (self == nullptr)
                               return;
                           auto& g = *self->grid;
                           switch (result)
                           {
                               case 1: g.rampSelectedVelocity(true); break;
                               case 2: g.rampSelectedVelocity(false); break;
                               case 3: g.scaleSelectedVelocityRange(0.65f); break;
                               case 4: g.scaleSelectedVelocityRange(1.45f); break;
                               case 5: g.createFlam(); break;
                               case 6: g.createDouble(); break;
                               case 7: g.humanizeSelection(); break;
                               case 8: g.fillSelectionGaps(); break;
                               case 9: g.mirrorSelection(); break;
                               case 10: g.thinSelection(); break;
                               case 11: g.selectRowsContainingSelection(); break;
                               default: break;
                           }
                           self->focusGrid();
                       });
}

void GgdDrumEditor::showSlotMenu(int slot)
{
    const bool other = slot != currentSlot();
    juce::PopupMenu menu;
    menu.addSectionHeader("Slot " + juce::String(slot + 1) + "  " + displayName(layer(), slot));
    menu.addItem(1, "Open", other);
    menu.addItem(2, "Copy the current pattern here", other);
    menu.addSeparator();
    menu.addItem(3, "Clear slot", slotHasContent(slot));

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    menu.setLookAndFeel(&lookAndFeel);
    menu.showMenuAsync(juce::PopupMenu::Options().withMinimumWidth(220),
                       [safe, slot](int result)
                       {
                           auto* self = safe.getComponent();
                           if (self == nullptr)
                               return;
                           if (result == 1)
                               self->selectPatternSlot(slot);
                           else if (result == 2)
                               self->copyPatternToSlot(slot);
                           else if (result == 3)
                               self->clearSlot(slot);
                       });
}

//==============================================================================
void GgdDrumEditor::loadGroove(const juce::File& file)
{
    const auto* map = activeMap();
    if (map == nullptr)
        return;

    const auto result = GgdMidiImporter::parseFile(file, *map, project.getCanonicalRows(), GGD_MAX_PATTERN_TICKS);
    if (!result.ok)
    {
        showAlert("MIDI import failed", result.error);
        return;
    }

    const int slot = currentSlot();
    auto& pattern = currentPattern();
    pattern.clear(false);
    pattern.activate(result.numerator, result.denominator, result.bars);
    for (const auto& imported : result.events)
        if (imported.canonicalRow >= 0 && imported.canonicalRow < project.getCanonicalRows().size())
            pattern.addEvent(project.storageRowForCanonical(imported.canonicalRow), imported.tick,
                             imported.velocity, SEQ_PROB_ON, imported.durationTicks);

    layer().setPatternName(fitPatternName(result.fileName).toRawUTF8(), slot);
    auto& slotHistory = history[static_cast<size_t>(slot)];
    slotHistory.source = file;
    slotHistory.sourceIsPattern = false;

    grid->patternGeometryChanged();
    commitEdit();
    viewport.setViewPosition(0, viewport.getViewPositionY());
    updateBrowserHighlight();
    showStatus("Loaded " + file.getFileNameWithoutExtension() + "  |  " + result.summary());
}

void GgdDrumEditor::loadPatternFile(const juce::File& file)
{
    GgdPatternSnapshot snapshot;
    juce::String error;
    if (!GgdPatternFile::read(file, snapshot, error))
    {
        showAlert("Pattern load failed", error);
        return;
    }

    const int slot = currentSlot();
    GgdPatternFile::restore(snapshot, ui(), project, GgdProject::drumLayer, slot);
    auto& slotHistory = history[static_cast<size_t>(slot)];
    slotHistory.source = file;
    slotHistory.sourceIsPattern = true;

    grid->patternGeometryChanged();
    commitEdit();
    viewport.setViewPosition(0, viewport.getViewPositionY());
    updateBrowserHighlight();
    showStatus("Loaded pattern " + displayName(layer(), slot) + ". Undo brings back what was here.");
}

void GgdDrumEditor::chooseMidiToImport()
{
    auto start = browser != nullptr ? browser->getGrooveRoot() : juce::File();
    if (!start.isDirectory())
        start = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser>("Import a GGD MIDI groove", start, "*.mid;*.midi");
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser& fc)
                         {
                             if (auto* self = safe.getComponent())
                             {
                                 const auto file = fc.getResult();
                                 if (file.existsAsFile())
                                     self->loadGroove(file);
                             }
                         });
}

juce::String GgdDrumEditor::currentPatternFileName() const
{
    return juce::File::createLegalFileName(displayName(layer(), currentSlot()));
}

void GgdDrumEditor::savePatternToLibrary()
{
    const int slot = currentSlot();
    const auto snapshot = GgdPatternFile::capture(layer(), project, slot);

    auto root = browser != nullptr ? browser->getPatternRoot() : juce::File();
    if (!root.isDirectory())
        root = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    const auto suggested = root.getChildFile(currentPatternFileName()).withFileExtension(GgdPatternFile::extension);

    chooser = std::make_unique<juce::FileChooser>("Save Stochas GGD pattern", suggested, "*.sggdp");
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
                         [safe, snapshot, slot](const juce::FileChooser& fc)
                         {
                             auto* self = safe.getComponent();
                             if (self == nullptr)
                                 return;
                             auto file = fc.getResult();
                             if (file == juce::File())
                                 return;
                             juce::String error;
                             if (!GgdPatternFile::write(file, snapshot, error))
                             {
                                 self->showAlert("Pattern save failed", error);
                                 return;
                             }
                             if (!file.hasFileExtension(GgdPatternFile::extension))
                                 file = file.withFileExtension(GgdPatternFile::extension);

                             auto& slotHistory = self->history[static_cast<size_t>(slot)];
                             slotHistory.source = file;
                             slotHistory.sourceIsPattern = true;
                             if (self->browser != nullptr)
                                 self->browser->refresh();
                             self->updateBrowserHighlight();
                             self->showStatus("Saved " + file.getFileName());
                         });
}

void GgdDrumEditor::exportMidiFile()
{
    const auto* map = activeMap();
    if (map == nullptr)
        return;

    const auto snapshot = GgdPatternFile::capture(layer(), project, currentSlot());
    const auto kit = *map;
    auto root = browser != nullptr ? browser->getPatternRoot() : juce::File();
    if (!root.isDirectory())
        root = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    const auto suggested = root.getChildFile(currentPatternFileName()).withFileExtension(".mid");

    chooser = std::make_unique<juce::FileChooser>("Export the pattern as MIDI", suggested, "*.mid;*.midi");
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
                         [safe, snapshot, kit](const juce::FileChooser& fc)
                         {
                             auto* self = safe.getComponent();
                             if (self == nullptr)
                                 return;
                             const auto file = fc.getResult();
                             if (file == juce::File())
                                 return;
                             const auto result = GgdMidiExporter::writeFile(file, snapshot, kit);
                             if (!result.ok)
                             {
                                 self->showAlert("MIDI export failed", result.error);
                                 return;
                             }
                             self->showStatus(result.summary() + ". Probability stays in Stochas GGD.");
                         });
}

void GgdDrumEditor::beginMidiDrag(juce::Component* source)
{
    const auto* map = activeMap();
    if (source == nullptr || map == nullptr)
        return;

    if (currentPattern().getEventCount() == 0)
    {
        showStatus("This pattern is empty: there is nothing to drag yet");
        return;
    }

    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Stochas GGD MIDI Drag");
    if (folder.createDirectory().failed())
    {
        showStatus("Could not create a temporary folder for the MIDI file");
        return;
    }

    // Hosts may read a dropped file some time after the drop, so files are
    // kept for a while and cleaned up on a later drag.
    const auto staleBefore = juce::Time::getCurrentTime() - juce::RelativeTime::minutes(10);
    for (const auto& old : folder.findChildFiles(juce::File::findFiles, false, "*.mid"))
        if (old.getLastModificationTime() < staleBefore)
            old.deleteFile();

    // A readable file name: DAWs name the dropped clip after it.
    auto file = folder.getChildFile(currentPatternFileName() + ".mid");
    if (file.exists() && !file.deleteFile())
        file = folder.getNonexistentChildFile(currentPatternFileName(), ".mid", false);

    const auto snapshot = GgdPatternFile::capture(layer(), project, currentSlot());
    const auto result = GgdMidiExporter::writeFile(file, snapshot, *map);
    if (!result.ok)
    {
        showStatus("MIDI drag failed: " + result.error);
        file.deleteFile();
        return;
    }

    const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles(
        { file.getFullPathName() }, false, source);

    if (!started)
    {
        file.deleteFile();
        showStatus("The DAW did not accept a file drag. Use Pattern > Export MIDI file instead.");
        return;
    }
    showStatus("Drop the clip onto a track");
}

void GgdDrumEditor::showAlert(const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(juce::MessageBoxIconType::WarningIcon)
                                     .withTitle(title)
                                     .withMessage(message)
                                     .withButton("OK")
                                     .withAssociatedComponent(this),
                                 nullptr);
}

void GgdDrumEditor::updateBrowserHighlight()
{
    if (browser == nullptr)
        return;
    const auto& slotHistory = history[static_cast<size_t>(currentSlot())];
    if (slotHistory.source == juce::File())
        browser->clearLoaded();
    else if (slotHistory.sourceIsPattern)
        browser->setLoadedPattern(slotHistory.source);
    else
        browser->setLoadedGroove(slotHistory.source);
}

//==============================================================================
void GgdDrumEditor::openSettings()
{
    if (settingsPanel != nullptr)
    {
        if (settingsPanel->isShowing())
            settingsPanel->grabKeyboardFocus();
        return;
    }
    padEditor.reset();

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    settingsPanel = std::make_unique<GgdSettingsPanel>(
        prefs,
        [safe](const GgdPreferences& next)
        {
            if (auto* self = safe.getComponent())
            {
                auto merged = next;
                // The sheet does not own layout choices.
                merged.zoom = self->prefs.zoom;
                merged.gridTicks = self->prefs.gridTicks;
                merged.showLibrary = self->prefs.showLibrary;
                merged.editorWidth = self->prefs.editorWidth;
                merged.editorHeight = self->prefs.editorHeight;
                self->applyPreferences(merged, true);
                if (self->settingsPanel != nullptr)
                    self->settingsPanel->repaint();
            }
        },
        [safe]
        {
            // Close after the click that requested it has been delivered.
            juce::MessageManager::callAsync([safe]
            {
                if (auto* self = safe.getComponent())
                    self->closeSettings();
            });
        });

    addAndMakeVisible(*settingsPanel);
    settingsPanel->sendLookAndFeelChange();
    settingsPanel->setBounds(getLocalBounds());
    settingsPanel->toFront(true);
    if (settingsPanel->isShowing())
        settingsPanel->grabKeyboardFocus();
}

void GgdDrumEditor::closeSettings()
{
    if (settingsPanel == nullptr)
        return;
    settingsPanel.reset();
    focusGrid();
}

void GgdDrumEditor::openPadEditor(int focusPad)
{
    if (settingsPanel != nullptr)
        return;
    if (padEditor == nullptr)
    {
        auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
        padEditor = std::make_unique<GgdPadEditor>(
            ui().getDrumMachine(),
            [safe](const GgdDrumMachineKit& pads)
            {
                if (auto* self = safe.getComponent())
                    self->applyPadChanges(pads);
            },
            [safe](int pad)
            {
                if (auto* self = safe.getComponent())
                    self->auditionPad(pad);
            },
            [safe]
            {
                // Close after the click or key that requested it has been delivered.
                juce::MessageManager::callAsync([safe]
                {
                    if (auto* self = safe.getComponent())
                        self->closePadEditor();
                });
            });
        addAndMakeVisible(*padEditor);
        padEditor->sendLookAndFeelChange();
        padEditor->setBounds(getLocalBounds());
        padEditor->toFront(true);
    }

    if (focusPad >= 0)
        padEditor->focusPad(focusPad);
    else if (padEditor->isShowing())
        padEditor->grabKeyboardFocus();
}

void GgdDrumEditor::closePadEditor()
{
    if (padEditor == nullptr)
        return;
    padEditor.reset();
    focusGrid();
}

void GgdDrumEditor::focusGrid()
{
    // Only a component that is on screen can take keyboard focus.
    if (grid != nullptr && grid->isShowing() && !overlayOpen())
        grid->grabKeyboardFocus();
}

void GgdDrumEditor::showStatus(const juce::String& text)
{
    statusText = text;
    statusUntilMs = nowMs() + 5000.0;
    repaint(contextArea);
}

//==============================================================================
bool GgdDrumEditor::slotHasContent(int slot) const
{
    const auto* pattern = layer().getEventPattern(slot);
    return (pattern != nullptr && pattern->getEventCount() > 0) || !isDefaultName(nameOf(layer(), slot));
}

juce::String GgdDrumEditor::slotTooltip(int slot) const
{
    const auto* pattern = layer().getEventPattern(slot);
    juce::String tip;
    tip << "Slot " << (slot + 1) << ": " << displayName(layer(), slot);
    if (pattern != nullptr)
    {
        tip << "\n" << pattern->getNumerator() << "/" << pattern->getDenominator() << ", "
            << plural(pattern->getBars(), "bar", "bars") << ", " << plural(pattern->getEventCount(), "hit", "hits");
    }
    tip << "\nRight-click for slot actions";
    return tip;
}

void GgdDrumEditor::refreshAll()
{
    refreshHeader();
    refreshSlots();
    refreshHistoryButtons();
    refreshContextStrip();
    refreshZoomControls();
}

void GgdDrumEditor::refreshHeader()
{
    const auto& pattern = currentPattern();
    const int slot = currentSlot();

    // Leave text being typed alone, unless it belongs to another slot.
    if (!patternName.hasKeyboardFocus(true) || nameFieldSlot != slot)
    {
        const auto name = nameOf(layer(), slot);
        patternName.setText(isDefaultName(name) ? juce::String() : name, false);
        nameFieldSlot = slot;
    }
    patternName.setTextToShowWhenEmpty("Pattern " + juce::String(slot + 1) + " (click to name)",
                                       tc(GgdThemeRole::faint));

    numeratorBox.setSelectedId(pattern.getNumerator(), juce::dontSendNotification);
    denominatorBox.setSelectedId(pattern.getDenominator(), juce::dontSendNotification);
    barsStepper->setValue(pattern.getBars(), GgdPatternOps::maxBarsFor(pattern.getNumerator(), pattern.getDenominator()));

    const int parked = pattern.getParkedCount();
    const bool showParked = parked > 0;
    if (showParked)
    {
        parkedButton.setButtonText(juce::String(parked) + " hidden");
        parkedButton.setColour(juce::TextButton::textColourOffId, tc(GgdThemeRole::warning));
        parkedButton.setTooltip(plural(parked, "hit sits", "hits sit")
                                + " outside the current meter or bar count. They come back if the pattern "
                                  "grows again. Click to discard them.");
    }
    if (parkedButton.isVisible() != showParked)
    {
        parkedButton.setVisible(showParked);
        resized();
    }

    for (int i = 0; i < static_cast<int>(std::size(gridChoices)); ++i)
        if (gridChoices[i].ticks == prefs.gridTicks)
            gridBox.setSelectedId(i + 1, juce::dontSendNotification);

    kitBox.setSelectedId(project.getActiveMapIndex(ui()) + 1, juce::dontSendNotification);
    const bool showPads = project.isDrumMachine(project.getActiveMapIndex(ui()));
    if (padsButton.isVisible() != showPads)
    {
        padsButton.setVisible(showPads);
        resized();
    }
    syncBox.setSelectedId(ui().getSyncMode() == GgdTimeline::SyncMode::free ? 2 : 1, juce::dontSendNotification);

    const bool select = grid->getToolMode() == GgdDrumGrid::ToolMode::select;
    drawButton.setToggleState(!select, juce::dontSendNotification);
    selectButton.setToggleState(select, juce::dontSendNotification);

    // On is the normal state and stays calm; Off is the one to notice.
    const bool enabled = !layer().getMuted();
    outputButton.setToggleState(!enabled, juce::dontSendNotification);
    outputButton.setButtonText(enabled ? "Seq On" : "Seq Off");
    outputButton.setStyle(enabled ? GgdButton::Style::normal : GgdButton::Style::danger);

    libraryButton.setToggleState(prefs.showLibrary, juce::dontSendNotification);
}

void GgdDrumEditor::refreshSlots()
{
    for (int i = 0; i < SEQ_MAX_PATTERNS; ++i)
        slotStrip->setSlotTooltip(i, slotTooltip(i));
    std::array<bool, SEQ_MAX_PATTERNS> content {};
    for (int i = 0; i < SEQ_MAX_PATTERNS; ++i)
        content[static_cast<size_t>(i)] = slotHasContent(i);
    slotStrip->setState(currentSlot(), playingSlot, content);
}

void GgdDrumEditor::refreshHistoryButtons()
{
    const auto& slotHistory = history[static_cast<size_t>(currentSlot())];
    undoButton.setEnabled(!slotHistory.undo.empty());
    redoButton.setEnabled(!slotHistory.redo.empty());
}

void GgdDrumEditor::refreshZoomControls()
{
    const float zoom = grid->getZoomScale();
    zoomSlider.setValue(zoom, juce::dontSendNotification);
    zoomValueButton.setButtonText(juce::String(juce::roundToInt(zoom * 100.0f)) + "%");
}

void GgdDrumEditor::refreshContextStrip()
{
    const bool select = grid->getToolMode() == GgdDrumGrid::ToolMode::select;
    const int count = grid->getSelectedCount();
    const bool tools = select && count > 0;

    const int probability = tools ? grid->getSelectedProbability() : -1;
    for (size_t i = 0; i < chanceButtons.size(); ++i)
        chanceButtons[i].setToggleState(probability == chanceValues[i], juce::dontSendNotification);

    const bool wasShowingTools = deleteButton.isVisible();
    if (wasShowingTools != tools)
        layoutContextStrip();
    repaint(contextArea);
}

void GgdDrumEditor::layoutContextStrip()
{
    const bool tools = grid->getToolMode() == GgdDrumGrid::ToolMode::select && grid->getSelectedCount() > 0;
    auto strip = contextArea.reduced(14, 10);

    std::vector<juce::Component*> all { &ghostButton, &accentButton, &quantizeButton, &humanizeButton,
                                        &flamButton, &doubleButton, &moreButton, &deleteButton };
    for (auto& button : chanceButtons)
        all.push_back(&button);

    if (!tools)
    {
        for (auto* component : all)
            component->setVisible(false);
        selectionInfoArea = {};
        hintArea = strip;
        return;
    }

    // Wide enough for everything? Otherwise Flam/Double and Humanize live
    // in the More menu only.
    const int full = 150 + 122 + 184 + 166 + 122 + 84 + 76 + 6 * 12;
    const bool showFlam = strip.getWidth() >= full;
    const bool showHumanize = strip.getWidth() >= full - 134;

    selectionInfoArea = strip.removeFromLeft(140);
    strip.removeFromLeft(10);

    auto place = [&strip](juce::Component& component, int width)
    {
        component.setBounds(strip.removeFromLeft(width));
        component.setVisible(true);
    };

    place(ghostButton, 58);
    place(accentButton, 64);
    strip.removeFromLeft(12);
    for (auto& button : chanceButtons)
        place(button, 46);
    strip.removeFromLeft(12);
    place(quantizeButton, showHumanize ? 80 : 86);
    quantizeButton.setSegment(false, showHumanize);
    if (showHumanize)
        place(humanizeButton, 86);
    else
        humanizeButton.setVisible(false);
    strip.removeFromLeft(12);
    if (showFlam)
    {
        place(flamButton, 56);
        place(doubleButton, 66);
        strip.removeFromLeft(12);
    }
    else
    {
        flamButton.setVisible(false);
        doubleButton.setVisible(false);
    }
    place(moreButton, 84);
    strip.removeFromLeft(12);
    place(deleteButton, 76);
    strip.removeFromLeft(14);
    hintArea = strip;
}

void GgdDrumEditor::layoutViewport()
{
    grid->setViewportSize(viewport.getWidth(), viewport.getHeight(), viewport.getScrollBarThickness());
}

//==============================================================================
void GgdDrumEditor::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(headerHeight);
    auto row1 = header.removeFromTop(row1Height).reduced(14, 9);
    auto row2 = header.reduced(14, 8);

    // row 1: identity and pattern ------------------------------------------------
    brandArea = row1.removeFromLeft(168);
    row1.removeFromLeft(6);
    slotStrip->setBounds(row1.removeFromLeft(8 * 31));
    row1.removeFromLeft(10);

    settingsButton.setBounds(row1.removeFromRight(34));
    row1.removeFromRight(4);
    libraryButton.setBounds(row1.removeFromRight(34));
    row1.removeFromRight(12);
    dragButton->setBounds(row1.removeFromRight(112));
    row1.removeFromRight(8);
    outputButton.setBounds(row1.removeFromRight(94));
    row1.removeFromRight(12);
    transport->setBounds(row1.removeFromRight(juce::jlimit(150, 220, row1.getWidth() - 380)));
    row1.removeFromRight(12);

    const int nameWidth = juce::jlimit(150, 300, row1.getWidth() - 106);
    patternName.setBounds(row1.removeFromLeft(nameWidth));
    row1.removeFromLeft(6);
    patternMenuButton.setBounds(row1.removeFromLeft(100));

    // row 2: editing tools ------------------------------------------------------
    drawButton.setBounds(row2.removeFromLeft(74));
    selectButton.setBounds(row2.removeFromLeft(80));
    row2.removeFromLeft(10);
    undoButton.setBounds(row2.removeFromLeft(36));
    redoButton.setBounds(row2.removeFromLeft(36));
    row2.removeFromLeft(18);

    numeratorBox.setBounds(row2.removeFromLeft(56));
    meterSlashArea = row2.removeFromLeft(16);
    denominatorBox.setBounds(row2.removeFromLeft(56));
    row2.removeFromLeft(10);
    barsStepper->setBounds(row2.removeFromLeft(124));
    if (parkedButton.isVisible())
    {
        row2.removeFromLeft(4);
        parkedButton.setBounds(row2.removeFromLeft(112));
    }
    row2.removeFromLeft(14);
    gridBox.setBounds(row2.removeFromLeft(108));

    syncBox.setBounds(row2.removeFromRight(150));
    row2.removeFromRight(8);
    kitBox.setBounds(row2.removeFromRight(juce::jlimit(140, 190, row2.getWidth() - 260)));
    if (padsButton.isVisible())
    {
        row2.removeFromRight(4);
        padsButton.setBounds(row2.removeFromRight(78));
    }
    row2.removeFromRight(18);

    row2.removeFromLeft(16);
    auto zoom = row2.removeFromLeft(juce::jmin(row2.getWidth(), 250));
    fitButton.setBounds(zoom.removeFromRight(32));
    zoom.removeFromRight(2);
    zoomValueButton.setBounds(zoom.removeFromRight(56));
    zoom.removeFromRight(2);
    zoomSlider.setBounds(zoom);

    // body -------------------------------------------------------------------
    if (prefs.showLibrary && browser != nullptr)
    {
        const int browserWidth = juce::jlimit(270, 340, getWidth() / 5);
        browser->setBounds(bounds.removeFromRight(browserWidth));
    }
    contextArea = bounds.removeFromBottom(contextHeight);
    viewport.setBounds(bounds);
    layoutViewport();
    layoutContextStrip();

    if (settingsPanel != nullptr)
        settingsPanel->setBounds(getLocalBounds());
    if (padEditor != nullptr)
        padEditor->setBounds(getLocalBounds());

    if (prefs.editorWidth != getWidth() || prefs.editorHeight != getHeight())
    {
        prefs.editorWidth = getWidth();
        prefs.editorHeight = getHeight();
        if (isShowing())
            preferenceStore->save(prefs);
    }
}

bool GgdDrumEditor::keyPressed(const juce::KeyPress& key)
{
    // Keys a text field did not use (Ctrl+D, Ctrl+Z after its own undo
    // history...) must not edit the pattern behind it.
    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    // Keys that reach the editor itself (focus on a button, for example)
    // still drive the timeline.
    if (!overlayOpen() && grid != nullptr)
        return grid->keyPressed(key);
    return false;
}

//==============================================================================
void GgdDrumEditor::paint(juce::Graphics& g)
{
    g.fillAll(tc(GgdThemeRole::background));
    paintHeader(g);

    g.setColour(tc(GgdThemeRole::outside));
    g.fillRect(viewport.getBounds());

    paintContextStrip(g);

    if (browser != nullptr && browser->isVisible())
    {
        g.setColour(tc(GgdThemeRole::border));
        g.fillRect(browser->getX() - 1, browser->getY(), 1, browser->getHeight());
    }
}

void GgdDrumEditor::paintHeader(juce::Graphics& g)
{
    const auto header = getLocalBounds().removeFromTop(headerHeight);
    juce::ColourGradient fill(tc(GgdThemeRole::panelRaised), 0.0f, 0.0f, tc(GgdThemeRole::panel), 0.0f,
                              static_cast<float>(row1Height), false);
    g.setGradientFill(fill);
    g.fillRect(header.withHeight(row1Height));
    g.setColour(tc(GgdThemeRole::panel));
    g.fillRect(header.withTrimmedTop(row1Height));

    g.setColour(tc(GgdThemeRole::borderSoft));
    g.fillRect(14, row1Height, getWidth() - 28, 1);
    g.setColour(tc(GgdThemeRole::border));
    g.fillRect(0, headerHeight - 1, getWidth(), 1);

    // brand: a small mark (three hits on a lane) and the product name
    auto brand = brandArea;
    auto mark = brand.removeFromLeft(30).withSizeKeepingCentre(30, 30).toFloat();
    juce::ColourGradient markFill(tc(GgdThemeRole::accent), mark.getX(), mark.getY(),
                                  tc(GgdThemeRole::accentSecondary), mark.getRight(), mark.getBottom(), false);
    g.setGradientFill(markFill);
    g.fillRoundedRectangle(mark, 8.0f);
    const auto ink = tc(GgdThemeRole::accentText);
    g.setColour(ink);
    const float barWidth = 4.0f;
    const float heights[] = { 14.0f, 8.0f, 18.0f };
    for (int i = 0; i < 3; ++i)
    {
        const float x = mark.getX() + 7.5f + i * 6.0f;
        g.fillRoundedRectangle(x, mark.getBottom() - 6.0f - heights[i], barWidth, heights[i], 1.6f);
    }

    brand.removeFromLeft(10);
    auto title = brand.withTrimmedBottom(14).toFloat();
    g.setColour(tc(GgdThemeRole::text));
    g.setFont(GgdFonts::bold(15.0f));
    g.drawText("Stochas", title.withTrimmedTop(2.0f), juce::Justification::bottomLeft, false);
    const float stochasWidth = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), "Stochas ");
    g.setColour(tc(GgdThemeRole::accent));
    g.drawText("GGD", title.withTrimmedTop(2.0f).withTrimmedLeft(stochasWidth), juce::Justification::bottomLeft,
               false);
    g.setColour(tc(GgdThemeRole::faint));
    g.setFont(GgdFonts::medium(10.5f));
    g.drawText(juce::String("Drum sequencer  v") + JucePlugin_VersionString,
               brand.withTrimmedTop(brand.getHeight() - 14), juce::Justification::topLeft, false);

    // meter slash
    g.setColour(tc(GgdThemeRole::muted));
    g.setFont(GgdFonts::medium(14.0f));
    g.drawText("/", meterSlashArea, juce::Justification::centred, false);
}

void GgdDrumEditor::paintContextStrip(juce::Graphics& g)
{
    if (contextArea.isEmpty())
        return;

    g.setColour(tc(GgdThemeRole::panel));
    g.fillRect(contextArea);
    g.setColour(tc(GgdThemeRole::border));
    g.fillRect(contextArea.getX(), contextArea.getY(), contextArea.getWidth(), 1);

    const bool select = grid->getToolMode() == GgdDrumGrid::ToolMode::select;
    const int count = grid->getSelectedCount();

    if (!selectionInfoArea.isEmpty())
    {
        auto info = selectionInfoArea.toFloat();
        g.setColour(tc(GgdThemeRole::text));
        g.setFont(GgdFonts::semibold(13.0f));
        g.drawText(plural(count, "hit", "hits"), info.removeFromTop(info.getHeight() * 0.55f),
                   juce::Justification::bottomLeft, false);

        const int velocity = grid->getSelectedVelocity();
        const int probability = grid->getSelectedProbability();
        juce::String detail = "vel " + juce::String(velocity);
        detail << "  |  " << (probability >= 0 ? juce::String(probability) + "%" : juce::String("mixed chance"));
        g.setColour(tc(GgdThemeRole::muted));
        g.setFont(GgdFonts::regular(11.5f));
        g.drawText(detail, info, juce::Justification::topLeft, false);
    }

    auto area = hintArea.toFloat();
    if (area.getWidth() < 40.0f)
        return;

    if (statusText.isNotEmpty())
    {
        auto line = area;
        g.setColour(tc(GgdThemeRole::accent));
        g.fillEllipse(juce::Rectangle<float>(6.0f, 6.0f).withCentre({ line.getX() + 4.0f, line.getCentreY() }));
        line.removeFromLeft(16.0f);
        g.setColour(tc(GgdThemeRole::text));
        g.setFont(GgdFonts::regular(12.5f));
        g.drawFittedText(statusText, line.toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);
        return;
    }

    struct Hint
    {
        const char* key;
        const char* label;
    };
    static const Hint drawHints[] = { { "Click", "add" }, { "Drag", "paint" }, { "Right-drag", "erase" },
                                      { "Shift-drag", "velocity" }, { "Alt-drag", "timing" },
                                      { "Ctrl+Wheel", "zoom" } };
    static const Hint selectHints[] = { { "Drag", "select" }, { "Shift", "add" }, { "Ctrl+A", "all" },
                                        { "Ctrl+V", "paste" }, { "Esc", "deselect" } };
    static const Hint toolHints[] = { { "Drag", "move" }, { "Alt-drag", "copy" }, { "Arrows", "nudge" },
                                      { "Shift-drag", "velocity" } };

    const Hint* hints = drawHints;
    size_t hintCount = std::size(drawHints);
    if (select && count > 0)
    {
        hints = toolHints;
        hintCount = std::size(toolHints);
    }
    else if (select)
    {
        hints = selectHints;
        hintCount = std::size(selectHints);
    }

    const bool mac = (juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0;
    const auto keyFont = GgdFonts::semibold(11.0f);
    const auto labelFont = GgdFonts::regular(12.0f);
    float x = area.getX();
    for (size_t i = 0; i < hintCount; ++i)
    {
        auto key = juce::String(hints[i].key);
        if (mac)
            key = key.replace("Ctrl", "Cmd").replace("Alt", "Opt");
        const juce::String label(hints[i].label);
        const float keyWidth = juce::GlyphArrangement::getStringWidth(keyFont, key) + 14.0f;
        const float labelWidth = juce::GlyphArrangement::getStringWidth(labelFont, label);
        if (x + keyWidth + 6.0f + labelWidth > area.getRight())
            break;

        const juce::Rectangle<float> cap(x, area.getCentreY() - 10.0f, keyWidth, 20.0f);
        g.setColour(tc(GgdThemeRole::panelRaised));
        g.fillRoundedRectangle(cap, 4.0f);
        g.setColour(tc(GgdThemeRole::border));
        g.drawRoundedRectangle(cap.reduced(0.5f), 4.0f, 1.0f);
        g.setColour(tc(GgdThemeRole::text).withAlpha(0.85f));
        g.setFont(keyFont);
        g.drawText(key, cap, juce::Justification::centred, false);

        x = cap.getRight() + 6.0f;
        g.setColour(tc(GgdThemeRole::muted));
        g.setFont(labelFont);
        g.drawText(label, juce::Rectangle<float>(x, area.getY(), labelWidth + 2.0f, area.getHeight()),
                   juce::Justification::centredLeft, false);
        x += labelWidth + 18.0f;
    }
}
