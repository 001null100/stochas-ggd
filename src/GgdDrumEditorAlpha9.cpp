#include "GgdDrumEditor.h"
#include "GgdMidiImporter.h"
#include "GgdPatternFile.h"
#include "GgdLibraryBrowser.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <vector>

namespace
{
constexpr juce::uint32 bg          = 0xff0d1013;
constexpr juce::uint32 outside     = 0xff090c0f;
constexpr juce::uint32 panel       = 0xff14191e;
constexpr juce::uint32 panelRaised = 0xff1a2026;
constexpr juce::uint32 panelSoft   = 0xff20272e;
constexpr juce::uint32 border      = 0xff2d363f;
constexpr juce::uint32 text        = 0xffedf2f5;
constexpr juce::uint32 muted       = 0xff8d99a5;
constexpr juce::uint32 accent      = 0xff70d6c1;
constexpr juce::uint32 accent2     = 0xff3f9f90;
constexpr juce::uint32 warm        = 0xffd6b470;

constexpr float defaultZoomScale = 1.25f;
constexpr float detail32ZoomThreshold = 3.5f;

juce::Colour c(juce::uint32 value) { return juce::Colour(value); }

int activeStorageRowCount(int canonicalCount)
{
    return juce::jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, canonicalCount);
}

int storageRowForCanonical(int canonicalRow, int canonicalCount)
{
    const int rowCount = activeStorageRowCount(canonicalCount);
    return (SEQ_MAX_ROWS - rowCount) + canonicalRow;
}

int stepsPerBarForMeter(int numerator, int denominator)
{
    return juce::jmax(1, numerator * 16 / denominator);
}

int clockDividerForMeter(int denominator)
{
    return juce::jlimit(SEQ_MIN_CLOCK_DIV, SEQ_MAX_CLOCK_DIV, 256 / denominator);
}

bool keyMatchesLetter(const juce::KeyPress& key, char upperCaseLetter)
{
    const int code = key.getKeyCode();
    return code == static_cast<int>(upperCaseLetter)
        || code == static_cast<int>(upperCaseLetter + ('a' - 'A'));
}

bool isHostUndoKey(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    return (mods.isCtrlDown() || mods.isCommandDown())
        && !mods.isAltDown()
        && keyMatchesLetter(key, 'Z');
}

bool isPluginUndoKey(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    return !mods.isCtrlDown() && !mods.isCommandDown() && !mods.isAltDown()
        && !mods.isShiftDown() && keyMatchesLetter(key, 'U');
}

void migrateLegacyGgdRows(SequenceLayer* layer, int canonicalCount)
{
    const int rowCount = activeStorageRowCount(canonicalCount);
    const int semanticCount = juce::jmin(canonicalCount, rowCount);
    const int firstPlayableRow = SEQ_MAX_ROWS - rowCount;

    bool legacyHasHits = false;
    bool playableRangeHasHits = false;

    for (int pat = 0; pat < SEQ_MAX_PATTERNS; ++pat)
    {
        for (int step = 0; step < SEQ_MAX_STEPS; ++step)
        {
            for (int row = 0; row < semanticCount; ++row)
            {
                legacyHasHits |= layer->getProb(row, step, pat) != SEQ_PROB_OFF;
                playableRangeHasHits |=
                    layer->getProb(firstPlayableRow + row, step, pat) != SEQ_PROB_OFF;
            }
        }
    }

    if (!legacyHasHits || playableRangeHasHits)
        return;

    for (int pat = 0; pat < SEQ_MAX_PATTERNS; ++pat)
    {
        for (int step = 0; step < SEQ_MAX_STEPS; ++step)
        {
            for (int row = 0; row < semanticCount; ++row)
            {
                const int dest = firstPlayableRow + row;
                const int prob = layer->getProb(row, step, pat);

                if (prob != SEQ_PROB_OFF)
                {
                    layer->setProb(dest, step, static_cast<int8_t>(prob), pat);
                    layer->setVel(dest, step, layer->getVel(row, step, pat), pat);
                    layer->setLength(dest, step, layer->getLength(row, step, pat), pat);
                    layer->setOffset(dest, step, layer->getOffset(row, step, pat), pat);
                }

                layer->setLength(row, step, 0, pat);
                layer->setProb(row, step, SEQ_PROB_OFF, pat);
                layer->setVel(row, step, 0, pat);
                layer->setOffset(row, step, 0, pat);
            }
        }
    }
}
}

class GgdDrumGrid : public juce::Component
{
public:
    enum class ToolMode { draw, select };

    GgdDrumGrid(SeqAudioProcessor& p,
                const juce::Array<GgdCanonicalRow>& canonical,
                std::function<void()> undoFn,
                std::function<void()> redoFn,
                std::function<void()> publishFn,
                std::function<void(float)> zoomFn,
                std::function<void(ToolMode)> toolFn)
        : processor(p),
          canonicalRows(canonical),
          undoCallback(std::move(undoFn)),
          redoCallback(std::move(redoFn)),
          publishCallback(std::move(publishFn)),
          zoomCallback(std::move(zoomFn)),
          toolCallback(std::move(toolFn))
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void setMap(const GgdKitMap* newMap)
    {
        map = newMap;
        clearSelection(false);
        rebuildLayout();
        repaint();
    }

    void setMeter(int numerator, int denominator)
    {
        meterNumerator = juce::jmax(1, numerator);
        meterDenominator = juce::jmax(1, denominator);
        clearSelection(false);
        rebuildLayout();
        repaint();
    }

    void refreshSize()
    {
        rebuildLayout();
        repaint();
    }

    void setToolMode(ToolMode newMode)
    {
        if (toolMode == newMode)
            return;

        toolMode = newMode;
        dragMode = DragMode::none;
        marqueeRect = {};
        moveDeltaSteps = 0;
        moveDeltaRows = 0;
        duplicateDrag = false;
        setMouseCursor(toolMode == ToolMode::draw
            ? juce::MouseCursor::CrosshairCursor
            : juce::MouseCursor::NormalCursor);

        if (toolCallback)
            toolCallback(toolMode);
        repaint();
    }

    ToolMode getToolMode() const { return toolMode; }

    void setPlayPosition(int newPosition)
    {
        const double now = juce::Time::getMillisecondCounterHiRes();

        if (newPosition < 0)
        {
            playPosition = -1;
            playStepStartMs = now;
            repaint();
            return;
        }

        if (playPosition >= 0 && newPosition != playPosition)
        {
            auto* layer = processor.mData.getUISeqData()->getLayer(0);
            const int numSteps = juce::jmax(1, layer->getNumSteps());
            int delta = (newPosition - playPosition + numSteps) % numSteps;
            if (delta <= 0)
                delta = 1;

            const double elapsed = now - playStepStartMs;
            const double observed = elapsed / static_cast<double>(delta);
            if (observed >= 12.0 && observed <= 2000.0)
            {
                if (!hasPlayStepEstimate)
                {
                    playStepMs = observed;
                    hasPlayStepEstimate = true;
                }
                else
                {
                    playStepMs = playStepMs * 0.72 + observed * 0.28;
                }
            }
        }

        if (newPosition != playPosition)
        {
            playPosition = newPosition;
            playStepStartMs = now;
        }

        repaint();
    }

    float getZoomScale() const { return zoomScale; }

    void resetZoom()
    {
        setZoomScale(defaultZoomScale);
    }

    void setZoomScale(float newScale)
    {
        auto* viewport = findParentComponentOfClass<juce::Viewport>();
        if (viewport == nullptr)
        {
            zoomScale = snapZoom(newScale);
            rebuildLayout();
            notifyZoomChanged();
            repaint();
            return;
        }

        const int oldViewX = viewport->getViewPositionX();
        const int viewWidth = viewport->getViewArea().getWidth();
        const float anchorInViewport = static_cast<float>(nameWidth)
            + static_cast<float>(juce::jmax(0, viewWidth - nameWidth)) * 0.5f;
        const float anchorInContent = static_cast<float>(oldViewX) + anchorInViewport;
        applyZoom(newScale, anchorInContent, anchorInViewport);
    }

    juce::String resolutionText() const
    {
        return detail32Active() ? "1/32" : "1/16";
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto mods = key.getModifiers();

        // Ctrl/Cmd+A and Ctrl/Cmd+Z do not have a physical-key fallback.
        if (isHostUndoKey(key) && undoCallback)
        {
            undoCallback();
            return true;
        }

        if ((mods.isCtrlDown() || mods.isCommandDown()) && !mods.isAltDown()
            && !mods.isShiftDown() && keyMatchesLetter(key, 'A'))
        {
            if (toolMode == ToolMode::select)
            {
                selectAllHits();
                return true;
            }
        }

        if ((mods.isCtrlDown() || mods.isCommandDown()) && !mods.isAltDown()
            && !mods.isShiftDown() && toolMode == ToolMode::select)
        {
            if (keyMatchesLetter(key, 'C'))
            {
                copySelectionToClipboard();
                return true;
            }
            if (keyMatchesLetter(key, 'V'))
            {
                pasteClipboard();
                return true;
            }
        }

        // Plain editor keys are deliberately consumed here but executed only by
        // pollFallbackShortcuts(). Some hosts send both JUCE key events and expose
        // the same physical key state, which previously caused double nudges/undo.
        const bool noCommand = !mods.isCtrlDown() && !mods.isCommandDown() && !mods.isShiftDown();
        if (noCommand)
        {
            if (!mods.isAltDown()
                && (keyMatchesLetter(key, 'D') || keyMatchesLetter(key, 'S')
                    || keyMatchesLetter(key, 'U') || keyMatchesLetter(key, 'Y')
                    || key.getKeyCode() == juce::KeyPress::deleteKey
                    || key.getKeyCode() == juce::KeyPress::backspaceKey
                    || key.getKeyCode() == juce::KeyPress::escapeKey))
                return true;

            if (key.getKeyCode() == juce::KeyPress::leftKey
                || key.getKeyCode() == juce::KeyPress::rightKey
                || key.getKeyCode() == juce::KeyPress::upKey
                || key.getKeyCode() == juce::KeyPress::downKey)
                return true;
        }

        return false;
    }

