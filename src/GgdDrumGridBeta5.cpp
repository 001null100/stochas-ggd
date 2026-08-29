#include "GgdDrumGridBeta.h"
#include "GgdUiTheme.h"

#include <algorithm>
#include <cmath>
#include <limits>

// Beta 5 keeps the proven event-editing implementation, but replaces hit
// testing and the mouse paths that need resolution-independent erasing,
// interpolated paint drags and articulation-row audition.
#define setPlayPosition setPlayPositionLegacy
#define paint paintLegacy
#define eventAt eventAtLegacy
#define mouseDown mouseDownLegacy
#define mouseDrag mouseDragLegacy
#define mouseUp mouseUpLegacy
#include "GgdDrumGridBeta.cpp"
#undef mouseUp
#undef mouseDrag
#undef mouseDown
#undef eventAt
#undef paint
#undef setPlayPosition

namespace
{
juce::Colour gc(GgdThemeRole role)
{
    return ggdThemeColour(role);
}
}

void GgdDrumGrid::setPlayPosition(int stepPosition)
{
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (stepPosition < 0)
    {
        playStepPosition = -1;
        playStepStartMs = now;
        hasPlayStepEstimate = false;
        repaint();
        return;
    }

    if (playStepPosition >= 0 && stepPosition != playStepPosition)
    {
        const int numSteps = juce::jmax(
            1, (patternLengthTicks() + GGD_TICKS_PER_16TH - 1) / GGD_TICKS_PER_16TH);
        int delta = (stepPosition - playStepPosition + numSteps) % numSteps;
        if (delta <= 0)
            delta = 1;

        const double elapsed = now - playStepStartMs;
        const double observed = elapsed / static_cast<double>(delta);
        if (observed >= 8.0 && observed <= 2000.0)
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

    if (stepPosition != playStepPosition)
    {
        playStepPosition = stepPosition;
        playStepStartMs = now;
    }

    repaint();
}

float GgdDrumGrid::interpolatedPlayTick() const
{
    if (playStepPosition < 0)
        return -1.0f;

    const int length = patternLengthTicks();
    if (length <= 0)
        return -1.0f;

    double tick = static_cast<double>(playStepPosition) * GGD_TICKS_PER_16TH;
    if (hasPlayStepEstimate && playStepMs > 0.0)
    {
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - playStepStartMs;
        const double fraction = juce::jlimit(0.0, 1.20, elapsed / playStepMs);
        tick += fraction * GGD_TICKS_PER_16TH;
    }

    tick = std::fmod(tick, static_cast<double>(length));
    if (tick < 0.0)
        tick += length;
    return static_cast<float>(tick);
}

void GgdDrumGrid::paint(juce::Graphics& g)
{
    g.fillAll(gc(GgdThemeRole::outside));

    const int length = patternLengthTicks();
    const int barTicks = ticksPerBar();
    const int beatTicks = juce::jmax(1, GGD_EVENT_PPQ * 4 / meterDenominator);
    const bool tripletGrid = snapTicks == GGD_TICKS_PER_8TH_TRIPLET
                          || snapTicks == GGD_TICKS_PER_16TH_TRIPLET;
    const bool fineGrid = snapTicks == GGD_TICKS_PER_32ND
                       || snapTicks == GGD_TICKS_PER_16TH_TRIPLET;
    const int majorSubdivisionTicks = tripletGrid
        ? GGD_TICKS_PER_8TH_TRIPLET : GGD_TICKS_PER_16TH;
    const int minorSubdivisionTicks = tripletGrid
        ? GGD_TICKS_PER_16TH_TRIPLET : GGD_TICKS_PER_32ND;

    const int gridLeft = nameWidth;
    const int gridRight = static_cast<int>(std::ceil(xForTick(static_cast<float>(length))));
    const int activeWidth = juce::jmax(0, gridRight - gridLeft);
    const int stickyX = currentViewX();

    g.setColour(gc(GgdThemeRole::background));
    g.fillRect(gridLeft, 0, activeWidth, getHeight());
    g.setColour(gc(GgdThemeRole::panel));
    g.fillRect(gridLeft, 0, activeWidth, rulerHeight);

    for (int barStart = 0, bar = 0; barStart < length; barStart += barTicks, ++bar)
    {
        const float x0 = xForTick(static_cast<float>(barStart));
        const float x1 = xForTick(static_cast<float>(juce::jmin(length, barStart + barTicks)));
        if ((bar & 1) != 0)
        {
            g.setColour(gc(GgdThemeRole::panelRaised).withAlpha(0.20f));
            g.fillRect(juce::Rectangle<float>(
                x0, 0.0f, x1 - x0, static_cast<float>(getHeight())));
        }

        g.setColour(gc(GgdThemeRole::text).withAlpha(0.94f));
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText("BAR " + juce::String(bar + 1),
                   static_cast<int>(std::round(x0)) + 9, 4,
                   juce::jmax(44, static_cast<int>(std::round(x1 - x0)) - 12),
                   17, juce::Justification::centredLeft, false);
    }

    // Timing hierarchy: bar > beat > primary subdivision > fine subdivision.
    for (int tick = 0; tick <= length; tick += barTicks)
    {
        const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
        g.setColour(gc(GgdThemeRole::barLine));
        g.fillRect(x - 1, 0, 2, rulerHeight);
    }

    for (int tick = 0; tick <= length; tick += beatTicks)
    {
        if (tick % barTicks == 0)
            continue;
        const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
        g.setColour(gc(GgdThemeRole::beatLine).withAlpha(0.96f));
        g.fillRect(x, rulerHeight - 21, 1, 21);
    }

    for (int tick = 0; tick <= length; tick += majorSubdivisionTicks)
    {
        if (tick % barTicks == 0 || tick % beatTicks == 0)
            continue;
        const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
        g.setColour(gc(GgdThemeRole::subdivisionLine).withAlpha(0.90f));
        g.fillRect(x, rulerHeight - 11, 1, 11);
    }

    if (fineGrid)
    {
        for (int tick = 0; tick <= length; tick += minorSubdivisionTicks)
        {
            if (tick % majorSubdivisionTicks == 0)
                continue;
            const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
            g.setColour(gc(GgdThemeRole::fineSubdivisionLine).withAlpha(0.84f));
            g.fillRect(x, rulerHeight - 6, 1, 6);
        }
    }

    for (int barStart = 0; barStart < length; barStart += barTicks)
    {
        for (int beat = 0; beat < meterNumerator; ++beat)
        {
            const int tick = barStart + beat * beatTicks;
            if (tick >= length)
                break;
            const float x = xForTick(static_cast<float>(tick));
            g.setColour(gc(GgdThemeRole::muted).withAlpha(0.96f));
            g.setFont(10.0f);
            g.drawText(juce::String(beat + 1),
                       static_cast<int>(std::round(x)) + 5, 21,
                       juce::jmax(20, static_cast<int>(
                           std::round(xForTick(static_cast<float>(juce::jmin(length, tick + beatTicks))) - x)) - 6),
                       13, juce::Justification::centredLeft, false);
        }
    }

    g.setColour(gc(GgdThemeRole::borderStrong).withAlpha(0.72f));
    g.drawHorizontalLine(rulerHeight - 1, static_cast<float>(gridLeft),
                         static_cast<float>(gridRight));

    int visibleRowIndex = 0;
    for (const auto& item : layout)
    {
        if (item.header)
        {
            g.setColour(gc(GgdThemeRole::groupFill));
            g.fillRect(gridLeft, item.y, activeWidth, item.height);
            g.setColour(gc(GgdThemeRole::groupLine).withAlpha(0.95f));
            g.fillRect(gridLeft, item.y, activeWidth, 3);
            g.setColour(gc(GgdThemeRole::borderStrong).withAlpha(0.68f));
            g.fillRect(gridLeft, item.y + item.height - 1, activeWidth, 1);
            continue;
        }

        const bool alternate = (visibleRowIndex++ & 1) != 0;
        g.setColour(alternate ? gc(GgdThemeRole::rowAlternate)
                              : gc(GgdThemeRole::background));
        g.fillRect(gridLeft, item.y, activeWidth, item.height);

        for (int barStart = 0, bar = 0; barStart < length; barStart += barTicks, ++bar)
        {
            if ((bar & 1) == 0)
                continue;
            const float x0 = xForTick(static_cast<float>(barStart));
            const float x1 = xForTick(static_cast<float>(juce::jmin(length, barStart + barTicks)));
            g.setColour(gc(GgdThemeRole::panelSoft).withAlpha(0.10f));
            g.fillRect(juce::Rectangle<float>(
                x0, static_cast<float>(item.y), x1 - x0, static_cast<float>(item.height)));
        }

        g.setColour(gc(GgdThemeRole::borderSoft).withAlpha(0.58f));
        g.drawHorizontalLine(item.y + item.height - 1,
                             static_cast<float>(gridLeft),
                             static_cast<float>(gridRight));

        for (int tick = 0; tick <= length; tick += barTicks)
        {
            const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
            g.setColour(gc(GgdThemeRole::barLine).withAlpha(0.90f));
            g.fillRect(x - 1, item.y, 2, item.height);
        }

        for (int tick = 0; tick <= length; tick += beatTicks)
        {
            if (tick % barTicks == 0)
                continue;
            const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
            g.setColour(gc(GgdThemeRole::beatLine).withAlpha(0.82f));
            g.fillRect(x, item.y, 1, item.height);
        }

        for (int tick = 0; tick <= length; tick += majorSubdivisionTicks)
        {
            if (tick % barTicks == 0 || tick % beatTicks == 0)
                continue;
            const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
            g.setColour(gc(GgdThemeRole::subdivisionLine).withAlpha(0.54f));
            g.fillRect(x, item.y, 1, item.height);
        }

        if (fineGrid)
        {
            const float rowMid = static_cast<float>(item.y) + item.height * 0.5f;
            for (int tick = 0; tick <= length; tick += minorSubdivisionTicks)
            {
                if (tick % majorSubdivisionTicks == 0)
                    continue;
                const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
                g.setColour(gc(GgdThemeRole::fineSubdivisionLine).withAlpha(0.66f));
                g.drawVerticalLine(x, rowMid - 5.0f, rowMid + 5.0f);
            }
        }
    }

    // A hit's timestamp marks its start, not its visual centre. Keeping the body
    // to the right of the timing line makes tick 0 fully visible and prevents
    // every note from looking half a subdivision early.
    if (toolMode == ToolMode::draw && hoverCanonicalRow >= 0 && hoverTick >= 0)
    {
        const int y = layoutYForCanonical(hoverCanonicalRow);
        if (y >= 0)
        {
            const float x = xForTick(static_cast<float>(hoverTick));
            const float w = juce::jmax(
                5.0f, static_cast<float>(snapTicks) / GGD_EVENT_PPQ
                    * pixelsPerQuarter * zoomScale);
            const auto target = juce::Rectangle<float>(
                x + 2.0f, static_cast<float>(y + 3),
                juce::jmax(2.0f, w - 4.0f), static_cast<float>(rowHeight - 6));
            g.setColour(gc(GgdThemeRole::accent).withAlpha(0.08f));
            g.fillRoundedRectangle(target, 4.0f);
            g.setColour(gc(GgdThemeRole::accent).withAlpha(0.35f));
            g.drawRoundedRectangle(target, 4.0f, 1.0f);
        }
    }

    if (const auto* p = pattern())
    {
        for (int index = 0; index < p->getEventCount(); ++index)
        {
            const auto* event = p->getEvent(index);
            if (event == nullptr)
                continue;

            const int canonical = canonicalRowForStorage(event->row);
            const int y = layoutYForCanonical(canonical);
            if (canonical < 0 || y < 0)
                continue;

            const float x = xForTick(static_cast<float>(event->tick));
            const float slotPixels = static_cast<float>(snapTicks) / GGD_EVENT_PPQ
                                   * pixelsPerQuarter * zoomScale;
            const float hitW = juce::jlimit(8.0f, 30.0f, slotPixels * 0.66f);
            const float v = static_cast<float>(event->velocity) / 127.0f;
            const float hitH = 7.0f + 13.0f * v;
            const float cy = y + rowHeight * 0.5f;
            juce::Rectangle<float> hit(x + 2.0f, cy - hitH * 0.5f, hitW, hitH);
            const bool ghost = event->velocity <= 55;
            const EventRef ref { canonical, event->tick };

            if (ghost)
            {
                g.setColour(gc(GgdThemeRole::accent).withAlpha(0.14f + 0.26f * v));
                g.fillRoundedRectangle(hit, juce::jmin(4.0f, hitW * 0.35f));
                g.setColour(gc(GgdThemeRole::accent).withAlpha(0.66f));
                g.drawRoundedRectangle(hit, juce::jmin(4.0f, hitW * 0.35f), 1.4f);
            }
            else
            {
                g.setColour(gc(GgdThemeRole::accent).withAlpha(0.16f));
                g.fillRoundedRectangle(hit.expanded(1.5f),
                                       juce::jmin(5.0f, hitW * 0.35f + 1.0f));
                g.setColour(gc(GgdThemeRole::accent).withAlpha(0.56f + 0.40f * v));
                g.fillRoundedRectangle(hit, juce::jmin(4.0f, hitW * 0.35f));
            }

            if (isSelected(ref))
            {
                g.setColour(gc(GgdThemeRole::selection).withAlpha(0.16f));
                g.fillRoundedRectangle(hit.expanded(4.0f),
                                       juce::jmin(6.0f, hitW * 0.35f + 2.0f));
                g.setColour(gc(GgdThemeRole::selection));
                g.drawRoundedRectangle(
                    hit.expanded(2.5f), juce::jmin(5.0f, hitW * 0.35f + 1.0f), 1.8f);
            }
        }
    }

    if (dragMode == DragMode::move && (dragDeltaTicks != 0 || dragDeltaRows != 0))
    {
        for (const auto& state : dragSelectionStates)
        {
            const int row = state.ref.row + dragDeltaRows;
            const int tick = state.ref.tick + dragDeltaTicks;
            const int y = layoutYForCanonical(row);
            if (y < 0 || tick < 0 || tick >= length)
                continue;
            const float x = xForTick(static_cast<float>(tick));
            g.setColour(gc(GgdThemeRole::warm).withAlpha(0.72f));
            g.fillRoundedRectangle(x + 2.0f, y + rowHeight * 0.5f - 5.0f,
                                   10.0f, 10.0f, 3.0f);
        }
    }

    if (dragMode == DragMode::marquee)
    {
        g.setColour(gc(GgdThemeRole::accent).withAlpha(0.10f));
        g.fillRect(marqueeRect);
        g.setColour(gc(GgdThemeRole::accent).withAlpha(0.92f));
        g.drawRect(marqueeRect, 1.2f);
    }

    const float smoothTick = interpolatedPlayTick();
    if (smoothTick >= 0.0f)
    {
        const float playX = xForTick(smoothTick);
        const auto playhead = gc(GgdThemeRole::playhead);
        g.setColour(playhead.withAlpha(0.08f));
        g.fillRect(juce::Rectangle<float>(
            playX - 4.0f, static_cast<float>(rulerHeight), 8.0f,
            static_cast<float>(getHeight() - rulerHeight)));
        g.setColour(playhead.withAlpha(0.96f));
        g.fillRect(juce::Rectangle<float>(
            playX - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight())));

        juce::Path marker;
        marker.addTriangle(playX - 5.0f, 0.0f,
                           playX + 5.0f, 0.0f,
                           playX, 7.0f);
        g.setColour(playhead);
        g.fillPath(marker);
    }

    g.setColour(gc(GgdThemeRole::barLine).withAlpha(0.82f));
    g.fillRect(gridRight - 1, 0, 2, getHeight());

    g.setColour(gc(GgdThemeRole::panel));
    g.fillRect(stickyX, 0, nameWidth, rulerHeight);
    g.setColour(gc(GgdThemeRole::borderStrong).withAlpha(0.62f));
    g.fillRect(stickyX + nameWidth - 1, 0, 1, getHeight());
    g.setColour(gc(GgdThemeRole::text));
    g.setFont(juce::Font(11.5f, juce::Font::bold));
    g.drawText(toolMode == ToolMode::draw
                   ? "ARTICULATIONS  |  DRAW" : "ARTICULATIONS  |  SELECT",
               stickyX + 16, 3, nameWidth - 32, 18,
               juce::Justification::centredLeft, false);
    g.setColour(gc(GgdThemeRole::muted));
    g.setFont(10.0f);

    juce::String gridText = "1/16";
    if (snapTicks == GGD_TICKS_PER_32ND) gridText = "1/32";
    else if (snapTicks == GGD_TICKS_PER_8TH_TRIPLET) gridText = "1/8T";
    else if (snapTicks == GGD_TICKS_PER_16TH_TRIPLET) gridText = "1/16T";
    const auto zoomCaption = juce::String(static_cast<int>(std::round(zoomScale * 100.0f)))
                           + "%  |  " + gridText + " GRID";
    g.drawText(zoomCaption, stickyX + 16, 21, nameWidth - 32, 17,
               juce::Justification::centredLeft, false);

    visibleRowIndex = 0;
    for (const auto& item : layout)
    {
        if (item.header)
        {
            g.setColour(gc(GgdThemeRole::groupFill));
            g.fillRect(stickyX, item.y, nameWidth, item.height);
            g.setColour(gc(GgdThemeRole::groupLine));
            g.fillRect(stickyX, item.y, 5, item.height);
            g.fillRect(stickyX, item.y, nameWidth, 3);
            g.setColour(gc(GgdThemeRole::borderStrong).withAlpha(0.70f));
            g.fillRect(stickyX, item.y + item.height - 1, nameWidth, 1);

            const bool collapsed = collapsedGroups.count(item.groupId) != 0;
            const float arrowX = static_cast<float>(stickyX + 18);
            const float arrowY = static_cast<float>(item.y + item.height / 2);
            juce::Path arrow;
            if (collapsed)
                arrow.addTriangle(arrowX - 2.5f, arrowY - 4.0f,
                                  arrowX - 2.5f, arrowY + 4.0f,
                                  arrowX + 3.5f, arrowY);
            else
                arrow.addTriangle(arrowX - 4.0f, arrowY - 2.5f,
                                  arrowX + 4.0f, arrowY - 2.5f,
                                  arrowX, arrowY + 3.5f);
            g.setColour(gc(GgdThemeRole::groupLine));
            g.fillPath(arrow);

            g.setColour(gc(GgdThemeRole::text).withAlpha(0.92f));
            g.setFont(juce::Font(10.8f, juce::Font::bold));
            g.drawText(item.groupLabel.toUpperCase(), stickyX + 30, item.y,
                       nameWidth - 42, item.height,
                       juce::Justification::centredLeft, false);
            continue;
        }

        const bool alternate = (visibleRowIndex++ & 1) != 0;
        g.setColour(alternate ? gc(GgdThemeRole::rowAlternate)
                              : gc(GgdThemeRole::panel));
        g.fillRect(stickyX, item.y, nameWidth, item.height);
        g.setColour(gc(GgdThemeRole::borderSoft).withAlpha(0.62f));
        g.drawHorizontalLine(item.y + item.height - 1,
                             static_cast<float>(stickyX),
                             static_cast<float>(stickyX + nameWidth));
        g.setColour(gc(GgdThemeRole::text));
        g.setFont(12.0f);
        g.drawText(item.label, stickyX + 16, item.y + 1, nameWidth - 78,
                   item.height - 2, juce::Justification::centredLeft, true);
        if (item.noteName.isNotEmpty())
        {
            g.setColour(gc(GgdThemeRole::muted));
            g.setFont(10.0f);
            g.drawText(item.noteName, stickyX + nameWidth - 64, item.y + 1, 48,
                       item.height - 2, juce::Justification::centredRight, false);
        }
    }
}

