#include "GgdDrumGridBeta.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

juce::Colour c(juce::uint32 value) { return juce::Colour(value); }

bool keyMatchesLetter(const juce::KeyPress& key, char upper)
{
    const int code = key.getKeyCode();
    return code == static_cast<int>(upper)
        || code == static_cast<int>(upper + ('a' - 'A'));
}
}

GgdDrumGrid::GgdDrumGrid(SeqAudioProcessor& p,
                         const juce::Array<GgdCanonicalRow>& rows,
                         std::function<void()> undoFn,
                         std::function<void()> redoFn,
                         std::function<void()> publishFn,
                         std::function<void(float)> zoomFn,
                         std::function<void(ToolMode)> toolFn,
                         std::function<void()> selectionFn)
    : processor(p),
      canonicalRows(rows),
      undoCallback(std::move(undoFn)),
      redoCallback(std::move(redoFn)),
      publishCallback(std::move(publishFn)),
      zoomCallback(std::move(zoomFn)),
      toolCallback(std::move(toolFn)),
      selectionCallback(std::move(selectionFn))
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    rebuildLayout();
}

SequenceLayer* GgdDrumGrid::layer() const
{
    return processor.mData.getUISeqData()->getLayer(0);
}

GgdEventPattern* GgdDrumGrid::pattern() const
{
    auto* l = layer();
    return l->getEventPattern(l->getCurrentPattern());
}

int GgdDrumGrid::storageRowForCanonical(int canonicalRow) const
{
    const int count = juce::jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, canonicalRows.size());
    return (SEQ_MAX_ROWS - count) + canonicalRow;
}

int GgdDrumGrid::canonicalRowForStorage(int storageRow) const
{
    const int count = juce::jlimit(SEQ_MIN_ROWS, SEQ_MAX_ROWS, canonicalRows.size());
    const int canonical = storageRow - (SEQ_MAX_ROWS - count);
    return canonical >= 0 && canonical < canonicalRows.size() ? canonical : -1;
}

int GgdDrumGrid::patternLengthTicks() const
{
    const auto* p = pattern();
    return p != nullptr && p->isActive()
        ? p->getLengthTicks()
        : GGD_EVENT_PPQ * 4;
}

int GgdDrumGrid::ticksPerBar() const
{
    return juce::jmax(1, GGD_EVENT_PPQ * meterNumerator * 4 / meterDenominator);
}

float GgdDrumGrid::xForTick(float tick) const
{
    return static_cast<float>(nameWidth)
         + (tick / static_cast<float>(GGD_EVENT_PPQ)) * pixelsPerQuarter * zoomScale;
}

float GgdDrumGrid::tickForX(float x) const
{
    return (x - static_cast<float>(nameWidth))
         / (pixelsPerQuarter * zoomScale) * static_cast<float>(GGD_EVENT_PPQ);
}

int GgdDrumGrid::snappedTickForX(float x) const
{
    const float raw = tickForX(x);
    const int snapped = static_cast<int>(std::llround(raw / snapTicks)) * snapTicks;
    return juce::jlimit(0, juce::jmax(0, patternLengthTicks() - 1), snapped);
}

int GgdDrumGrid::currentViewX() const
{
    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        return viewport->getViewPositionX();
    return 0;
}

const GgdDrumGrid::LayoutItem* GgdDrumGrid::itemAtY(float y) const
{
    for (const auto& item : layout)
        if (y >= item.y && y < item.y + item.height)
            return &item;
    return nullptr;
}

int GgdDrumGrid::canonicalRowAtY(float y) const
{
    const auto* item = itemAtY(y);
    return item != nullptr && !item->header ? item->canonicalRow : -1;
}

int GgdDrumGrid::layoutYForCanonical(int row) const
{
    for (const auto& item : layout)
        if (!item.header && item.canonicalRow == row)
            return item.y;
    return -1;
}

void GgdDrumGrid::setMap(const GgdKitMap* newMap)
{
    map = newMap;
    clearSelection(false);
    rebuildLayout();
    repaint();
}

void GgdDrumGrid::setMeter(int numerator, int denominator, int bars)
{
    meterNumerator = juce::jmax(1, numerator);
    meterDenominator = juce::jmax(1, denominator);
    meterBars = juce::jmax(1, bars);
    clearSelection(false);
    rebuildLayout();
    repaint();
}

void GgdDrumGrid::setSnapTicks(int ticks)
{
    snapTicks = juce::jmax(1, ticks);
    repaint();
}

void GgdDrumGrid::refreshSize()
{
    rebuildLayout();
    repaint();
}