    void pollFallbackShortcuts(bool active)
    {
        auto letterDown = [](char upperCaseLetter)
        {
            return juce::KeyPress::isKeyCurrentlyDown(static_cast<int>(upperCaseLetter))
                || juce::KeyPress::isKeyCurrentlyDown(
                    static_cast<int>(upperCaseLetter + ('a' - 'A')));
        };

        auto edge = [](bool down, bool& wasDown)
        {
            const bool pressed = down && !wasDown;
            wasDown = down;
            return pressed;
        };

        const bool dPressed = edge(letterDown('D'), fallbackDDown);
        const bool sPressed = edge(letterDown('S'), fallbackSDown);
        const bool uPressed = edge(letterDown('U'), fallbackUDown);
        const bool yPressed = edge(letterDown('Y'), fallbackYDown);
        const bool leftPressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey), fallbackLeftDown);
        const bool rightPressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey), fallbackRightDown);
        const bool upPressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::upKey), fallbackUpDown);
        const bool downPressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::downKey), fallbackDownDown);
        const bool deletePressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::deleteKey), fallbackDeleteDown);
        const bool backspacePressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::backspaceKey), fallbackBackspaceDown);
        const bool escapePressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::escapeKey), fallbackEscapeDown);

        if (!active)
            return;

        const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
        const bool commandFree = !mods.isCtrlDown() && !mods.isCommandDown() && !mods.isShiftDown();
        const bool alt = mods.isAltDown();
        if (!commandFree)
            return;

        if (!alt)
        {
            if (dPressed)
                setToolMode(ToolMode::draw);
            else if (sPressed)
                setToolMode(ToolMode::select);
            else if (uPressed && undoCallback)
                undoCallback();
            else if (yPressed && redoCallback)
                redoCallback();
        }

        if (toolMode != ToolMode::select)
            return;

        if (!alt && (deletePressed || backspacePressed))
            deleteSelection();
        else if (!alt && escapePressed)
            clearSelection();
        else if (leftPressed)
            nudgeSelection(-1, 0, alt);
        else if (rightPressed)
            nudgeSelection(1, 0, alt);
        else if (upPressed)
            nudgeSelection(0, -1, alt);
        else if (downPressed)
            nudgeSelection(0, 1, alt);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(c(outside));

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();
        const int pattern = layer->getCurrentPattern();
        const float stepWidth = storageStepWidth();
        const int stepsPerBar = stepsPerBarForMeter(meterNumerator, meterDenominator);
        const int stepsPerBeat = juce::jmax(1, 16 / meterDenominator);
        const int viewX = currentViewX();
        const int stickyX = viewX;
        const int gridLeft = nameWidth;
        const int gridRight = static_cast<int>(std::ceil(xForStorageStepF(static_cast<float>(numSteps))));
        const int activeWidth = juce::jmax(0, gridRight - gridLeft);
        const bool detail32 = detail32Active();

        g.setColour(c(bg));
        g.fillRect(gridLeft, 0, activeWidth, getHeight());

        g.setColour(c(panel));
        g.fillRect(gridLeft, 0, activeWidth, rulerHeight);

        for (int barStart = 0; barStart < numSteps; barStart += stepsPerBar)
        {
            const int bar = barStart / stepsPerBar;
            const float x0 = xForStorageStepF(static_cast<float>(barStart));
            const float x1 = xForStorageStepF(static_cast<float>(juce::jmin(numSteps, barStart + stepsPerBar)));

            if ((bar & 1) != 0)
            {
                g.setColour(c(panelRaised).withAlpha(0.25f));
                g.fillRect(juce::Rectangle<float>(x0, 0.0f, x1 - x0,
                                                   static_cast<float>(getHeight())));
            }

            g.setColour(c(text));
            g.setFont(juce::Font(11.5f, juce::Font::bold));
            g.drawText("BAR " + juce::String(bar + 1),
                       static_cast<int>(std::round(x0)) + 8, 4,
                       juce::jmax(44, static_cast<int>(std::round(x1 - x0)) - 12),
                       17, juce::Justification::centredLeft, false);
        }

        for (int step = 0; step <= numSteps; ++step)
        {
            const float x = xForStorageStepF(static_cast<float>(step));
            const bool isBar = step % stepsPerBar == 0;
            const bool isBeat = step % stepsPerBeat == 0;

            if (isBar)
            {
                g.setColour(c(accent2).withAlpha(0.82f));
                g.fillRect(static_cast<int>(std::round(x)) - 1, 0, 2, rulerHeight);
            }
            else if (isBeat)
            {
                g.setColour(c(border).brighter(0.46f).withAlpha(0.98f));
                g.fillRect(static_cast<int>(std::round(x)), rulerHeight - 18, 1, 18);
            }
            else
            {
                g.setColour(c(border).brighter(0.08f).withAlpha(0.82f));
                g.fillRect(static_cast<int>(std::round(x)), rulerHeight - 9, 1, 9);
            }

            if (detail32 && step < numSteps)
            {
                const float hx = x + stepWidth * 0.5f;
                g.setColour(c(border).withAlpha(0.52f));
                g.fillRect(static_cast<int>(std::round(hx)), rulerHeight - 5, 1, 5);
            }
        }

        for (int step = 0; step < numSteps; step += stepsPerBeat)
        {
            const float x = xForStorageStepF(static_cast<float>(step));
            const int beat = (step % stepsPerBar) / stepsPerBeat + 1;
            g.setColour(c(muted).withAlpha(0.90f));
            g.setFont(10.0f);
            g.drawText(juce::String(beat),
                       static_cast<int>(std::round(x)) + 5, 21,
                       juce::jmax(20, static_cast<int>(std::round(stepWidth * stepsPerBeat)) - 6),
                       13, juce::Justification::centredLeft, false);
        }

        g.setColour(c(border).brighter(0.08f));
        g.drawHorizontalLine(rulerHeight - 1, static_cast<float>(gridLeft),
                             static_cast<float>(gridRight));

        int visibleRowIndex = 0;
        for (const auto& item : layout)
        {
            if (item.header)
            {
                g.setColour(c(panelRaised));
                g.fillRect(gridLeft, item.y, activeWidth, item.height);
                g.setColour(c(accent2).withAlpha(0.70f));
                g.fillRect(gridLeft, item.y, activeWidth, 2);
                g.setColour(c(border).brighter(0.22f).withAlpha(0.92f));
                g.fillRect(gridLeft, item.y + item.height - 1, activeWidth, 1);
                continue;
            }

            const bool alternate = (visibleRowIndex++ & 1) != 0;
            g.setColour(alternate ? c(panelRaised).withAlpha(0.32f) : c(bg));
            g.fillRect(gridLeft, item.y, activeWidth, item.height);

            for (int barStart = 0; barStart < numSteps; barStart += stepsPerBar)
            {
                const int bar = barStart / stepsPerBar;
                if ((bar & 1) == 0)
                    continue;

                const float x0 = xForStorageStepF(static_cast<float>(barStart));
                const float x1 = xForStorageStepF(static_cast<float>(juce::jmin(numSteps, barStart + stepsPerBar)));
                g.setColour(c(panelSoft).withAlpha(0.10f));
                g.fillRect(juce::Rectangle<float>(x0, static_cast<float>(item.y),
                                                   x1 - x0, static_cast<float>(item.height)));
            }

            g.setColour(c(border).withAlpha(0.42f));
            g.drawHorizontalLine(item.y + item.height - 1,
                                 static_cast<float>(gridLeft),
                                 static_cast<float>(gridRight));

            const float rowMid = static_cast<float>(item.y) + item.height * 0.5f;

            for (int step = 0; step <= numSteps; ++step)
            {
                const float x = xForStorageStepF(static_cast<float>(step));
                const bool isBar = step % stepsPerBar == 0;
                const bool isBeat = step % stepsPerBeat == 0;

                if (isBar)
                {
                    g.setColour(c(accent2).withAlpha(0.72f));
                    g.fillRect(static_cast<int>(std::round(x)) - 1, item.y, 2, item.height);
                }
                else if (isBeat)
                {
                    g.setColour(c(border).brighter(0.40f).withAlpha(0.96f));
                    g.fillRect(static_cast<int>(std::round(x)), item.y, 1, item.height);
                }
                else
                {
                    g.setColour(c(border).brighter(0.04f).withAlpha(0.78f));
                    g.drawVerticalLine(static_cast<int>(std::round(x)),
                                       rowMid - 7.0f, rowMid + 7.0f);
                }

                if (detail32 && step < numSteps)
                {
                    const float hx = x + stepWidth * 0.5f;
                    g.setColour(c(border).withAlpha(0.46f));
                    g.drawVerticalLine(static_cast<int>(std::round(hx)),
                                       rowMid - 4.0f, rowMid + 4.0f);
                }
            }

            if (toolMode == ToolMode::draw
                && item.canonicalRow == hoverRow && hoverStep >= 0)
            {
                float x0 = xForStorageStepF(static_cast<float>(hoverStep));
                float width = stepWidth;
                if (detail32)
                {
                    width *= 0.5f;
                    if (hoverSecondHalf)
                        x0 += width;
                }

                g.setColour(c(accent).withAlpha(0.075f));
                g.fillRoundedRectangle(
                    juce::Rectangle<float>(x0 + 1.0f, static_cast<float>(item.y + 2),
                                           juce::jmax(1.0f, width - 2.0f),
                                           static_cast<float>(item.height - 4)),
                    4.0f);
            }

            const int storageRow = storageRowForCanonical(item.canonicalRow, canonicalRows.size());

            for (int step = 0; step < numSteps; ++step)
            {
                const int prob = layer->getProb(storageRow, step, pattern);
                if (prob < 0)
                    continue;

                int velocity = layer->getVel(storageRow, step, pattern);
                velocity = juce::jlimit(1, 127, velocity <= 0 ? 1 : velocity);
                const float v = static_cast<float>(velocity) / 127.0f;
                const float slotStart = xForStorageStepF(static_cast<float>(step));
                const float slotWidth = juce::jmax(2.0f, stepWidth);
                const float cy = static_cast<float>(item.y) + item.height * 0.5f;
                const float hitH = 7.0f + 13.0f * v;
                const bool ghost = velocity <= 55;
                const int offset = layer->getOffset(storageRow, step, pattern);
                const int retrigger = layer->getLength(storageRow, step, pattern);
                const bool selected = isSelected(item.canonicalRow, step);

                auto drawHit = [&](float centreX, float visualSlotWidth)
                {
                    const float hitW = juce::jlimit(8.0f, 30.0f, visualSlotWidth * 0.66f);
                    juce::Rectangle<float> hit(centreX - hitW * 0.5f,
                                               cy - hitH * 0.5f,
                                               hitW, hitH);
                    if (ghost)
                    {
                        g.setColour(c(accent).withAlpha(0.22f + 0.42f * v));
                        g.drawRoundedRectangle(hit, juce::jmin(4.0f, hitW * 0.35f), 1.5f);
                    }
                    else
                    {
                        g.setColour(c(accent).withAlpha(0.46f + 0.50f * v));
                        g.fillRoundedRectangle(hit, juce::jmin(4.0f, hitW * 0.35f));
                    }

                    if (selected)
                    {
                        g.setColour(c(text).withAlpha(0.98f));
                        g.drawRoundedRectangle(hit.expanded(2.5f),
                                               juce::jmin(5.0f, hitW * 0.35f + 1.0f), 1.7f);
                    }
                };

                bool spatial32 = false;
                if (detail32 && retrigger == -1 && offset == 0)
                {
                    drawHit(slotStart + slotWidth * 0.25f, slotWidth * 0.5f);
                    drawHit(slotStart + slotWidth * 0.75f, slotWidth * 0.5f);
                    spatial32 = true;
                }
                else if (detail32 && retrigger >= 0 && offset == 50)
                {
                    drawHit(slotStart + slotWidth * 0.75f, slotWidth * 0.5f);
                    spatial32 = true;
                }
                else if (detail32 && retrigger >= 0)
                {
                    drawHit(slotStart + slotWidth * 0.25f, slotWidth * 0.5f);
                    spatial32 = true;
                }
                else
                {
                    drawHit(slotStart + slotWidth * 0.5f, slotWidth);
                }

                if (offset != 0 && !(detail32 && offset == 50 && retrigger >= 0))
                {
                    const float markerBase = detail32 && spatial32
                        ? slotStart + slotWidth * 0.25f
                        : slotStart + slotWidth * 0.5f;
                    const float markerX = markerBase + (static_cast<float>(offset) / 50.0f)
                                               * juce::jmin(slotWidth * 0.36f, 24.0f);
                    g.setColour(c(text).withAlpha(0.90f));
                    g.drawVerticalLine(static_cast<int>(std::round(markerX)),
                                       cy - hitH * 0.36f, cy + hitH * 0.36f);
                }

                if (retrigger < 0 && !(detail32 && retrigger == -1 && offset == 0))
                {
                    g.setColour(c(warm).withAlpha(0.94f));
                    g.setFont(juce::Font(9.5f, juce::Font::bold));
                    g.drawText("x" + juce::String((-retrigger) + 1),
                               static_cast<int>(std::round(slotStart)), item.y + 1,
                               juce::jmax(16, static_cast<int>(std::round(slotWidth))),
                               item.height - 2,
                               juce::Justification::centredBottom, false);
                }
            }
        }

        if (toolMode == ToolMode::select && dragMode == DragMode::marquee
            && !marqueeRect.isEmpty())
        {
            g.setColour(c(accent).withAlpha(0.10f));
            g.fillRect(marqueeRect);
            g.setColour(c(accent).withAlpha(0.88f));
            g.drawRect(marqueeRect, 1.2f);
        }

        if (toolMode == ToolMode::select && dragMode == DragMode::moveSelection
            && (moveDeltaSteps != 0 || moveDeltaRows != 0))
        {
            drawMovePreview(g);
        }

        const float smoothPosition = interpolatedPlayPosition(numSteps);
        if (smoothPosition >= 0.0f)
        {
            const float playX = xForStorageStepF(smoothPosition);
            g.setColour(c(accent).withAlpha(0.10f));
            g.fillRect(juce::Rectangle<float>(playX - 3.0f,
                                               static_cast<float>(rulerHeight), 6.0f,
                                               static_cast<float>(getHeight() - rulerHeight)));
            g.setColour(c(accent).withAlpha(0.94f));
            g.fillRect(juce::Rectangle<float>(playX - 1.0f, 0.0f, 2.0f,
                                               static_cast<float>(getHeight())));
        }

        g.setColour(c(accent2).withAlpha(0.82f));
        g.fillRect(gridRight - 1, 0, 2, getHeight());

        paintVelocityPopup(g);

        g.setColour(c(panel));
        g.fillRect(stickyX, 0, nameWidth, rulerHeight);
        g.setColour(c(text));
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText(toolMode == ToolMode::draw ? "ARTICULATIONS | DRAW" : "ARTICULATIONS | SELECT",
                   stickyX + 16, 3, nameWidth - 32, 18,
                   juce::Justification::centredLeft, false);
        g.setColour(c(muted));
        g.setFont(10.0f);
        const auto zoomCaption = juce::String(static_cast<int>(std::round(zoomScale * 100.0f)))
                               + "% | " + resolutionText() + " GRID";
        g.drawText(zoomCaption, stickyX + 16, 21, nameWidth - 32, 17,
                   juce::Justification::centredLeft, false);

        visibleRowIndex = 0;
        for (const auto& item : layout)
        {
            if (item.header)
            {
                g.setColour(c(panelRaised));
                g.fillRect(stickyX, item.y, nameWidth, item.height);
                g.setColour(c(accent2).withAlpha(0.82f));
                g.fillRect(stickyX, item.y, 4, item.height);
                g.fillRect(stickyX, item.y, nameWidth, 2);
                g.setColour(c(border).brighter(0.22f).withAlpha(0.92f));
                g.fillRect(stickyX, item.y + item.height - 1, nameWidth, 1);
                g.setColour(c(text).withAlpha(0.82f));
                g.setFont(juce::Font(10.5f, juce::Font::bold));
                const bool collapsed = collapsedGroups.count(item.groupId) != 0;
                const auto groupCaption = juce::String(collapsed ? "[+] " : "[-] ")
                                        + item.groupLabel.toUpperCase();
                g.drawText(groupCaption, stickyX + 17, item.y,
                           nameWidth - 30, item.height,
                           juce::Justification::centredLeft, false);
                continue;
            }

            const bool alternate = (visibleRowIndex++ & 1) != 0;
            g.setColour(alternate ? c(panelRaised) : c(panel));
            g.fillRect(stickyX, item.y, nameWidth, item.height);
            g.setColour(c(border).withAlpha(0.50f));
            g.drawHorizontalLine(item.y + item.height - 1,
                                 static_cast<float>(stickyX),
                                 static_cast<float>(stickyX + nameWidth));

            g.setColour(c(text));
            g.setFont(12.0f);
            g.drawText(item.label, stickyX + 16, item.y + 1, nameWidth - 78,
                       item.height - 2, juce::Justification::centredLeft, true);

            g.setColour(c(muted));
            g.setFont(10.0f);
            g.drawText(item.noteName, stickyX + nameWidth - 64, item.y + 1, 48,
                       item.height - 2, juce::Justification::centredRight, false);
        }

        g.setColour(c(border).brighter(0.22f));
        g.drawVerticalLine(stickyX + nameWidth, 0.0f,
                           static_cast<float>(getHeight()));
        g.setColour(c(bg).withAlpha(0.34f));
        g.fillRect(stickyX + nameWidth + 1, 0, 3, getHeight());
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        int row = -1;
        int step = -1;
        bool secondHalf = false;
        if (displayCellAt(e.position, row, step, secondHalf))
        {
            if (row != hoverRow || step != hoverStep || secondHalf != hoverSecondHalf)
            {
                hoverRow = row;
                hoverStep = step;
                hoverSecondHalf = secondHalf;
                repaint();
            }
        }
        else if (hoverRow != -1 || hoverStep != -1)
        {
            hoverRow = -1;
            hoverStep = -1;
            hoverSecondHalf = false;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoverRow = -1;
        hoverStep = -1;
        hoverSecondHalf = false;
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        dragMode = DragMode::none;
        lastPaintRow = -1;
        lastPaintStep = -1;
        lastPaintSecondHalf = false;
        moveDeltaSteps = 0;
        moveDeltaRows = 0;
        duplicateDrag = false;

        if (e.position.x < static_cast<float>(currentViewX() + nameWidth))
        {
            for (const auto& item : layout)
            {
                if (!item.header || e.position.y < item.y || e.position.y >= item.y + item.height)
                    continue;
                if (collapsedGroups.count(item.groupId) != 0)
                    collapsedGroups.erase(item.groupId);
                else
                    collapsedGroups.insert(item.groupId);
                rebuildLayout();
                repaint();
                return;
            }
        }

        if (toolMode == ToolMode::select)
        {
            selectionMouseDown(e);
            return;
        }

        int canonicalRow = -1;
        int step = -1;
        bool secondHalf = false;
        if (!displayCellAt(e.position, canonicalRow, step, secondHalf))
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
        const bool isOn = displayHalfIsOn(layer, storageRow, step, pattern, secondHalf);

        dragRow = canonicalRow;
        dragStep = step;
        dragSecondHalf = secondHalf;
        dragStart = e.position;

        const auto mods = e.mods;
        const bool command = mods.isCtrlDown() || mods.isCommandDown();

        if (command && mods.isShiftDown() && !mods.isAltDown())
        {
            const bool anyOn = layer->getProb(storageRow, step, pattern) >= 0;
            if (!anyOn)
            {
                setCell(canonicalRow, step, true);
                layer->setLength(storageRow, step, -1, pattern);
                layer->setOffset(storageRow, step, 0, pattern);
            }
            else
            {
                const int len = layer->getLength(storageRow, step, pattern);
                const int next = len >= 0 ? -1 : len > -3 ? len - 1 : 0;
                layer->setLength(storageRow, step, static_cast<int8_t>(next), pattern);
                if (next < 0)
                    layer->setOffset(storageRow, step, 0, pattern);
            }

            dragMode = DragMode::special;
            repaint();
            return;
        }

        if (command && !mods.isAltDown())
        {
            if (!isOn)
            {
                setDisplayCell(canonicalRow, step, secondHalf, true);
                layer->setVel(storageRow, step, 42, pattern);
            }
            else
            {
                const int oldVelocity = juce::jlimit(
                    1, 127, static_cast<int>(layer->getVel(storageRow, step, pattern)));
                layer->setVel(storageRow, step,
                              static_cast<int8_t>(oldVelocity <= 55 ? 100 : 42), pattern);
            }

            dragMode = DragMode::special;
            showVelocityPopup(canonicalRow, step, secondHalf,
                              juce::jlimit(1, 127,
                                  static_cast<int>(layer->getVel(storageRow, step, pattern))));
            repaint();
            return;
        }

        if (mods.isShiftDown() && isOn)
        {
            dragMode = DragMode::velocity;
            dragStartValue = juce::jlimit(
                1, 127, static_cast<int>(layer->getVel(storageRow, step, pattern)));
            showVelocityPopup(canonicalRow, step, secondHalf, dragStartValue, true);
            return;
        }

        if (mods.isAltDown() && isOn)
        {
            dragMode = DragMode::timing;
            dragStartValue = layer->getOffset(storageRow, step, pattern);
            return;
        }

        if (mods.isRightButtonDown())
        {
            dragMode = DragMode::erase;
            setDisplayCell(canonicalRow, step, secondHalf, false);
        }
        else
        {
            dragMode = isOn ? DragMode::erase : DragMode::paint;
            setDisplayCell(canonicalRow, step, secondHalf, !isOn);
        }

        lastPaintRow = canonicalRow;
        lastPaintStep = step;
        lastPaintSecondHalf = secondHalf;
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (toolMode == ToolMode::select)
        {
            selectionMouseDrag(e);
            return;
        }

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();

        if (dragMode == DragMode::velocity && dragRow >= 0)
        {
            const int storageRow = storageRowForCanonical(dragRow, canonicalRows.size());
            const int delta = static_cast<int>((dragStart.y - e.position.y) * 1.5f);
            const int velocity = juce::jlimit(1, 127, dragStartValue + delta);
            layer->setVel(storageRow, dragStep, static_cast<int8_t>(velocity), pattern);
            showVelocityPopup(dragRow, dragStep, dragSecondHalf, velocity, true);
            repaint();
            return;
        }

        if (dragMode == DragMode::timing && dragRow >= 0)
        {
            const int storageRow = storageRowForCanonical(dragRow, canonicalRows.size());
            const float pixelsForHalfStep = juce::jmax(8.0f, storageStepWidth() * 0.5f);
            const float dx = e.position.x - dragStart.x;
            const int delta = static_cast<int>(std::round((dx / pixelsForHalfStep) * 50.0f));
            const int offset = juce::jlimit(-50, 50, dragStartValue + delta);
            layer->setOffset(storageRow, dragStep, static_cast<int8_t>(offset), pattern);
            repaint();
            return;
        }

        if (dragMode != DragMode::paint && dragMode != DragMode::erase)
            return;

        int canonicalRow = -1;
        int step = -1;
        bool secondHalf = false;
        if (!displayCellAt(e.position, canonicalRow, step, secondHalf))
            return;

        if (canonicalRow == lastPaintRow && step == lastPaintStep
            && secondHalf == lastPaintSecondHalf)
            return;

        setDisplayCell(canonicalRow, step, secondHalf, dragMode == DragMode::paint);
        lastPaintRow = canonicalRow;
        lastPaintStep = step;
        lastPaintSecondHalf = secondHalf;
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (toolMode == ToolMode::select)
        {
            selectionMouseUp();
            return;
        }

        if (dragMode != DragMode::none)
            publishChange();

        if (dragMode == DragMode::velocity)
            velocityPopupPinned = false;

        dragMode = DragMode::none;
    }

    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override
    {
        auto* viewport = findParentComponentOfClass<juce::Viewport>();
        if (viewport == nullptr)
            return;

        const float primaryDelta =
            std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY : wheel.deltaX;

        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            if (std::abs(primaryDelta) < 0.0001f)
                return;

            const int oldViewX = viewport->getViewPositionX();
            float anchorInViewport = e.position.x - static_cast<float>(oldViewX);
            float anchorInContent = e.position.x;

            if (anchorInViewport < static_cast<float>(nameWidth))
            {
                anchorInViewport = static_cast<float>(nameWidth);
                anchorInContent = static_cast<float>(oldViewX + nameWidth);
            }

            const float direction = primaryDelta > 0.0f ? 0.25f : -0.25f;
            applyZoom(zoomScale + direction, anchorInContent, anchorInViewport);
            return;
        }

        if (e.mods.isAltDown())
        {
            int canonicalRow = -1;
            int step = -1;
            bool secondHalf = false;
            if (displayCellAt(e.position, canonicalRow, step, secondHalf))
            {
                auto* layer = processor.mData.getUISeqData()->getLayer(0);
                const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
                const int pattern = layer->getCurrentPattern();

                if (displayHalfIsOn(layer, storageRow, step, pattern, secondHalf))
                {
                    if (e.mods.isShiftDown())
                    {
                        if (cycleHatArticulation(canonicalRow, step,
                                                 primaryDelta >= 0.0f ? 1 : -1))
                            return;
                    }
                    else
                    {
                        const int amount = juce::jmax(
                            1, static_cast<int>(std::round(std::abs(primaryDelta) * 24.0f)));
                        const int signedAmount = primaryDelta >= 0.0f ? amount : -amount;

                        if (toolMode == ToolMode::select && isSelected(canonicalRow, step)
                            && !selection.empty())
                        {
                            adjustSelectionVelocity(signedAmount);
                            const int velocity = juce::jlimit(
                                1, 127,
                                static_cast<int>(layer->getVel(storageRow, step, pattern)));
                            showVelocityPopup(canonicalRow, step, secondHalf, velocity);
                        }
                        else
                        {
                            const int oldVelocity = juce::jlimit(
                                1, 127, static_cast<int>(layer->getVel(storageRow, step, pattern)));
                            const int velocity = juce::jlimit(1, 127, oldVelocity + signedAmount);

                            if (velocity != oldVelocity)
                            {
                                layer->setVel(storageRow, step, static_cast<int8_t>(velocity), pattern);
                                publishChange();
                                showVelocityPopup(canonicalRow, step, secondHalf, velocity);
                                repaint();
                            }
                        }
                        return;
                    }
                }
            }
        }

        const float scrollScale = 170.0f;
        int x = viewport->getViewPositionX();
        int y = viewport->getViewPositionY();

        if (std::abs(wheel.deltaX) > 0.0001f)
            x -= static_cast<int>(std::round(wheel.deltaX * scrollScale));
        else if (e.mods.isShiftDown())
            x -= static_cast<int>(std::round(wheel.deltaY * scrollScale));
        else
            y -= static_cast<int>(std::round(wheel.deltaY * scrollScale));

        viewport->setViewPosition(juce::jmax(0, x), juce::jmax(0, y));
    }

