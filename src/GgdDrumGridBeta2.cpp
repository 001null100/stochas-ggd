#include "GgdDrumGridBeta.h"

// Keep all Beta 1 event editing/interaction behavior intact and replace only
// the two presentation methods which regressed during the engine transition.
#define setPlayPosition setPlayPositionLegacy
#define paint paintLegacy
#include "GgdDrumGridBeta.cpp"
#undef paint
#undef setPlayPosition

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
                // Light smoothing keeps the playhead fluid without becoming
                // sluggish when tempo changes.
                playStepMs = playStepMs * 0.72 + observed * 0.28;
            }
        }
    }

    if (stepPosition != playStepPosition)
    {
        playStepPosition = stepPosition;
        playStepStartMs = now;
    }

    // Repaint even while the notifier remains on the same 1/16 step so the
    // timer can interpolate continuously between engine notifications.
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
        // A small amount of bounded extrapolation avoids a visible freeze if a
        // host UI notification arrives a frame late.
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
    g.fillAll(c(outside));

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

    g.setColour(c(bg));
    g.fillRect(gridLeft, 0, activeWidth, getHeight());
    g.setColour(c(panel));
    g.fillRect(gridLeft, 0, activeWidth, rulerHeight);

    // Alternating bars were one of the useful visual anchors in Alpha 10.
    for (int barStart = 0, bar = 0; barStart < length; barStart += barTicks, ++bar)
    {
        const float x0 = xForTick(static_cast<float>(barStart));
        const float x1 = xForTick(static_cast<float>(juce::jmin(length, barStart + barTicks)));
        if ((bar & 1) != 0)
        {
            g.setColour(c(panelRaised).withAlpha(0.25f));
            g.fillRect(juce::Rectangle<float>(
                x0, 0.0f, x1 - x0, static_cast<float>(getHeight())));
        }

        g.setColour(c(text));
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText("BAR " + juce::String(bar + 1),
                   static_cast<int>(std::round(x0)) + 8, 4,
                   juce::jmax(44, static_cast<int>(std::round(x1 - x0)) - 12),
                   17, juce::Justification::centredLeft, false);
    }

    // Ruler hierarchy: bars > beats > primary subdivision > fine subdivision.
    for (int tick = 0; tick <= length; tick += barTicks)
    {
        const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
        g.setColour(c(accent2).withAlpha(0.84f));
        g.fillRect(x - 1, 0, 2, rulerHeight);
    }

    for (int tick = 0; tick <= length; tick += beatTicks)
    {
        if (tick % barTicks == 0)
            continue;
        const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
        g.setColour(c(border).brighter(0.48f).withAlpha(0.98f));
        g.fillRect(x, rulerHeight - 18, 1, 18);
    }

    for (int tick = 0; tick <= length; tick += majorSubdivisionTicks)
    {
        if (tick % barTicks == 0 || tick % beatTicks == 0)
            continue;
        const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
        g.setColour(c(border).brighter(0.08f).withAlpha(0.84f));
        g.fillRect(x, rulerHeight - 9, 1, 9);
    }

    if (fineGrid)
    {
        for (int tick = 0; tick <= length; tick += minorSubdivisionTicks)
        {
            if (tick % majorSubdivisionTicks == 0)
                continue;
            const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
            g.setColour(c(border).withAlpha(0.52f));
            g.fillRect(x, rulerHeight - 5, 1, 5);
        }
    }

    // Beat numbers are deliberately independent of snap mode.
    for (int barStart = 0; barStart < length; barStart += barTicks)
    {
        for (int beat = 0; beat < meterNumerator; ++beat)
        {
            const int tick = barStart + beat * beatTicks;
            if (tick >= length)
                break;
            const float x = xForTick(static_cast<float>(tick));
            g.setColour(c(muted).withAlpha(0.90f));
            g.setFont(10.0f);
            g.drawText(juce::String(beat + 1),
                       static_cast<int>(std::round(x)) + 5, 21,
                       juce::jmax(20, static_cast<int>(
                           std::round(xForTick(static_cast<float>(juce::jmin(length, tick + beatTicks))) - x)) - 6),
                       13, juce::Justification::centredLeft, false);
        }
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

        for (int barStart = 0, bar = 0; barStart < length; barStart += barTicks, ++bar)
        {
            if ((bar & 1) == 0)
                continue;
            const float x0 = xForTick(static_cast<float>(barStart));
            const float x1 = xForTick(static_cast<float>(juce::jmin(length, barStart + barTicks)));
            g.setColour(c(panelSoft).withAlpha(0.10f));
            g.fillRect(juce::Rectangle<float>(
                x0, static_cast<float>(item.y), x1 - x0, static_cast<float>(item.height)));
        }

        g.setColour(c(border).withAlpha(0.42f));
        g.drawHorizontalLine(item.y + item.height - 1,
                             static_cast<float>(gridLeft),
                             static_cast<float>(gridRight));

        const float rowMid = static_cast<float>(item.y) + item.height * 0.5f;

        for (int tick = 0; tick <= length; tick += barTicks)
        {
            const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
            g.setColour(c(accent2).withAlpha(0.74f));
            g.fillRect(x - 1, item.y, 2, item.height);
        }

        for (int tick = 0; tick <= length; tick += beatTicks)
        {
            if (tick % barTicks == 0)
                continue;
            const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
            g.setColour(c(border).brighter(0.42f).withAlpha(0.96f));
            g.fillRect(x, item.y, 1, item.height);
        }

        for (int tick = 0; tick <= length; tick += majorSubdivisionTicks)
        {
            if (tick % barTicks == 0 || tick % beatTicks == 0)
                continue;
            const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
            g.setColour(c(border).brighter(0.04f).withAlpha(0.78f));
            g.drawVerticalLine(x, rowMid - 7.0f, rowMid + 7.0f);
        }

        if (fineGrid)
        {
            for (int tick = 0; tick <= length; tick += minorSubdivisionTicks)
            {
                if (tick % majorSubdivisionTicks == 0)
                    continue;
                const int x = static_cast<int>(std::round(xForTick(static_cast<float>(tick))));
                g.setColour(c(border).withAlpha(0.46f));
                g.drawVerticalLine(x, rowMid - 4.0f, rowMid + 4.0f);
            }
        }
    }

    if (toolMode == ToolMode::draw && hoverCanonicalRow >= 0 && hoverTick >= 0)
    {
        const int y = layoutYForCanonical(hoverCanonicalRow);
        if (y >= 0)
        {
            const float x = xForTick(static_cast<float>(hoverTick));
            const float w = juce::jmax(
                3.0f, static_cast<float>(snapTicks) / GGD_EVENT_PPQ
                    * pixelsPerQuarter * zoomScale);
            g.setColour(c(accent).withAlpha(0.075f));
            g.fillRoundedRectangle(
                x - w * 0.5f + 1.0f, static_cast<float>(y + 2),
                juce::jmax(1.0f, w - 2.0f), static_cast<float>(rowHeight - 4), 4.0f);
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
            juce::Rectangle<float> hit(x - hitW * 0.5f, cy - hitH * 0.5f, hitW, hitH);
            const bool ghost = event->velocity <= 55;
            const EventRef ref { canonical, event->tick };

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

            if (isSelected(ref))
            {
                g.setColour(c(text).withAlpha(0.98f));
                g.drawRoundedRectangle(
                    hit.expanded(2.5f), juce::jmin(5.0f, hitW * 0.35f + 1.0f), 1.7f);
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
            g.setColour(c(warm).withAlpha(0.55f));
            g.fillRoundedRectangle(x - 5.0f, y + rowHeight * 0.5f - 5.0f,
                                   10.0f, 10.0f, 3.0f);
        }
    }

    if (dragMode == DragMode::marquee)
    {
        g.setColour(c(accent).withAlpha(0.10f));
        g.fillRect(marqueeRect);
        g.setColour(c(accent).withAlpha(0.88f));
        g.drawRect(marqueeRect, 1.2f);
    }

    const float smoothTick = interpolatedPlayTick();
    if (smoothTick >= 0.0f)
    {
        const float playX = xForTick(smoothTick);
        g.setColour(c(accent).withAlpha(0.10f));
        g.fillRect(juce::Rectangle<float>(
            playX - 3.0f, static_cast<float>(rulerHeight), 6.0f,
            static_cast<float>(getHeight() - rulerHeight)));
        g.setColour(c(accent).withAlpha(0.94f));
        g.fillRect(juce::Rectangle<float>(
            playX - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight())));
    }

    g.setColour(c(accent2).withAlpha(0.82f));
    g.fillRect(gridRight - 1, 0, 2, getHeight());

    // Sticky labels are painted after the playhead so the timeline never bleeds
    // through the articulation column while horizontally scrolled.
    g.setColour(c(panel));
    g.fillRect(stickyX, 0, nameWidth, rulerHeight);
    g.setColour(c(text));
    g.setFont(juce::Font(11.5f, juce::Font::bold));
    g.drawText(toolMode == ToolMode::draw
                   ? "ARTICULATIONS | DRAW" : "ARTICULATIONS | SELECT",
               stickyX + 16, 3, nameWidth - 32, 18,
               juce::Justification::centredLeft, false);
    g.setColour(c(muted));
    g.setFont(10.0f);

    juce::String gridText = "1/16";
    if (snapTicks == GGD_TICKS_PER_32ND) gridText = "1/32";
    else if (snapTicks == GGD_TICKS_PER_8TH_TRIPLET) gridText = "1/8T";
    else if (snapTicks == GGD_TICKS_PER_16TH_TRIPLET) gridText = "1/16T";
    const auto zoomCaption = juce::String(static_cast<int>(std::round(zoomScale * 100.0f)))
                           + "% | " + gridText + " GRID";
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
            const auto caption = juce::String(collapsed ? "[+] " : "[-] ")
                               + item.groupLabel.toUpperCase();
            g.drawText(caption, stickyX + 17, item.y, nameWidth - 30, item.height,
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
        if (item.noteName.isNotEmpty())
        {
            g.setColour(c(muted));
            g.setFont(10.0f);
            g.drawText(item.noteName, stickyX + nameWidth - 64, item.y + 1, 48,
                       item.height - 2, juce::Justification::centredRight, false);
        }
    }
}