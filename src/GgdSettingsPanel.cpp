#include "GgdSettingsPanel.h"

#include <cmath>

namespace
{
juce::Colour tc(GgdThemeRole role)
{
    return ggdThemeColour(role);
}

juce::String& settingsFolderName()
{
    static juce::String name { "Stochas GGD" };
    return name;
}

juce::PropertiesFile::Options preferenceFileOptions()
{
    // Same file as 1.x so existing preferences carry over.
    juce::PropertiesFile::Options options;
    options.applicationName = "Stochas GGD Appearance";
    options.filenameSuffix = "settings";
    options.folderName = ggdSettingsFolderName();
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.millisecondsBeforeSaving = 500;
    return options;
}

bool readBool(const juce::PropertiesFile& file, const char* key, bool fallback)
{
    return file.getIntValue(key, fallback ? 1 : 0) != 0;
}

int readInt(const juce::PropertiesFile& file, const char* key, int fallback, int lo, int hi)
{
    return juce::jlimit(lo, hi, file.getIntValue(key, fallback));
}

bool isKnownGrid(int ticks)
{
    switch (ticks)
    {
        case 480: case 240: case 120: case 320: case 160: case 80:
            return true;
        default:
            return false;
    }
}

constexpr int cardPadding = 28;
constexpr int rowHeight = 30;
constexpr int sectionHeight = 26;
}

//==============================================================================
juce::String ggdSettingsFolderName()
{
    return settingsFolderName();
}

void ggdSetSettingsFolderName(const juce::String& name)
{
    settingsFolderName() = name;
}

//==============================================================================
bool GgdPreferences::operator==(const GgdPreferences& o) const
{
    return theme == o.theme && colourByInstrument == o.colourByInstrument
        && followPlayhead == o.followPlayhead && smoothPlayhead == o.smoothPlayhead
        && playheadGlow == o.playheadGlow && autoFineGrid == o.autoFineGrid
        && shiftHoverVelocity == o.shiftHoverVelocity && audition == o.audition
        && playedEffects == o.playedEffects && playedRipple == o.playedRipple
        && playedLaneFlash == o.playedLaneFlash && velocityReactive == o.velocityReactive
        && reduceMotion == o.reduceMotion && effectIntensityPercent == o.effectIntensityPercent
        && effectDecayMs == o.effectDecayMs && showLibrary == o.showLibrary
        && gridTicks == o.gridTicks && std::abs(zoom - o.zoom) < 0.001f
        && editorWidth == o.editorWidth && editorHeight == o.editorHeight;
}

//==============================================================================
GgdPreferenceStore::GgdPreferenceStore()
    : file(std::make_unique<juce::PropertiesFile>(preferenceFileOptions()))
{
}

GgdPreferenceStore::~GgdPreferenceStore()
{
    file->saveIfNeeded();
}

GgdPreferences GgdPreferenceStore::load() const
{
    GgdPreferences p;
    const auto& f = *file;
    p.theme = readInt(f, "theme", p.theme, 0, static_cast<int>(GgdThemeId::count) - 1);
    p.colourByInstrument = readBool(f, "colourByInstrument", p.colourByInstrument);
    p.followPlayhead = readBool(f, "followPlayhead", p.followPlayhead);
    p.smoothPlayhead = readBool(f, "smoothPlayhead", p.smoothPlayhead);
    p.playheadGlow = readBool(f, "playheadGlow", p.playheadGlow);
    p.autoFineGrid = readBool(f, "autoFineGrid", p.autoFineGrid);
    p.shiftHoverVelocity = readBool(f, "shiftHoverVelocityInspector", p.shiftHoverVelocity);
    p.audition = readBool(f, "articulationAudition", p.audition);
    p.playedEffects = readBool(f, "playedHitEffects", p.playedEffects);
    p.playedRipple = readBool(f, "playedHitRipple", p.playedRipple);
    p.playedLaneFlash = readBool(f, "playedLaneFlash", p.playedLaneFlash);
    p.velocityReactive = readBool(f, "playedVelocityReactive", p.velocityReactive);
    p.reduceMotion = readBool(f, "reduceMotion", p.reduceMotion);
    p.effectIntensityPercent = readInt(f, "playedEffectIntensity", p.effectIntensityPercent, 0, 150);
    p.effectDecayMs = readInt(f, "playedEffectDecayMs", p.effectDecayMs, 100, 1400);
    p.showLibrary = readBool(f, "showLibrary", p.showLibrary);
    const int grid = f.getIntValue("gridTicks", p.gridTicks);
    p.gridTicks = isKnownGrid(grid) ? grid : 240;
    p.zoom = juce::jlimit(0.5f, 4.0f, static_cast<float>(f.getDoubleValue("zoom", p.zoom)));
    p.editorWidth = readInt(f, "editorWidth", p.editorWidth, 800, 8000);
    p.editorHeight = readInt(f, "editorHeight", p.editorHeight, 500, 6000);
    return p;
}