private:
    struct LayoutItem
    {
        bool header = false;
        int y = 0;
        int height = 0;
        int canonicalRow = -1;
        juce::String groupId;
        juce::String groupLabel;
        juce::String label;
        juce::String noteName;
    };

    struct CellRef
    {
        int row = -1;
        int step = -1;
    };

    struct CellState
    {
        CellRef ref;
        int prob = SEQ_PROB_OFF;
        int velocity = 0;
        int length = 0;
        int offset = 0;
    };

public:
    int getSelectionCount() const { return static_cast<int>(selection.size()); }
    bool hasClipboard() const { return !clipboard.empty(); }

    bool copySelectionToClipboard()
    {
        if (selection.empty())
            return false;

        int minStep = selection.front().step;
        int maxStep = selection.front().step;
        for (const auto& ref : selection)
        {
            minStep = juce::jmin(minStep, ref.step);
            maxStep = juce::jmax(maxStep, ref.step);
        }

        clipboard.clear();
        clipboard.reserve(selection.size());
        for (const auto& ref : selection)
        {
            auto state = snapshotCell(ref.row, ref.step);
            state.ref.step -= minStep;
            clipboard.push_back(std::move(state));
        }
        clipboardSpanSteps = juce::jmax(1, maxStep - minStep + 1);
        return true;
    }

    bool pasteClipboard()
    {
        if (clipboard.empty())
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();
        int startStep = 0;
        if (!selection.empty())
        {
            int maxSelected = selection.front().step;
            for (const auto& ref : selection)
                maxSelected = juce::jmax(maxSelected, ref.step);
            startStep = maxSelected + 1;
        }

        auto fitsAt = [&](int anchor)
        {
            for (const auto& state : clipboard)
            {
                const int step = anchor + state.ref.step;
                if (step < 0 || step >= numSteps || cellOccupied(state.ref.row, step))
                    return false;
            }
            return true;
        };

        while (startStep < numSteps && !fitsAt(startStep))
            startStep += clipboardSpanSteps;
        if (!fitsAt(startStep))
            return false;

        selection.clear();
        for (const auto& state : clipboard)
        {
            const int step = startStep + state.ref.step;
            writeCellState(state, state.ref.row, step);
            selection.push_back({ state.ref.row, step });
        }
        publishChange();
        repaint();
        return true;
    }

    void adjustSelectedVelocityBy(int delta) { adjustSelectionVelocity(delta); }

    void adjustSelectedTimingBy(int delta)
    {
        if (selection.empty() || delta == 0)
            return;
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        bool changed = false;
        for (const auto& ref : selection)
        {
            const int storageRow = storageRowForCanonical(ref.row, canonicalRows.size());
            const int oldOffset = layer->getOffset(storageRow, ref.step, pattern);
            const int newOffset = juce::jlimit(-50, 50, oldOffset + delta);
            if (newOffset != oldOffset)
            {
                layer->setOffset(storageRow, ref.step, static_cast<int8_t>(newOffset), pattern);
                changed = true;
            }
        }
        if (changed)
        {
            publishChange();
            repaint();
        }
    }

    void humanizeSelected()
    {
        if (selection.empty())
            return;
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        auto& random = juce::Random::getSystemRandom();
        for (const auto& ref : selection)
        {
            const int storageRow = storageRowForCanonical(ref.row, canonicalRows.size());
            const int velocity = juce::jlimit(1, 127,
                static_cast<int>(layer->getVel(storageRow, ref.step, pattern))
                + random.nextInt(13) - 6);
            const int offset = juce::jlimit(-50, 50,
                static_cast<int>(layer->getOffset(storageRow, ref.step, pattern))
                + random.nextInt(9) - 4);
            layer->setVel(storageRow, ref.step, static_cast<int8_t>(velocity), pattern);
            layer->setOffset(storageRow, ref.step, static_cast<int8_t>(offset), pattern);
        }
        publishChange();
        repaint();
    }

    void deleteSelected() { deleteSelection(); }

