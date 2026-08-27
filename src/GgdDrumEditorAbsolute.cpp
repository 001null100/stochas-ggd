#include "GgdDrumEditor.h"

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

constexpr int maxUserBars = 8;

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

bool isUndoKey(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    return (mods.isCtrlDown() || mods.isCommandDown())
        && !mods.isAltDown()
        && key.getKeyCode() == 'Z';
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
    GgdDrumGrid(SeqAudioProcessor& p,
                const juce::Array<GgdCanonicalRow>& canonical,
                std::function<void()> undoFn,
                std::function<void(float)> zoomFn)
        : processor(p),
          canonicalRows(canonical),
          undoCallback(std::move(undoFn)),
          zoomCallback(std::move(zoomFn))
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void setMap(const GgdKitMap* newMap)
    {
        map = newMap;
        rebuildLayout();
        repaint();
    }

    void setMeter(int numerator, int denominator)
    {
        meterNumerator = juce::jmax(1, numerator);
        meterDenominator = juce::jmax(1, denominator);
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

    float getZoomScale() const { return zoomScale; }

    void resetZoom()
    {
        setZoomScale(1.0f);
    }

    void setZoomScale(float newScale)
    {
        auto* viewport = findParentComponentOfClass<juce::Viewport>();
        if (viewport == nullptr)
        {
            zoomScale = juce::jlimit(minZoomScale, maxZoomScale, newScale);
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

    juce::String resolutionText() const { return "1/16"; }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (isUndoKey(key) && undoCallback)
        {
            undoCallback();
            return true;
        }
        return false;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(c(outside));

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();
        const int pattern = layer->getCurrentPattern();
        const float stepWidth = storageStepWidth();
        const int snap = snapSteps();
        const int stepsPerBar = stepsPerBarForMeter(meterNumerator, meterDenominator);
        const int stepsPerBeat = juce::jmax(1, 16 / meterDenominator);
        const int viewX = currentViewX();
        const int stickyX = viewX;
        const int gridLeft = nameWidth;
        const int gridRight = static_cast<int>(std::ceil(xForStorageStepF(numSteps)));
        const int activeWidth = juce::jmax(0, gridRight - gridLeft);

        g.setColour(c(bg));
        g.fillRect(gridLeft, 0, activeWidth, getHeight());

        g.setColour(c(panel));
        g.fillRect(gridLeft, 0, activeWidth, rulerHeight);

        for (int barStart = 0; barStart < numSteps; barStart += stepsPerBar)
        {
            const int bar = barStart / stepsPerBar;
            const float x0 = xForStorageStepF(barStart);
            const float x1 = xForStorageStepF(juce::jmin(numSteps, barStart + stepsPerBar));

            if ((bar & 1) != 0)
            {
                g.setColour(c(panelRaised).withAlpha(0.25f));
                g.fillRect(juce::Rectangle<float>(x0, 0.0f, x1 - x0,
                                                   static_cast<float>(getHeight())));
            }

            g.setColour(c(accent2).withAlpha(0.60f));
            g.drawVerticalLine(static_cast<int>(std::round(x0)), 0.0f,
                               static_cast<float>(getHeight()));

            g.setColour(c(text));
            g.setFont(juce::Font(11.5f, juce::Font::bold));
            g.drawText("BAR " + juce::String(bar + 1),
                       static_cast<int>(std::round(x0)) + 7, 4,
                       juce::jmax(44, static_cast<int>(std::round(x1 - x0)) - 10),
                       17, juce::Justification::centredLeft, false);
        }

        for (int step = 0; step < numSteps; step += stepsPerBeat)
        {
            const float x = xForStorageStepF(step);
            const int beat = (step % stepsPerBar) / stepsPerBeat + 1;
            g.setColour(c(muted).withAlpha(0.86f));
            g.setFont(10.0f);
            g.drawText(juce::String(beat),
                       static_cast<int>(std::round(x)) + 5, 23,
                       juce::jmax(20, static_cast<int>(std::round(stepWidth * stepsPerBeat)) - 6),
                       15, juce::Justification::centredLeft, false);
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
                g.setColour(c(accent2).withAlpha(0.36f));
                g.drawHorizontalLine(item.y + item.height - 1,
                                     static_cast<float>(gridLeft),
                                     static_cast<float>(gridRight));
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

                const float x0 = xForStorageStepF(barStart);
                const float x1 = xForStorageStepF(juce::jmin(numSteps, barStart + stepsPerBar));
                g.setColour(c(panelSoft).withAlpha(0.10f));
                g.fillRect(juce::Rectangle<float>(x0, static_cast<float>(item.y),
                                                   x1 - x0, static_cast<float>(item.height)));
            }

            g.setColour(c(border).withAlpha(0.52f));
            g.drawHorizontalLine(item.y + item.height - 1,
                                 static_cast<float>(gridLeft),
                                 static_cast<float>(gridRight));

            for (int step = 0; step <= numSteps; step += snap)
            {
                const float x = xForStorageStepF(step);
                if (step % stepsPerBar == 0)
                    g.setColour(c(accent2).withAlpha(0.55f));
                else if (step % stepsPerBeat == 0)
                    g.setColour(c(border).brighter(0.26f).withAlpha(0.92f));
                else
                    g.setColour(c(border).withAlpha(0.54f));

                g.drawVerticalLine(static_cast<int>(std::round(x)),
                                   static_cast<float>(item.y),
                                   static_cast<float>(item.y + item.height));
            }

            if (item.canonicalRow == hoverRow && hoverStep >= 0)
            {
                const float x0 = xForStorageStepF(hoverStep);
                const float x1 = xForStorageStepF(juce::jmin(numSteps, hoverStep + snap));
                g.setColour(c(accent).withAlpha(0.075f));
                g.fillRoundedRectangle(
                    juce::Rectangle<float>(x0 + 1.0f, static_cast<float>(item.y + 2),
                                           juce::jmax(1.0f, x1 - x0 - 2.0f),
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
                const float slotStart = xForStorageStepF(step);
                const float slotEnd = xForStorageStepF(juce::jmin(numSteps, step + 1));
                const float slotWidth = juce::jmax(2.0f, slotEnd - slotStart);
                const float cx = slotStart + slotWidth * 0.5f;
                const float cy = static_cast<float>(item.y) + item.height * 0.5f;
                const float hitW = juce::jlimit(8.0f, 30.0f, slotWidth * 0.66f);
                const float hitH = 7.0f + 13.0f * v;
                const bool ghost = velocity <= 55;

                juce::Rectangle<float> hit(cx - hitW * 0.5f, cy - hitH * 0.5f,
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

                const int offset = layer->getOffset(storageRow, step, pattern);
                if (offset != 0)
                {
                    const float markerX = cx + (static_cast<float>(offset) / 50.0f)
                                               * juce::jmin(slotWidth * 0.36f, 24.0f);
                    g.setColour(c(text).withAlpha(0.90f));
                    g.drawVerticalLine(static_cast<int>(std::round(markerX)),
                                       cy - hitH * 0.36f, cy + hitH * 0.36f);
                }

                const int retrigger = layer->getLength(storageRow, step, pattern);
                if (retrigger < 0)
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

        if (playPosition >= 0 && playPosition < numSteps)
        {
            const float x0 = xForStorageStepF(playPosition);
            const float x1 = xForStorageStepF(juce::jmin(numSteps, playPosition + 1));
            g.setColour(c(accent).withAlpha(0.095f));
            g.fillRect(juce::Rectangle<float>(x0, static_cast<float>(rulerHeight),
                                               juce::jmax(1.0f, x1 - x0),
                                               static_cast<float>(getHeight() - rulerHeight)));
            g.setColour(c(accent).withAlpha(0.72f));
            g.drawVerticalLine(static_cast<int>(std::round(x0)),
                               static_cast<float>(rulerHeight),
                               static_cast<float>(getHeight()));
        }

        // A hard end marker makes short patterns feel intentionally finite rather
        // than as if the grid simply failed to fill the window.
        g.setColour(c(accent2).withAlpha(0.78f));
        g.drawVerticalLine(gridRight, 0.0f, static_cast<float>(getHeight()));

        // Sticky articulation lane is painted last and therefore remains fixed
        // while the absolute timeline scrolls beneath it.
        g.setColour(c(panel));
        g.fillRect(stickyX, 0, nameWidth, rulerHeight);
        g.setColour(c(text));
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText("ARTICULATIONS", stickyX + 16, 3, nameWidth - 32, 18,
                   juce::Justification::centredLeft, false);
        g.setColour(c(muted));
        g.setFont(10.0f);
        const auto zoomCaption = juce::String(static_cast<int>(std::round(zoomScale * 100.0f)))
                               + "%  •  " + resolutionText() + " GRID";
        g.drawText(zoomCaption, stickyX + 16, 21, nameWidth - 32, 17,
                   juce::Justification::centredLeft, false);

        visibleRowIndex = 0;
        for (const auto& item : layout)
        {
            if (item.header)
            {
                g.setColour(c(panelRaised));
                g.fillRect(stickyX, item.y, nameWidth, item.height);
                g.setColour(c(accent2).withAlpha(0.62f));
                g.fillRect(stickyX, item.y, 3, item.height);
                g.setColour(c(muted));
                g.setFont(juce::Font(10.0f, juce::Font::bold));
                g.drawText(item.groupLabel.toUpperCase(), stickyX + 16, item.y,
                           nameWidth - 28, item.height,
                           juce::Justification::centredLeft, false);
                continue;
            }

            const bool alternate = (visibleRowIndex++ & 1) != 0;
            g.setColour(alternate ? c(panelRaised) : c(panel));
            g.fillRect(stickyX, item.y, nameWidth, item.height);
            g.setColour(c(border).withAlpha(0.58f));
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
        if (snappedCellAt(e.position, row, step))
        {
            if (row != hoverRow || step != hoverStep)
            {
                hoverRow = row;
                hoverStep = step;
                repaint();
            }
        }
        else if (hoverRow != -1 || hoverStep != -1)
        {
            hoverRow = -1;
            hoverStep = -1;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoverRow = -1;
        hoverStep = -1;
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        dragMode = DragMode::none;
        lastPaintRow = -1;
        lastPaintStep = -1;

        int canonicalRow = -1;
        int step = -1;
        if (!snappedCellAt(e.position, canonicalRow, step))
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
        const bool isOn = layer->getProb(storageRow, step, pattern) >= 0;

        dragRow = canonicalRow;
        dragStep = step;
        dragStart = e.position;

        const auto mods = e.mods;
        const bool command = mods.isCtrlDown() || mods.isCommandDown();

        if (command && mods.isShiftDown() && !mods.isAltDown())
        {
            if (!isOn)
            {
                setCell(canonicalRow, step, true);
                layer->setLength(storageRow, step, -1, pattern);
            }
            else
            {
                const int len = layer->getLength(storageRow, step, pattern);
                const int next = len >= 0 ? -1 : len > -3 ? len - 1 : 0;
                layer->setLength(storageRow, step, static_cast<int8_t>(next), pattern);
            }

            dragMode = DragMode::special;
            repaint();
            return;
        }

        if (command && !mods.isAltDown())
        {
            if (!isOn)
            {
                setCell(canonicalRow, step, true);
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
            repaint();
            return;
        }

        if (mods.isShiftDown() && isOn)
        {
            dragMode = DragMode::velocity;
            dragStartValue = juce::jlimit(
                1, 127, static_cast<int>(layer->getVel(storageRow, step, pattern)));
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
            const int storageRow = storageRowForCanonical(dragRow, canonicalRows.size());
            const int delta = static_cast<int>((dragStart.y - e.position.y) * 1.5f);
            const int velocity = juce::jlimit(1, 127, dragStartValue + delta);
            layer->setVel(storageRow, dragStep, static_cast<int8_t>(velocity), pattern);
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
        if (!snappedCellAt(e.position, canonicalRow, step))
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

            // Scale an absolute pixels-per-beat system. No viewport dimension or
            // pattern length participates in this calculation, so zooming back to
            // the same percentage always returns to exactly the same geometry.
            const float multiplier = std::pow(2.0f, primaryDelta * 1.8f);
            applyZoom(zoomScale * multiplier, anchorInContent, anchorInViewport);
            return;
        }

        if (e.mods.isAltDown())
        {
            int canonicalRow = -1;
            int step = -1;
            if (exactCellAt(e.position, canonicalRow, step))
            {
                auto* layer = processor.mData.getUISeqData()->getLayer(0);
                const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
                const int pattern = layer->getCurrentPattern();

                if (layer->getProb(storageRow, step, pattern) >= 0)
                {
                    if (e.mods.isShiftDown())
                    {
                        if (cycleHatArticulation(canonicalRow, step,
                                                 primaryDelta >= 0.0f ? 1 : -1))
                            return;
                    }
                    else
                    {
                        const int oldVelocity = juce::jlimit(
                            1, 127, static_cast<int>(layer->getVel(storageRow, step, pattern)));
                        const int amount = juce::jmax(
                            1, static_cast<int>(std::round(std::abs(primaryDelta) * 24.0f)));
                        const int velocity = juce::jlimit(
                            1, 127, oldVelocity + (primaryDelta >= 0.0f ? amount : -amount));

                        if (velocity != oldVelocity)
                        {
                            layer->setVel(storageRow, step, static_cast<int8_t>(velocity), pattern);
                            publishChange();
                            repaint();
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
        juce::String groupLabel;
        juce::String label;
        juce::String noteName;
    };

    enum class DragMode { none, paint, erase, velocity, timing, special };

    SeqAudioProcessor& processor;
    const juce::Array<GgdCanonicalRow>& canonicalRows;
    std::function<void()> undoCallback;
    std::function<void(float)> zoomCallback;
    const GgdKitMap* map = nullptr;
    std::vector<LayoutItem> layout;

    int meterNumerator = 4;
    int meterDenominator = 4;
    int playPosition = -1;
    float zoomScale = 1.0f;
    DragMode dragMode = DragMode::none;
    int dragRow = -1;
    int dragStep = -1;
    int dragStartValue = 0;
    juce::Point<float> dragStart;
    int lastPaintRow = -1;
    int lastPaintStep = -1;
    int hoverRow = -1;
    int hoverStep = -1;

    static constexpr int nameWidth = 224;
    static constexpr int rulerHeight = 42;
    static constexpr int groupHeight = 25;
    static constexpr int rowHeight = 32;
    static constexpr float defaultPixelsPerQuarter = 128.0f;
    static constexpr float minZoomScale = 0.5f;
    static constexpr float maxZoomScale = 4.0f;

    int snapSteps() const { return 1; }

    int viewportWidth() const
    {
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
            return juce::jmax(nameWidth + 240, viewport->getViewArea().getWidth());
        return 984;
    }

    float storageStepWidth() const
    {
        // SequenceData stores one step per sixteenth. Four storage steps are a
        // quarter note, so this is the single source of horizontal musical scale.
        return (defaultPixelsPerQuarter * zoomScale) / 4.0f;
    }

    float xForStorageStepF(int step) const
    {
        return static_cast<float>(nameWidth) + static_cast<float>(step) * storageStepWidth();
    }

    int currentViewX() const
    {
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
            return viewport->getViewPositionX();
        return 0;
    }

    void notifyZoomChanged()
    {
        if (zoomCallback)
            zoomCallback(zoomScale);
    }

    void applyZoom(float requestedScale, float anchorInContent, float anchorInViewport)
    {
        const float newScale = juce::jlimit(minZoomScale, maxZoomScale, requestedScale);
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
        notifyZoomChanged();
        repaint();
    }

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
                        row.noteName = "-";

                    layout.push_back(row);
                    y += rowHeight;
                }
            }
        }

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int patternRight = static_cast<int>(
            std::ceil(xForStorageStepF(layer->getNumSteps())));
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

    bool snappedCellAt(juce::Point<float> p, int& canonicalRow, int& step) const
    {
        if (!exactCellAt(p, canonicalRow, step))
            return false;
        const int snap = snapSteps();
        step = (step / snap) * snap;
        return true;
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

            publishChange();
            repaint();
            return true;
        }

        return false;
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
    setWantsKeyboardFocus(true);

    maps = GgdKitMapLibrary::loadBuiltInMaps();
    canonicalRows = GgdKitMapLibrary::buildCanonicalRows(maps);

    grid = std::make_unique<GgdDrumGrid>(
        processor,
        canonicalRows,
        [this] { performUndo(); },
        [this](float scale) { refreshZoomControls(scale); });
    gridViewport.setViewedComponent(grid.get(), false);
    gridViewport.setScrollBarsShown(true, true);
    gridViewport.setScrollBarThickness(10);
    gridViewport.setScrollOnDragEnabled(false);
    addAndMakeVisible(gridViewport);

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
    for (int i = 0; i < maps.size(); ++i)
        kitSelector.addItem(maps.getReference(i).library, i + 1);
    kitSelector.onChange = [this]
    {
        if (kitSelector.getSelectedId() > 0)
            setActiveMap(kitSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(kitSelector);

    patternSelector.setTooltip("Pattern slot");
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

    meterLabel.setText("METER", juce::dontSendNotification);
    meterLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    meterLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(meterLabel);

    for (int numerator = 1; numerator <= 16; ++numerator)
        numeratorSelector.addItem(juce::String(numerator), numerator);
    numeratorSelector.setTooltip("Time signature numerator");
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

    barsSelector.setTooltip("Pattern length in bars");
    barsSelector.onChange = [this]
    {
        const int bars = barsSelector.getSelectedId();
        if (bars > 0)
        {
            activeBars = bars;
            applyPatternGeometry();
        }
    };
    addAndMakeVisible(barsSelector);

    zoomLabel.setText("ZOOM", juce::dontSendNotification);
    zoomLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    zoomLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(zoomLabel);

    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange(0.5, 4.0, 0.0);
    zoomSlider.setSkewFactorFromMidPoint(1.0);
    zoomSlider.setValue(1.0, juce::dontSendNotification);
    zoomSlider.setDoubleClickReturnValue(true, 1.0);
    zoomSlider.setTooltip("Absolute timeline zoom. 100% = 128 px per quarter note. Ctrl+wheel zooms under the pointer.");
    zoomSlider.onValueChange = [this]
    {
        if (grid != nullptr)
            grid->setZoomScale(static_cast<float>(zoomSlider.getValue()));
    };
    addAndMakeVisible(zoomSlider);

    zoomValueLabel.setText("100%  •  1/16", juce::dontSendNotification);
    zoomValueLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    zoomValueLabel.setColour(juce::Label::textColourId, c(text));
    zoomValueLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(zoomValueLabel);

    fitZoomButton.setButtonText("100%");
    fitZoomButton.setTooltip("Reset timeline magnification to the standard musical scale");
    fitZoomButton.onClick = [this]
    {
        if (grid != nullptr)
            grid->resetZoom();
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

    undoButton.setTooltip("Undo the most recent sequence edit (Ctrl+Z)");
    undoButton.onClick = [this] { performUndo(); };
    addAndMakeVisible(undoButton);

    duplicateButton.setTooltip("Duplicate the current pattern into the next slot");
    duplicateButton.onClick = [this] { duplicateCurrentPattern(); };
    addAndMakeVisible(duplicateButton);

    clearButton.setTooltip("Clear the current pattern");
    clearButton.onClick = [this] { clearCurrentPattern(); };
    addAndMakeVisible(clearButton);

    hintLabel.setText(
        "Click/drag paint  •  Right-drag erase  •  Shift velocity  •  Alt timing  •  "
        "Ctrl+wheel zoom  •  Ctrl click ghost  •  Ctrl+Shift roll  •  Alt+Shift wheel hats",
        juce::dontSendNotification);
    hintLabel.setFont(juce::Font(10.0f));
    hintLabel.setColour(juce::Label::textColourId, c(muted));
    addAndMakeVisible(hintLabel);

    initialiseDrumState();
    refreshControlsFromModel();
    if (!maps.isEmpty())
        grid->setMap(&maps.getReference(activeMapIndex));

    setResizable(true, true);
    setResizeLimits(900, 560, 1900, 1400);
    setSize(1160, 780);
    startTimerHz(30);
}

GgdDrumEditor::~GgdDrumEditor()
{
    stopTimer();
    gridViewport.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
}

bool GgdDrumEditor::keyPressed(const juce::KeyPress& key)
{
    if (isUndoKey(key))
    {
        performUndo();
        return true;
    }
    return false;
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

    auto first = juce::Rectangle<int>(pad, 7, getWidth() - pad * 2, 43);
    productLabel.setBounds(first.removeFromLeft(142));
    first.removeFromLeft(3);
    transportStatus.setBounds(first.removeFromLeft(72).reduced(0, 8));
    first.removeFromLeft(gap + 2);

    const int kitWidth = juce::jlimit(176, 236, getWidth() / 5);
    kitSelector.setBounds(first.removeFromLeft(kitWidth).reduced(0, 5));
    first.removeFromLeft(gap);
    patternSelector.setBounds(first.removeFromLeft(152).reduced(0, 5));
    first.removeFromLeft(gap);
    patternName.setBounds(first.reduced(0, 5));

    auto second = juce::Rectangle<int>(pad, 60, getWidth() - pad * 2, 43);

    auto actions = second.removeFromRight(201);
    clearButton.setBounds(actions.removeFromRight(56).reduced(0, 5));
    actions.removeFromRight(5);
    duplicateButton.setBounds(actions.removeFromRight(78).reduced(0, 5));
    actions.removeFromRight(5);
    undoButton.setBounds(actions.removeFromRight(57).reduced(0, 5));
    second.removeFromRight(12);

    meterLabel.setBounds(second.removeFromLeft(46));
    numeratorSelector.setBounds(second.removeFromLeft(52).reduced(0, 5));
    meterSlash.setBounds(second.removeFromLeft(17));
    denominatorSelector.setBounds(second.removeFromLeft(60).reduced(0, 5));
    second.removeFromLeft(12);

    barsLabel.setBounds(second.removeFromLeft(49));
    barsSelector.setBounds(second.removeFromLeft(88).reduced(0, 5));
    second.removeFromLeft(14);

    zoomLabel.setBounds(second.removeFromLeft(39));
    fitZoomButton.setBounds(second.removeFromRight(50).reduced(0, 5));
    second.removeFromRight(5);
    zoomValueLabel.setBounds(second.removeFromRight(82));
    second.removeFromRight(6);
    zoomSlider.setBounds(second.reduced(0, 7));

    gridViewport.setBounds(
        0, topAreaHeight, getWidth(),
        getHeight() - topAreaHeight - bottomAreaHeight);
    hintLabel.setBounds(
        14, getHeight() - bottomAreaHeight,
        getWidth() - 28, bottomAreaHeight);

    if (grid != nullptr)
        grid->refreshSize();
}

void GgdDrumEditor::timerCallback()
{
    const int pos = processor.mNotifier.getPlayPosition(0);
    grid->setPlayPosition(pos);

    if (pos >= 0)
    {
        transportStatus.setColour(juce::Label::textColourId, c(accent));
        transportStatus.setText("PLAY " + juce::String(pos + 1),
                                juce::dontSendNotification);
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
        preservedBars = juce::jlimit(
            1, maxUserBars,
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
    publishModelChange();
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
        publishModelChange();
}

void GgdDrumEditor::applyPatternGeometry(bool publish)
{
    if (maps.isEmpty())
        return;

    timeSigNumerator = juce::jlimit(1, 16, timeSigNumerator);
    if (timeSigDenominator != 4 && timeSigDenominator != 8 && timeSigDenominator != 16)
        timeSigDenominator = 4;

    const int stepsPerBar = stepsPerBarForMeter(timeSigNumerator, timeSigDenominator);
    const int maxBars = juce::jlimit(1, maxUserBars, SEQ_MAX_STEPS / stepsPerBar);
    activeBars = juce::jlimit(1, maxBars, activeBars);

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    layer->setStepsPerMeasure(timeSigNumerator);
    layer->setClockDivider(clockDividerForMeter(timeSigDenominator));
    layer->setNumSteps(activeBars * stepsPerBar);
    updatePersistenceTag();

    rebuildBarsSelector();
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
    const int maxBars = juce::jlimit(1, maxUserBars, SEQ_MAX_STEPS / stepsPerBar);
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

    rebuildBarsSelector();
    barsSelector.setSelectedId(activeBars, juce::dontSendNotification);
    refreshPatternSelectorLabels();

    grid->setMeter(timeSigNumerator, timeSigDenominator);
    grid->setMap(&maps.getReference(activeMapIndex));
    refreshZoomControls(grid->getZoomScale());
}

void GgdDrumEditor::rebuildBarsSelector()
{
    const int stepsPerBar = stepsPerBarForMeter(timeSigNumerator, timeSigDenominator);
    const int maxBars = juce::jlimit(1, maxUserBars, SEQ_MAX_STEPS / stepsPerBar);

    barsSelector.clear(juce::dontSendNotification);
    for (int bars = 1; bars <= maxBars; ++bars)
        barsSelector.addItem(
            juce::String(bars) + (bars == 1 ? " bar" : " bars"), bars);

    barsSelector.setSelectedId(
        juce::jlimit(1, maxBars, activeBars), juce::dontSendNotification);
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
    zoomValueLabel.setText(juce::String(percent) + "%  •  1/16",
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
    processor.mData.undo();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    refreshControlsFromModel();
    grid->refreshSize();
}

void GgdDrumEditor::duplicateCurrentPattern()
{
    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);

    const int source = layer->getCurrentPattern();
    const int target = (source + 1) % SEQ_MAX_PATTERNS;
    const juce::String sourceName(layer->getPatternName(source));

    seq->copyPatternData(0, target, 0, source);

    juce::String copyName = sourceName == juce::String(SEQ_DEFAULT_PAT_NAME)
        ? "Pattern " + juce::String(target + 1)
        : sourceName + " copy";
    copyName = copyName.substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(copyName.toRawUTF8(), target);
    layer->setCurrentPattern(target);

    publishModelChange();
    refreshControlsFromModel();
    grid->repaint();
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

    if (n < 1 || n > 16 || (d != 4 && d != 8 && d != 16))
        return false;

    numerator = n;
    denominator = d;
    return true;
}