void GgdPreferenceStore::save(const GgdPreferences& p)
{
    if (load() == p)
        return;

    auto& f = *file;
    f.setValue("theme", p.theme);
    f.setValue("colourByInstrument", p.colourByInstrument ? 1 : 0);
    f.setValue("followPlayhead", p.followPlayhead ? 1 : 0);
    f.setValue("smoothPlayhead", p.smoothPlayhead ? 1 : 0);
    f.setValue("playheadGlow", p.playheadGlow ? 1 : 0);
    f.setValue("autoFineGrid", p.autoFineGrid ? 1 : 0);
    f.setValue("shiftHoverVelocityInspector", p.shiftHoverVelocity ? 1 : 0);
    f.setValue("articulationAudition", p.audition ? 1 : 0);
    f.setValue("playedHitEffects", p.playedEffects ? 1 : 0);
    f.setValue("playedHitRipple", p.playedRipple ? 1 : 0);
    f.setValue("playedLaneFlash", p.playedLaneFlash ? 1 : 0);
    f.setValue("playedVelocityReactive", p.velocityReactive ? 1 : 0);
    f.setValue("reduceMotion", p.reduceMotion ? 1 : 0);
    f.setValue("playedEffectIntensity", p.effectIntensityPercent);
    f.setValue("playedEffectDecayMs", p.effectDecayMs);
    f.setValue("showLibrary", p.showLibrary ? 1 : 0);
    f.setValue("gridTicks", p.gridTicks);
    f.setValue("zoom", static_cast<double>(p.zoom));
    f.setValue("editorWidth", p.editorWidth);
    f.setValue("editorHeight", p.editorHeight);
    sendChangeMessage();
}