private:
    enum class DragMode
    {
        none,
        paint,
        erase,
        velocity,
        timing,
        special,
        marquee,
        moveSelection
    };

    SeqAudioProcessor& processor;
    const juce::Array<GgdCanonicalRow>& canonicalRows;
    std::function<void()> undoCallback;
    std::function<void()> redoCallback;
    std::function<void()> publishCallback;
    std::function<void(float)> zoomCallback;
    std::function<void(ToolMode)> toolCallback;
    const GgdKitMap* map = nullptr;
    std::vector<LayoutItem> layout;
    std::vector<CellRef> selection;
    std::vector<CellRef> marqueeBaseSelection;
    std::vector<CellState> clipboard;
    int clipboardSpanSteps = 1;
    std::set<juce::String> collapsedGroups;

    int meterNumerator = 4;
    int meterDenominator = 4;
    int playPosition = -1;
    double playStepStartMs = 0.0;
    double playStepMs = 125.0;
    bool hasPlayStepEstimate = false;
    float zoomScale = defaultZoomScale;
    ToolMode toolMode = ToolMode::draw;
    DragMode dragMode = DragMode::none;
    int dragRow = -1;
    int dragStep = -1;
    bool dragSecondHalf = false;
    int dragStartValue = 0;
    juce::Point<float> dragStart;
    int lastPaintRow = -1;
    int lastPaintStep = -1;
    bool lastPaintSecondHalf = false;
    int hoverRow = -1;
    int hoverStep = -1;
    bool hoverSecondHalf = false;

    juce::Point<float> marqueeStart;
    juce::Rectangle<float> marqueeRect;
    bool marqueeAdditive = false;
    int moveAnchorRow = -1;
    int moveAnchorStep = -1;
    int moveDeltaSteps = 0;
    int moveDeltaRows = 0;
    bool duplicateDrag = false;

    int velocityPopupRow = -1;
    int velocityPopupStep = -1;
    int velocityPopupValue = 0;
    bool velocityPopupSecondHalf = false;
    bool velocityPopupPinned = false;
    double velocityPopupUntilMs = 0.0;

    bool fallbackDDown = false;
    bool fallbackSDown = false;
    bool fallbackUDown = false;
    bool fallbackYDown = false;
    bool fallbackLeftDown = false;
    bool fallbackRightDown = false;
    bool fallbackUpDown = false;
    bool fallbackDownDown = false;
    bool fallbackDeleteDown = false;
    bool fallbackBackspaceDown = false;
    bool fallbackEscapeDown = false;

    static constexpr int nameWidth = 224;
    static constexpr int rulerHeight = 42;
    static constexpr int groupHeight = 30;
    static constexpr int rowHeight = 32;
    static constexpr float defaultPixelsPerQuarter = 128.0f;
    static constexpr float minZoomScale = 0.5f;
    static constexpr float maxZoomScale = 4.0f;

    bool detail32Active() const { return zoomScale >= detail32ZoomThreshold; }

    float snapZoom(float scale) const
    {
        const float clamped = juce::jlimit(minZoomScale, maxZoomScale, scale);
        return std::round(clamped * 4.0f) / 4.0f;
    }

    int viewportWidth() const
    {
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
            return juce::jmax(nameWidth + 240, viewport->getViewArea().getWidth());
        return 984;
    }

    float storageStepWidth() const
    {
        return (defaultPixelsPerQuarter * zoomScale) / 4.0f;
    }

    float xForStorageStepF(float step) const
    {
        return static_cast<float>(nameWidth) + step * storageStepWidth();
    }

    int currentViewX() const
    {
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
            return viewport->getViewPositionX();
        return 0;
    }

    float interpolatedPlayPosition(int numSteps) const
    {
        if (playPosition < 0 || numSteps <= 0)
            return -1.0f;

        const double now = juce::Time::getMillisecondCounterHiRes();
        const double duration = juce::jmax(16.0, playStepMs);
        const float fraction = static_cast<float>(
            juce::jlimit(0.0, 0.995, (now - playStepStartMs) / duration));
        float position = static_cast<float>(playPosition) + fraction;
        while (position >= static_cast<float>(numSteps))
            position -= static_cast<float>(numSteps);
        return position;
    }

    void notifyZoomChanged()
    {
        if (zoomCallback)
            zoomCallback(zoomScale);
    }

    void applyZoom(float requestedScale, float anchorInContent, float anchorInViewport)
    {
        const float newScale = snapZoom(requestedScale);
        if (std::abs(newScale - zoomScale) < 0.0001f)
            return;

        auto* viewport = findParentComponentOfClass<juce::Viewport>();
        const float oldStepWidth = storageStepWidth();
        const float stepAtAnchor = oldStepWidth > 0.0f
            ? (anchorInContent - static_cast<float>(nameWidth)) / oldStepWidth
            : 0.0f;

        zoomScale = newScale;
        rebuildLayout();

        if (viewport != nullptr)
        {
            const float newAnchorInContent = static_cast<float>(nameWidth)
                + stepAtAnchor * storageStepWidth();
            const int requestedViewX = static_cast<int>(
                std::round(newAnchorInContent - anchorInViewport));
            viewport->setViewPosition(juce::jmax(0, requestedViewX),
                                      viewport->getViewPositionY());
        }

        hoverRow = -1;
        hoverStep = -1;
        hoverSecondHalf = false;
        notifyZoomChanged();
        repaint();
    }

    void rebuildLayout()
    {
        layout.clear();
        int y = rulerHeight;

        if (map != nullptr)
        {
            struct Family
            {
                const char* id;
                const char* label;
                const char* prefix;
            };
            static constexpr Family families[] = {
                { "kick", "Kick", "kick." },
                { "snare", "Snare", "snare." },
                { "toms", "Toms", "tom." },
                { "hats", "Hi-Hat", "hihat." },
                { "ride", "Ride", "ride." },
                { "crashes", "Crashes", "crash." },
                { "china", "China", "china." },
                { "splashes", "Splashes", "splash." },
                { "other", "Other", "" }
            };

            std::vector<const GgdArticulation*> articulations;
            for (const auto& group : map->groups)
                for (const auto& articulation : group.articulations)
                    articulations.push_back(&articulation);

            std::set<juce::String> placed;
            for (const auto& family : families)
            {
                std::vector<const GgdArticulation*> familyItems;
                for (const auto* articulation : articulations)
                {
                    const bool ordinaryFamily = juce::String(family.id) != "other";
                    if ((ordinaryFamily && articulation->semanticId.startsWith(family.prefix))
                        || (!ordinaryFamily && placed.count(articulation->semanticId) == 0))
                    {
                        if (placed.insert(articulation->semanticId).second)
                            familyItems.push_back(articulation);
                    }
                }

                if (familyItems.empty())
                    continue;

                LayoutItem headerItem;
                headerItem.header = true;
                headerItem.y = y;
                headerItem.height = groupHeight;
                headerItem.groupId = family.id;
                headerItem.groupLabel = family.label;
                layout.push_back(headerItem);
                y += groupHeight;

                if (collapsedGroups.count(headerItem.groupId) != 0)
                    continue;

                for (const auto* articulation : familyItems)
                {
                    const int canonical = GgdKitMapLibrary::findCanonicalRow(
                        canonicalRows, articulation->semanticId);
                    if (canonical < 0)
                        continue;

                    LayoutItem row;
                    row.y = y;
                    row.height = rowHeight;
                    row.canonicalRow = canonical;
                    row.groupId = family.id;
                    row.groupLabel = family.label;
                    row.label = articulation->label;
                    if (const auto* binding = articulation->primaryNoteBinding())
                        row.noteName = binding->noteName;
                    else
                        row.noteName = "-";

                    layout.push_back(row);
                    y += rowHeight;
                }
            }
        }

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int patternRight = static_cast<int>(
            std::ceil(xForStorageStepF(static_cast<float>(layer->getNumSteps()))));
        setSize(juce::jmax(viewportWidth(), patternRight), juce::jmax(320, y + 12));
    }

    bool rowAtY(float y, int& canonicalRow) const
    {
        for (const auto& item : layout)
        {
            if (!item.header && y >= item.y && y < item.y + item.height)
            {
                canonicalRow = item.canonicalRow;
                return true;
            }
        }
        return false;
    }

    int rowY(int canonicalRow) const
    {
        for (const auto& item : layout)
            if (!item.header && item.canonicalRow == canonicalRow)
                return item.y;
        return -1;
    }

    std::vector<int> visibleRows() const
    {
        std::vector<int> rows;
        for (const auto& item : layout)
            if (!item.header)
                rows.push_back(item.canonicalRow);
        return rows;
    }

    int visibleRowIndexForCanonical(int canonicalRow) const
    {
        int index = 0;
        for (const auto& item : layout)
        {
            if (item.header)
                continue;
            if (item.canonicalRow == canonicalRow)
                return index;
            ++index;
        }
        return -1;
    }

    int shiftedCanonicalRow(int canonicalRow, int deltaRows) const
    {
        const auto rows = visibleRows();
        const int index = visibleRowIndexForCanonical(canonicalRow);
        const int target = index + deltaRows;
        if (index < 0 || target < 0 || target >= static_cast<int>(rows.size()))
            return -1;
        return rows[static_cast<size_t>(target)];
    }

    bool exactCellAt(juce::Point<float> p, int& canonicalRow, int& step) const
    {
        if (p.y < rulerHeight || p.x < currentViewX() + nameWidth)
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        step = static_cast<int>(std::floor(
            (p.x - static_cast<float>(nameWidth)) / storageStepWidth()));
        if (step < 0 || step >= layer->getNumSteps())
            return false;

        return rowAtY(p.y, canonicalRow);
    }

    bool displayCellAt(juce::Point<float> p,
                       int& canonicalRow,
                       int& step,
                       bool& secondHalf) const
    {
        if (!exactCellAt(p, canonicalRow, step))
            return false;

        secondHalf = false;
        if (detail32Active())
        {
            const float localX = p.x - xForStorageStepF(static_cast<float>(step));
            secondHalf = localX >= storageStepWidth() * 0.5f;
        }
        return true;
    }

    bool displayHalfIsOn(SequenceLayer* layer,
                         int storageRow,
                         int step,
                         int pattern,
                         bool secondHalf) const
    {
        if (layer->getProb(storageRow, step, pattern) < 0)
            return false;

        if (!detail32Active())
            return true;

        const int length = layer->getLength(storageRow, step, pattern);
        const int offset = layer->getOffset(storageRow, step, pattern);

        if (length == -1 && offset == 0)
            return true;
        if (length < -1)
            return true;

        return secondHalf ? (offset == 50) : (offset != 50);
    }

    void setDisplayCell(int canonicalRow, int step, bool secondHalf, bool on)
    {
        if (!detail32Active())
        {
            setCell(canonicalRow, step, on);
            return;
        }

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
        const bool anyOn = layer->getProb(storageRow, step, pattern) >= 0;
        const int length = layer->getLength(storageRow, step, pattern);
        const int offset = layer->getOffset(storageRow, step, pattern);

        if (length < -1)
        {
            if (!on)
                setCell(canonicalRow, step, false);
            return;
        }

        const bool pair = anyOn && length == -1 && offset == 0;
        const bool firstOn = anyOn && (pair || offset != 50);
        const bool secondOn = anyOn && (pair || offset == 50);
        const bool targetOn = secondHalf ? secondOn : firstOn;

        if (on)
        {
            if (targetOn)
                return;

            if (!anyOn)
            {
                setCell(canonicalRow, step, true);
                layer->setLength(storageRow, step, 0, pattern);
                layer->setOffset(storageRow, step,
                                 static_cast<int8_t>(secondHalf ? 50 : 0), pattern);
            }
            else
            {
                layer->setLength(storageRow, step, -1, pattern);
                layer->setOffset(storageRow, step, 0, pattern);
            }
            return;
        }

        if (!targetOn)
            return;

        if (pair)
        {
            layer->setLength(storageRow, step, 0, pattern);
            layer->setOffset(storageRow, step,
                             static_cast<int8_t>(secondHalf ? 0 : 50), pattern);
        }
        else
        {
            setCell(canonicalRow, step, false);
        }
    }

    void setCell(int canonicalRow, int step, bool on)
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());

        if (!on)
        {
            layer->setLength(storageRow, step, 0, pattern);
            layer->setProb(storageRow, step, SEQ_PROB_OFF, pattern);
            layer->setVel(storageRow, step, 0, pattern);
            layer->setOffset(storageRow, step, 0, pattern);
            return;
        }

        layer->setProb(storageRow, step, SEQ_PROB_ON, pattern);
        if (layer->getVel(storageRow, step, pattern) <= 0)
            layer->setVel(storageRow, step, 100, pattern);
    }

    bool sameCell(const CellRef& a, const CellRef& b) const
    {
        return a.row == b.row && a.step == b.step;
    }

    bool isSelected(int row, int step) const
    {
        const CellRef target { row, step };
        return std::any_of(selection.begin(), selection.end(),
                           [&](const CellRef& value) { return sameCell(value, target); });
    }

    void addSelection(int row, int step)
    {
        if (!isSelected(row, step))
            selection.push_back({ row, step });
    }

    void removeSelection(int row, int step)
    {
        selection.erase(
            std::remove_if(selection.begin(), selection.end(),
                           [&](const CellRef& value)
                           {
                               return value.row == row && value.step == step;
                           }),
            selection.end());
    }

    void toggleSelection(int row, int step)
    {
        if (isSelected(row, step))
            removeSelection(row, step);
        else
            addSelection(row, step);
        repaint();
    }

    void clearSelection(bool repaintNow = true)
    {
        selection.clear();
        marqueeBaseSelection.clear();
        if (repaintNow)
            repaint();
    }

    CellState snapshotCell(int row, int step) const
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(row, canonicalRows.size());

        CellState state;
        state.ref = { row, step };
        state.prob = layer->getProb(storageRow, step, pattern);
        state.velocity = layer->getVel(storageRow, step, pattern);
        state.length = layer->getLength(storageRow, step, pattern);
        state.offset = layer->getOffset(storageRow, step, pattern);
        return state;
    }

    void writeCellState(const CellState& state, int row, int step)
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(row, canonicalRows.size());
        layer->setProb(storageRow, step, static_cast<int8_t>(state.prob), pattern);
        layer->setVel(storageRow, step, static_cast<int8_t>(state.velocity), pattern);
        layer->setLength(storageRow, step, static_cast<int8_t>(state.length), pattern);
        layer->setOffset(storageRow, step, static_cast<int8_t>(state.offset), pattern);
    }

    void clearCellRaw(int row, int step)
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(row, canonicalRows.size());
        layer->setLength(storageRow, step, 0, pattern);
        layer->setProb(storageRow, step, SEQ_PROB_OFF, pattern);
        layer->setVel(storageRow, step, 0, pattern);
        layer->setOffset(storageRow, step, 0, pattern);
    }

    bool cellOccupied(int row, int step) const
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(row, canonicalRows.size());
        return layer->getProb(storageRow, step, pattern) >= 0;
    }

    bool selectionTargetsAreValid(int deltaSteps, int deltaRows, bool copying = false) const
    {
        if (selection.empty())
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();

        for (const auto& ref : selection)
        {
            const int targetRow = shiftedCanonicalRow(ref.row, deltaRows);
            const int targetStep = ref.step + deltaSteps;
            if (targetRow < 0 || targetStep < 0 || targetStep >= numSteps)
                return false;

            if (cellOccupied(targetRow, targetStep))
            {
                if (copying || !isSelected(targetRow, targetStep))
                    return false;
            }
        }

        return true;
    }

    bool duplicateSelectionByDelta(int deltaSteps, int deltaRows)
    {
        if ((deltaSteps == 0 && deltaRows == 0) || selection.empty())
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();
        std::set<std::pair<int, int>> targets;

        for (const auto& ref : selection)
        {
            const int targetRow = shiftedCanonicalRow(ref.row, deltaRows);
            const int targetStep = ref.step + deltaSteps;
            if (targetRow < 0 || targetStep < 0 || targetStep >= numSteps)
                return false;
            if (!targets.emplace(targetRow, targetStep).second)
                return false;
            if (cellOccupied(targetRow, targetStep))
                return false;
        }

        std::vector<CellRef> duplicated;
        duplicated.reserve(selection.size());
        for (const auto& ref : selection)
        {
            const auto state = snapshotCell(ref.row, ref.step);
            const int targetRow = shiftedCanonicalRow(ref.row, deltaRows);
            const int targetStep = ref.step + deltaSteps;
            writeCellState(state, targetRow, targetStep);
            duplicated.push_back({ targetRow, targetStep });
        }

        selection = std::move(duplicated);
        publishChange();
        repaint();
        return true;
    }

    bool nudgeSelectionHalfStep(int direction, bool duplicate)
    {
        if (selection.empty() || (direction != -1 && direction != 1))
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();

        struct Destination
        {
            CellState state;
            int step = 0;
            int offset = 0;
        };

        std::vector<Destination> destinations;
        destinations.reserve(selection.size());
        std::set<std::pair<int, int>> targetKeys;

        for (const auto& ref : selection)
        {
            auto state = snapshotCell(ref.row, ref.step);
            if (state.length < 0)
                return false; // retrigger/roll cells cannot be split losslessly yet

            const int phase = state.offset == 50 ? 1 : 0;
            const int halfIndex = ref.step * 2 + phase + direction;
            if (halfIndex < 0)
                return false;

            const int targetStep = halfIndex / 2;
            const int targetPhase = halfIndex % 2;
            if (targetStep < 0 || targetStep >= numSteps)
                return false;

            // Duplicating to the other half of the same 1/16 cell is exactly an x2 retrigger.
            if (duplicate && targetStep == ref.step)
            {
                if (selection.size() != 1)
                    return false;
                state.length = -1;
                state.offset = 0;
                writeCellState(state, ref.row, ref.step);
                publishChange();
                repaint();
                return true;
            }

            const auto key = std::make_pair(ref.row, targetStep);
            if (!targetKeys.emplace(key).second)
                return false;

            if (cellOccupied(ref.row, targetStep))
            {
                if (duplicate || !isSelected(ref.row, targetStep))
                    return false;
            }

            state.offset = targetPhase == 0 ? 0 : 50;
            destinations.push_back({ state, targetStep, state.offset });
        }

        if (!duplicate)
            for (const auto& ref : selection)
                clearCellRaw(ref.row, ref.step);

        std::vector<CellRef> moved;
        moved.reserve(destinations.size());
        for (auto& destination : destinations)
        {
            destination.state.offset = destination.offset;
            writeCellState(destination.state, destination.state.ref.row, destination.step);
            moved.push_back({ destination.state.ref.row, destination.step });
        }

        selection = std::move(moved);
        publishChange();
        repaint();
        return true;
    }

    bool nudgeSelection(int horizontalDirection, int verticalDirection, bool duplicate)
    {
        if (selection.empty())
            return false;

        if (verticalDirection != 0)
            return duplicate
                ? duplicateSelectionByDelta(0, verticalDirection)
                : moveSelection(0, verticalDirection);

        if (horizontalDirection == 0)
            return false;

        if (detail32Active())
            return nudgeSelectionHalfStep(horizontalDirection, duplicate);

        return duplicate
            ? duplicateSelectionByDelta(horizontalDirection, 0)
            : moveSelection(horizontalDirection, 0);
    }

    bool moveSelection(int deltaSteps, int deltaRows)
    {
        if ((deltaSteps == 0 && deltaRows == 0) || selection.empty())
            return false;

        if (!selectionTargetsAreValid(deltaSteps, deltaRows, false))
            return false;

        std::vector<CellState> states;
        states.reserve(selection.size());
        for (const auto& ref : selection)
            states.push_back(snapshotCell(ref.row, ref.step));

        for (const auto& ref : selection)
            clearCellRaw(ref.row, ref.step);

        std::vector<CellRef> moved;
        moved.reserve(states.size());
        for (const auto& state : states)
        {
            const int row = shiftedCanonicalRow(state.ref.row, deltaRows);
            const int step = state.ref.step + deltaSteps;
            writeCellState(state, row, step);
            moved.push_back({ row, step });
        }

        selection = std::move(moved);
        publishChange();
        repaint();
        return true;
    }

    bool duplicateSelectionAt(int deltaSteps, int deltaRows)
    {
        if ((deltaSteps == 0 && deltaRows == 0) || selection.empty())
            return false;

        if (!selectionTargetsAreValid(deltaSteps, deltaRows, true))
            return false;

        std::vector<CellState> states;
        states.reserve(selection.size());
        for (const auto& ref : selection)
            states.push_back(snapshotCell(ref.row, ref.step));

        std::vector<CellRef> duplicated;
        duplicated.reserve(states.size());
        for (const auto& state : states)
        {
            const int row = shiftedCanonicalRow(state.ref.row, deltaRows);
            const int step = state.ref.step + deltaSteps;
            writeCellState(state, row, step);
            duplicated.push_back({ row, step });
        }

        selection = std::move(duplicated);
        publishChange();
        repaint();
        return true;
    }

    void deleteSelection()
    {
        if (selection.empty())
            return;

        for (const auto& ref : selection)
            clearCellRaw(ref.row, ref.step);
        selection.clear();
        publishChange();
        repaint();
    }

    void selectAllHits()
    {
        selection.clear();
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int numSteps = layer->getNumSteps();

        for (const auto& item : layout)
        {
            if (item.header)
                continue;
            const int storageRow = storageRowForCanonical(item.canonicalRow, canonicalRows.size());
            for (int step = 0; step < numSteps; ++step)
                if (layer->getProb(storageRow, step, pattern) >= 0)
                    selection.push_back({ item.canonicalRow, step });
        }
        repaint();
    }

    void adjustSelectionVelocity(int delta)
    {
        if (selection.empty() || delta == 0)
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        bool changed = false;

        for (const auto& ref : selection)
        {
            const int storageRow = storageRowForCanonical(ref.row, canonicalRows.size());
            const int oldVelocity = juce::jlimit(
                1, 127, static_cast<int>(layer->getVel(storageRow, ref.step, pattern)));
            const int velocity = juce::jlimit(1, 127, oldVelocity + delta);
            if (velocity != oldVelocity)
            {
                layer->setVel(storageRow, ref.step, static_cast<int8_t>(velocity), pattern);
                changed = true;
            }
        }

        if (changed)
        {
            publishChange();
            repaint();
        }
    }

    void selectionMouseDown(const juce::MouseEvent& e)
    {
        int row = -1;
        int step = -1;
        bool secondHalf = false;
        const bool inCell = displayCellAt(e.position, row, step, secondHalf);
        bool hit = false;

        if (inCell)
        {
            auto* layer = processor.mData.getUISeqData()->getLayer(0);
            const int storageRow = storageRowForCanonical(row, canonicalRows.size());
            hit = displayHalfIsOn(layer, storageRow, step,
                                  layer->getCurrentPattern(), secondHalf);
        }

        if (hit)
        {
            if (e.mods.isShiftDown())
            {
                toggleSelection(row, step);
                dragMode = DragMode::none;
                return;
            }

            if (!isSelected(row, step))
            {
                selection.clear();
                addSelection(row, step);
            }

            duplicateDrag = e.mods.isAltDown();
            dragMode = DragMode::moveSelection;
            dragStart = e.position;
            moveAnchorRow = row;
            moveAnchorStep = step;
            repaint();
            return;
        }

        marqueeAdditive = e.mods.isShiftDown();
        marqueeBaseSelection = marqueeAdditive ? selection : std::vector<CellRef>();
        if (!marqueeAdditive)
            selection.clear();
        marqueeStart = e.position;
        marqueeRect = juce::Rectangle<float>(e.position.x, e.position.y, 0.0f, 0.0f);
        dragMode = DragMode::marquee;
        repaint();
    }

    void selectionMouseDrag(const juce::MouseEvent& e)
    {
        if (dragMode == DragMode::marquee)
        {
            const float left = juce::jmin(marqueeStart.x, e.position.x);
            const float top = juce::jmin(marqueeStart.y, e.position.y);
            const float right = juce::jmax(marqueeStart.x, e.position.x);
            const float bottom = juce::jmax(marqueeStart.y, e.position.y);
            marqueeRect = juce::Rectangle<float>(left, top, right - left, bottom - top);
            updateMarqueeSelection();
            repaint();
            return;
        }

        if (dragMode == DragMode::moveSelection)
        {
            int row = -1;
            int step = -1;
            bool secondHalf = false;
            if (!displayCellAt(e.position, row, step, secondHalf))
                return;

            moveDeltaSteps = step - moveAnchorStep;
            const int anchorIndex = visibleRowIndexForCanonical(moveAnchorRow);
            const int currentIndex = visibleRowIndexForCanonical(row);
            moveDeltaRows = (anchorIndex >= 0 && currentIndex >= 0)
                ? currentIndex - anchorIndex : 0;
            repaint();
        }
    }

    void selectionMouseUp()
    {
        if (dragMode == DragMode::moveSelection
            && (moveDeltaSteps != 0 || moveDeltaRows != 0))
        {
            if (duplicateDrag)
                duplicateSelectionAt(moveDeltaSteps, moveDeltaRows);
            else
                moveSelection(moveDeltaSteps, moveDeltaRows);
        }

        dragMode = DragMode::none;
        marqueeRect = {};
        moveDeltaSteps = 0;
        moveDeltaRows = 0;
        duplicateDrag = false;
        repaint();
    }

    void updateMarqueeSelection()
    {
        selection = marqueeBaseSelection;
        if (marqueeRect.isEmpty())
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int numSteps = layer->getNumSteps();

        for (const auto& item : layout)
        {
            if (item.header)
                continue;

            const int storageRow = storageRowForCanonical(item.canonicalRow, canonicalRows.size());
            const float cy = static_cast<float>(item.y) + item.height * 0.5f;
            for (int step = 0; step < numSteps; ++step)
            {
                if (layer->getProb(storageRow, step, pattern) < 0)
                    continue;

                float cx = xForStorageStepF(static_cast<float>(step)) + storageStepWidth() * 0.5f;
                if (detail32Active() && layer->getLength(storageRow, step, pattern) >= 0)
                {
                    cx = xForStorageStepF(static_cast<float>(step))
                       + storageStepWidth() * (layer->getOffset(storageRow, step, pattern) == 50
                           ? 0.75f : 0.25f);
                }

                if (marqueeRect.contains(cx, cy))
                    addSelection(item.canonicalRow, step);
            }
        }
    }

    void drawMovePreview(juce::Graphics& g) const
    {
        const bool valid = selectionTargetsAreValid(moveDeltaSteps, moveDeltaRows, duplicateDrag);
        if (!valid)
            g.setColour(c(warm).withAlpha(0.16f));
        else if (duplicateDrag)
            g.setColour(c(warm).withAlpha(0.22f));
        else
            g.setColour(c(accent).withAlpha(0.18f));

        for (const auto& ref : selection)
        {
            const int targetRow = shiftedCanonicalRow(ref.row, moveDeltaRows);
            const int targetStep = ref.step + moveDeltaSteps;
            const int y = rowY(targetRow);
            if (targetRow < 0 || targetStep < 0 || y < 0)
                continue;

            const float x = xForStorageStepF(static_cast<float>(targetStep));
            g.fillRoundedRectangle(
                juce::Rectangle<float>(x + 3.0f, static_cast<float>(y + 5),
                                       juce::jmax(6.0f, storageStepWidth() - 6.0f),
                                       static_cast<float>(rowHeight - 10)),
                4.0f);
        }
    }

    void showVelocityPopup(int row, int step, bool secondHalf, int value,
                           bool pinned = false)
    {
        velocityPopupRow = row;
        velocityPopupStep = step;
        velocityPopupSecondHalf = secondHalf;
        velocityPopupValue = juce::jlimit(1, 127, value);
        velocityPopupPinned = pinned;
        velocityPopupUntilMs = juce::Time::getMillisecondCounterHiRes() + 720.0;
        repaint();
    }

    void paintVelocityPopup(juce::Graphics& g)
    {
        if (velocityPopupRow < 0 || velocityPopupStep < 0)
            return;

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (!velocityPopupPinned && now > velocityPopupUntilMs)
            return;

        const int y = rowY(velocityPopupRow);
        if (y < 0)
            return;

        float x = xForStorageStepF(static_cast<float>(velocityPopupStep))
                + storageStepWidth() * 0.5f;
        if (detail32Active())
            x = xForStorageStepF(static_cast<float>(velocityPopupStep))
              + storageStepWidth() * (velocityPopupSecondHalf ? 0.75f : 0.25f);

        const juce::String valueText(velocityPopupValue);
        const float width = 52.0f;
        const float height = 23.0f;
        const float popupY = juce::jmax(static_cast<float>(rulerHeight + 2),
                                        static_cast<float>(y) - 25.0f);
        juce::Rectangle<float> bubble(x - width * 0.5f, popupY, width, height);

        g.setColour(c(outside).withAlpha(0.96f));
        g.fillRoundedRectangle(bubble, 5.0f);
        g.setColour(c(accent).withAlpha(0.92f));
        g.drawRoundedRectangle(bubble, 5.0f, 1.2f);
        g.setColour(c(text));
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText(valueText, bubble.toNearestInt(), juce::Justification::centred, false);
    }

    bool cycleHatArticulation(int canonicalRow, int step, int direction)
    {
        if (map == nullptr || canonicalRow < 0 || canonicalRow >= canonicalRows.size())
            return false;

        static const char* hats[] = {
            "hihat.tip_tight",
            "hihat.edge_tight",
            "hihat.tip_closed",
            "hihat.edge_closed",
            "hihat.open_1",
            "hihat.open_2",
            "hihat.open_3"
        };

        const auto semanticId = canonicalRows.getReference(canonicalRow).semanticId;
        int sourceIndex = -1;
        for (int i = 0; i < static_cast<int>(std::size(hats)); ++i)
        {
            if (semanticId == hats[i])
            {
                sourceIndex = i;
                break;
            }
        }

        if (sourceIndex < 0)
            return false;

        for (int attempt = 1; attempt <= static_cast<int>(std::size(hats)); ++attempt)
        {
            const int count = static_cast<int>(std::size(hats));
            const int targetIndex = (sourceIndex + direction * attempt + count * 2) % count;
            const juce::String targetId(hats[targetIndex]);

            if (map->findArticulation(targetId) == nullptr)
                continue;

            const int targetCanonical = GgdKitMapLibrary::findCanonicalRow(canonicalRows, targetId);
            if (targetCanonical < 0 || targetCanonical == canonicalRow)
                continue;

            auto* layer = processor.mData.getUISeqData()->getLayer(0);
            const int pattern = layer->getCurrentPattern();
            const int src = storageRowForCanonical(canonicalRow, canonicalRows.size());
            const int dst = storageRowForCanonical(targetCanonical, canonicalRows.size());

            const int prob = layer->getProb(src, step, pattern);
            if (prob < 0)
                return false;

            const int vel = layer->getVel(src, step, pattern);
            const int len = layer->getLength(src, step, pattern);
            const int offs = layer->getOffset(src, step, pattern);

            layer->setProb(dst, step, static_cast<int8_t>(prob), pattern);
            layer->setVel(dst, step, static_cast<int8_t>(vel), pattern);
            layer->setLength(dst, step, static_cast<int8_t>(len), pattern);
            layer->setOffset(dst, step, static_cast<int8_t>(offs), pattern);

            layer->setLength(src, step, 0, pattern);
            layer->setProb(src, step, SEQ_PROB_OFF, pattern);
            layer->setVel(src, step, 0, pattern);
            layer->setOffset(src, step, 0, pattern);

            if (isSelected(canonicalRow, step))
            {
                removeSelection(canonicalRow, step);
                addSelection(targetCanonical, step);
            }

            publishChange();
            repaint();
            return true;
        }

        return false;
    }

    void publishChange()
    {
        if (publishCallback)
            publishCallback();
        else
        {
            processor.mData.swap();
            processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
        }
    }
};