void GgdDrumGrid::patternChanged()
{
    clearSelection(false);
    dragMode = DragMode::none;
    rebuildLayout();
    repaint();
    notifySelectionChanged();
}

void GgdDrumGrid::setToolMode(ToolMode mode)
{
    if (toolMode == mode)
        return;
    toolMode = mode;
    dragMode = DragMode::none;
    marqueeRect = {};
    setMouseCursor(toolMode == ToolMode::draw
        ? juce::MouseCursor::CrosshairCursor
        : juce::MouseCursor::NormalCursor);
    if (toolCallback)
        toolCallback(toolMode);
    repaint();
}

void GgdDrumGrid::setPlayPosition(int stepPosition)
{
    if (playStepPosition == stepPosition)
        return;
    playStepPosition = stepPosition;
    repaint();
}

float GgdDrumGrid::snapZoom(float scale) const
{
    const float clamped = juce::jlimit(minZoomScale, maxZoomScale, scale);
    return std::round(clamped * 4.0f) / 4.0f;
}

void GgdDrumGrid::notifyZoomChanged()
{
    if (zoomCallback)
        zoomCallback(zoomScale);
}

void GgdDrumGrid::applyZoom(float scale, float anchorContent, float anchorViewport)
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    const float oldScale = zoomScale;
    zoomScale = snapZoom(scale);
    if (std::abs(zoomScale - oldScale) < 0.001f)
        return;

    const float musicalTick = tickForX(anchorContent);
    rebuildLayout();

    if (viewport != nullptr)
    {
        const float newAnchorContent = xForTick(musicalTick);
        const int target = juce::jmax(
            0, static_cast<int>(std::round(newAnchorContent - anchorViewport)));
        viewport->setViewPosition(target, viewport->getViewPositionY());
    }

    notifyZoomChanged();
    repaint();
}

void GgdDrumGrid::setZoomScale(float scale)
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    if (viewport == nullptr)
    {
        zoomScale = snapZoom(scale);
        rebuildLayout();
        notifyZoomChanged();
        repaint();
        return;
    }

    const int viewX = viewport->getViewPositionX();
    const float anchorViewport = static_cast<float>(viewport->getViewWidth()) * 0.5f;
    applyZoom(scale, static_cast<float>(viewX) + anchorViewport, anchorViewport);
}

void GgdDrumGrid::resetZoom()
{
    setZoomScale(1.25f);
}

void GgdDrumGrid::rebuildLayout()
{
    layout.clear();
    int y = rulerHeight;
    juce::String currentGroup;

    for (int row = 0; row < canonicalRows.size(); ++row)
    {
        const auto& canonical = canonicalRows.getReference(row);
        if (canonical.groupId != currentGroup)
        {
            currentGroup = canonical.groupId;
            LayoutItem header;
            header.header = true;
            header.y = y;
            header.height = headerHeight;
            header.groupId = canonical.groupId;
            header.groupLabel = canonical.groupLabel;
            layout.push_back(std::move(header));
            y += headerHeight;
        }

        if (collapsedGroups.count(canonical.groupId) != 0)
            continue;

        LayoutItem item;
        item.y = y;
        item.height = rowHeight;
        item.canonicalRow = row;
        item.groupId = canonical.groupId;
        item.groupLabel = canonical.groupLabel;
        item.label = canonical.defaultLabel;

        if (map != nullptr)
        {
            if (const auto* articulation = map->findArticulation(canonical.semanticId))
            {
                if (articulation->label.isNotEmpty())
                    item.label = articulation->label;
                if (const auto* binding = articulation->primaryNoteBinding())
                    item.noteName = binding->noteName;
            }
        }

        layout.push_back(std::move(item));
        y += rowHeight;
    }

    const int width = static_cast<int>(std::ceil(xForTick(
        static_cast<float>(patternLengthTicks())))) + 24;
    setSize(juce::jmax(500, width), juce::jmax(200, y + 8));
}

