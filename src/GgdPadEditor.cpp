#include "GgdPadEditor.h"

namespace
{
juce::Colour tc(GgdThemeRole role)
{
    return ggdThemeColour(role);
}

constexpr int cardPadding = 28;
constexpr int padRowHeight = 36;
}

//==============================================================================
// One pad: number and colour, name field, note selector, audition button.
class GgdPadEditor::PadRow final : public juce::Component
{
public:
    PadRow(GgdPadEditor& editor, int padIndex) : owner(editor), pad(padIndex)
    {
        name.setFont(GgdFonts::regular(13.0f));
        name.setJustification(juce::Justification::centredLeft);
        name.setIndents(8, 0);
        name.setTextToShowWhenEmpty(GgdDrumMachineKit::defaultName(pad), tc(GgdThemeRole::faint));
        name.setText(owner.pads.getName(pad), false);
        name.setTooltip("Name shown on this pad's row");
        name.onTextChange = [this]
        {
            owner.pads.setName(pad, name.getText());
            owner.emit();
        };
        name.onReturnKey = [this] { owner.focusPad(pad + 1 < owner.pads.getPadCount() ? pad + 1 : pad); };
        addAndMakeVisible(name);

        for (int note = 0; note < 128; ++note)
            noteBox.addItem(GgdDrumMachineKit::noteName(note) + "   (" + juce::String(note) + ")", note + 1);
        noteBox.setSelectedId(owner.pads.getNote(pad) + 1, juce::dontSendNotification);
        noteBox.setTooltip("MIDI note this pad sends");
        noteBox.setWantsKeyboardFocus(false);
        noteBox.onChange = [this]
        {
            owner.pads.setNote(pad, noteBox.getSelectedId() - 1);
            owner.emit();
        };
        addAndMakeVisible(noteBox);

        play.setStyle(GgdButton::Style::quiet);
        play.setTooltip("Play this pad");
        play.onClick = [this]
        {
            if (owner.audition)
                owner.audition(pad);
        };
        addAndMakeVisible(play);
    }

    void focusName()
    {
        if (name.isShowing())
        {
            name.grabKeyboardFocus();
            name.selectAll();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const auto colour = ggdInstrumentColour(GgdDrumMachineKit::semanticId(pad));
        auto number = area.removeFromLeft(34.0f);
        g.setColour(colour);
        g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({ number.getX() + 6.0f, number.getCentreY() }));
        g.setColour(tc(GgdThemeRole::muted));
        g.setFont(GgdFonts::medium(12.0f));
        g.drawText(juce::String(pad + 1), number.withTrimmedLeft(14.0f), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 3);
        area.removeFromLeft(34);
        play.setBounds(area.removeFromRight(32));
        area.removeFromRight(6);
        noteBox.setBounds(area.removeFromRight(124));
        area.removeFromRight(8);
        name.setBounds(area);
    }

    void syncFromModel()
    {
        if (!name.hasKeyboardFocus(true))
            name.setText(owner.pads.getName(pad), false);
        noteBox.setSelectedId(owner.pads.getNote(pad) + 1, juce::dontSendNotification);
    }

private:
    GgdPadEditor& owner;
    int pad;
    juce::TextEditor name;
    juce::ComboBox noteBox;
    GgdButton play { {}, GgdIcon::play };
};