GgdDrumEditor::GgdDrumEditor(SeqAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    configureLookAndFeel();
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);

    maps = GgdKitMapLibrary::loadBuiltInMaps();
    canonicalRows = GgdKitMapLibrary::buildCanonicalRows(maps);

    grid = std::make_unique<GgdDrumGrid>(
        processor,
        canonicalRows,
        [this] { performUndo(); },
        [this] { performRedo(); },
        [this] { publishModelChange(); },
        [this](float scale) { refreshZoomControls(scale); },
        [this](GgdDrumGrid::ToolMode mode)
        {
            const bool draw = mode == GgdDrumGrid::ToolMode::draw;
            drawModeButton.setToggleState(draw, juce::dontSendNotification);
            selectModeButton.setToggleState(!draw, juce::dontSendNotification);
            updateContextStrip();
        });
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

    productLabel.setText("STOCHAS GGD", juce::dontSendNotification);
    productLabel.setFont(juce::Font(17.0f, juce::Font::bold));
    productLabel.setColour(juce::Label::textColourId, c(text));
    addAndMakeVisible(productLabel);

    transportStatus.setText("READY", juce::dontSendNotification);
    transportStatus.setFont(juce::Font(10.5f, juce::Font::bold));
    transportStatus.setJustificationType(juce::Justification::centred);
    transportStatus.setColour(juce::Label::textColourId, c(muted));
    transportStatus.setColour(juce::Label::backgroundColourId, c(panelRaised));
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

    patternSelector.setTooltip("Pattern slot");
    patternSelector.setMouseClickGrabsKeyboardFocus(false);
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
        resetHistoryForCurrentPattern(false);
        grid->repaint();
    };
    addAndMakeVisible(patternSelector);


    importMidiButton.setTooltip("Import an official GGD Groove Player .mid/.midi file using the decoded kit translation table");
    importMidiButton.setWantsKeyboardFocus(false);
    importMidiButton.onClick = [this] { chooseMidiFile(); };
    addAndMakeVisible(importMidiButton);

    drawModeButton.setClickingTogglesState(true);
    drawModeButton.setRadioGroupId(2001);
    drawModeButton.setToggleState(true, juce::dontSendNotification);
    drawModeButton.setTooltip("Draw tool (D)");
    drawModeButton.setMouseClickGrabsKeyboardFocus(false);
    drawModeButton.onClick = [this]
    {
        if (grid != nullptr)
        {
            grid->setToolMode(GgdDrumGrid::ToolMode::draw);
            grid->grabKeyboardFocus();
        }
    };
    addAndMakeVisible(drawModeButton);

    selectModeButton.setClickingTogglesState(true);
    selectModeButton.setRadioGroupId(2001);
    selectModeButton.setTooltip("Select/edit tool (S)");
    selectModeButton.setMouseClickGrabsKeyboardFocus(false);
    selectModeButton.onClick = [this]
    {
        if (grid != nullptr)
        {
            grid->setToolMode(GgdDrumGrid::ToolMode::select);
            grid->grabKeyboardFocus();
        }
    };
    addAndMakeVisible(selectModeButton);

    meterLabel.setText("METER", juce::dontSendNotification);
    meterLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    meterLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(meterLabel);

    for (int numerator = 1; numerator <= SEQ_MAX_STEPS_PER_MEASURE; ++numerator)
        numeratorSelector.addItem(juce::String(numerator), numerator);
    numeratorSelector.setTooltip("Time signature numerator");
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
    meterSlash.setFont(juce::Font(15.0f, juce::Font::bold));
    meterSlash.setColour(juce::Label::textColourId, c(muted));
    meterSlash.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(meterSlash);

    denominatorSelector.addItem("4", 1);
    denominatorSelector.addItem("8", 2);
    denominatorSelector.addItem("16", 3);
    denominatorSelector.setTooltip("Time signature denominator");
    denominatorSelector.setMouseClickGrabsKeyboardFocus(false);
    denominatorSelector.onChange = [this]
    {
        const int id = denominatorSelector.getSelectedId();
        if (id > 0)
        {
            static constexpr int values[] = { 4, 8, 16 };
            timeSigDenominator = values[id - 1];
            applyPatternGeometry();
        }
    };
    addAndMakeVisible(denominatorSelector);

    barsLabel.setText("LENGTH", juce::dontSendNotification);
    barsLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    barsLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(barsLabel);

    barsEditor.setMultiLine(false);
    barsEditor.setReturnKeyStartsNewLine(false);
    barsEditor.setInputRestrictions(4, "0123456789");
    barsEditor.setJustification(juce::Justification::centred);
    barsEditor.setTooltip("Pattern length in bars. The maximum is derived from the current meter and the 1024-step engine capacity.");
    barsEditor.onReturnKey = [this]
    {
        commitBarCountEditor();
        barsEditor.giveAwayKeyboardFocus();
        if (grid != nullptr)
            grid->grabKeyboardFocus();
    };
    barsEditor.onFocusLost = [this] { commitBarCountEditor(); };
    addAndMakeVisible(barsEditor);

    zoomLabel.setText("ZOOM", juce::dontSendNotification);
    zoomLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    zoomLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(zoomLabel);

    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange(0.5, 4.0, 0.25);
    zoomSlider.setSkewFactorFromMidPoint(1.25);
    zoomSlider.setValue(defaultZoomScale, juce::dontSendNotification);
    zoomSlider.setDoubleClickReturnValue(true, defaultZoomScale);
    zoomSlider.setMouseClickGrabsKeyboardFocus(false);
    zoomSlider.setTooltip("Absolute timeline zoom in 25% steps. 125% is the default; 350%+ exposes the 1/32 detail grid.");
    zoomSlider.onValueChange = [this]
    {
        if (grid != nullptr)
            grid->setZoomScale(static_cast<float>(zoomSlider.getValue()));
    };
    addAndMakeVisible(zoomSlider);

    zoomValueLabel.setText("125% | 1/16", juce::dontSendNotification);
    zoomValueLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    zoomValueLabel.setColour(juce::Label::textColourId, c(text));
    zoomValueLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(zoomValueLabel);

    fitZoomButton.setButtonText("125%");
    fitZoomButton.setTooltip("Reset timeline magnification to the default working scale");
    fitZoomButton.setMouseClickGrabsKeyboardFocus(false);
    fitZoomButton.onClick = [this]
    {
        if (grid != nullptr)
        {
            grid->resetZoom();
            grid->grabKeyboardFocus();
        }
    };
    addAndMakeVisible(fitZoomButton);

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
        refreshPatternSelectorLabels();
        if (grid != nullptr)
            grid->grabKeyboardFocus();
    };
    patternName.onFocusLost = [this]
    {
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        if (patternName.getText() != juce::String(layer->getPatternName()))
        {
            layer->setPatternName(patternName.getText().toRawUTF8());
            publishModelChange();
            refreshPatternSelectorLabels();
        }
    };
    addAndMakeVisible(patternName);

    patternActionsButton.setTooltip("Pattern slot operations");
    patternActionsButton.setMouseClickGrabsKeyboardFocus(false);
    patternActionsButton.onClick = [this] { showPatternActions(); };
    addAndMakeVisible(patternActionsButton);

    undoButton.setTooltip("Undo through the editor history. U is the plugin-local shortcut; Bitwig may intercept Ctrl+Z.");
    undoButton.setMouseClickGrabsKeyboardFocus(false);
    undoButton.onClick = [this]
    {
        performUndo();
        if (grid != nullptr)
            grid->grabKeyboardFocus();
    };
    addAndMakeVisible(undoButton);

    redoButton.setTooltip("Redo the most recently undone edit. Y is the plugin-local shortcut.");
    redoButton.setMouseClickGrabsKeyboardFocus(false);
    redoButton.onClick = [this]
    {
        performRedo();
        if (grid != nullptr)
            grid->grabKeyboardFocus();
    };
    addAndMakeVisible(redoButton);

    clearButton.setTooltip("Clear the current pattern");
    clearButton.setMouseClickGrabsKeyboardFocus(false);
    clearButton.onClick = [this]
    {
        clearCurrentPattern();
        if (grid != nullptr)
            grid->grabKeyboardFocus();
    };
    addAndMakeVisible(clearButton);

    selectionStatusLabel.setFont(juce::Font(10.5f, juce::Font::bold));
    selectionStatusLabel.setColour(juce::Label::textColourId, c(text));
    addAndMakeVisible(selectionStatusLabel);

    auto prepareContextButton = [this](juce::TextButton& button)
    {
        button.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(button);
    };
    prepareContextButton(copyButton);
    prepareContextButton(pasteButton);
    prepareContextButton(velocityDownButton);
    prepareContextButton(velocityUpButton);
    prepareContextButton(timingEarlierButton);
    prepareContextButton(timingLaterButton);
    prepareContextButton(humanizeButton);
    prepareContextButton(deleteSelectionButton);

    copyButton.onClick = [this] { if (grid) grid->copySelectionToClipboard(); };
    pasteButton.onClick = [this] { if (grid) grid->pasteClipboard(); };
    velocityDownButton.onClick = [this] { if (grid) grid->adjustSelectedVelocityBy(-5); };
    velocityUpButton.onClick = [this] { if (grid) grid->adjustSelectedVelocityBy(5); };
    timingEarlierButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(-5); };
    timingLaterButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(5); };
    humanizeButton.onClick = [this] { if (grid) grid->humanizeSelected(); };
    deleteSelectionButton.onClick = [this] { if (grid) grid->deleteSelected(); };

    hintLabel.setText(
        "Draw: click/drag | right-drag erase | Shift-drag velocity | Alt-drag timing",
        juce::dontSendNotification);
    hintLabel.setFont(juce::Font(10.0f));
    hintLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(hintLabel);

    initialiseDrumState();
    refreshControlsFromModel();
    if (!maps.isEmpty())
        grid->setMap(&maps.getReference(activeMapIndex));

    initialiseCleanPatternFingerprints();
    resetHistoryForCurrentPattern(false);
    updateContextStrip();

    setResizable(true, true);
    setResizeLimits(1080, 600, 2200, 1500);
    setSize(1420, 820);
    startTimerHz(60);

    juce::MessageManager::callAsync(
        [safe = juce::Component::SafePointer<GgdDrumEditor>(this)]
        {
            if (safe != nullptr && safe->grid != nullptr)
                safe->grid->grabKeyboardFocus();
        });
}