void GgdDrumGrid::paint(juce::Graphics& g)
{
    g.fillAll(c(outside));

    const int length = patternLengthTicks();
    const int barTicks = ticksPerBar();
    const int meterBeatTicks = juce::jmax(1, GGD_EVENT_PPQ * 4 / meterDenominator);
    const int gridLeft = nameWidth;
    const int gridRight = static_cast<int>(std::ceil(xForTick(static_cast<float>(length))));
    const int activeWidth = juce::jmax(0, gridRight - gridLeft);
    const int stickyX = currentViewX();

    g.setColour(c(bg));
    g.fillRect(gridLeft, 0, activeWidth, getHeight());
    g.setColour(c(panel));
    g.fillRect(gridLeft, 0, activeWidth, rulerHeight);

    for (int barStart = 0, bar = 0; barStart < length; barStart += barTicks, ++bar)
    {
        const float x0 = xForTick(static_cast<float>(barStart));
        const float x1 = xForTick(static_cast<float>(juce::jmin(length, barStart + barTicks)));
        if ((bar & 1) != 0)
        {
            g.setColour(c(panelRaised).withAlpha(0.24f));
            g.fillRect(juce::Rectangle<float>(x0, 0.0f, x1 - x0,
                                               static_cast<float>(getHeight())));
        }
        g.setColour(c(accent2).withAlpha(0.72f));
        g.drawVerticalLine(static_cast<int>(std::round(x0)), 0.0f,
                           static_cast<float>(getHeight()));
        g.setColour(c(text));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("BAR " + juce::String(bar + 1),
                   static_cast<int>(x0) + 7, 3,
                   juce::jmax(36, static_cast<int>(x1 - x0) - 9), 17,
                   juce::Justification::centredLeft, false);
    }

    for (int tick = 0; tick <= length; tick += snapTicks)
    {
        const float x = xForTick(static_cast<float>(tick));
        const bool bar = (tick % barTicks) == 0;
        const bool beat = (tick % meterBeatTicks) == 0;
        g.setColour(bar ? c(accent2).withAlpha(0.70f)
                        : beat ? c(border).brighter(0.28f).withAlpha(0.95f)
                               : c(border).withAlpha(0.48f));
        g.drawVerticalLine(static_cast<int>(std::round(x)),
                           static_cast<float>(rulerHeight - (bar ? 15 : beat ? 10 : 6)),
                           static_cast<float>(getHeight()));
    }

    for (const auto& item : layout)
    {
        if (item.header)
        {
            g.setColour(c(panelRaised));
            g.fillRect(gridLeft, item.y, activeWidth, item.height);
            g.setColour(c(accent2).withAlpha(0.38f));
            g.drawHorizontalLine(item.y + item.height - 1,
                                 static_cast<float>(gridLeft),
                                 static_cast<float>(gridRight));
            continue;
        }

        g.setColour((item.canonicalRow & 1) != 0
            ? c(panelRaised).withAlpha(0.26f)
            : c(bg));
        g.fillRect(gridLeft, item.y, activeWidth, item.height);
        g.setColour(c(border).withAlpha(0.48f));
        g.drawHorizontalLine(item.y + item.height - 1,
                             static_cast<float>(gridLeft),
                             static_cast<float>(gridRight));
    }

    if (toolMode == ToolMode::draw && hoverCanonicalRow >= 0 && hoverTick >= 0)
    {
        const int y = layoutYForCanonical(hoverCanonicalRow);
        if (y >= 0)
        {
            const float x = xForTick(static_cast<float>(hoverTick));
            const float w = juce::jmax(3.0f,
                static_cast<float>(snapTicks) / GGD_EVENT_PPQ * pixelsPerQuarter * zoomScale);
            g.setColour(c(accent).withAlpha(0.08f));
            g.fillRoundedRectangle(x - w * 0.5f + 1.0f,
                                   static_cast<float>(y + 3), w - 2.0f,
                                   static_cast<float>(rowHeight - 6), 3.0f);
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
            const float hitW = juce::jlimit(7.0f, 28.0f, slotPixels * 0.60f);
            const float v = static_cast<float>(event->velocity) / 127.0f;
            const float hitH = 7.0f + v * 14.0f;
            const float cy = y + rowHeight * 0.5f;
            juce::Rectangle<float> hit(x - hitW * 0.5f, cy - hitH * 0.5f, hitW, hitH);
            const bool ghost = event->velocity <= 55;
            const EventRef ref { canonical, event->tick };

            if (ghost)
            {
                g.setColour(c(accent).withAlpha(0.28f + 0.45f * v));
                g.drawRoundedRectangle(hit, 3.0f, 1.5f);
            }
            else
            {
                g.setColour(c(accent).withAlpha(0.52f + 0.44f * v));
                g.fillRoundedRectangle(hit, 3.0f);
            }

            if (isSelected(ref))
            {
                g.setColour(c(text).withAlpha(0.96f));
                g.drawRoundedRectangle(hit.expanded(2.0f), 4.0f, 1.6f);
            }
        }
    }

    if (playStepPosition >= 0)
    {
        const int tick = playStepPosition * GGD_TICKS_PER_16TH;
        if (tick >= 0 && tick < length)
        {
            const float x = xForTick(static_cast<float>(tick));
            g.setColour(c(warm).withAlpha(0.90f));
            g.fillRect(juce::Rectangle<float>(x - 1.0f, 0.0f, 2.0f,
                                               static_cast<float>(getHeight())));
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
        g.setColour(c(accent).withAlpha(0.12f));
        g.fillRect(marqueeRect);
        g.setColour(c(accent).withAlpha(0.90f));
        g.drawRect(marqueeRect, 1.0f);
    }

    // Sticky row labels and family headers are painted last so timeline content
    // cannot bleed through them while horizontally scrolled.
    g.setColour(c(panel));
    g.fillRect(stickyX, 0, nameWidth, rulerHeight);
    g.setColour(c(border));
    g.drawVerticalLine(stickyX + nameWidth - 1, 0.0f,
                       static_cast<float>(getHeight()));

    for (const auto& item : layout)
    {
        if (item.header)
        {
            g.setColour(c(panelRaised));
            g.fillRect(stickyX, item.y, nameWidth, item.height);
            g.setColour(c(accent2));
            g.setFont(juce::Font(10.5f, juce::Font::bold));
            const bool collapsed = collapsedGroups.count(item.groupId) != 0;
            g.drawText(collapsed ? ">" : "v", stickyX + 8, item.y, 16, item.height,
                       juce::Justification::centred, false);
            g.setColour(c(text).withAlpha(0.90f));
            g.drawText(item.groupLabel.toUpperCase(), stickyX + 28, item.y,
                       nameWidth - 34, item.height,
                       juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour((item.canonicalRow & 1) != 0
                ? c(panelRaised).withAlpha(0.95f) : c(panel));
            g.fillRect(stickyX, item.y, nameWidth, item.height);
            g.setColour(c(text));
            g.setFont(11.0f);
            g.drawText(item.label, stickyX + 11, item.y,
                       nameWidth - 58, item.height,
                       juce::Justification::centredLeft, true);
            if (item.noteName.isNotEmpty())
            {
                g.setColour(c(muted));
                g.setFont(9.5f);
                g.drawText(item.noteName, stickyX + nameWidth - 48, item.y,
                           39, item.height,
                           juce::Justification::centredRight, false);
            }
        }
    }
}

bool GgdDrumGrid::sameEvent(const EventRef& a, const EventRef& b) const
{
    return a.row == b.row && a.tick == b.tick;
}

bool GgdDrumGrid::isSelected(const EventRef& ref) const
{
    return std::any_of(selection.begin(), selection.end(),
                       [&](const EventRef& value) { return sameEvent(value, ref); });
}

void GgdDrumGrid::notifySelectionChanged()
{
    if (selectionCallback)
        selectionCallback();
}

void GgdDrumGrid::addSelection(const EventRef& ref)
{
    if (!isSelected(ref))
        selection.push_back(ref);
}

void GgdDrumGrid::removeSelection(const EventRef& ref)
{
    selection.erase(std::remove_if(selection.begin(), selection.end(),
                                   [&](const EventRef& value)
                                   {
                                       return sameEvent(value, ref);
                                   }), selection.end());
}

void GgdDrumGrid::toggleSelection(const EventRef& ref)
{
    if (isSelected(ref))
        removeSelection(ref);
    else
        addSelection(ref);
    notifySelectionChanged();
    repaint();
}

void GgdDrumGrid::clearSelection(bool notify)
{
    if (selection.empty())
        return;
    selection.clear();
    if (notify)
        notifySelectionChanged();
    repaint();
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
    const float threshold = juce::jlimit(6.0f, 13.0f,
        static_cast<float>(snapTicks) / GGD_EVENT_PPQ * pixelsPerQuarter * zoomScale * 0.45f);
    float best = std::numeric_limits<float>::max();
    int bestTick = -1;

    for (int i = 0; i < p->getEventCount(); ++i)
    {
        const auto* event = p->getEvent(i);
        if (event == nullptr || event->row != storage)
            continue;
        const float distance = std::abs(x - xForTick(static_cast<float>(event->tick)));
        if (distance <= threshold && distance < best)
        {
            best = distance;
            bestTick = event->tick;
        }
    }

    return { canonical, bestTick };
}

GgdDrumGrid::EventState GgdDrumGrid::snapshotEvent(const EventRef& ref) const
{
    EventState result;
    result.ref = ref;
    if (const auto* p = pattern())
    {
        if (const auto* event = p->findEventPtr(storageRowForCanonical(ref.row), ref.tick))
        {
            result.velocity = event->velocity;
            result.probability = event->probability;
            result.durationTicks = event->durationTicks;
        }
    }
    return result;
}

bool GgdDrumGrid::eventOccupied(int row, int tick) const
{
    const auto* p = pattern();
    return p != nullptr
        && p->findEvent(storageRowForCanonical(row), tick) >= 0;
}

bool GgdDrumGrid::writeEvent(const EventState& state, int row, int tick)
{
    auto* p = pattern();
    if (p == nullptr || row < 0 || row >= canonicalRows.size()
        || tick < 0 || tick >= p->getLengthTicks())
        return false;

    return p->addEvent(storageRowForCanonical(row), tick,
                       state.velocity, state.probability, state.durationTicks);
}

bool GgdDrumGrid::removeEvent(const EventRef& ref)
{
    auto* p = pattern();
    return p != nullptr
        && p->removeEvent(storageRowForCanonical(ref.row), ref.tick);
}

void GgdDrumGrid::publishChange()
{
    if (publishCallback)
        publishCallback();
    notifySelectionChanged();
    repaint();
}

void GgdDrumGrid::repaintAndResize()
{
    rebuildLayout();
    repaint();
}

void GgdDrumGrid::paintAt(int row, int tick)
{
    if (row < 0 || tick < 0 || row == lastPaintRow && tick == lastPaintTick)
        return;

    lastPaintRow = row;
    lastPaintTick = tick;
    bool changed = false;
    if (paintErase)
    {
        if (eventOccupied(row, tick))
            changed = removeEvent({ row, tick });
    }
    else if (!eventOccupied(row, tick))
    {
        EventState state;
        state.ref = { row, tick };
        state.durationTicks = juce::jmin(GGD_DEFAULT_EVENT_DURATION_TICKS,
                                         juce::jmax(1, snapTicks));
        changed = writeEvent(state, row, tick);
    }

    if (changed)
        publishChange();
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
    if (row < 0 || e.position.x < nameWidth)
        return;

    const auto hit = eventAt(e.position.x, e.position.y);

    if (toolMode == ToolMode::draw)
    {
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
    if (dragMode == DragMode::paint)
    {
        const int row = canonicalRowAtY(e.position.y);
        if (row >= 0 && e.position.x >= nameWidth)
            paintAt(row, snappedTickForX(e.position.x));
        return;
    }

    if (dragMode == DragMode::velocity && dragEvent.tick >= 0)
    {
        auto* p = pattern();
        if (p == nullptr)
            return;
        auto* event = p->findEventPtr(storageRowForCanonical(dragEvent.row), dragEvent.tick);
        if (event == nullptr)
            return;
        const int delta = static_cast<int>(std::round((dragStartPoint.y - e.position.y) * 0.8f));
        const int velocity = juce::jlimit(1, 127, dragStartVelocity + delta);
        if (event->velocity != velocity)
        {
            event->velocity = static_cast<std::uint8_t>(velocity);
            publishChange();
        }
        return;
    }

    if (dragMode == DragMode::timing && dragEvent.tick >= 0)
    {
        auto* p = pattern();
        if (p == nullptr)
            return;
        const float pixelsPerTick = pixelsPerQuarter * zoomScale / GGD_EVENT_PPQ;
        const int delta = static_cast<int>(std::llround(
            (e.position.x - dragStartPoint.x) / pixelsPerTick));
        const int target = juce::jlimit(0, p->getLengthTicks() - 1, dragStartTick + delta);
        if (target != dragEvent.tick
            && !eventOccupied(dragEvent.row, target)
            && p->moveEvent(storageRowForCanonical(dragEvent.row), dragEvent.tick,
                            storageRowForCanonical(dragEvent.row), target))
        {
            dragEvent.tick = target;
            publishChange();
        }
        return;
    }

    if (dragMode == DragMode::marquee)
    {
        marqueeRect = juce::Rectangle<float>(marqueeStart, e.position).getSmallestIntegerContainer().toFloat();
        updateMarqueeSelection();
        repaint();
        return;
    }

    if (dragMode == DragMode::move)
    {
        const float pixelsPerTick = pixelsPerQuarter * zoomScale / GGD_EVENT_PPQ;
        dragDeltaTicks = static_cast<int>(std::llround(
            (e.position.x - dragStartPoint.x) / pixelsPerTick / snapTicks)) * snapTicks;
        dragDeltaRows = static_cast<int>(std::llround(
            (e.position.y - dragStartPoint.y) / static_cast<float>(rowHeight)));
        repaint();
    }
}

void GgdDrumGrid::mouseUp(const juce::MouseEvent&)
{
    if (dragMode == DragMode::move)
        finishSelectionMove();
    else if (dragMode == DragMode::marquee)
        notifySelectionChanged();

    dragMode = DragMode::none;
    marqueeRect = {};
    lastPaintRow = -1;
    lastPaintTick = -1;
    repaint();
}

void GgdDrumGrid::mouseMove(const juce::MouseEvent& e)
{
    const int row = canonicalRowAtY(e.position.y);
    const int tick = row >= 0 && e.position.x >= nameWidth
        ? snappedTickForX(e.position.x) : -1;
    if (row != hoverCanonicalRow || tick != hoverTick)
    {
        hoverCanonicalRow = row;
        hoverTick = tick;
        repaint();
    }
}

void GgdDrumGrid::mouseExit(const juce::MouseEvent&)
{
    hoverCanonicalRow = -1;
    hoverTick = -1;
    repaint();
}

void GgdDrumGrid::mouseWheelMove(const juce::MouseEvent& e,
                                 const juce::MouseWheelDetails& wheel)
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        const float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
        const float anchorContent = e.position.x;
        const float anchorViewport = viewport != nullptr
            ? e.position.x - viewport->getViewPositionX() : e.position.x;
        applyZoom(zoomScale + (delta > 0.0f ? 0.25f : -0.25f),
                  anchorContent, anchorViewport);
        return;
    }

    if (viewport == nullptr)
        return;

    const bool horizontal = e.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY);
    if (horizontal)
    {
        const float delta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
        viewport->setViewPosition(
            juce::jmax(0, viewport->getViewPositionX() - static_cast<int>(delta * 180.0f)),
            viewport->getViewPositionY());
    }
    else
    {
        viewport->setViewPosition(
            viewport->getViewPositionX(),
            juce::jmax(0, viewport->getViewPositionY() - static_cast<int>(wheel.deltaY * 180.0f)));
    }
}

