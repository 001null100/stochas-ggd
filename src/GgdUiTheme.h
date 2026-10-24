#pragma once

#include <JuceHeader.h>

// Semantic colour roles shared by the editor, timeline and browser. Every
// theme defines all of them, so presentation code never hard-codes colours.
enum class GgdThemeRole
{
    background,
    outside,
    panel,
    panelRaised,
    panelSoft,
    borderSoft,
    border,
    borderStrong,
    text,
    muted,
    faint,
    accent,
    accentSecondary,
    accentText,
    warm,
    danger,
    warning,
    success,
    barLine,
    beatLine,
    subdivisionLine,
    fineSubdivisionLine,
    beatShade,
    groupFill,
    groupLine,
    rowAlternate,
    rowHover,
    selection,
    playhead,
    shadow,
    count
};

// Theme order is persisted in the user's preferences; append new themes.
enum class GgdThemeId
{
    graphite = 0,
    midnight,
    ember,
    contrast,
    count
};

juce::Colour ggdThemeColour(GgdThemeRole role);
juce::Colour ggdThemeColour(GgdThemeRole role, GgdThemeId theme);
juce::String ggdThemeName(GgdThemeId theme);
GgdThemeId ggdThemeFromIndex(int index);
int ggdThemeIndex(GgdThemeId theme);
GgdThemeId ggdCurrentTheme();
void ggdSetCurrentTheme(GgdThemeId theme);

// Instrument family colour for a semantic articulation id ("kick.hit",
// "hihat.open_2", ...), tuned for the current theme.
juce::Colour ggdInstrumentColour(const juce::String& semanticId);
juce::Colour ggdInstrumentColour(const juce::String& semanticId, GgdThemeId theme);

// Embedded Inter typefaces (SIL OFL, see resources/fonts).
namespace GgdFonts
{
enum class Weight
{
    regular,
    medium,
    semibold,
    bold
};

juce::Font font(float height, Weight weight = Weight::regular);
inline juce::Font regular(float height) { return font(height, Weight::regular); }
inline juce::Font medium(float height) { return font(height, Weight::medium); }
inline juce::Font semibold(float height) { return font(height, Weight::semibold); }
inline juce::Font bold(float height) { return font(height, Weight::bold); }
juce::Typeface::Ptr typeface(Weight weight);
}

// Small vector icon set, drawn at any size in the current text colour.
enum class GgdIcon
{
    none,
    undo,
    redo,
    settings,
    drag,
    pencil,
    pointer,
    folder,
    refresh,
    save,
    menu,
    power,
    play,
    stop,
    warning,
    duplicate,
    triplet,
    fit,
    chevronDown,
    chevronRight,
    close,
    search,
    file
};

void ggdDrawIcon(juce::Graphics& g, GgdIcon icon, juce::Rectangle<float> area, juce::Colour colour,
                 float strokeWidth = 1.6f);

// Button with an optional icon left of the text (or icon only). Toggle
// buttons render as accent-filled when on.
class GgdButton : public juce::TextButton
{
public:
    explicit GgdButton(const juce::String& buttonText = {}, GgdIcon icon = GgdIcon::none);

    void setIcon(GgdIcon newIcon);
    GgdIcon getIcon() const { return icon; }

    // Visual weight: quiet buttons have no fill until hovered, primary
    // buttons use the accent colour.
    enum class Style { normal, quiet, primary, danger };
    void setStyle(Style newStyle);
    Style getStyle() const { return style; }

    // Joins this button to its neighbours as a segmented control.
    void setSegment(bool joinLeft, bool joinRight);
    bool joinsLeft() const { return segmentLeft; }
    bool joinsRight() const { return segmentRight; }

private:
    GgdIcon icon = GgdIcon::none;
    Style style = Style::normal;
    bool segmentLeft = false;
    bool segmentRight = false;
};

class GgdLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GgdLookAndFeel();
    void syncThemeColours();

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator,
                           bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader(juce::Graphics&, const juce::Rectangle<int>& area,
                                    const juce::String& sectionName) override;
    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override { return 5; }

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;

    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 10; }

    void drawTabButton(juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    int getTabButtonBestWidth(juce::TabBarButton&, int tabDepth) override;
    void drawTabbedButtonBarBackground(juce::TabbedButtonBar&, juce::Graphics&) override;
    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int w, int h) override;

    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;

    void drawTreeviewPlusMinusBox(juce::Graphics&, const juce::Rectangle<float>& area,
                                  juce::Colour backgroundColour, bool isOpen, bool isMouseOver) override;

    void drawCornerResizer(juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

    void drawAlertBox(juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea,
                      juce::TextLayout&) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    juce::Font getLabelFont(juce::Label&) override;
};