GgdDrumEditor::~GgdDrumEditor()
{
    stopTimer();
    gridViewport.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
}

bool GgdDrumEditor::keyPressed(const juce::KeyPress& key)
{
    return grid != nullptr && grid->keyPressed(key);
}

void GgdDrumEditor::configureLookAndFeel()
{
    lookAndFeel.setColour(juce::ComboBox::backgroundColourId, c(panelRaised));
    lookAndFeel.setColour(juce::ComboBox::textColourId, c(text));
    lookAndFeel.setColour(juce::ComboBox::outlineColourId, c(border));
    lookAndFeel.setColour(juce::ComboBox::arrowColourId, c(muted));
    lookAndFeel.setColour(juce::PopupMenu::backgroundColourId, c(panelRaised));
    lookAndFeel.setColour(juce::PopupMenu::textColourId, c(text));
    lookAndFeel.setColour(juce::PopupMenu::highlightedBackgroundColourId, c(accent2));
    lookAndFeel.setColour(juce::PopupMenu::highlightedTextColourId, c(text));
    lookAndFeel.setColour(juce::TextEditor::backgroundColourId, c(panelRaised));
    lookAndFeel.setColour(juce::TextEditor::textColourId, c(text));
    lookAndFeel.setColour(juce::TextEditor::outlineColourId, c(border));
    lookAndFeel.setColour(juce::TextEditor::focusedOutlineColourId, c(accent2));
    lookAndFeel.setColour(juce::TextButton::buttonColourId, c(panelRaised));
    lookAndFeel.setColour(juce::TextButton::buttonOnColourId, c(accent2));
    lookAndFeel.setColour(juce::TextButton::textColourOffId, c(text));
    lookAndFeel.setColour(juce::TextButton::textColourOnId, c(text));
    lookAndFeel.setColour(juce::ScrollBar::thumbColourId, c(border).brighter(0.30f));
    lookAndFeel.setColour(juce::ScrollBar::trackColourId, c(bg));
    lookAndFeel.setColour(juce::Slider::trackColourId, c(accent2));
    lookAndFeel.setColour(juce::Slider::backgroundColourId, c(border));
    lookAndFeel.setColour(juce::Slider::thumbColourId, c(accent));
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
}