//==============================================================================
GgdPadEditor::GgdPadEditor(const GgdDrumMachineKit& current,
                           std::function<void(const GgdDrumMachineKit&)> onChange,
                           std::function<void(int)> onAudition,
                           std::function<void()> onClose)
    : pads(current), changed(std::move(onChange)), audition(std::move(onAudition)), closed(std::move(onClose))
{
    setOpaque(false);
    setWantsKeyboardFocus(true);

    for (int count = 1; count <= GGD_DRUM_MACHINE_MAX_PADS; ++count)
        countBox.addItem(juce::String(count) + (count == 1 ? " pad" : " pads"), count);
    countBox.setSelectedId(pads.getPadCount(), juce::dontSendNotification);
    countBox.setTooltip("How many pads the kit has. Extra pads continue up the keyboard from the last one.");
    countBox.setWantsKeyboardFocus(false);
    countBox.onChange = [this]
    {
        const int count = countBox.getSelectedId();
        if (count <= 0 || count == pads.getPadCount())
            return;
        // New pads continue chromatically from the last pad.
        for (int pad = pads.getPadCount(); pad < count; ++pad)
            if (pads.getName(pad).isEmpty() && pads.getNote(pad) == GgdDrumMachineKit::defaultNote(pad) && pad > 0)
                pads.setNote(pad, juce::jmin(127, pads.getNote(pad - 1) + 1));
        pads.setPadCount(count);
        rebuildRows();
        emit();
    };
    addAndMakeVisible(countBox);

    resetButton.setStyle(GgdButton::Style::quiet);
    resetButton.setTooltip("16 pads on C1 to D#2 (MIDI 36-51) with default names");
    resetButton.onClick = [this]
    {
        pads.reset();
        countBox.setSelectedId(pads.getPadCount(), juce::dontSendNotification);
        rebuildRows();
        emit();
    };
    addAndMakeVisible(resetButton);

    doneButton.setStyle(GgdButton::Style::primary);
    doneButton.onClick = [this] { if (closed) closed(); };
    addAndMakeVisible(doneButton);

    closeButton.setStyle(GgdButton::Style::quiet);
    closeButton.setTooltip("Close (Esc)");
    closeButton.onClick = [this] { if (closed) closed(); };
    addAndMakeVisible(closeButton);

    viewport.setViewedComponent(&list, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(8);
    addAndMakeVisible(viewport);

    rebuildRows();
}

GgdPadEditor::~GgdPadEditor()
{
    viewport.setViewedComponent(nullptr, false);
}

void GgdPadEditor::rebuildRows()
{
    rows.clear();
    for (int pad = 0; pad < pads.getPadCount(); ++pad)
        list.addAndMakeVisible(rows.add(new PadRow(*this, pad)));
    // Rows created while parented here may predate our look and feel.
    list.sendLookAndFeelChange();
    layoutRows();
}

void GgdPadEditor::layoutRows()
{
    const int width = viewport.getMaximumVisibleWidth();
    list.setSize(width, rows.size() * padRowHeight);
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds(0, i * padRowHeight, width - 4, padRowHeight);
}

void GgdPadEditor::emit()
{
    if (changed)
        changed(pads);
    repaint();
}

void GgdPadEditor::focusPad(int pad)
{
    if (pad < 0 || pad >= rows.size())
        return;
    auto* row = rows[pad];
    viewport.setViewPosition(0, juce::jmax(0, row->getY() - padRowHeight * 2));
    row->focusName();
}

juce::Rectangle<int> GgdPadEditor::getCardBounds() const
{
    const int width = juce::jmin(560, getWidth() - 48);
    const int height = juce::jmin(640, getHeight() - 40);
    return juce::Rectangle<int>(width, height).withCentre(getLocalBounds().getCentre());
}

void GgdPadEditor::paint(juce::Graphics& g)
{
    g.fillAll(tc(GgdThemeRole::shadow).withMultipliedAlpha(0.85f));

    const auto card = getCardBounds();
    const auto cardF = card.toFloat();
    for (int i = 0; i < 6; ++i)
    {
        g.setColour(juce::Colours::black.withAlpha(0.05f));
        g.fillRoundedRectangle(cardF.expanded(2.0f + i * 2.0f).translated(0.0f, 4.0f), 14.0f + i * 2.0f);
    }
    g.setColour(tc(GgdThemeRole::panelRaised));
    g.fillRoundedRectangle(cardF, 12.0f);
    g.setColour(tc(GgdThemeRole::borderStrong).withAlpha(0.7f));
    g.drawRoundedRectangle(cardF.reduced(0.5f), 12.0f, 1.0f);

    auto inner = card.reduced(cardPadding, 22);
    g.setColour(tc(GgdThemeRole::text));
    g.setFont(GgdFonts::semibold(18.0f));
    g.drawText("Drum machine pads", inner.removeFromTop(30), juce::Justification::centredLeft, false);
    g.setColour(tc(GgdThemeRole::muted));
    g.setFont(GgdFonts::regular(12.0f));
    g.drawText("Name each pad and pick the note it sends. Saved with this project.", inner.removeFromTop(18),
               juce::Justification::centredLeft, false);

    // column headings
    g.setColour(tc(GgdThemeRole::accent));
    g.setFont(GgdFonts::bold(10.5f));
    auto headings = headingArea;
    headings.removeFromLeft(34);
    headings.removeFromRight(38);
    auto note = headings.removeFromRight(124);
    headings.removeFromRight(8);
    g.drawText("NAME", headings, juce::Justification::centredLeft, false);
    g.drawText("NOTE", note, juce::Justification::centredLeft, false);

    g.setColour(tc(GgdThemeRole::borderSoft));
    g.fillRect(card.getX() + cardPadding, headingArea.getBottom() + 3, card.getWidth() - cardPadding * 2, 1);
    g.fillRect(card.getX() + cardPadding, viewport.getBottom() + 8, card.getWidth() - cardPadding * 2, 1);
}

void GgdPadEditor::resized()
{
    const auto card = getCardBounds();
    closeButton.setBounds(card.getRight() - cardPadding - 26, card.getY() + 20, 28, 28);

    auto inner = card.reduced(cardPadding, 22);
    inner.removeFromTop(30 + 18 + 14);

    auto controls = inner.removeFromTop(30);
    countBox.setBounds(controls.removeFromLeft(120));
    controls.removeFromLeft(8);
    resetButton.setBounds(controls.removeFromLeft(160));
    inner.removeFromTop(12);

    headingArea = inner.removeFromTop(20);
    inner.removeFromTop(6);

    auto footer = inner.removeFromBottom(34);
    doneButton.setBounds(footer.removeFromRight(96));
    inner.removeFromBottom(18);
    viewport.setBounds(inner);
    layoutRows();
}

void GgdPadEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!getCardBounds().contains(e.getPosition()) && closed)
        closed();
}

bool GgdPadEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (closed)
            closed();
        return true;
    }
    return true; // modal: nothing leaks to the timeline underneath
}
