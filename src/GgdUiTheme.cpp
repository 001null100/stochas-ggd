#include "GgdUiTheme.h"

#include <atomic>

namespace
{
using Palette = std::array<juce::uint32, static_cast<size_t>(GgdThemeRole::count)>;

constexpr Palette graphite {{
    0xff0d1013, // background
    0xff090c0f, // outside
    0xff14191e, // panel
    0xff1a2026, // panelRaised
    0xff20272e, // panelSoft
    0xff252d34, // borderSoft
    0xff37434d, // border
    0xff667784, // borderStrong
    0xfff2f5f7, // text
    0xff9aa7b1, // muted
    0xff70d6c1, // accent
    0xff3f9f90, // accentSecondary
    0xffd6b470, // warm
    0xffe47a83, // danger
    0xff70d6c1, // barLine
    0xff7b8d9a, // beatLine
    0xff46535d, // subdivisionLine
    0xff2b343c, // fineSubdivisionLine
    0xff192329, // groupFill
    0xff5d8186, // groupLine
    0xff12171b, // rowAlternate
    0xfff7fbfd, // selection
    0xffffd166  // playhead
}};

constexpr Palette midnight {{
    0xff0a0f18,
    0xff070a10,
    0xff101827,
    0xff172236,
    0xff1d2b42,
    0xff26354a,
    0xff3c5069,
    0xff7188a3,
    0xfff2f7ff,
    0xff9fb0c5,
    0xff62d5ff,
    0xff4b78df,
    0xffc8a7ff,
    0xffff7d94,
    0xff5cd6ff,
    0xff8097b3,
    0xff465b74,
    0xff2a3749,
    0xff14243a,
    0xff537a9b,
    0xff0e1623,
    0xffffffff,
    0xffffca66
}};

constexpr Palette ember {{
    0xff110d0c,
    0xff0b0807,
    0xff1b1512,
    0xff251c18,
    0xff30231e,
    0xff392a24,
    0xff574138,
    0xff8b6a5c,
    0xfffff5ee,
    0xffbea89a,
    0xffffb067,
    0xffcf704b,
    0xffffd17b,
    0xffff7b73,
    0xffffad61,
    0xffa88674,
    0xff60483e,
    0xff3a2b25,
    0xff281b17,
    0xffa7654f,
    0xff18110f,
    0xffffffff,
    0xff75d9ff
}};

constexpr Palette contrast {{
    0xff050607,
    0xff000000,
    0xff0d0f11,
    0xff171a1e,
    0xff20242a,
    0xff30363d,
    0xff68717a,
    0xffaeb7c0,
    0xffffffff,
    0xffc1c8cf,
    0xff62f5d2,
    0xff17b89c,
    0xffffd166,
    0xffff7585,
    0xff79ffe1,
    0xffd9e0e6,
    0xff858f98,
    0xff444b52,
    0xff151b1d,
    0xff8bd8c9,
    0xff0a0c0d,
    0xffffffff,
    0xffffd84d
}};

std::atomic<int> currentTheme { 0 };

const Palette& paletteFor(GgdThemeId id)
{
    switch (id)
    {
        case GgdThemeId::midnight: return midnight;
        case GgdThemeId::ember: return ember;
        case GgdThemeId::contrast: return contrast;
        default: return graphite;
    }
}

juce::Colour role(GgdThemeRole value)
{
    return ggdThemeColour(value);
}
}

juce::Colour ggdThemeColour(GgdThemeRole roleId)
{
    const auto theme = ggdThemeFromIndex(currentTheme.load(std::memory_order_relaxed));
    const auto index = static_cast<size_t>(roleId);
    return juce::Colour(paletteFor(theme)[index]);
}

juce::String ggdThemeName(GgdThemeId theme)
{
    switch (theme)
    {
        case GgdThemeId::midnight: return "Midnight";
        case GgdThemeId::ember: return "Ember";
        case GgdThemeId::contrast: return "Contrast";
        default: return "Graphite";
    }
}