//==============================================================================
// A small live preview of one theme: timeline background, a few coloured
// hits, an accent bar and the theme name.
class GgdSettingsPanel::ThemeSwatch final : public juce::Component
{
public:
    ThemeSwatch(int themeIndex, std::function<void(int)> onPick)
        : index(themeIndex), pick(std::move(onPick))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTitle(ggdThemeName(ggdThemeFromIndex(index)) + " theme");
        setRepaintsOnMouseActivity(true);
    }

    void setSelected(bool shouldBeSelected)
    {
        if (selected != shouldBeSelected)
        {
            selected = shouldBeSelected;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        // Drawn with this swatch's own palette.
        const auto theme = ggdThemeFromIndex(index);
        const auto background = ggdThemeColour(GgdThemeRole::background, theme);
        const auto panel = ggdThemeColour(GgdThemeRole::panel, theme);
        const auto accent = ggdThemeColour(GgdThemeRole::playhead, theme);
        const auto line = ggdThemeColour(GgdThemeRole::beatLine, theme);
        const auto kick = ggdInstrumentColour("kick.hit", theme);
        const auto snare = ggdInstrumentColour("snare.hit", theme);
        const auto hat = ggdInstrumentColour("hihat.closed", theme);

        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto preview = bounds.removeFromTop(bounds.getHeight() - 22.0f);

        g.setColour(background);
        g.fillRoundedRectangle(preview, 7.0f);
        g.setColour(panel);
        g.fillRoundedRectangle(preview.withWidth(preview.getWidth() * 0.26f), 7.0f);
        g.fillRect(preview.withWidth(preview.getWidth() * 0.26f).withTrimmedLeft(7.0f));

        auto lanes = preview.withTrimmedLeft(preview.getWidth() * 0.26f + 6.0f).reduced(2.0f, 7.0f);
        g.setColour(line);
        for (int i = 1; i < 4; ++i)
            g.fillRect(lanes.getX() + lanes.getWidth() * i / 4.0f, preview.getY() + 4.0f, 1.0f,
                       preview.getHeight() - 8.0f);

        const float laneHeight = lanes.getHeight() / 3.0f;
        const juce::Colour colours[] = { hat, snare, kick };
        const int pattern[3][8] = { { 1, 1, 1, 1, 1, 1, 1, 1 }, { 0, 0, 1, 0, 0, 0, 1, 0 }, { 1, 0, 0, 0, 1, 1, 0, 0 } };
        for (int lane = 0; lane < 3; ++lane)
            for (int step = 0; step < 8; ++step)
                if (pattern[lane][step] != 0)
                {
                    const float x = lanes.getX() + lanes.getWidth() * (step + 0.5f) / 8.0f;
                    const float y = lanes.getY() + laneHeight * (lane + 0.5f);
                    g.setColour(colours[lane].withAlpha(lane == 0 && (step & 1) ? 0.55f : 0.95f));
                    g.fillRoundedRectangle(x - 2.5f, y - 3.5f, 5.0f, 7.0f, 1.5f);
                }

        g.setColour(accent);
        g.fillRect(lanes.getX() + lanes.getWidth() * 0.62f, preview.getY() + 3.0f, 1.5f, preview.getHeight() - 6.0f);

        const bool hover = isMouseOver(true);
        g.setColour(selected ? tc(GgdThemeRole::accent)
                             : tc(GgdThemeRole::border).withAlpha(hover ? 1.0f : 0.7f));
        g.drawRoundedRectangle(preview.reduced(0.5f), 7.0f, selected ? 2.0f : 1.0f);

        g.setColour(selected ? tc(GgdThemeRole::text) : tc(GgdThemeRole::muted));
        g.setFont(selected ? GgdFonts::semibold(12.0f) : GgdFonts::medium(12.0f));
        g.drawText(ggdThemeName(ggdThemeFromIndex(index)), bounds, juce::Justification::centred, false);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && pick)
            pick(index);
    }

private:
    int index = 0;
    bool selected = false;
    std::function<void(int)> pick;
};