void GgdDrumGrid::updateMarqueeSelection()
{
    selection = marqueeBaseSelection;
    const auto* p = pattern();
    if (p == nullptr)
        return;

    for (int i = 0; i < p->getEventCount(); ++i)
    {
        const auto* event = p->getEvent(i);
        if (event == nullptr)
            continue;
        const int canonical = canonicalRowForStorage(event->row);
        const int y = layoutYForCanonical(canonical);
        if (canonical < 0 || y < 0)
            continue;
        const juce::Point<float> point(
            xForTick(static_cast<float>(event->tick)),
            y + rowHeight * 0.5f);
        if (marqueeRect.contains(point))
            addSelection({ canonical, event->tick });
    }
    notifySelectionChanged();
}

void GgdDrumGrid::beginSelectionMove(const juce::MouseEvent& e, bool duplicate)
{
    dragMode = DragMode::move;
    dragStartPoint = e.position;
    duplicateDrag = duplicate;
    dragDeltaTicks = 0;
    dragDeltaRows = 0;
    dragSelectionStates.clear();
    dragSelectionStates.reserve(selection.size());
    for (const auto& ref : selection)
        dragSelectionStates.push_back(snapshotEvent(ref));
}

void GgdDrumGrid::finishSelectionMove()
{
    if (dragDeltaTicks != 0 || dragDeltaRows != 0)
        moveSelectionBy(dragDeltaTicks, dragDeltaRows, duplicateDrag);
    dragSelectionStates.clear();
    dragDeltaTicks = 0;
    dragDeltaRows = 0;
}