GgdThemeId ggdThemeFromIndex(int index)
{
    return static_cast<GgdThemeId>(juce::jlimit(
        0, static_cast<int>(GgdThemeId::count) - 1, index));
}

int ggdThemeIndex(GgdThemeId theme)
{
    return static_cast<int>(theme);
}

GgdThemeId ggdCurrentTheme()
{
    return ggdThemeFromIndex(currentTheme.load(std::memory_order_relaxed));
}

void ggdSetCurrentTheme(GgdThemeId theme)
{
    currentTheme.store(ggdThemeIndex(theme), std::memory_order_relaxed);
}

juce::Colour ggdLegacyThemeColour(juce::uint32 original)
{
    switch (original)
    {
        case 0xff0d1013: return role(GgdThemeRole::background);
        case 0xff090c0f: return role(GgdThemeRole::outside);
        case 0xff101418: return role(GgdThemeRole::background);
        case 0xff14191e: return role(GgdThemeRole::panel);
        case 0xff171d22: return role(GgdThemeRole::panelRaised);
        case 0xff1a2026: return role(GgdThemeRole::panelRaised);
        case 0xff20272e: return role(GgdThemeRole::panelSoft);
        case 0xff2d363f: return role(GgdThemeRole::border);
        case 0xffedf2f5: return role(GgdThemeRole::text);
        case 0xff8d99a5: return role(GgdThemeRole::muted);
        case 0xff70d6c1: return role(GgdThemeRole::accent);
        case 0xff3f9f90: return role(GgdThemeRole::accentSecondary);
        case 0xffd6b470: return role(GgdThemeRole::warm);
        default: return juce::Colour(original);
    }
}

GgdLookAndFeel::GgdLookAndFeel()
{
    syncThemeColours();
}

void GgdLookAndFeel::syncThemeColours()
{
    setColour(juce::ResizableWindow::backgroundColourId, role(GgdThemeRole::background));
    setColour(juce::TextButton::buttonColourId, role(GgdThemeRole::panelRaised));
    setColour(juce::TextButton::buttonOnColourId, role(GgdThemeRole::accentSecondary));
    setColour(juce::TextButton::textColourOffId, role(GgdThemeRole::text));
    setColour(juce::TextButton::textColourOnId, role(GgdThemeRole::text));
    setColour(juce::ComboBox::backgroundColourId, role(GgdThemeRole::panel));
    setColour(juce::ComboBox::outlineColourId, role(GgdThemeRole::border));
    setColour(juce::ComboBox::textColourId, role(GgdThemeRole::text));
    setColour(juce::ComboBox::arrowColourId, role(GgdThemeRole::muted));
    setColour(juce::PopupMenu::backgroundColourId, role(GgdThemeRole::panelRaised));
    setColour(juce::PopupMenu::textColourId, role(GgdThemeRole::text));
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              role(GgdThemeRole::accentSecondary).withAlpha(0.65f));
    setColour(juce::PopupMenu::highlightedTextColourId, role(GgdThemeRole::text));
    setColour(juce::TextEditor::backgroundColourId, role(GgdThemeRole::panel));
    setColour(juce::TextEditor::outlineColourId, role(GgdThemeRole::border));
    setColour(juce::TextEditor::focusedOutlineColourId, role(GgdThemeRole::accent));
    setColour(juce::TextEditor::textColourId, role(GgdThemeRole::text));
    setColour(juce::TextEditor::highlightColourId,
              role(GgdThemeRole::accentSecondary).withAlpha(0.65f));
    setColour(juce::Slider::trackColourId, role(GgdThemeRole::border));
    setColour(juce::Slider::thumbColourId, role(GgdThemeRole::accent));
    setColour(juce::ScrollBar::thumbColourId, role(GgdThemeRole::borderStrong));
    setColour(juce::ScrollBar::backgroundColourId, role(GgdThemeRole::outside));
}

void GgdLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                          juce::Button& button,
                                          const juce::Colour&,
                                          bool highlighted,
                                          bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.75f);
    const float radius = juce::jmin(5.0f, bounds.getHeight() * 0.22f);
    const bool on = button.getToggleState();

    auto fill = on ? role(GgdThemeRole::accentSecondary)
                   : role(GgdThemeRole::panelRaised);
    if (!button.isEnabled())
        fill = role(GgdThemeRole::panel).interpolatedWith(role(GgdThemeRole::outside), 0.35f);
    else if (down)
        fill = on ? role(GgdThemeRole::accentSecondary).darker(0.18f)
                  : role(GgdThemeRole::panelSoft).brighter(0.08f);
    else if (highlighted)
        fill = on ? role(GgdThemeRole::accentSecondary).brighter(0.08f)
                  : role(GgdThemeRole::panelSoft);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, radius);

    auto outline = on ? role(GgdThemeRole::accent).withAlpha(0.88f)
                      : role(GgdThemeRole::border).withAlpha(highlighted ? 1.0f : 0.82f);
    g.setColour(outline);
    g.drawRoundedRectangle(bounds, radius, on ? 1.35f : 1.0f);

    if (on)
    {
        g.setColour(role(GgdThemeRole::accent).withAlpha(0.32f));
        g.fillRoundedRectangle(bounds.removeFromBottom(2.0f), 1.0f);
    }
}

void GgdLookAndFeel::drawComboBox(juce::Graphics& g,
                                  int width,
                                  int height,
                                  bool isButtonDown,
                                  int,
                                  int,
                                  int,
                                  int,
                                  juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f,
                                         static_cast<float>(width - 1),
                                         static_cast<float>(height - 1));
    const float radius = juce::jmin(5.0f, height * 0.20f);
    g.setColour(role(GgdThemeRole::panel));
    g.fillRoundedRectangle(bounds, radius);
    g.setColour((box.hasKeyboardFocus(true) ? role(GgdThemeRole::accent)
                                            : role(GgdThemeRole::border))
                    .withAlpha(isButtonDown ? 1.0f : 0.88f));
    g.drawRoundedRectangle(bounds, radius, box.hasKeyboardFocus(true) ? 1.35f : 1.0f);

    const float cx = static_cast<float>(width - 15);
    const float cy = static_cast<float>(height) * 0.50f;
    juce::Path arrow;
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 2.0f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(role(GgdThemeRole::muted));
    g.strokePath(arrow, juce::PathStrokeType(1.5f,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void GgdLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                      int x,
                                      int y,
                                      int width,
                                      int height,
                                      float sliderPos,
                                      float,
                                      float,
                                      const juce::Slider::SliderStyle style,
                                      juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider(
            g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const float centreY = static_cast<float>(y) + height * 0.5f;
    const float left = static_cast<float>(x) + 4.0f;
    const float right = static_cast<float>(x + width) - 4.0f;
    const float thumb = juce::jlimit(left, right, sliderPos);

    g.setColour(role(GgdThemeRole::borderSoft));
    g.fillRoundedRectangle(left, centreY - 2.0f, right - left, 4.0f, 2.0f);
    g.setColour(role(GgdThemeRole::accentSecondary));
    g.fillRoundedRectangle(left, centreY - 2.0f, juce::jmax(0.0f, thumb - left), 4.0f, 2.0f);

    const float radius = slider.isMouseOverOrDragging() ? 6.0f : 5.0f;
    g.setColour(role(GgdThemeRole::accent).withAlpha(0.24f));
    g.fillEllipse(thumb - radius - 2.0f, centreY - radius - 2.0f,
                  (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f);
    g.setColour(role(GgdThemeRole::accent));
    g.fillEllipse(thumb - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
}

juce::Font GgdLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::jlimit(10.0f, 12.0f, buttonHeight * 0.42f),
                      juce::Font::plain);
}

juce::Font GgdLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(11.5f, juce::Font::plain);
}