//==============================================================================
GgdSettingsPanel::GgdSettingsPanel(const GgdPreferences& current,
                                   std::function<void(const GgdPreferences&)> onChange,
                                   std::function<void()> onClose)
    : prefs(current), changed(std::move(onChange)), closed(std::move(onClose))
{
    setOpaque(false);
    setWantsKeyboardFocus(true);
    setInterceptsMouseClicks(true, true);

    for (int i = 0; i < static_cast<int>(GgdThemeId::count); ++i)
    {
        auto* swatch = swatches.add(new ThemeSwatch(i, [this](int index)
        {
            prefs.theme = index;
            emit();
        }));
        addAndMakeVisible(swatch);
    }

    setupToggle(colourToggle, "Colour hits by instrument",
                "Kick, snare, toms, hats and cymbals each get their own colour", &GgdPreferences::colourByInstrument);
    setupToggle(followToggle, "Follow the playhead",
                "Scroll long patterns so the playhead stays in view while the host plays",
                &GgdPreferences::followPlayhead);
    setupToggle(smoothToggle, "Smooth playhead motion",
                "Move the playhead continuously instead of stepping in sixteenths", &GgdPreferences::smoothPlayhead);
    setupToggle(glowToggle, "Playhead glow", "Soft trail ahead of the playhead", &GgdPreferences::playheadGlow);
    setupToggle(autoFineToggle, "Finer grid when zoomed in",
                "At 350% zoom and above the grid halves (1/16 becomes 1/32, 1/8T becomes 1/16T)",
                &GgdPreferences::autoFineGrid);
    setupToggle(shiftHoverToggle, "Hold Shift to inspect velocity",
                "Holding Shift over a hit shows its velocity", &GgdPreferences::shiftHoverVelocity);
    setupToggle(auditionToggle, "Click a row name to audition",
                "Plays the mapped GGD note so you can hear the articulation", &GgdPreferences::audition);
    setupToggle(effectsToggle, "Light up played hits",
                "Hits glow as the sequencer plays them. Muted and skipped (probability) hits stay dark.",
                &GgdPreferences::playedEffects);
    setupToggle(rippleToggle, "Ripple", "An expanding ring around each played hit", &GgdPreferences::playedRipple);
    setupToggle(laneFlashToggle, "Lane flash", "Briefly highlights the row of each played hit",
                &GgdPreferences::playedLaneFlash);
    setupToggle(velocityToggle, "Scale with velocity", "Louder hits produce brighter feedback",
                &GgdPreferences::velocityReactive);
    setupToggle(reduceMotionToggle, "Reduce motion", "Keeps feedback static: no growing blooms or ripples",
                &GgdPreferences::reduceMotion);

    auto setupSlider = [this](juce::Label& label, const juce::String& text, juce::Slider& slider,
                              double lo, double hi, double step, const juce::String& suffix)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(GgdFonts::regular(12.5f));
        label.setColour(juce::Label::textColourId, tc(GgdThemeRole::text));
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setRange(lo, hi, step);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
        slider.setTextValueSuffix(suffix);
        slider.setMouseClickGrabsKeyboardFocus(false);
        slider.onValueChange = [this] { emit(); };
        addAndMakeVisible(slider);
    };
    setupSlider(intensityLabel, "Strength", intensitySlider, 0.0, 150.0, 1.0, " %");
    setupSlider(decayLabel, "Decay", decaySlider, 100.0, 1400.0, 10.0, " ms");
    intensitySlider.setDoubleClickReturnValue(true, 100.0);
    decaySlider.setDoubleClickReturnValue(true, 420.0);

    resetButton.setStyle(GgdButton::Style::quiet);
    resetButton.setTooltip("Restore the default editor preferences");
    resetButton.onClick = [this]
    {
        GgdPreferences defaults;
        // Layout choices are not "settings" in this sheet; keep them.
        defaults.showLibrary = prefs.showLibrary;
        defaults.gridTicks = prefs.gridTicks;
        defaults.zoom = prefs.zoom;
        defaults.editorWidth = prefs.editorWidth;
        defaults.editorHeight = prefs.editorHeight;
        prefs = defaults;
        refreshControls();
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

    refreshControls();
}

GgdSettingsPanel::~GgdSettingsPanel() = default;

void GgdSettingsPanel::setupToggle(juce::ToggleButton& button, const juce::String& text,
                                   const juce::String& tooltip, bool GgdPreferences::*field)
{
    button.setButtonText(text);
    button.setTooltip(tooltip);
    button.setMouseClickGrabsKeyboardFocus(false);
    button.onClick = [this, &button, field]
    {
        prefs.*field = button.getToggleState();
        emit();
    };
    addAndMakeVisible(button);
}

void GgdSettingsPanel::refreshControls()
{
    for (int i = 0; i < swatches.size(); ++i)
        swatches[i]->setSelected(i == prefs.theme);

    colourToggle.setToggleState(prefs.colourByInstrument, juce::dontSendNotification);
    followToggle.setToggleState(prefs.followPlayhead, juce::dontSendNotification);
    smoothToggle.setToggleState(prefs.smoothPlayhead, juce::dontSendNotification);
    glowToggle.setToggleState(prefs.playheadGlow, juce::dontSendNotification);
    autoFineToggle.setToggleState(prefs.autoFineGrid, juce::dontSendNotification);
    shiftHoverToggle.setToggleState(prefs.shiftHoverVelocity, juce::dontSendNotification);
    auditionToggle.setToggleState(prefs.audition, juce::dontSendNotification);
    effectsToggle.setToggleState(prefs.playedEffects, juce::dontSendNotification);
    rippleToggle.setToggleState(prefs.playedRipple, juce::dontSendNotification);
    laneFlashToggle.setToggleState(prefs.playedLaneFlash, juce::dontSendNotification);
    velocityToggle.setToggleState(prefs.velocityReactive, juce::dontSendNotification);
    reduceMotionToggle.setToggleState(prefs.reduceMotion, juce::dontSendNotification);
    intensitySlider.setValue(prefs.effectIntensityPercent, juce::dontSendNotification);
    decaySlider.setValue(prefs.effectDecayMs, juce::dontSendNotification);
    updateEnablement();
}