GgdDrumGrid::EventRef GgdDrumGrid::eventAt(float x, float y) const
{
    const int canonical = canonicalRowAtY(y);
    if (canonical < 0)
        return {};

    const auto* p = pattern();
    if (p == nullptr)
        return {};

    const int storage = storageRowForCanonical(canonical);
    const float slotPixels = static_cast<float>(snapTicks) / GGD_EVENT_PPQ
                           * pixelsPerQuarter * zoomScale;
    const float hitW = juce::jlimit(8.0f, 30.0f, slotPixels * 0.66f);
    float best = std::numeric_limits<float>::max();
    int bestTick = -1;

    for (int i = 0; i < p->getEventCount(); ++i)
    {
        const auto* event = p->getEvent(i);
        if (event == nullptr || event->row != storage)
            continue;

        const float left = xForTick(static_cast<float>(event->tick)) - 2.0f;
        const float right = left + hitW + 8.0f;
        if (x < left || x > right)
            continue;

        const float distance = x < left ? left - x : x > right ? x - right : 0.0f;
        if (distance < best)
        {
            best = distance;
            bestTick = event->tick;
        }
    }

    return { canonical, bestTick };
}

void GgdDrumGrid::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragStartPoint = e.position;

    if (const auto* item = itemAtY(e.position.y))
    {
        if (item->header)
        {
            if (collapsedGroups.count(item->groupId) != 0)
                collapsedGroups.erase(item->groupId);
            else
                collapsedGroups.insert(item->groupId);
            rebuildLayout();
            repaint();
            return;
        }
    }

    const int row = canonicalRowAtY(e.position.y);
    if (row < 0)
        return;

    const int stickyX = currentViewX();
    if (e.position.x >= stickyX && e.position.x < stickyX + nameWidth)
    {
        const int storage = storageRowForCanonical(row);
        if (layer()->getCurNote(storage) >= 0)
        {
            processor.mIncomingData.addToFifo(SEQ_MIDI_NOTEON, 0, storage);
            dragEvent = { row, -2 }; // sentinel for articulation preview
        }
        return;
    }

    if (e.position.x < nameWidth)
        return;

    const auto hit = eventAt(e.position.x, e.position.y);

    if (toolMode == ToolMode::draw)
    {
        // Right-click is always erase. It never turns into paint just because
        // the currently selected snap point happens to be empty.
        if (e.mods.isRightButtonDown())
        {
            paintErase = true;
            lastPaintRow = row;
            lastPaintTick = -1;
            dragMode = DragMode::paint;
            dragEvent = { row, -3 }; // sentinel for free-resolution erase drag
            if (hit.tick >= 0 && removeEvent(hit))
                publishChange();
            return;
        }

        if (hit.tick >= 0 && e.mods.isShiftDown())
        {
            dragMode = DragMode::velocity;
            dragEvent = hit;
            dragStartVelocity = snapshotEvent(hit).velocity;
            return;
        }

        if (hit.tick >= 0 && e.mods.isAltDown())
        {
            dragMode = DragMode::timing;
            dragEvent = hit;
            dragStartTick = hit.tick;
            return;
        }

        const int tick = snappedTickForX(e.position.x);
        paintErase = eventOccupied(row, tick);
        lastPaintRow = -1;
        lastPaintTick = -1;
        dragMode = DragMode::paint;
        dragEvent = {};
        paintAt(row, tick);
        return;
    }

    if (hit.tick >= 0)
    {
        if (e.mods.isShiftDown())
        {
            toggleSelection(hit);
            return;
        }

        if (!isSelected(hit))
        {
            selection.clear();
            addSelection(hit);
            notifySelectionChanged();
        }
        beginSelectionMove(e, e.mods.isAltDown());
        repaint();
        return;
    }

    marqueeAdditive = e.mods.isShiftDown();
    marqueeBaseSelection = marqueeAdditive ? selection : std::vector<EventRef>();
    if (!marqueeAdditive)
        selection.clear();
    marqueeStart = e.position;
    marqueeRect = juce::Rectangle<float>(e.position.x, e.position.y, 0.0f, 0.0f);
    dragMode = DragMode::marquee;
    updateMarqueeSelection();
}

void GgdDrumGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (dragMode != DragMode::paint)
    {
        mouseDragLegacy(e);
        return;
    }

    const int row = canonicalRowAtY(e.position.y);
    if (row < 0 || e.position.x < nameWidth)
        return;

    if (dragEvent.tick == -3)
    {
        // Resolution-independent eraser sweep. Remove any actual event body the
        // pointer crossed, including triplets while a straight grid is active.
        auto* p = pattern();
        if (p == nullptr)
            return;

        const int storage = storageRowForCanonical(row);
        const float lo = juce::jmin(dragStartPoint.x, e.position.x) - 5.0f;
        const float hi = juce::jmax(dragStartPoint.x, e.position.x) + 5.0f;
        const float slotPixels = static_cast<float>(snapTicks) / GGD_EVENT_PPQ
                               * pixelsPerQuarter * zoomScale;
        const float hitW = juce::jlimit(8.0f, 30.0f, slotPixels * 0.66f);
        bool changed = false;

        for (int i = p->getEventCount() - 1; i >= 0; --i)
        {
            const auto* event = p->getEvent(i);
            if (event == nullptr || event->row != storage)
                continue;
            const float left = xForTick(static_cast<float>(event->tick));
            const float right = left + hitW + 4.0f;
            if (right >= lo && left <= hi)
            {
                const int tick = event->tick;
                changed |= p->removeEvent(storage, tick);
            }
        }

        dragStartPoint = e.position;
        if (changed)
            publishChange();
        return;
    }

    const int target = snappedTickForX(e.position.x);
    if (lastPaintRow == row && lastPaintTick >= 0 && target != lastPaintTick)
    {
        const int step = target > lastPaintTick ? snapTicks : -snapTicks;
        const int first = lastPaintTick + step;
        for (int tick = first;
             step > 0 ? tick <= target : tick >= target;
             tick += step)
            paintAt(row, tick);
    }
    else
    {
        paintAt(row, target);
    }
}

void GgdDrumGrid::mouseUp(const juce::MouseEvent& e)
{
    if (dragEvent.tick == -2)
    {
        const int storage = storageRowForCanonical(dragEvent.row);
        processor.mIncomingData.addToFifo(SEQ_MIDI_NOTEOFF, 0, storage);
        dragEvent = {};
        dragMode = DragMode::none;
        repaint();
        return;
    }

    mouseUpLegacy(e);
    if (dragEvent.tick == -3)
        dragEvent = {};
}