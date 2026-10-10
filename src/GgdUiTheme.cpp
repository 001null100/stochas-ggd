#include "GgdUiTheme.h"
#include "GgdAssets.h"

#include <atomic>

namespace
{
struct Palette
{
    juce::uint32 background, outside, panel, panelRaised, panelSoft;
    juce::uint32 borderSoft, border, borderStrong;
    juce::uint32 text, muted, faint;
    juce::uint32 accent, accentSecondary, accentText;
    juce::uint32 warm, danger, warning, success;
    juce::uint32 barLine, beatLine, subdivisionLine, fineSubdivisionLine, beatShade;
    juce::uint32 groupFill, groupLine, rowAlternate, rowHover;
    juce::uint32 selection, playhead, shadow;

    juce::uint32 get(GgdThemeRole role) const
    {
        switch (role)
        {
            case GgdThemeRole::background:          return background;
            case GgdThemeRole::outside:             return outside;
            case GgdThemeRole::panel:               return panel;
            case GgdThemeRole::panelRaised:         return panelRaised;
            case GgdThemeRole::panelSoft:           return panelSoft;
            case GgdThemeRole::borderSoft:          return borderSoft;
            case GgdThemeRole::border:              return border;
            case GgdThemeRole::borderStrong:        return borderStrong;
            case GgdThemeRole::text:                return text;
            case GgdThemeRole::muted:               return muted;
            case GgdThemeRole::faint:               return faint;
            case GgdThemeRole::accent:              return accent;
            case GgdThemeRole::accentSecondary:     return accentSecondary;
            case GgdThemeRole::accentText:          return accentText;
            case GgdThemeRole::warm:                return warm;
            case GgdThemeRole::danger:              return danger;
            case GgdThemeRole::warning:             return warning;
            case GgdThemeRole::success:             return success;
            case GgdThemeRole::barLine:             return barLine;
            case GgdThemeRole::beatLine:            return beatLine;
            case GgdThemeRole::subdivisionLine:     return subdivisionLine;
            case GgdThemeRole::fineSubdivisionLine: return fineSubdivisionLine;
            case GgdThemeRole::beatShade:           return beatShade;
            case GgdThemeRole::groupFill:           return groupFill;
            case GgdThemeRole::groupLine:           return groupLine;
            case GgdThemeRole::rowAlternate:        return rowAlternate;
            case GgdThemeRole::rowHover:            return rowHover;
            case GgdThemeRole::selection:           return selection;
            case GgdThemeRole::playhead:            return playhead;
            case GgdThemeRole::shadow:              return shadow;
            case GgdThemeRole::count:               break;
        }
        return text;
    }
};

constexpr Palette graphite {
    0xff111418, 0xff0b0d10, 0xff161a1f, 0xff1d2228, 0xff262c33,
    0xff22282f, 0xff2e363f, 0xff4a5560,
    0xffe9eef2, 0xff9ca7b1, 0xff69747f,
    0xff5fd4bd, 0xff2c8a79, 0xff03120f,
    0xffeab768, 0xffef6b75, 0xfff1b54c, 0xff6bd48d,
    0xff5a7983, 0xff37424c, 0xff272e36, 0xff1f252c, 0x09ffffff,
    0xff181c21, 0xff3f5157, 0xff14171b, 0x12ffffff,
    0xffffffff, 0xffffcf5c, 0xa8000000
};

constexpr Palette midnight {
    0xff0d121b, 0xff090d14, 0xff111827, 0xff172134, 0xff1f2a41,
    0xff1c2537, 0xff29354d, 0xff43526f,
    0xffebf2ff, 0xff9dadc6, 0xff6a7b9d,
    0xff5ec9ff, 0xff2f6fd2, 0xff03101c,
    0xffd2abff, 0xffff7189, 0xffffc35f, 0xff6edc9c,
    0xff4c6890, 0xff2d3b56, 0xff212b3f, 0xff1a2233, 0x0a9fc0ff,
    0xff121a29, 0xff3a5378, 0xff0f1520, 0x14a8ccff,
    0xffffffff, 0xffffc861, 0xb0000000
};

constexpr Palette ember {
    0xff14100e, 0xff0e0b09, 0xff1a1512, 0xff231c18, 0xff2d241f,
    0xff2a211c, 0xff3a2e27, 0xff5e4a41,
    0xfffbf1ea, 0xffc4ae9f, 0xff8d786b,
    0xffffa25c, 0xffc4622d, 0xff1b0b01,
    0xffffd37b, 0xffff6f69, 0xffffc35b, 0xff9cd57b,
    0xff8b6551, 0xff48392f, 0xff31271f, 0xff271f19, 0x0bffd0b0,
    0xff1b1512, 0xff6f4d3d, 0xff171210, 0x14ffc8a0,
    0xffffffff, 0xff70d7ff, 0xb0000000
};

constexpr Palette contrast {
    0xff050607, 0xff000000, 0xff0b0d0f, 0xff15181b, 0xff20252a,
    0xff22272c, 0xff3b434b, 0xff808c97,
    0xffffffff, 0xffcbd2d8, 0xff929ca5,
    0xff4ff2c9, 0xff12a588, 0xff001a13,
    0xffffd166, 0xffff6b7e, 0xffffcd4d, 0xff6cf08f,
    0xffaab6bf, 0xff58626b, 0xff363d44, 0xff24292e, 0x10ffffff,
    0xff0f1214, 0xff7fd9c7, 0xff0a0c0e, 0x1cffffff,
    0xffffffff, 0xffffe24d, 0xc8000000
};

std::atomic<int> currentTheme { 0 };

const Palette& paletteFor(GgdThemeId id)
{
    switch (id)
    {
        case GgdThemeId::midnight: return midnight;
        case GgdThemeId::ember:    return ember;
        case GgdThemeId::contrast: return contrast;
        default:                   return graphite;
    }
}

juce::Colour role(GgdThemeRole value)
{
    return ggdThemeColour(value);
}

bool isSegmented(const juce::Button& button, bool& left, bool& right)
{
    if (auto* ggd = dynamic_cast<const GgdButton*>(&button))
    {
        left = ggd->joinsLeft();
        right = ggd->joinsRight();
        return left || right;
    }
    left = right = false;
    return false;
}

juce::Path roundedOutline(juce::Rectangle<float> bounds, float radius, bool joinLeft, bool joinRight)
{
    juce::Path path;
    path.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                             radius, radius,
                             !joinLeft, !joinRight, !joinLeft, !joinRight);
    return path;
}
}