void GgdDrumEditor::resized()
{
    const int pad = 12;
    const int gap = 7;
    const int editorWidth = juce::jmax(680, getWidth() - browserWidth);

    auto first = juce::Rectangle<int>(pad, 7, editorWidth - pad * 2, 43);
    productLabel.setBounds(first.removeFromLeft(142));
    first.removeFromLeft(3);
    transportStatus.setBounds(first.removeFromLeft(72).reduced(0, 8));
    first.removeFromLeft(gap + 2);

    const int kitWidth = juce::jlimit(166, 220, editorWidth / 5);
    kitSelector.setBounds(first.removeFromLeft(kitWidth).reduced(0, 5));
    first.removeFromLeft(gap);
    patternSelector.setBounds(first.removeFromLeft(142).reduced(0, 5));
    first.removeFromLeft(gap);
    patternActionsButton.setBounds(first.removeFromLeft(76).reduced(0, 5));
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

    barsLabel.setBounds(second.removeFromLeft(45));
    barsEditor.setBounds(second.removeFromLeft(58).reduced(0, 5));
    second.removeFromLeft(8);

    zoomLabel.setBounds(second.removeFromLeft(36));
    fitZoomButton.setBounds(second.removeFromRight(54).reduced(0, 5));
    second.removeFromRight(4);
    zoomValueLabel.setBounds(second.removeFromRight(82));
    second.removeFromRight(5);
    zoomSlider.setBounds(second.reduced(0, 7));

    const int contentHeight = getHeight() - topAreaHeight;
    gridViewport.setBounds(0, topAreaHeight, editorWidth,
                           contentHeight - bottomAreaHeight);
    if (libraryBrowser)
        libraryBrowser->setBounds(editorWidth, topAreaHeight,
                                  getWidth() - editorWidth, contentHeight);

    auto strip = juce::Rectangle<int>(8, getHeight() - bottomAreaHeight + 2,
                                      editorWidth - 16, bottomAreaHeight - 4);
    selectionStatusLabel.setBounds(strip.removeFromLeft(110));
    strip.removeFromLeft(5);

    auto place = [&](juce::TextButton& button, int width)
    {
        button.setBounds(strip.removeFromLeft(width).reduced(0, 4));
        strip.removeFromLeft(4);
    };
    place(copyButton, 48);
    place(pasteButton, 50);
    place(velocityDownButton, 50);
    place(velocityUpButton, 50);
    place(timingEarlierButton, 58);
    place(timingLaterButton, 52);
    place(humanizeButton, 68);
    place(deleteSelectionButton, 54);
    hintLabel.setBounds(strip);

    if (grid != nullptr)
        grid->refreshSize();
}

void GgdDrumEditor::timerCallback()
{
    const int pos = processor.mNotifier.getPlayPosition(0);
    grid->setPlayPosition(pos);

    const bool textEntryActive =
        patternName.hasKeyboardFocus(true) || barsEditor.hasKeyboardFocus(true);
    const bool shortcutSurfaceActive =
        !textEntryActive && (grid->hasKeyboardFocus(true) || isMouseOverOrDragging(true));
    grid->pollFallbackShortcuts(shortcutSurfaceActive);

    if (pos >= 0)
    {
        transportStatus.setColour(juce::Label::textColourId, c(accent));
        transportStatus.setText("PLAY", juce::dontSendNotification);
    }
    else
    {
        transportStatus.setColour(juce::Label::textColourId, c(muted));
        transportStatus.setText("READY", juce::dontSendNotification);
    }

    if (processor.mNotifier.doesUINeedUpdate())
    {
        refreshControlsFromModel();
        grid->refreshSize();
    }

    updateContextStrip();
    grid->repaint();
}

void GgdDrumEditor::initialiseDrumState()
{
    if (maps.isEmpty() || canonicalRows.isEmpty())
        return;

    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const juce::String oldLayerName(layer->getLayerName());
    const bool legacyConfigured = oldLayerName.startsWith("GGD:");
    const bool configuredV2 = oldLayerName.startsWith("GGD2:");
    const bool configuredV3 = oldLayerName.startsWith("GGD3:");
    const bool alreadyConfigured = legacyConfigured || configuredV2 || configuredV3;

    int preservedBars = 1;

    if (!alreadyConfigured)
    {
        seq->clearLayer(0);
        layer = seq->getLayer(0);
        layer->setNumSteps(16);
        layer->setMidiChannel(1);
        layer->setCurrentPattern(0);

        for (int i = 1; i < SEQ_MAX_LAYERS; ++i)
            seq->getLayer(i)->setMuted(true);
    }
    else
    {
        if (legacyConfigured)
            migrateLegacyGgdRows(layer, canonicalRows.size());

        if (configuredV3)
            parseMeterFromLayerName(oldLayerName, timeSigNumerator, timeSigDenominator);

        const int oldStepsPerBar = stepsPerBarForMeter(timeSigNumerator, timeSigDenominator);
        const int maxBarsForMeter = juce::jmax(1, SEQ_MAX_STEPS / oldStepsPerBar);
        preservedBars = juce::jlimit(
            1, maxBarsForMeter,
            juce::jmax(1, (layer->getNumSteps() + oldStepsPerBar - 1) / oldStepsPerBar));
    }

    activeBars = preservedBars;
    activeMapIndex = alreadyConfigured ? mapIndexFromLayerName(oldLayerName) : 0;
    activeMapIndex = juce::jlimit(0, maps.size() - 1, activeMapIndex);

    seq->setMidiPassthru(SEQ_MIDI_PASSTHRU_ALL);
    seq->setMidiRespond(SEQ_MIDI_RESPOND_NO);

    layer->setNoteSource(true);
    layer->setMonoMode(false);
    layer->setMaxRows(activeStorageRowCount(canonicalRows.size()));
    layer->setMaxPoly(juce::jmin(SEQ_DEFAULT_MAX_POLY, layer->getMaxRows()));

    applyPatternGeometry(false);
    applyActiveMapBindings(false);
    publishModelChange(false);
}

void GgdDrumEditor::applyActiveMapBindings(bool publish)
{
    if (maps.isEmpty())
        return;

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const auto& map = maps.getReference(activeMapIndex);

    const int rowCount = activeStorageRowCount(canonicalRows.size());
    const int semanticCount = juce::jmin(canonicalRows.size(), rowCount);
    layer->setMaxRows(rowCount);
    layer->setNoteSource(true);

    for (int canonicalRow = 0; canonicalRow < semanticCount; ++canonicalRow)
    {
        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
        const auto& canonical = canonicalRows.getReference(canonicalRow);
        const auto* articulation = map.findArticulation(canonical.semanticId);
        const auto* binding = articulation != nullptr ? articulation->primaryNoteBinding() : nullptr;

        const int midi = binding != nullptr ? binding->midi : SEQ_NOTE_OFF;
        layer->setNote(storageRow, static_cast<int8_t>(midi), true);

        const auto displayName =
            (articulation != nullptr ? articulation->label : canonical.defaultLabel)
                .substring(0, SEQ_MAX_NOTELABEL_LEN - 1);
        layer->setNoteName(storageRow, displayName.toRawUTF8());
    }

    updatePersistenceTag();
    if (publish)
        publishModelChange(false);
}

void GgdDrumEditor::applyPatternGeometry(bool publish)
{
    if (maps.isEmpty())
        return;

    timeSigNumerator = juce::jlimit(1, SEQ_MAX_STEPS_PER_MEASURE, timeSigNumerator);
    if (timeSigDenominator != 4 && timeSigDenominator != 8 && timeSigDenominator != 16)
        timeSigDenominator = 4;

    const int stepsPerBar = stepsPerBarForMeter(timeSigNumerator, timeSigDenominator);
    const int maxBars = juce::jmax(1, SEQ_MAX_STEPS / stepsPerBar);
    activeBars = juce::jlimit(1, maxBars, activeBars);

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    layer->setStepsPerMeasure(timeSigNumerator);
    layer->setClockDivider(clockDividerForMeter(timeSigDenominator));
    layer->setNumSteps(activeBars * stepsPerBar);
    updatePersistenceTag();

    barsEditor.setText(juce::String(activeBars), false);
    barsEditor.setTooltip(
        "Pattern length in bars. Current meter allows 1-" + juce::String(maxBars)
        + " bars within the 1024-step engine capacity.");
    if (grid != nullptr)
    {
        grid->setMeter(timeSigNumerator, timeSigDenominator);
        grid->refreshSize();
    }

    if (publish)
        publishModelChange();
}

void GgdDrumEditor::refreshControlsFromModel()
{
    if (maps.isEmpty())
        return;

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const juce::String layerName(layer->getLayerName());

    const int mapFromState = mapIndexFromLayerName(layerName);
    if (mapFromState >= 0 && mapFromState < maps.size())
        activeMapIndex = mapFromState;

    parseMeterFromLayerName(layerName, timeSigNumerator, timeSigDenominator);

    const int stepsPerBar = stepsPerBarForMeter(timeSigNumerator, timeSigDenominator);
    const int maxBars = juce::jmax(1, SEQ_MAX_STEPS / stepsPerBar);
    activeBars = juce::jlimit(
        1, maxBars,
        juce::jmax(1, (layer->getNumSteps() + stepsPerBar - 1) / stepsPerBar));

    kitSelector.setSelectedId(activeMapIndex + 1, juce::dontSendNotification);
    patternSelector.setSelectedId(layer->getCurrentPattern() + 1,
                                  juce::dontSendNotification);
    patternName.setText(layer->getPatternName(), false);
    numeratorSelector.setSelectedId(timeSigNumerator, juce::dontSendNotification);

    const int denominatorId = timeSigDenominator == 4 ? 1 : timeSigDenominator == 8 ? 2 : 3;
    denominatorSelector.setSelectedId(denominatorId, juce::dontSendNotification);

    barsEditor.setText(juce::String(activeBars), false);
    barsEditor.setTooltip(
        "Pattern length in bars. Current meter allows 1-" + juce::String(maxBars)
        + " bars within the 1024-step engine capacity.");
    refreshPatternSelectorLabels();

    grid->setMeter(timeSigNumerator, timeSigDenominator);
    grid->setMap(&maps.getReference(activeMapIndex));
    refreshZoomControls(grid->getZoomScale());
}

void GgdDrumEditor::commitBarCountEditor()
{
    const int stepsPerBar = stepsPerBarForMeter(timeSigNumerator, timeSigDenominator);
    const int maxBars = juce::jmax(1, SEQ_MAX_STEPS / stepsPerBar);
    const int requested = barsEditor.getText().getIntValue();

    if (requested <= 0)
    {
        barsEditor.setText(juce::String(activeBars), false);
        return;
    }

    activeBars = juce::jlimit(1, maxBars, requested);
    barsEditor.setText(juce::String(activeBars), false);
    applyPatternGeometry();
}

void GgdDrumEditor::refreshPatternSelectorLabels()
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);

    for (int i = 0; i < SEQ_MAX_PATTERNS; ++i)
    {
        const juce::String name(layer->getPatternName(i));
        const bool custom = name.isNotEmpty() && name != juce::String(SEQ_DEFAULT_PAT_NAME);
        patternSelector.changeItemText(
            i + 1,
            custom ? juce::String(i + 1) + ": " + name
                   : "Pattern " + juce::String(i + 1));
    }
}

void GgdDrumEditor::refreshZoomControls(float scale)
{
    zoomSlider.setValue(scale, juce::dontSendNotification);
    const int percent = static_cast<int>(std::round(scale * 100.0f));
    const juce::String gridText = scale >= detail32ZoomThreshold ? "1/32" : "1/16";
    zoomValueLabel.setText(juce::String(percent) + "% | " + gridText,
                           juce::dontSendNotification);
}