void GgdSettingsPanel::updateEnablement()
{
    const bool effects = prefs.playedEffects;
    rippleToggle.setEnabled(effects && !prefs.reduceMotion);
    laneFlashToggle.setEnabled(effects);
    velocityToggle.setEnabled(effects);
    reduceMotionToggle.setEnabled(effects);
    intensitySlider.setEnabled(effects);
    decaySlider.setEnabled(effects);
    intensityLabel.setAlpha(effects ? 1.0f : 0.45f);
    decayLabel.setAlpha(effects ? 1.0f : 0.45f);
    glowToggle.setEnabled(true);
}

void GgdSettingsPanel::emit()
{
    prefs.effectIntensityPercent = static_cast<int>(std::lround(intensitySlider.getValue()));
    prefs.effectDecayMs = static_cast<int>(std::lround(decaySlider.getValue()));
    for (int i = 0; i < swatches.size(); ++i)
        swatches[i]->setSelected(i == prefs.theme);
    updateEnablement();

    // Colours may have changed with the theme.
    intensityLabel.setColour(juce::Label::textColourId, tc(GgdThemeRole::text));
    decayLabel.setColour(juce::Label::textColourId, tc(GgdThemeRole::text));

    if (changed)
        changed(prefs);
    repaint();
}

juce::Rectangle<int> GgdSettingsPanel::getCardBounds() const
{
    const int width = juce::jmin(820, getWidth() - 48);
    const int height = juce::jmin(560, getHeight() - 40);
    return juce::Rectangle<int>(width, height).withCentre(getLocalBounds().getCentre());
}

void GgdSettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(tc(GgdThemeRole::shadow).withMultipliedAlpha(0.85f));

    const auto card = getCardBounds();
    const auto cardF = card.toFloat();

    // soft drop shadow
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
    auto titleRow = inner.removeFromTop(30);
    g.setColour(tc(GgdThemeRole::text));
    g.setFont(GgdFonts::semibold(18.0f));
    g.drawText("Settings", titleRow, juce::Justification::centredLeft, false);
    g.setColour(tc(GgdThemeRole::muted));
    g.setFont(GgdFonts::regular(12.0f));
    g.drawText("Preferences apply to every Stochas GGD window on this computer.",
               inner.removeFromTop(18), juce::Justification::centredLeft, false);

    // footer separator
    const int footerTop = card.getBottom() - 22 - 34 - 14;
    g.setColour(tc(GgdThemeRole::borderSoft));
    g.fillRect(card.getX() + cardPadding, footerTop, card.getWidth() - cardPadding * 2, 1);
    g.setColour(tc(GgdThemeRole::faint));
    g.setFont(GgdFonts::regular(11.5f));
    g.drawText("Sync mode and Seq On/Off are saved with each project.",
               juce::Rectangle<int>(card.getX() + cardPadding, footerTop + 14, card.getWidth() / 2, 34),
               juce::Justification::centredLeft, true);

    // section headings (positions mirror resized())
    auto drawHeading = [&g](juce::Rectangle<int> area, const juce::String& text)
    {
        g.setColour(tc(GgdThemeRole::accent));
        g.setFont(GgdFonts::bold(10.5f));
        g.drawText(text.toUpperCase(), area.withHeight(sectionHeight), juce::Justification::centredLeft, false);
    };

    auto body = card.reduced(cardPadding, 22);
    body.removeFromTop(30 + 18 + 16);
    body.removeFromBottom(34 + 14 + 14);
    const int gap = 36;
    auto left = body.removeFromLeft((body.getWidth() - gap) / 2);
    body.removeFromLeft(gap);
    auto right = body;

    // column divider
    g.setColour(tc(GgdThemeRole::borderSoft));
    g.fillRect(right.getX() - gap / 2, right.getY() + 4, 1, right.getHeight() - 8);

    drawHeading(left, "Appearance");
    left.removeFromTop(sectionHeight + 76 + rowHeight + 14);
    drawHeading(left, "Playhead");
    left.removeFromTop(sectionHeight + rowHeight * 3 + 14);
    drawHeading(left, "Editing");

    drawHeading(right, "Played-note feedback");

    if (!feedbackNoteArea.isEmpty())
    {
        g.setColour(tc(GgdThemeRole::faint));
        g.setFont(GgdFonts::regular(11.5f));
        g.drawFittedText("Feedback follows the notes the sequencer really plays: muted hits and hits skipped "
                         "by their chance setting stay dark. It never affects MIDI timing.",
                         feedbackNoteArea, juce::Justification::topLeft, 3, 0.95f);
    }
}