juce::Colour ggdThemeColour(GgdThemeRole roleId)
{
    return juce::Colour(paletteFor(ggdCurrentTheme()).get(roleId));
}

juce::Colour ggdThemeColour(GgdThemeRole roleId, GgdThemeId theme)
{
    return juce::Colour(paletteFor(theme).get(roleId));
}

juce::String ggdThemeName(GgdThemeId theme)
{
    switch (theme)
    {
        case GgdThemeId::midnight: return "Midnight";
        case GgdThemeId::ember:    return "Ember";
        case GgdThemeId::contrast: return "Contrast";
        default:                   return "Graphite";
    }
}

GgdThemeId ggdThemeFromIndex(int index)
{
    return static_cast<GgdThemeId>(juce::jlimit(0, static_cast<int>(GgdThemeId::count) - 1, index));
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

juce::Colour ggdInstrumentColour(const juce::String& semanticId)
{
    return ggdInstrumentColour(semanticId, ggdCurrentTheme());
}

juce::Colour ggdInstrumentColour(const juce::String& semanticId, GgdThemeId theme)
{
    juce::uint32 base = 0xff9fb3c2; // aux / percussion
    if (semanticId.startsWith("kick"))        base = 0xffff9a4f;
    else if (semanticId.startsWith("snare"))  base = 0xffff6d84;
    else if (semanticId.startsWith("tom"))    base = 0xffb48dff;
    else if (semanticId.startsWith("hihat"))  base = 0xff4fd8d0;
    else if (semanticId.startsWith("mini_hat")) base = 0xff7fe3c8;
    else if (semanticId.startsWith("ride"))   base = 0xff5aaaff;
    else if (semanticId.startsWith("crash"))  base = 0xffffd35f;
    else if (semanticId.startsWith("china"))  base = 0xff9de56d;
    else if (semanticId.startsWith("splash")) base = 0xffff90d2;
    else if (semanticId.startsWith("stack"))  base = 0xffdcb98e;

    const auto colour = juce::Colour(base);
    switch (theme)
    {
        case GgdThemeId::contrast: return colour.brighter(0.18f);
        case GgdThemeId::ember:    return colour.interpolatedWith(juce::Colour(0xffffd2b0), 0.12f);
        case GgdThemeId::midnight: return colour.interpolatedWith(juce::Colour(0xffb8d4ff), 0.10f);
        default:                   return colour;
    }
}

//==============================================================================
namespace GgdFonts
{
juce::Typeface::Ptr typeface(Weight weight)
{
    static juce::Typeface::Ptr regularFace = juce::Typeface::createSystemTypefaceFor(
        GgdAssets::InterRegular_otf, static_cast<size_t>(GgdAssets::InterRegular_otfSize));
    static juce::Typeface::Ptr mediumFace = juce::Typeface::createSystemTypefaceFor(
        GgdAssets::InterMedium_otf, static_cast<size_t>(GgdAssets::InterMedium_otfSize));
    static juce::Typeface::Ptr semiboldFace = juce::Typeface::createSystemTypefaceFor(
        GgdAssets::InterSemiBold_otf, static_cast<size_t>(GgdAssets::InterSemiBold_otfSize));
    static juce::Typeface::Ptr boldFace = juce::Typeface::createSystemTypefaceFor(
        GgdAssets::InterBold_otf, static_cast<size_t>(GgdAssets::InterBold_otfSize));

    switch (weight)
    {
        case Weight::medium:   return mediumFace;
        case Weight::semibold: return semiboldFace;
        case Weight::bold:     return boldFace;
        default:               return regularFace;
    }
}

juce::Font font(float height, Weight weight)
{
    if (auto face = typeface(weight))
        return juce::Font(juce::FontOptions(face).withPointHeight(height));
    return juce::Font(juce::FontOptions(height * 1.2f, weight == Weight::regular
                                                            ? juce::Font::plain : juce::Font::bold));
}
}

//==============================================================================
void ggdDrawIcon(juce::Graphics& g, GgdIcon icon, juce::Rectangle<float> area, juce::Colour colour,
                 float strokeWidth)
{
    if (icon == GgdIcon::none || area.isEmpty())
        return;

    const float size = juce::jmin(area.getWidth(), area.getHeight());
    auto box = juce::Rectangle<float>(size, size).withCentre(area.getCentre());
    const float s = size / 16.0f; // icons are designed on a 16 unit grid
    auto p = [&box, s](float x, float y) { return juce::Point<float>(box.getX() + x * s, box.getY() + y * s); };
    const juce::PathStrokeType stroke(strokeWidth * juce::jmax(0.75f, s), juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded);
    juce::Path path;
    g.setColour(colour);

    switch (icon)
    {
        case GgdIcon::undo:
        case GgdIcon::redo:
        {
            const bool redo = icon == GgdIcon::redo;
            auto px = [redo](float x) { return redo ? 16.0f - x : x; };
            path.startNewSubPath(p(px(4.0f), 6.5f));
            path.lineTo(p(px(10.0f), 6.5f));
            path.cubicTo(p(px(12.8f), 6.5f), p(px(13.5f), 8.6f), p(px(13.5f), 9.8f));
            path.cubicTo(p(px(13.5f), 11.3f), p(px(12.4f), 13.0f), p(px(10.0f), 13.0f));
            path.lineTo(p(px(7.0f), 13.0f));
            g.strokePath(path, stroke);
            juce::Path head;
            head.startNewSubPath(p(px(6.8f), 3.6f));
            head.lineTo(p(px(3.8f), 6.5f));
            head.lineTo(p(px(6.8f), 9.4f));
            g.strokePath(head, stroke);
            break;
        }
        case GgdIcon::settings:
        {
            const auto centre = p(8.0f, 8.0f);
            juce::Path gear;
            const int teeth = 8;
            for (int i = 0; i < teeth * 2; ++i)
            {
                const float angle = juce::MathConstants<float>::twoPi * (static_cast<float>(i) + 0.5f) / (teeth * 2);
                const float radius = ((i & 1) == 0 ? 6.6f : 5.1f) * s;
                const auto point = centre.getPointOnCircumference(radius, angle);
                if (i == 0)
                    gear.startNewSubPath(point);
                else
                    gear.lineTo(point);
            }
            gear.closeSubPath();
            g.strokePath(gear, juce::PathStrokeType(stroke.getStrokeThickness() * 0.9f,
                                                    juce::PathStrokeType::mitered,
                                                    juce::PathStrokeType::rounded));
            g.drawEllipse(juce::Rectangle<float>(4.4f * s, 4.4f * s).withCentre(centre),
                          stroke.getStrokeThickness() * 0.9f);
            break;
        }
        case GgdIcon::drag:
        {
            // MIDI clip with an outgoing arrow
            g.drawRoundedRectangle(juce::Rectangle<float>(p(2.0f, 4.0f), p(10.5f, 13.0f)), 1.6f * s,
                                   stroke.getStrokeThickness());
            g.fillRect(juce::Rectangle<float>(p(4.0f, 7.0f), p(6.6f, 8.2f)));
            g.fillRect(juce::Rectangle<float>(p(6.0f, 9.4f), p(8.6f, 10.6f)));
            path.startNewSubPath(p(9.0f, 2.5f));
            path.lineTo(p(14.0f, 2.5f));
            path.lineTo(p(14.0f, 7.5f));
            path.startNewSubPath(p(14.0f, 2.5f));
            path.lineTo(p(9.6f, 6.9f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::pencil:
        {
            path.startNewSubPath(p(3.0f, 13.0f));
            path.lineTo(p(3.6f, 10.2f));
            path.lineTo(p(10.8f, 3.0f));
            path.lineTo(p(13.0f, 5.2f));
            path.lineTo(p(5.8f, 12.4f));
            path.closeSubPath();
            g.strokePath(path, stroke);
            juce::Path tip;
            tip.startNewSubPath(p(9.2f, 4.6f));
            tip.lineTo(p(11.4f, 6.8f));
            g.strokePath(tip, stroke);
            break;
        }
        case GgdIcon::pointer:
        {
            path.startNewSubPath(p(4.0f, 2.5f));
            path.lineTo(p(12.5f, 9.6f));
            path.lineTo(p(8.4f, 10.0f));
            path.lineTo(p(10.4f, 13.8f));
            path.lineTo(p(8.8f, 14.5f));
            path.lineTo(p(6.9f, 10.8f));
            path.lineTo(p(4.0f, 13.4f));
            path.closeSubPath();
            g.strokePath(path, juce::PathStrokeType(stroke.getStrokeThickness(), juce::PathStrokeType::mitered,
                                                    juce::PathStrokeType::rounded));
            break;
        }
        case GgdIcon::folder:
        {
            path.startNewSubPath(p(2.0f, 4.0f));
            path.lineTo(p(6.2f, 4.0f));
            path.lineTo(p(7.6f, 5.6f));
            path.lineTo(p(14.0f, 5.6f));
            path.lineTo(p(14.0f, 12.8f));
            path.lineTo(p(2.0f, 12.8f));
            path.closeSubPath();
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::refresh:
        {
            path.addCentredArc(box.getCentreX(), box.getCentreY(), 5.2f * s, 5.2f * s, 0.0f,
                               0.55f, juce::MathConstants<float>::twoPi - 0.35f, true);
            g.strokePath(path, stroke);
            const auto end = p(8.0f, 8.0f).getPointOnCircumference(5.2f * s, -0.35f);
            juce::Path head;
            head.startNewSubPath(end.translated(-2.9f * s, -0.2f * s));
            head.lineTo(end);
            head.lineTo(end.translated(0.4f * s, -2.9f * s));
            g.strokePath(head, stroke);
            break;
        }
        case GgdIcon::save:
        {
            path.startNewSubPath(p(8.0f, 2.5f));
            path.lineTo(p(8.0f, 10.0f));
            path.startNewSubPath(p(4.8f, 7.0f));
            path.lineTo(p(8.0f, 10.2f));
            path.lineTo(p(11.2f, 7.0f));
            path.startNewSubPath(p(2.8f, 11.0f));
            path.lineTo(p(2.8f, 13.4f));
            path.lineTo(p(13.2f, 13.4f));
            path.lineTo(p(13.2f, 11.0f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::menu:
        {
            for (int i = 0; i < 3; ++i)
                g.fillEllipse(juce::Rectangle<float>(2.4f * s, 2.4f * s).withCentre(p(3.5f + i * 4.5f, 8.0f)));
            break;
        }
        case GgdIcon::power:
        {
            path.addCentredArc(box.getCentreX(), box.getCentreY() + 0.6f * s, 5.4f * s, 5.4f * s, 0.0f,
                               0.62f, juce::MathConstants<float>::twoPi - 0.62f, true);
            path.startNewSubPath(p(8.0f, 1.8f));
            path.lineTo(p(8.0f, 7.6f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::play:
        {
            path.addTriangle(p(4.5f, 3.0f), p(4.5f, 13.0f), p(13.0f, 8.0f));
            g.fillPath(path);
            break;
        }
        case GgdIcon::stop:
        {
            g.fillRoundedRectangle(juce::Rectangle<float>(p(4.0f, 4.0f), p(12.0f, 12.0f)), 1.2f * s);
            break;
        }
        case GgdIcon::warning:
        {
            path.startNewSubPath(p(8.0f, 2.2f));
            path.lineTo(p(14.4f, 13.4f));
            path.lineTo(p(1.6f, 13.4f));
            path.closeSubPath();
            g.strokePath(path, juce::PathStrokeType(stroke.getStrokeThickness(), juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
            g.fillRect(juce::Rectangle<float>(p(7.3f, 6.0f), p(8.7f, 9.8f)));
            g.fillEllipse(juce::Rectangle<float>(1.7f * s, 1.7f * s).withCentre(p(8.0f, 11.6f)));
            break;
        }
        case GgdIcon::duplicate:
        {
            g.drawRoundedRectangle(juce::Rectangle<float>(p(2.5f, 5.0f), p(10.5f, 13.5f)), 1.6f * s,
                                   stroke.getStrokeThickness());
            path.startNewSubPath(p(5.5f, 2.5f));
            path.lineTo(p(13.5f, 2.5f));
            path.lineTo(p(13.5f, 10.5f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::triplet:
        {
            g.setFont(GgdFonts::bold(11.0f * s));
            g.drawText("3", box, juce::Justification::centred, false);
            break;
        }
        case GgdIcon::fit:
        {
            path.startNewSubPath(p(2.5f, 6.0f));
            path.lineTo(p(2.5f, 2.5f));
            path.lineTo(p(6.0f, 2.5f));
            path.startNewSubPath(p(10.0f, 2.5f));
            path.lineTo(p(13.5f, 2.5f));
            path.lineTo(p(13.5f, 6.0f));
            path.startNewSubPath(p(13.5f, 10.0f));
            path.lineTo(p(13.5f, 13.5f));
            path.lineTo(p(10.0f, 13.5f));
            path.startNewSubPath(p(6.0f, 13.5f));
            path.lineTo(p(2.5f, 13.5f));
            path.lineTo(p(2.5f, 10.0f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::chevronDown:
        {
            path.startNewSubPath(p(4.0f, 6.0f));
            path.lineTo(p(8.0f, 10.0f));
            path.lineTo(p(12.0f, 6.0f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::chevronRight:
        {
            path.startNewSubPath(p(6.0f, 4.0f));
            path.lineTo(p(10.0f, 8.0f));
            path.lineTo(p(6.0f, 12.0f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::close:
        {
            path.startNewSubPath(p(4.0f, 4.0f));
            path.lineTo(p(12.0f, 12.0f));
            path.startNewSubPath(p(12.0f, 4.0f));
            path.lineTo(p(4.0f, 12.0f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::search:
        {
            g.drawEllipse(juce::Rectangle<float>(p(2.5f, 2.5f), p(11.0f, 11.0f)), stroke.getStrokeThickness());
            path.startNewSubPath(p(10.0f, 10.0f));
            path.lineTo(p(13.8f, 13.8f));
            g.strokePath(path, stroke);
            break;
        }
        case GgdIcon::file:
        {
            path.startNewSubPath(p(4.0f, 2.5f));
            path.lineTo(p(9.5f, 2.5f));
            path.lineTo(p(12.5f, 5.5f));
            path.lineTo(p(12.5f, 13.5f));
            path.lineTo(p(4.0f, 13.5f));
            path.closeSubPath();
            g.strokePath(path, stroke);
            juce::Path fold;
            fold.startNewSubPath(p(9.5f, 2.5f));
            fold.lineTo(p(9.5f, 5.5f));
            fold.lineTo(p(12.5f, 5.5f));
            g.strokePath(fold, stroke);
            break;
        }
        case GgdIcon::none:
            break;
    }
}

//==============================================================================
GgdButton::GgdButton(const juce::String& buttonText, GgdIcon newIcon)
    : juce::TextButton(buttonText), icon(newIcon)
{
    setMouseClickGrabsKeyboardFocus(false);
}

void GgdButton::setIcon(GgdIcon newIcon)
{
    if (icon != newIcon)
    {
        icon = newIcon;
        repaint();
    }
}

void GgdButton::setStyle(Style newStyle)
{
    if (style != newStyle)
    {
        style = newStyle;
        repaint();
    }
}

void GgdButton::setSegment(bool joinLeft, bool joinRight)
{
    segmentLeft = joinLeft;
    segmentRight = joinRight;
    repaint();
}

//==============================================================================
GgdLookAndFeel::GgdLookAndFeel()
{
    syncThemeColours();
}

juce::Typeface::Ptr GgdLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        if (auto face = GgdFonts::typeface(font.isBold() ? GgdFonts::Weight::semibold
                                                         : GgdFonts::Weight::regular))
            return face;
    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}

void GgdLookAndFeel::syncThemeColours()
{
    setColour(juce::ResizableWindow::backgroundColourId, role(GgdThemeRole::background));
    setColour(juce::TextButton::buttonColourId, role(GgdThemeRole::panelRaised));
    setColour(juce::TextButton::buttonOnColourId, role(GgdThemeRole::accent));
    setColour(juce::TextButton::textColourOffId, role(GgdThemeRole::text));
    setColour(juce::TextButton::textColourOnId, role(GgdThemeRole::accentText));
    setColour(juce::ToggleButton::textColourId, role(GgdThemeRole::text));
    setColour(juce::ToggleButton::tickColourId, role(GgdThemeRole::accent));
    setColour(juce::ComboBox::backgroundColourId, role(GgdThemeRole::panelRaised));
    setColour(juce::ComboBox::outlineColourId, role(GgdThemeRole::border));
    setColour(juce::ComboBox::textColourId, role(GgdThemeRole::text));
    setColour(juce::ComboBox::arrowColourId, role(GgdThemeRole::muted));
    setColour(juce::PopupMenu::backgroundColourId, role(GgdThemeRole::panelRaised));
    setColour(juce::PopupMenu::textColourId, role(GgdThemeRole::text));
    setColour(juce::PopupMenu::headerTextColourId, role(GgdThemeRole::muted));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, role(GgdThemeRole::accent).withAlpha(0.18f));
    setColour(juce::PopupMenu::highlightedTextColourId, role(GgdThemeRole::text));
    setColour(juce::TextEditor::backgroundColourId, role(GgdThemeRole::panel));
    setColour(juce::TextEditor::outlineColourId, role(GgdThemeRole::border));
    setColour(juce::TextEditor::focusedOutlineColourId, role(GgdThemeRole::accent));
    setColour(juce::TextEditor::textColourId, role(GgdThemeRole::text));
    setColour(juce::TextEditor::highlightColourId, role(GgdThemeRole::accent).withAlpha(0.32f));
    setColour(juce::TextEditor::highlightedTextColourId, role(GgdThemeRole::text));
    setColour(juce::CaretComponent::caretColourId, role(GgdThemeRole::accent));
    setColour(juce::Label::textColourId, role(GgdThemeRole::text));
    setColour(juce::Slider::trackColourId, role(GgdThemeRole::accent));
    setColour(juce::Slider::backgroundColourId, role(GgdThemeRole::borderSoft));
    setColour(juce::Slider::thumbColourId, role(GgdThemeRole::accent));
    setColour(juce::Slider::textBoxTextColourId, role(GgdThemeRole::text));
    setColour(juce::Slider::textBoxBackgroundColourId, role(GgdThemeRole::panel));
    setColour(juce::Slider::textBoxOutlineColourId, role(GgdThemeRole::border));
    setColour(juce::ScrollBar::thumbColourId, role(GgdThemeRole::borderStrong));
    setColour(juce::ScrollBar::backgroundColourId, role(GgdThemeRole::outside));
    setColour(juce::TreeView::backgroundColourId, role(GgdThemeRole::panel));
    setColour(juce::TreeView::linesColourId, role(GgdThemeRole::border));
    setColour(juce::TooltipWindow::backgroundColourId, role(GgdThemeRole::panelSoft));
    setColour(juce::TooltipWindow::textColourId, role(GgdThemeRole::text));
    setColour(juce::TooltipWindow::outlineColourId, role(GgdThemeRole::borderStrong));
    setColour(juce::AlertWindow::backgroundColourId, role(GgdThemeRole::panelRaised));
    setColour(juce::AlertWindow::textColourId, role(GgdThemeRole::text));
    setColour(juce::AlertWindow::outlineColourId, role(GgdThemeRole::border));
    setColour(juce::TabbedComponent::backgroundColourId, role(GgdThemeRole::panel));
    setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ResizableWindow::backgroundColourId, role(GgdThemeRole::background));
}

void GgdLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                          juce::Button& button,
                                          const juce::Colour&,
                                          bool highlighted,
                                          bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const float radius = juce::jmin(6.0f, bounds.getHeight() * 0.28f);
    const bool on = button.getToggleState();
    const bool enabled = button.isEnabled();

    bool joinLeft = false, joinRight = false;
    isSegmented(button, joinLeft, joinRight);

    auto style = GgdButton::Style::normal;
    if (auto* ggd = dynamic_cast<GgdButton*>(&button))
        style = ggd->getStyle();

    juce::Colour fill;
    juce::Colour outline;
    if (on && style == GgdButton::Style::quiet)
    {
        // Selected icon toggles are marked by a soft well, not a solid fill.
        fill = role(GgdThemeRole::panelSoft);
        if (down)
            fill = fill.darker(0.1f);
        outline = role(GgdThemeRole::border).withAlpha(highlighted ? 1.0f : 0.8f);
    }
    else if (on || style == GgdButton::Style::primary)
    {
        const auto accent = style == GgdButton::Style::danger ? role(GgdThemeRole::danger)
                                                              : role(GgdThemeRole::accent);
        fill = accent.withAlpha(on ? 0.92f : 0.85f);
        if (down)
            fill = fill.darker(0.15f);
        else if (highlighted)
            fill = fill.brighter(0.08f);
        outline = accent.brighter(0.1f);
    }
    else
    {
        const bool quiet = style == GgdButton::Style::quiet;
        fill = quiet ? juce::Colours::transparentBlack : role(GgdThemeRole::panelRaised);
        if (down)
            fill = role(GgdThemeRole::panelSoft).darker(0.1f);
        else if (highlighted)
            fill = role(GgdThemeRole::panelSoft);
        outline = quiet && !highlighted ? juce::Colours::transparentBlack
                                        : role(GgdThemeRole::border).withAlpha(highlighted ? 1.0f : 0.85f);
        if (style == GgdButton::Style::danger && highlighted)
            outline = role(GgdThemeRole::danger).withAlpha(0.7f);
    }

    if (!enabled)
    {
        fill = fill.withMultipliedAlpha(0.45f);
        outline = outline.withMultipliedAlpha(0.45f);
    }

    const auto shape = roundedOutline(bounds, radius, joinLeft, joinRight);
    g.setColour(fill);
    g.fillPath(shape);

    if (!on && style != GgdButton::Style::primary && style != GgdButton::Style::quiet && enabled)
    {
        // a faint top highlight gives raised buttons some depth
        g.setColour(juce::Colours::white.withAlpha(highlighted ? 0.05f : 0.03f));
        g.fillRect(bounds.withHeight(1.0f).reduced(radius, 0.0f));
    }

    g.setColour(outline);
    g.strokePath(shape, juce::PathStrokeType(1.0f));

    if (joinLeft)
    {
        g.setColour(role(GgdThemeRole::border));
        g.fillRect(bounds.getX(), bounds.getY() + 4.0f, 1.0f, bounds.getHeight() - 8.0f);
    }
}

void GgdLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    const bool on = button.getToggleState();
    auto style = GgdButton::Style::normal;
    auto icon = GgdIcon::none;
    if (auto* ggd = dynamic_cast<GgdButton*>(&button))
    {
        style = ggd->getStyle();
        icon = ggd->getIcon();
    }

    juce::Colour colour = (on && style == GgdButton::Style::quiet) ? role(GgdThemeRole::accent)
        : (on || style == GgdButton::Style::primary)
        ? role(GgdThemeRole::accentText)
        : (style == GgdButton::Style::quiet && !highlighted ? role(GgdThemeRole::muted)
                                                            : role(GgdThemeRole::text));
    // A colour set on the button itself (for example a warning) wins.
    if (!on && button.isColourSpecified(juce::TextButton::textColourOffId))
        colour = button.findColour(juce::TextButton::textColourOffId);
    if (!button.isEnabled())
        colour = colour.withMultipliedAlpha(0.4f);

    auto area = button.getLocalBounds().toFloat().reduced(8.0f, 0.0f);
    const auto text = button.getButtonText();
    const auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);

    const float iconSize = juce::jmin(16.0f, button.getHeight() - 10.0f);
    if (icon != GgdIcon::none && text.isEmpty())
    {
        ggdDrawIcon(g, icon, button.getLocalBounds().toFloat().withSizeKeepingCentre(iconSize, iconSize), colour);
        return;
    }

    if (icon != GgdIcon::none)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, text, 0.0f, 0.0f);
        const float textWidth = glyphs.getBoundingBox(0, -1, true).getWidth();
        const float total = iconSize + 6.0f + textWidth;
        const float left = juce::jmax(area.getX(), area.getCentreX() - total * 0.5f);
        ggdDrawIcon(g, icon, juce::Rectangle<float>(left, area.getCentreY() - iconSize * 0.5f, iconSize, iconSize), colour);
        area.setLeft(left + iconSize + 6.0f);
        g.setColour(colour);
        g.drawText(text, area, juce::Justification::centredLeft, false);
        return;
    }

    g.setColour(colour);
    g.drawFittedText(text, area.toNearestInt(), juce::Justification::centred, 1, 0.85f);
}

juce::Font GgdLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return GgdFonts::medium(juce::jlimit(11.0f, 12.5f, buttonHeight * 0.42f));
}

void GgdLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    // Toggles render as switches: label on the left, pill on the right.
    auto bounds = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();
    const float switchWidth = 32.0f;
    const float switchHeight = 18.0f;
    auto track = juce::Rectangle<float>(bounds.getRight() - switchWidth - 2.0f,
                                        bounds.getCentreY() - switchHeight * 0.5f,
                                        switchWidth, switchHeight);
    const float alpha = button.isEnabled() ? 1.0f : 0.4f;

    g.setColour((on ? role(GgdThemeRole::accent) : role(GgdThemeRole::panelSoft)).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(track, switchHeight * 0.5f);
    g.setColour((on ? role(GgdThemeRole::accent).brighter(0.15f)
                    : role(GgdThemeRole::borderStrong).withAlpha(highlighted ? 1.0f : 0.75f)).withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(track, switchHeight * 0.5f, 1.0f);

    const float knob = switchHeight - 6.0f;
    const float knobX = on ? track.getRight() - knob - 3.0f : track.getX() + 3.0f;
    g.setColour((on ? role(GgdThemeRole::accentText) : role(GgdThemeRole::muted)).withMultipliedAlpha(alpha));
    g.fillEllipse(knobX, track.getCentreY() - knob * 0.5f, knob, knob);

    g.setColour(role(GgdThemeRole::text).withMultipliedAlpha(alpha * (highlighted ? 1.0f : 0.92f)));
    g.setFont(GgdFonts::regular(12.5f));
    g.drawFittedText(button.getButtonText(),
                     bounds.withRight(track.getX() - 10.0f).toNearestInt(),
                     juce::Justification::centredLeft, 2, 0.9f);
}

void GgdLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                  int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width - 1), static_cast<float>(height - 1));
    const float radius = juce::jmin(6.0f, height * 0.28f);
    const bool hover = box.isMouseOver(true);
    const bool focus = box.hasKeyboardFocus(true) || isButtonDown;

    g.setColour(hover ? role(GgdThemeRole::panelSoft) : role(GgdThemeRole::panelRaised));
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.fillRect(bounds.withHeight(1.0f).reduced(radius, 0.0f));
    g.setColour(focus ? role(GgdThemeRole::accent).withAlpha(0.9f)
                      : role(GgdThemeRole::border).withAlpha(hover ? 1.0f : 0.85f));
    g.drawRoundedRectangle(bounds, radius, 1.0f);

    const auto arrowArea = juce::Rectangle<float>(static_cast<float>(width - 22), 0.0f, 16.0f, static_cast<float>(height));
    ggdDrawIcon(g, GgdIcon::chevronDown, arrowArea.withSizeKeepingCentre(12.0f, 12.0f),
                role(GgdThemeRole::muted).withAlpha(box.isEnabled() ? 1.0f : 0.4f), 1.7f);
}

juce::Font GgdLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return GgdFonts::medium(juce::jlimit(11.5f, 12.5f, box.getHeight() * 0.42f));
}

void GgdLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(2, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

juce::PopupMenu::Options GgdLookAndFeel::getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label& label)
{
    return juce::LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label)
        .withStandardItemHeight(26);
}

void GgdLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.fillAll(role(GgdThemeRole::panelRaised));
    g.setColour(role(GgdThemeRole::borderStrong).withAlpha(0.8f));
    g.drawRect(bounds, 1.0f);
}

void GgdLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                       bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                       const juce::String& text, const juce::String& shortcutKeyText,
                                       const juce::Drawable* icon, const juce::Colour* textColour)
{
    juce::ignoreUnused(icon);
    if (isSeparator)
    {
        auto line = area.toFloat().reduced(8.0f, 0.0f);
        g.setColour(role(GgdThemeRole::border));
        g.fillRect(line.withHeight(1.0f).withCentre(line.getCentre()));
        return;
    }

    auto r = area.reduced(4, 1).toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour(role(GgdThemeRole::accent).withAlpha(0.16f));
        g.fillRoundedRectangle(r, 4.0f);
    }

    auto colour = textColour != nullptr ? *textColour : role(GgdThemeRole::text);
    if (!isActive)
        colour = role(GgdThemeRole::faint);

    auto content = area.reduced(12, 0).toFloat();
    if (isTicked)
    {
        auto tick = content.removeFromLeft(14.0f);
        g.setColour(role(GgdThemeRole::accent));
        g.fillEllipse(tick.withSizeKeepingCentre(6.0f, 6.0f));
    }
    else
    {
        content.removeFromLeft(14.0f);
    }
    content.removeFromLeft(4.0f);

    if (hasSubMenu)
        ggdDrawIcon(g, GgdIcon::chevronRight, content.removeFromRight(14.0f).withSizeKeepingCentre(10.0f, 10.0f),
                    role(GgdThemeRole::muted));

    g.setColour(colour);
    g.setFont(getPopupMenuFont());
    g.drawFittedText(text, content.toNearestInt(), juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour(role(GgdThemeRole::faint));
        g.setFont(GgdFonts::regular(11.0f));
        g.drawText(shortcutKeyText, content, juce::Justification::centredRight, true);
    }
}

void GgdLookAndFeel::drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                                const juce::String& sectionName)
{
    g.setFont(GgdFonts::semibold(10.5f));
    g.setColour(role(GgdThemeRole::muted));
    g.drawFittedText(sectionName.toUpperCase(), area.reduced(14, 0), juce::Justification::bottomLeft, 1);
}

juce::Font GgdLookAndFeel::getPopupMenuFont()
{
    return GgdFonts::regular(12.5f);
}

void GgdLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float minSliderPos, float maxSliderPos,
                                      const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos,
                                               style, slider);
        return;
    }

    const float centreY = static_cast<float>(y) + height * 0.5f;
    const float left = static_cast<float>(x) + 6.0f;
    const float right = static_cast<float>(x + width) - 6.0f;
    const float thumb = juce::jlimit(left, right, sliderPos);
    const float alpha = slider.isEnabled() ? 1.0f : 0.4f;

    g.setColour(role(GgdThemeRole::borderSoft).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(left, centreY - 2.0f, right - left, 4.0f, 2.0f);
    g.setColour(role(GgdThemeRole::accent).withAlpha(0.85f * alpha));
    g.fillRoundedRectangle(left, centreY - 2.0f, juce::jmax(0.0f, thumb - left), 4.0f, 2.0f);

    const bool active = slider.isMouseOverOrDragging();
    const float radius = active ? 7.0f : 6.0f;
    if (active)
    {
        g.setColour(role(GgdThemeRole::accent).withAlpha(0.18f));
        g.fillEllipse(thumb - radius - 4.0f, centreY - radius - 4.0f, (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f);
    }
    g.setColour(role(GgdThemeRole::text).withMultipliedAlpha(alpha));
    g.fillEllipse(thumb - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(role(GgdThemeRole::accent).withMultipliedAlpha(alpha));
    g.drawEllipse(thumb - radius + 0.5f, centreY - radius + 0.5f, radius * 2.0f - 1.0f, radius * 2.0f - 1.0f, 1.5f);
}

juce::Label* GgdLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(GgdFonts::medium(12.0f));
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::textColourId, role(GgdThemeRole::text));
    label->setJustificationType(juce::Justification::centredRight);
    return label;
}

void GgdLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const float radius = juce::jmin(6.0f, height * 0.28f);
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(juce::Rectangle<float>(0.5f, 0.5f, width - 1.0f, height - 1.0f), radius);
}

void GgdLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (!editor.isEnabled())
        return;
    const float radius = juce::jmin(6.0f, height * 0.28f);
    const bool focused = editor.hasKeyboardFocus(true) && !editor.isReadOnly();
    g.setColour(focused ? editor.findColour(juce::TextEditor::focusedOutlineColourId)
                        : editor.findColour(juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle(juce::Rectangle<float>(0.5f, 0.5f, width - 1.0f, height - 1.0f), radius,
                           focused ? 1.4f : 1.0f);
}

void GgdLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width,
                                   int height, bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                   bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused(scrollbar);
    g.setColour(role(GgdThemeRole::outside));
    g.fillRect(x, y, width, height);

    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { static_cast<float>(x) + 2.0f, static_cast<float>(thumbStartPosition) + 2.0f,
                  static_cast<float>(width) - 4.0f, static_cast<float>(thumbSize) - 4.0f };
    else
        thumb = { static_cast<float>(thumbStartPosition) + 2.0f, static_cast<float>(y) + 2.0f,
                  static_cast<float>(thumbSize) - 4.0f, static_cast<float>(height) - 4.0f };

    if (thumb.getWidth() <= 0.0f || thumb.getHeight() <= 0.0f)
        return;

    const float alpha = isMouseDown ? 1.0f : isMouseOver ? 0.85f : 0.55f;
    g.setColour(role(GgdThemeRole::borderStrong).withAlpha(alpha));
    g.fillRoundedRectangle(thumb, juce::jmin(thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

void GgdLookAndFeel::drawTabButton(juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool)
{
    auto area = button.getActiveArea().toFloat().reduced(3.0f, 4.0f);
    const bool front = button.isFrontTab();

    if (front)
    {
        g.setColour(role(GgdThemeRole::panelSoft));
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(role(GgdThemeRole::border));
        g.drawRoundedRectangle(area, 5.0f, 1.0f);
    }
    else if (isMouseOver)
    {
        g.setColour(role(GgdThemeRole::panelRaised));
        g.fillRoundedRectangle(area, 5.0f);
    }

    g.setColour(front ? role(GgdThemeRole::text) : role(GgdThemeRole::muted));
    g.setFont(front ? GgdFonts::semibold(12.5f) : GgdFonts::medium(12.5f));
    g.drawText(button.getButtonText(), area, juce::Justification::centred, false);
}

int GgdLookAndFeel::getTabButtonBestWidth(juce::TabBarButton& button, int)
{
    auto& bar = button.getTabbedButtonBar();
    return juce::jmax(60, bar.getWidth() / juce::jmax(1, bar.getNumTabs()));
}

void GgdLookAndFeel::drawTabbedButtonBarBackground(juce::TabbedButtonBar& bar, juce::Graphics& g)
{
    auto area = bar.getLocalBounds().toFloat().reduced(1.0f, 2.0f);
    g.setColour(role(GgdThemeRole::panel));
    g.fillRoundedRectangle(area, 7.0f);
}

void GgdLookAndFeel::drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int, int)
{
}

juce::Rectangle<int> GgdLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                                      juce::Rectangle<int> parentArea)
{
    juce::TextLayout layout;
    juce::AttributedString text;
    text.append(tipText, GgdFonts::regular(12.0f), role(GgdThemeRole::text));
    layout.createLayoutWithBalancedLineLengths(text, 320.0f);
    const int w = static_cast<int>(layout.getWidth() + 20.0f);
    const int h = static_cast<int>(layout.getHeight() + 14.0f);
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                                w, h)
        .constrainedWithin(parentArea);
}

void GgdLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(width), static_cast<float>(height));
    g.setColour(role(GgdThemeRole::panelSoft));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(role(GgdThemeRole::borderStrong).withAlpha(0.8f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    juce::AttributedString attributed;
    attributed.append(text, GgdFonts::regular(12.0f), role(GgdThemeRole::text));
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths(attributed, static_cast<float>(width) - 20.0f);
    layout.draw(g, bounds.reduced(10.0f, 7.0f));
}

void GgdLookAndFeel::drawTreeviewPlusMinusBox(juce::Graphics& g, const juce::Rectangle<float>& area,
                                              juce::Colour, bool isOpen, bool isMouseOver)
{
    ggdDrawIcon(g, isOpen ? GgdIcon::chevronDown : GgdIcon::chevronRight, area.reduced(1.0f),
                isMouseOver ? role(GgdThemeRole::text) : role(GgdThemeRole::muted), 1.8f);
}

void GgdLookAndFeel::drawCornerResizer(juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    const float alpha = isMouseDragging ? 0.9f : isMouseOver ? 0.7f : 0.4f;
    g.setColour(role(GgdThemeRole::muted).withAlpha(alpha));
    for (int i = 0; i < 3; ++i)
    {
        const float offset = 3.0f + i * 4.0f;
        g.drawLine(static_cast<float>(w) - offset, static_cast<float>(h) - 2.0f,
                   static_cast<float>(w) - 2.0f, static_cast<float>(h) - offset, 1.2f);
    }
}

void GgdLookAndFeel::drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert,
                                  const juce::Rectangle<int>& textArea, juce::TextLayout& textLayout)
{
    auto bounds = alert.getLocalBounds().toFloat();
    g.setColour(role(GgdThemeRole::panelRaised));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(role(GgdThemeRole::borderStrong).withAlpha(0.8f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);
    textLayout.draw(g, textArea.toFloat().withTrimmedLeft(4.0f));
}

juce::Font GgdLookAndFeel::getAlertWindowTitleFont()
{
    return GgdFonts::semibold(15.0f);
}

juce::Font GgdLookAndFeel::getAlertWindowMessageFont()
{
    return GgdFonts::regular(13.0f);
}

juce::Font GgdLookAndFeel::getAlertWindowFont()
{
    return GgdFonts::regular(12.5f);
}

juce::Font GgdLookAndFeel::getLabelFont(juce::Label& label)
{
    return label.getFont();
}