void GgdDrumEditor::setActiveMap(int index)
{
    if (index < 0 || index >= maps.size() || index == activeMapIndex)
        return;

    activeMapIndex = index;
    applyActiveMapBindings(true);
    grid->setMap(&maps.getReference(activeMapIndex));
}

void GgdDrumEditor::performUndo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastUndoMs < 110.0 || undoHistory.empty())
        return;
    lastUndoMs = now;

    redoHistory.push_back(captureCurrentPattern());
    if (redoHistory.size() > maxHistoryDepth)
        redoHistory.pop_front();

    auto target = undoHistory.back();
    undoHistory.pop_back();
    restoringHistory = true;
    restorePatternSnapshot(target, false);
    restoringHistory = false;
    lastCommittedSnapshot = target;

    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();
}

void GgdDrumEditor::performRedo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastRedoMs < 110.0 || redoHistory.empty())
        return;
    lastRedoMs = now;

    undoHistory.push_back(captureCurrentPattern());
    if (undoHistory.size() > maxHistoryDepth)
        undoHistory.pop_front();

    auto target = redoHistory.back();
    redoHistory.pop_back();
    restoringHistory = true;
    restorePatternSnapshot(target, false);
    restoringHistory = false;
    lastCommittedSnapshot = target;

    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();
}

void GgdDrumEditor::chooseMidiFile()
{
    midiFileChooser = std::make_unique<juce::FileChooser>(
        "Import drum groove MIDI",
        juce::File(),
        "*.mid;*.midi");

    const int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles;
    auto safeThis = juce::Component::SafePointer<GgdDrumEditor>(this);
    midiFileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
    {
        if (auto* self = safeThis.getComponent())
        {
            const auto file = chooser.getResult();
            if (file.existsAsFile())
                self->importMidiFile(file);
        }
    });
}

void GgdDrumEditor::importMidiFile(const juce::File& file)
{
    if (maps.isEmpty() || activeMapIndex < 0 || activeMapIndex >= maps.size())
        return;

    auto result = GgdMidiImporter::parseFile(
        file, maps.getReference(activeMapIndex), canonicalRows, SEQ_MAX_STEPS);

    if (!result.ok)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("MIDI import failed")
                .withMessage(result.error)
                .withButton("OK"),
            nullptr);
        return;
    }

    auto safeThis = juce::Component::SafePointer<GgdDrumEditor>(this);
    requestPatternReplacement(
        "Load groove '" + file.getFileName() + "'?",
        [safeThis, result]() mutable
        {
            if (auto* self = safeThis.getComponent())
                self->applyMidiImport(result);
        });
}

void GgdDrumEditor::applyMidiImport(const GgdMidiImportResult& result)
{
    timeSigNumerator = result.numerator;
    timeSigDenominator = result.denominator;
    activeBars = result.bars;
    applyPatternGeometry(false);

    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const int pattern = layer->getCurrentPattern();
    seq->clearPattern(0, pattern);

    for (const auto& cell : result.cells)
    {
        if (cell.canonicalRow < 0 || cell.canonicalRow >= canonicalRows.size()
            || cell.step < 0 || cell.step >= layer->getNumSteps())
            continue;

        const int storageRow = storageRowForCanonical(cell.canonicalRow, canonicalRows.size());
        layer->setProb(storageRow, cell.step, SEQ_PROB_ON, pattern);
        layer->setVel(storageRow, cell.step,
                      static_cast<int8_t>(juce::jlimit(1, 127, cell.velocity)), pattern);
        layer->setLength(storageRow, cell.step,
                         static_cast<int8_t>(cell.retriggerLength), pattern);
        layer->setOffset(storageRow, cell.step,
                         static_cast<int8_t>(juce::jlimit(-50, 50, cell.offset)), pattern);
    }

    const auto importedName = result.fileName.substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(importedName.toRawUTF8(), pattern);
    publishModelChange();
    markCurrentPatternClean();
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();

    const auto summary = result.summary();
    hintLabel.setText(summary.substring(0, 220), juce::dontSendNotification);
    hintLabel.setTooltip(summary);

    if (result.unresolvedNotes > 0 || result.fallbackNotes > 0
        || result.collisions > 0 || result.truncatedNotes > 0)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("MIDI import report")
                .withMessage(summary)
                .withButton("OK"),
            nullptr);
    }
}

GgdPatternSnapshot GgdDrumEditor::capturePattern(int pattern) const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    return GgdPatternFile::capture(*layer, canonicalRows, pattern,
                                   timeSigNumerator, timeSigDenominator, activeBars);
}

GgdPatternSnapshot GgdDrumEditor::captureCurrentPattern() const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    return capturePattern(layer->getCurrentPattern());
}

void GgdDrumEditor::recordCommittedPatternEdit()
{
    const auto current = captureCurrentPattern();
    if (restoringHistory)
    {
        lastCommittedSnapshot = current;
        return;
    }

    if (lastCommittedSnapshot.has_value()
        && GgdPatternFile::fingerprint(*lastCommittedSnapshot)
            != GgdPatternFile::fingerprint(current))
    {
        undoHistory.push_back(*lastCommittedSnapshot);
        if (undoHistory.size() > maxHistoryDepth)
            undoHistory.pop_front();
        redoHistory.clear();
    }
    lastCommittedSnapshot = current;
}

void GgdDrumEditor::resetHistoryForCurrentPattern(bool markClean)
{
    undoHistory.clear();
    redoHistory.clear();
    lastCommittedSnapshot = captureCurrentPattern();
    if (markClean)
        markCurrentPatternClean();
}

void GgdDrumEditor::initialiseCleanPatternFingerprints()
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    for (int pattern = 0; pattern < SEQ_MAX_PATTERNS; ++pattern)
    {
        cleanPatternFingerprints[static_cast<size_t>(pattern)] =
            GgdPatternFile::fingerprint(capturePattern(pattern));
        cleanPatternFingerprintValid[static_cast<size_t>(pattern)] = true;
    }
    lastCommittedSnapshot = capturePattern(layer->getCurrentPattern());
}

void GgdDrumEditor::markCurrentPatternClean()
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const int pattern = layer->getCurrentPattern();
    cleanPatternFingerprints[static_cast<size_t>(pattern)] =
        GgdPatternFile::fingerprint(captureCurrentPattern());
    cleanPatternFingerprintValid[static_cast<size_t>(pattern)] = true;
}

bool GgdDrumEditor::currentPatternHasChanges() const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const int pattern = layer->getCurrentPattern();
    if (!cleanPatternFingerprintValid[static_cast<size_t>(pattern)])
        return true;
    return cleanPatternFingerprints[static_cast<size_t>(pattern)]
        != GgdPatternFile::fingerprint(captureCurrentPattern());
}

void GgdDrumEditor::restorePatternSnapshot(const GgdPatternSnapshot& snapshot, bool publish)
{
    timeSigNumerator = snapshot.numerator;
    timeSigDenominator = snapshot.denominator;
    const int stepsPerBar = stepsPerBarForMeter(timeSigNumerator, timeSigDenominator);
    activeBars = juce::jlimit(1, juce::jmax(1, SEQ_MAX_STEPS / stepsPerBar), snapshot.bars);
    applyPatternGeometry(false);

    auto* seq = processor.mData.getUISeqData();
    const int pattern = seq->getLayer(0)->getCurrentPattern();
    GgdPatternFile::restore(snapshot, *seq, canonicalRows, 0, pattern);
    updatePersistenceTag();

    if (publish)
        publishModelChange();
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
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Replace edited pattern?")
            .withMessage(description + " The current pattern has changes since it was loaded or saved.")
            .withButton("Replace")
            .withButton("Cancel"),
        [safe, replacement = std::move(replacement)](int result) mutable
        {
            if (result == 1 && safe != nullptr)
                replacement();
        });
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
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Pattern load failed")
                .withMessage(error)
                .withButton("OK"), nullptr);
        return;
    }

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    requestPatternReplacement(
        "Load pattern '" + file.getFileNameWithoutExtension() + "'?",
        [safe, file, snapshot]() mutable
        {
            if (auto* self = safe.getComponent())
                self->applyPatternFile(file, snapshot);
        });
}

void GgdDrumEditor::applyPatternFile(const juce::File&, const GgdPatternSnapshot& snapshot)
{
    restorePatternSnapshot(snapshot, true);
    markCurrentPatternClean();
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();
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
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("Choose a pattern folder")
                .withMessage("Open the Patterns tab and choose a library folder first.")
                .withButton("OK"), nullptr);
        return;
    }

    auto snapshot = captureCurrentPattern();
    juce::String safeName = snapshot.name.trim();
    if (safeName.isEmpty() || safeName == SEQ_DEFAULT_PAT_NAME)
        safeName = "Pattern";
    safeName = juce::File::createLegalFileName(safeName);
    const auto suggested = root.getChildFile(safeName + GgdPatternFile::extension);

    patternSaveChooser = std::make_unique<juce::FileChooser>(
        "Save Stochas GGD pattern", suggested, "*" + juce::String(GgdPatternFile::extension));
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    patternSaveChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe, snapshot](const juce::FileChooser& chooser) mutable
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
                        juce::MessageBoxOptions()
                            .withIconType(juce::MessageBoxIconType::WarningIcon)
                            .withTitle("Pattern save failed")
                            .withMessage(error)
                            .withButton("OK"), nullptr);
                    return;
                }
                self->markCurrentPatternClean();
                if (self->libraryBrowser)
                    self->libraryBrowser->refresh();
            }
        });
}

bool GgdDrumEditor::currentPatternSlotIsEmpty(int pattern) const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    for (int row = 0; row < canonicalRows.size(); ++row)
    {
        const int storageRow = storageRowForCanonical(row, canonicalRows.size());
        for (int step = 0; step < layer->getNumSteps(); ++step)
            if (layer->getProb(storageRow, step, pattern) >= 0)
                return false;
    }
    return true;
}

void GgdDrumEditor::duplicateCurrentPatternSlot()
{
    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const int source = layer->getCurrentPattern();
    int target = -1;
    for (int i = 1; i < SEQ_MAX_PATTERNS; ++i)
    {
        const int candidate = (source + i) % SEQ_MAX_PATTERNS;
        if (currentPatternSlotIsEmpty(candidate))
        {
            target = candidate;
            break;
        }
    }

    if (target < 0)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("No empty pattern slot")
                .withMessage("All eight internal pattern slots contain notes. Nothing was overwritten.")
                .withButton("OK"), nullptr);
        return;
    }

    seq->copyPatternData(0, target, 0, source);
    juce::String copyName(layer->getPatternName(source));
    if (copyName.isEmpty() || copyName == SEQ_DEFAULT_PAT_NAME)
        copyName = "Pattern " + juce::String(source + 1);
    copyName = (copyName + " copy").substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(copyName.toRawUTF8(), target);
    layer->setCurrentPattern(target);
    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    refreshControlsFromModel();
    markCurrentPatternClean();
    resetHistoryForCurrentPattern(false);
    grid->refreshSize();
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

void GgdDrumEditor::updateContextStrip()
{
    if (!grid)
        return;
    const bool select = grid->getToolMode() == GgdDrumGrid::ToolMode::select;
    const int selected = grid->getSelectionCount();
    selectionStatusLabel.setText(select ? juce::String(selected) + " selected" : "DRAW MODE",
                                 juce::dontSendNotification);

    copyButton.setVisible(select && selected > 0);
    pasteButton.setVisible(select && grid->hasClipboard());
    velocityDownButton.setVisible(select && selected > 0);
    velocityUpButton.setVisible(select && selected > 0);
    timingEarlierButton.setVisible(select && selected > 0);
    timingLaterButton.setVisible(select && selected > 0);
    humanizeButton.setVisible(select && selected > 0);
    deleteSelectionButton.setVisible(select && selected > 0);

    hintLabel.setVisible(!select || selected == 0);
    if (!select)
        hintLabel.setText("click/drag draw | right-drag erase | Shift-drag velocity | Alt-drag timing",
                          juce::dontSendNotification);
    else if (selected == 0)
        hintLabel.setText("click a hit or drag empty space to select | Alt-drag duplicates",
                          juce::dontSendNotification);
}

void GgdDrumEditor::publishModelChange(bool recordHistory)
{
    if (recordHistory)
        recordCommittedPatternEdit();
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
    updateContextStrip();
}

void GgdDrumEditor::updatePersistenceTag()
{
    if (maps.isEmpty())
        return;

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const auto tag = "GGD3:" + mapPersistenceToken(maps.getReference(activeMapIndex))
                   + ":" + juce::String(timeSigNumerator)
                   + "/" + juce::String(timeSigDenominator);
    layer->setLayerName(tag.toRawUTF8());
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
    if (!layerName.startsWith("GGD:")
        && !layerName.startsWith("GGD2:")
        && !layerName.startsWith("GGD3:"))
        return 0;

    auto remainder = layerName.fromFirstOccurrenceOf(":", false, false);
    const auto token = remainder.upToFirstOccurrenceOf(":", false, false).trim();

    for (int i = 0; i < maps.size(); ++i)
        if (mapPersistenceToken(maps.getReference(i)) == token)
            return i;

    return 0;
}

bool GgdDrumEditor::parseMeterFromLayerName(
    const juce::String& layerName, int& numerator, int& denominator) const
{
    if (!layerName.startsWith("GGD3:"))
        return false;

    auto remainder = layerName.fromFirstOccurrenceOf(":", false, false);
    remainder = remainder.fromFirstOccurrenceOf(":", false, false);
    const auto numeratorText = remainder.upToFirstOccurrenceOf("/", false, false);
    const auto denominatorText = remainder.fromFirstOccurrenceOf("/", false, false);

    const int n = numeratorText.getIntValue();
    const int d = denominatorText.getIntValue();

    if (n < 1 || n > SEQ_MAX_STEPS_PER_MEASURE || (d != 4 && d != 8 && d != 16))
        return false;

    numerator = n;
    denominator = d;
    return true;
}