void GgdSettingsPanel::resized()
{
    const auto card = getCardBounds();
    closeButton.setBounds(card.getRight() - cardPadding - 26, card.getY() + 20, 28, 28);

    auto footer = juce::Rectangle<int>(card.getX() + cardPadding, card.getBottom() - 22 - 34,
                                       card.getWidth() - cardPadding * 2, 34);
    doneButton.setBounds(footer.removeFromRight(96));
    footer.removeFromRight(8);
    resetButton.setBounds(footer.removeFromRight(140));

    auto body = card.reduced(cardPadding, 22);
    body.removeFromTop(30 + 18 + 16);
    body.removeFromBottom(34 + 14 + 14);
    const int gap = 36;
    auto left = body.removeFromLeft((body.getWidth() - gap) / 2);
    body.removeFromLeft(gap);
    auto right = body;

    left.removeFromTop(sectionHeight);
    auto swatchRow = left.removeFromTop(76);
    const int swatchGap = 10;
    const int swatchWidth = (swatchRow.getWidth() - swatchGap * (swatches.size() - 1)) / juce::jmax(1, swatches.size());
    for (auto* swatch : swatches)
    {
        swatch->setBounds(swatchRow.removeFromLeft(swatchWidth).withTrimmedBottom(4));
        swatchRow.removeFromLeft(swatchGap);
    }
    colourToggle.setBounds(left.removeFromTop(rowHeight));
    left.removeFromTop(14);

    left.removeFromTop(sectionHeight);
    followToggle.setBounds(left.removeFromTop(rowHeight));
    smoothToggle.setBounds(left.removeFromTop(rowHeight));
    glowToggle.setBounds(left.removeFromTop(rowHeight));
    left.removeFromTop(14);

    left.removeFromTop(sectionHeight);
    autoFineToggle.setBounds(left.removeFromTop(rowHeight));
    shiftHoverToggle.setBounds(left.removeFromTop(rowHeight));
    auditionToggle.setBounds(left.removeFromTop(rowHeight));

    right.removeFromTop(sectionHeight);
    effectsToggle.setBounds(right.removeFromTop(rowHeight));
    rippleToggle.setBounds(right.removeFromTop(rowHeight));
    laneFlashToggle.setBounds(right.removeFromTop(rowHeight));
    velocityToggle.setBounds(right.removeFromTop(rowHeight));
    reduceMotionToggle.setBounds(right.removeFromTop(rowHeight));
    right.removeFromTop(10);
    auto intensityRow = right.removeFromTop(rowHeight + 2);
    intensityLabel.setBounds(intensityRow.removeFromLeft(78));
    intensitySlider.setBounds(intensityRow);
    auto decayRow = right.removeFromTop(rowHeight + 2);
    decayLabel.setBounds(decayRow.removeFromLeft(78));
    decaySlider.setBounds(decayRow);
    right.removeFromTop(16);
    feedbackNoteArea = right.removeFromTop(60);
}

void GgdSettingsPanel::mouseDown(const juce::MouseEvent& e)
{
    if (!getCardBounds().contains(e.getPosition()) && closed)
        closed();
}

bool GgdSettingsPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey)
    {
        if (closed)
            closed();
        return true;
    }
    return true; // modal: nothing leaks to the grid underneath
}