bool GgdDrumGrid::moveSelectionBy(int deltaTicks, int deltaRows, bool duplicate)
{
    if (selection.empty() || (deltaTicks == 0 && deltaRows == 0))
        return false;

    const int length = patternLengthTicks();
    std::vector<EventState> states;
    states.reserve(selection.size());
    for (const auto& ref : selection)
        states.push_back(snapshotEvent(ref));

    for (const auto& state : states)
    {
        const int row = state.ref.row + deltaRows;
        const int tick = state.ref.tick + deltaTicks;
        if (row < 0 || row >= canonicalRows.size() || tick < 0 || tick >= length)
            return false;

        if (eventOccupied(row, tick))
        {
            const EventRef target { row, tick };
            if (duplicate || !isSelected(target))
                return false;
        }
    }

    if (!duplicate)
        for (const auto& state : states)
            removeEvent(state.ref);

    std::vector<EventRef> moved;
    moved.reserve(states.size());
    for (const auto& state : states)
    {
        const int row = state.ref.row + deltaRows;
        const int tick = state.ref.tick + deltaTicks;
        if (writeEvent(state, row, tick))
            moved.push_back({ row, tick });
    }

    selection = std::move(moved);
    publishChange();
    return true;
}

bool GgdDrumGrid::nudgeSelection(int horizontal, int vertical, bool duplicate)
{
    if (selection.empty())
        return false;
    return moveSelectionBy(horizontal * snapTicks, vertical, duplicate);
}

void GgdDrumGrid::selectAllHits()
{
    selection.clear();
    const auto* p = pattern();
    if (p != nullptr)
    {
        for (int i = 0; i < p->getEventCount(); ++i)
        {
            const auto* event = p->getEvent(i);
            if (event == nullptr)
                continue;
            const int row = canonicalRowForStorage(event->row);
            if (row >= 0)
                selection.push_back({ row, event->tick });
        }
    }
    notifySelectionChanged();
    repaint();
}

void GgdDrumGrid::selectAll()
{
    selectAllHits();
}

void GgdDrumGrid::copySelectionToClipboard()
{
    clipboard.clear();
    if (selection.empty())
        return;

    int minTick = selection.front().tick;
    int maxTick = minTick;
    for (const auto& ref : selection)
    {
        minTick = juce::jmin(minTick, ref.tick);
        maxTick = juce::jmax(maxTick, ref.tick);
    }

    clipboard.reserve(selection.size());
    for (const auto& ref : selection)
    {
        auto state = snapshotEvent(ref);
        state.ref.tick -= minTick;
        clipboard.push_back(state);
    }
    clipboardSpanTicks = juce::jmax(snapTicks, maxTick - minTick + snapTicks);
}

void GgdDrumGrid::pasteClipboard()
{
    if (clipboard.empty())
        return;

    const int length = patternLengthTicks();
    int anchor = 0;
    if (!selection.empty())
    {
        for (const auto& ref : selection)
            anchor = juce::jmax(anchor, ref.tick + snapTicks);
    }
    if (anchor + clipboardSpanTicks > length)
        anchor = 0;

    auto fits = [&](int candidate)
    {
        for (const auto& state : clipboard)
        {
            const int tick = candidate + state.ref.tick;
            if (tick < 0 || tick >= length || eventOccupied(state.ref.row, tick))
                return false;
        }
        return true;
    };

    while (anchor < length && !fits(anchor))
        anchor += snapTicks;
    if (anchor >= length)
        return;

    selection.clear();
    for (const auto& state : clipboard)
    {
        const int tick = anchor + state.ref.tick;
        if (writeEvent(state, state.ref.row, tick))
            selection.push_back({ state.ref.row, tick });
    }
    publishChange();
}

void GgdDrumGrid::adjustSelectionVelocity(int delta)
{
    if (selection.empty() || delta == 0)
        return;

    bool changed = false;
    auto* p = pattern();
    if (p == nullptr)
        return;
    for (const auto& ref : selection)
    {
        auto* event = p->findEventPtr(storageRowForCanonical(ref.row), ref.tick);
        if (event == nullptr)
            continue;
        const int next = juce::jlimit(1, 127, static_cast<int>(event->velocity) + delta);
        if (next != event->velocity)
        {
            event->velocity = static_cast<std::uint8_t>(next);
            changed = true;
        }
    }
    if (changed)
        publishChange();
}

void GgdDrumGrid::adjustSelectedVelocityBy(int delta)
{
    adjustSelectionVelocity(delta);
}

void GgdDrumGrid::adjustSelectedTimingBy(int deltaTicks)
{
    moveSelectionBy(deltaTicks, 0, false);
}

void GgdDrumGrid::quantizeSelected()
{
    if (selection.empty())
        return;

    std::vector<EventState> states;
    states.reserve(selection.size());
    for (const auto& ref : selection)
        states.push_back(snapshotEvent(ref));

    std::vector<EventRef> targets;
    targets.reserve(states.size());
    for (const auto& state : states)
    {
        const int tick = juce::jlimit(
            0, patternLengthTicks() - 1,
            static_cast<int>(std::llround(
                static_cast<double>(state.ref.tick) / snapTicks)) * snapTicks);
        const EventRef target { state.ref.row, tick };
        if (std::any_of(targets.begin(), targets.end(),
                        [&](const EventRef& other) { return sameEvent(target, other); }))
            return;
        if (eventOccupied(target.row, target.tick) && !isSelected(target))
            return;
        targets.push_back(target);
    }

    for (const auto& state : states)
        removeEvent(state.ref);
    selection.clear();
    for (size_t i = 0; i < states.size(); ++i)
    {
        if (writeEvent(states[i], targets[i].row, targets[i].tick))
            selection.push_back(targets[i]);
    }
    publishChange();
}

void GgdDrumGrid::humanizeSelected()
{
    if (selection.empty())
        return;

    juce::Random& random = juce::Random::getSystemRandom();
    std::vector<EventState> states;
    states.reserve(selection.size());
    for (const auto& ref : selection)
        states.push_back(snapshotEvent(ref));

    for (const auto& state : states)
        removeEvent(state.ref);

    selection.clear();
    for (auto state : states)
    {
        int tick = juce::jlimit(0, patternLengthTicks() - 1,
            state.ref.tick + random.nextInt(17) - 8);
        state.velocity = juce::jlimit(1, 127,
            state.velocity + random.nextInt(9) - 4);
        if (eventOccupied(state.ref.row, tick))
            tick = state.ref.tick;
        if (writeEvent(state, state.ref.row, tick))
            selection.push_back({ state.ref.row, tick });
    }
    publishChange();
}

void GgdDrumGrid::deleteSelection()
{
    if (selection.empty())
        return;
    bool changed = false;
    for (const auto& ref : selection)
        changed |= removeEvent(ref);
    selection.clear();
    if (changed)
        publishChange();
    else
        notifySelectionChanged();
}

void GgdDrumGrid::deleteSelected()
{
    deleteSelection();
}

bool GgdDrumGrid::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const bool plain = !mods.isCtrlDown() && !mods.isCommandDown() && !mods.isShiftDown();
    if (!plain)
        return false;

    if (keyMatchesLetter(key, 'D') || keyMatchesLetter(key, 'S')
        || keyMatchesLetter(key, 'A') || keyMatchesLetter(key, 'C')
        || keyMatchesLetter(key, 'V') || keyMatchesLetter(key, 'Z')
        || keyMatchesLetter(key, 'Y')
        || key.getKeyCode() == juce::KeyPress::deleteKey
        || key.getKeyCode() == juce::KeyPress::backspaceKey
        || key.getKeyCode() == juce::KeyPress::escapeKey
        || key.getKeyCode() == juce::KeyPress::leftKey
        || key.getKeyCode() == juce::KeyPress::rightKey
        || key.getKeyCode() == juce::KeyPress::upKey
        || key.getKeyCode() == juce::KeyPress::downKey)
        return true;
    return false;
}

void GgdDrumGrid::pollFallbackShortcuts(bool active)
{
    auto letterDown = [](char upper)
    {
        return juce::KeyPress::isKeyCurrentlyDown(static_cast<int>(upper))
            || juce::KeyPress::isKeyCurrentlyDown(static_cast<int>(upper + ('a' - 'A')));
    };
    auto edge = [](bool down, bool& previous)
    {
        const bool pressed = down && !previous;
        previous = down;
        return pressed;
    };

    const bool d = edge(letterDown('D'), fallbackDDown);
    const bool s = edge(letterDown('S'), fallbackSDown);
    const bool a = edge(letterDown('A'), fallbackADown);
    const bool cc = edge(letterDown('C'), fallbackCDown);
    const bool v = edge(letterDown('V'), fallbackVDown);
    const bool z = edge(letterDown('Z'), fallbackZDown);
    const bool y = edge(letterDown('Y'), fallbackYDown);
    const bool left = edge(juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey), fallbackLeftDown);
    const bool right = edge(juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey), fallbackRightDown);
    const bool up = edge(juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::upKey), fallbackUpDown);
    const bool down = edge(juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::downKey), fallbackDownDown);
    const bool del = edge(juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::deleteKey), fallbackDeleteDown);
    const bool back = edge(juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::backspaceKey), fallbackBackspaceDown);
    const bool esc = edge(juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::escapeKey), fallbackEscapeDown);

    if (!active)
        return;

    const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    const bool commandFree = !mods.isCtrlDown() && !mods.isCommandDown() && !mods.isShiftDown();
    if (!commandFree)
        return;
    const bool alt = mods.isAltDown();

    if (!alt)
    {
        if (d) setToolMode(ToolMode::draw);
        else if (s) setToolMode(ToolMode::select);
        else if (z && undoCallback) undoCallback();
        else if (y && redoCallback) redoCallback();
        else if (toolMode == ToolMode::select && a) selectAllHits();
        else if (toolMode == ToolMode::select && cc) copySelectionToClipboard();
        else if (toolMode == ToolMode::select && v) pasteClipboard();
    }

    if (toolMode != ToolMode::select)
        return;
    if (!alt && (del || back)) deleteSelection();
    else if (!alt && esc) clearSelection();
    else if (left) nudgeSelection(-1, 0, alt);
    else if (right) nudgeSelection(1, 0, alt);
    else if (up) nudgeSelection(0, -1, alt);
    else if (down) nudgeSelection(0, 1, alt);
}
