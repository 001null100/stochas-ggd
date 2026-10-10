#include "GgdDrumGrid.h"
#include "GgdUiTheme.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace
{
juce::Colour tc(GgdThemeRole role)
{
    return ggdThemeColour(role);
}

double nowMs()
{
    return juce::Time::getMillisecondCounterHiRes();
}

int firstEventIndexAtOrAfter(const GgdEventPattern& pattern, int tick)
{
    int lo = 0;
    int hi = pattern.getEventCount();
    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (pattern.getEvent(mid)->tick < tick)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

// The keys the timeline owns, in a fixed order shared by key events and
// polling. KeyPress codes are not constant expressions, hence the switch.
int shortcutKeyCode(int index)
{
    switch (index)
    {
        case 0:  return 'D';
        case 1:  return 'S';
        case 2:  return 'A';
        case 3:  return 'C';
        case 4:  return 'V';
        case 5:  return 'Z';
        case 6:  return 'Y';
        case 7:  return 'Q';
        case 8:  return juce::KeyPress::leftKey;
        case 9:  return juce::KeyPress::rightKey;
        case 10: return juce::KeyPress::upKey;
        case 11: return juce::KeyPress::downKey;
        case 12: return juce::KeyPress::deleteKey;
        case 13: return juce::KeyPress::backspaceKey;
        case 14: return juce::KeyPress::escapeKey;
        default: return 0;
    }
}

int normaliseKeyCode(int keyCode)
{
    return keyCode >= 'a' && keyCode <= 'z' ? keyCode - ('a' - 'A') : keyCode;
}

int shortcutIndexForKey(int keyCode, int count)
{
    keyCode = normaliseKeyCode(keyCode);
    for (int i = 0; i < count; ++i)
        if (shortcutKeyCode(i) == keyCode)
            return i;
    return -1;
}

bool isShortcutKeyDown(int index)
{
    const int code = shortcutKeyCode(index);
    if (code >= 'A' && code <= 'Z')
        return juce::KeyPress::isKeyCurrentlyDown(code) || juce::KeyPress::isKeyCurrentlyDown(code + ('a' - 'A'));
    return juce::KeyPress::isKeyCurrentlyDown(code);
}

juce::String gridLabel(int ticks)
{
    switch (ticks)
    {
        case GGD_EVENT_PPQ / 2:          return "1/8";
        case GGD_TICKS_PER_32ND:         return "1/32";
        case GGD_TICKS_PER_8TH_TRIPLET:  return "1/8T";
        case GGD_TICKS_PER_16TH_TRIPLET: return "1/16T";
        case GGD_EVENT_PPQ / 12:         return "1/32T";
        default:                         return "1/16";
    }
}
}

GgdDrumGrid::GgdDrumGrid(SeqAudioProcessor& p, const GgdProject& proj, Callbacks cb)
    : processor(p), project(proj), callbacks(std::move(cb))
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    setRepaintsOnMouseActivity(false);
    rebuildLayout();
}

//==============================================================================
SequenceLayer* GgdDrumGrid::layer() const
{
    return processor.mData.getUISeqData()->getLayer(GgdProject::drumLayer);
}

GgdEventPattern* GgdDrumGrid::pattern() const
{
    auto* l = layer();
    return l->getEventPattern(l->getCurrentPattern());
}

int GgdDrumGrid::patternLengthTicks() const
{
    const auto* p = pattern();
    return p != nullptr && p->isActive() ? p->getLengthTicks() : GGD_EVENT_PPQ * 4;
}

int GgdDrumGrid::barTicks() const
{
    const auto* p = pattern();
    return p != nullptr ? p->getBarTicks() : GGD_EVENT_PPQ * 4;
}

int GgdDrumGrid::beatTicks() const
{
    const auto* p = pattern();
    return juce::jmax(1, GGD_EVENT_PPQ * 4 / (p != nullptr ? p->getDenominator() : 4));
}

int GgdDrumGrid::meterNumerator() const
{
    const auto* p = pattern();
    return p != nullptr ? p->getNumerator() : 4;
}

float GgdDrumGrid::xForTick(double tick) const
{
    return static_cast<float>(nameWidth) + static_cast<float>(tick) * pixelsPerTick();
}

double GgdDrumGrid::tickForX(float x) const
{
    return static_cast<double>(x - static_cast<float>(nameWidth)) / pixelsPerTick();
}

float GgdDrumGrid::hitWidth() const
{
    return juce::jlimit(8.0f, 30.0f, cellWidth() * 0.62f);
}

int GgdDrumGrid::cellTickForX(float x) const
{
    const int length = patternLengthTicks();
    const double raw = juce::jmax(0.0, tickForX(x));
    const int cell = static_cast<int>(std::floor(raw / snapTicks)) * snapTicks;
    const int lastCell = juce::jmax(0, ((length - 1) / snapTicks) * snapTicks);
    return juce::jlimit(0, lastCell, cell);
}

int GgdDrumGrid::quantize(int tick) const
{
    const int length = patternLengthTicks();
    const int maxGridTick = juce::jmax(0, ((length - 1) / snapTicks) * snapTicks);
    const int nearest = static_cast<int>(std::llround(static_cast<double>(tick) / snapTicks)) * snapTicks;
    return juce::jlimit(0, maxGridTick, nearest);
}

juce::Point<int> GgdDrumGrid::viewPosition() const
{
    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        return viewport->getViewPosition();
    return {};
}

juce::Rectangle<int> GgdDrumGrid::viewArea() const
{
    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        return viewport->getViewArea();
    return getLocalBounds();
}

bool GgdDrumGrid::overLabelColumn(juce::Point<float> position) const
{
    return position.x < static_cast<float>(viewPosition().x + nameWidth);
}

bool GgdDrumGrid::overRuler(juce::Point<float> position) const
{
    return position.y < static_cast<float>(viewPosition().y + rulerHeight);
}

const GgdDrumGrid::LayoutItem* GgdDrumGrid::itemAtY(float y) const
{
    for (const auto& item : layout)
        if (y >= item.y && y < item.y + item.height)
            return &item;
    return nullptr;
}

int GgdDrumGrid::rowAtY(float y) const
{
    const auto* item = itemAtY(y);
    return item != nullptr && !item->header ? item->canonicalRow : -1;
}

int GgdDrumGrid::rowY(int canonical) const
{
    if (const auto* item = itemForRow(canonical))
        return item->y;
    return -1;
}

const GgdDrumGrid::LayoutItem* GgdDrumGrid::itemForRow(int canonical) const
{
    for (const auto& item : layout)
        if (!item.header && item.canonicalRow == canonical)
            return &item;
    return nullptr;
}

juce::Colour GgdDrumGrid::colourForRow(int canonical) const
{
    if (!prefs.colourByInstrument)
        return tc(GgdThemeRole::accent);
    if (const auto* item = itemForRow(canonical))
        return item->colour;
    return tc(GgdThemeRole::accent);
}

//==============================================================================
void GgdDrumGrid::setKitMap(const GgdKitMap* newMap)
{
    map = newMap;
    pulses.clear();
    setSelection({}, false);
    rebuildLayout();
    repaint();
}

void GgdDrumGrid::setPreferences(const Preferences& newPreferences)
{
    prefs = newPreferences;
    if (!prefs.playedEffects)
        pulses.clear();
    if (!prefs.shiftHoverVelocity && dragMode == DragMode::none)
        hidePopup();
    rebuildLayout();
    repaint();
}

void GgdDrumGrid::patternGeometryChanged()
{
    setSelection({}, false);
    dragMode = DragMode::none;
    pulses.clear();
    hidePopup();
    hideChips();
    rebuildLayout();
    repaint();
    notifySelection();
}

void GgdDrumGrid::patternReplaced()
{
    dragMode = DragMode::none;
    hidePopup();
    hideChips();
    pulses.clear();
    // keep only selected hits that still exist
    std::vector<EventRef> kept;
    for (const auto& ref : selection)
        if (occupied(ref.row, ref.tick))
            kept.push_back(ref);
    selection = std::move(kept);
    rebuildLayout();
    repaint();
    notifySelection();
}

void GgdDrumGrid::setSnapTicks(int ticks)
{
    ticks = juce::jmax(1, ticks);
    if (snapTicks == ticks)
        return;
    snapTicks = ticks;
    repaint();
}

void GgdDrumGrid::setToolMode(ToolMode mode)
{
    if (toolMode == mode)
        return;
    toolMode = mode;
    dragMode = DragMode::none;
    marqueeRect = {};
    setMouseCursor(toolMode == ToolMode::draw ? juce::MouseCursor::CrosshairCursor
                                              : juce::MouseCursor::NormalCursor);
    if (callbacks.onToolChanged)
        callbacks.onToolChanged(toolMode);
    repaint();
}

void GgdDrumGrid::setTrailingPadding(int pixels)
{
    pixels = juce::jmax(0, pixels);
    if (pixels == trailingPadding)
        return;
    trailingPadding = pixels;
    updateSize();
}

float GgdDrumGrid::getTimelineContentWidthPixels() const
{
    return juce::jmax(0.0f, xForTick(patternLengthTicks()) - static_cast<float>(nameWidth));
}

void GgdDrumGrid::rebuildLayout()
{
    layout.clear();
    int y = rulerHeight;
    bool odd = false;
    const auto& canonicalRows = project.getCanonicalRows();

    auto addHeader = [&](const juce::String& id, const juce::String& label, const juce::Colour& colour)
    {
        LayoutItem header;
        header.header = true;
        header.y = y;
        header.height = headerHeight;
        header.groupId = id;
        header.groupLabel = label;
        header.colour = colour;
        layout.push_back(std::move(header));
        y += headerHeight;
    };

    auto addRow = [&](int canonical, const juce::String& groupId, const juce::String& groupLabel,
                      const juce::String& label, const juce::String& noteName)
    {
        LayoutItem item;
        item.y = y;
        item.height = rowHeight;
        item.canonicalRow = canonical;
        item.groupId = groupId;
        item.groupLabel = groupLabel;
        item.label = label;
        item.noteName = noteName;
        item.colour = ggdInstrumentColour(canonicalRows.getReference(canonical).semanticId);
        item.odd = odd;
        odd = !odd;
        layout.push_back(std::move(item));
        y += rowHeight;
    };

    if (map != nullptr)
    {
        // The visible rows belong to the active kit, in the kit's own order.
        for (const auto& group : map->groups)
        {
            const auto firstColour = group.articulations.isEmpty()
                ? tc(GgdThemeRole::accent)
                : ggdInstrumentColour(group.articulations.getReference(0).semanticId);
            addHeader(group.id, group.label, firstColour);
            if (collapsedGroups.count(group.id) != 0)
                continue;
            odd = false;
            for (const auto& articulation : group.articulations)
            {
                const int canonical = project.findCanonicalRow(articulation.semanticId);
                if (canonical < 0)
                    continue;
                const auto* binding = articulation.primaryNoteBinding();
                addRow(canonical, group.id, group.label,
                       articulation.label.isNotEmpty() ? articulation.label
                                                       : canonicalRows.getReference(canonical).defaultLabel,
                       binding != nullptr ? binding->noteName : juce::String());
            }
        }
    }
    else
    {
        juce::String currentGroup;
        for (int row = 0; row < canonicalRows.size(); ++row)
        {
            const auto& canonical = canonicalRows.getReference(row);
            if (canonical.groupId != currentGroup)
            {
                currentGroup = canonical.groupId;
                addHeader(canonical.groupId, canonical.groupLabel, ggdInstrumentColour(canonical.semanticId));
                odd = false;
            }
            if (collapsedGroups.count(canonical.groupId) == 0)
                addRow(row, canonical.groupId, canonical.groupLabel, canonical.defaultLabel, {});
        }
    }

    updateSize();
}

void GgdDrumGrid::setViewportSize(int width, int height, int scrollbarThickness)
{
    if (width == viewportWidth && height == viewportHeight && scrollbarThickness == viewportScrollbar)
        return;
    viewportWidth = width;
    viewportHeight = height;
    viewportScrollbar = scrollbarThickness;
    updateSize();
}

void GgdDrumGrid::updateSize()
{
    int contentBottom = rulerHeight;
    if (!layout.empty())
        contentBottom = layout.back().y + layout.back().height;

    const int contentWidth = static_cast<int>(std::ceil(xForTick(patternLengthTicks()))) + 28 + trailingPadding;
    const int contentHeight = contentBottom + 16;
    int width = juce::jmax(400, contentWidth);
    int height = juce::jmax(160, contentHeight);

    if (viewportWidth > 0 && viewportHeight > 0)
    {
        // Mirror the viewport's scrollbar logic so the grid fills exactly the
        // visible area whenever the pattern is smaller than it.
        int viewWidth = viewportWidth;
        int viewHeight = viewportHeight;
        bool vertical = contentHeight > viewHeight;
        if (vertical)
            viewWidth -= viewportScrollbar;
        if (contentWidth > viewWidth)
        {
            viewHeight -= viewportScrollbar;
            if (!vertical && contentHeight > viewHeight)
            {
                vertical = true;
                viewWidth -= viewportScrollbar;
            }
        }
        width = juce::jmax(contentWidth, viewWidth);
        height = juce::jmax(contentHeight, viewHeight);
    }
    setSize(width, height);
}

//==============================================================================
GgdDrumGrid::EventRef GgdDrumGrid::hitAt(juce::Point<float> position) const
{
    const int row = rowAtY(position.y);
    const auto* p = pattern();
    if (row < 0 || p == nullptr)
        return {};

    const int storage = storageRow(row);
    const float halfWidth = hitWidth() * 0.5f + 4.0f;
    const int fromTick = static_cast<int>(std::floor(tickForX(position.x - halfWidth - cellWidth())));
    const int toTick = static_cast<int>(std::ceil(tickForX(position.x + halfWidth)));

    float best = std::numeric_limits<float>::max();
    int bestTick = -1;
    for (int i = firstEventIndexAtOrAfter(*p, fromTick); i < p->getEventCount(); ++i)
    {
        const auto* event = p->getEvent(i);
        if (event->tick > toTick)
            break;
        if (event->row != storage)
            continue;
        const float distance = std::abs(position.x - hitCentreX(event->tick));
        if (distance <= halfWidth && distance < best)
        {
            best = distance;
            bestTick = event->tick;
        }
    }
    return bestTick >= 0 ? EventRef { row, bestTick } : EventRef {};
}

GgdDrumGrid::EventState GgdDrumGrid::stateOf(const EventRef& ref) const
{
    EventState state;
    state.ref = ref;
    if (const auto* p = pattern())
    {
        if (const auto* event = p->findEventPtr(storageRow(ref.row), ref.tick))
        {
            state.velocity = event->velocity;
            state.probability = event->probability;
            state.durationTicks = event->durationTicks;
        }
    }
    return state;
}

bool GgdDrumGrid::occupied(int row, int tick) const
{
    const auto* p = pattern();
    return p != nullptr && row >= 0 && p->findEvent(storageRow(row), tick) >= 0;
}

bool GgdDrumGrid::writeEvent(const EventState& state, int row, int tick)
{
    auto* p = pattern();
    if (p == nullptr || row < 0 || row >= project.getCanonicalRows().size()
        || tick < 0 || tick >= p->getLengthTicks())
        return false;
    return p->addEvent(storageRow(row), tick, state.velocity, state.probability, state.durationTicks);
}

bool GgdDrumGrid::removeEvent(const EventRef& ref)
{
    auto* p = pattern();
    return p != nullptr && ref.valid() && p->removeEvent(storageRow(ref.row), ref.tick);
}

bool GgdDrumGrid::isSelected(const EventRef& ref) const
{
    return std::find(selection.begin(), selection.end(), ref) != selection.end();
}

void GgdDrumGrid::setSelection(std::vector<EventRef> refs, bool notify)
{
    selection = std::move(refs);
    if (notify)
        notifySelection();
    repaint();
}

void GgdDrumGrid::toggleSelected(const EventRef& ref)
{
    auto it = std::find(selection.begin(), selection.end(), ref);
    if (it != selection.end())
        selection.erase(it);
    else
        selection.push_back(ref);
    notifySelection();
    repaint();
}

void GgdDrumGrid::notifySelection()
{
    if (callbacks.onSelectionChanged)
        callbacks.onSelectionChanged();
}

void GgdDrumGrid::commitEdit()
{
    if (callbacks.onEdit)
        callbacks.onEdit();
    repaint();
}

void GgdDrumGrid::status(const juce::String& text)
{
    if (callbacks.onStatus)
        callbacks.onStatus(text);
}

//==============================================================================
int GgdDrumGrid::getSelectedProbability() const
{
    if (selection.empty())
        return -1;
    const int first = stateOf(selection.front()).probability;
    for (size_t i = 1; i < selection.size(); ++i)
        if (stateOf(selection[i]).probability != first)
            return -1;
    return first;
}

int GgdDrumGrid::getSelectedVelocity() const
{
    if (selection.empty())
        return -1;
    int total = 0;
    for (const auto& ref : selection)
        total += stateOf(ref).velocity;
    return static_cast<int>(std::lround(static_cast<double>(total) / static_cast<double>(selection.size())));
}

std::vector<std::pair<int, int>> GgdDrumGrid::getSelectionCoordinates() const
{
    std::vector<std::pair<int, int>> result;
    result.reserve(selection.size());
    for (const auto& ref : selection)
        result.emplace_back(ref.row, ref.tick);
    return result;
}

void GgdDrumGrid::restoreSelectionCoordinates(const std::vector<std::pair<int, int>>& coordinates)
{
    std::vector<EventRef> restored;
    for (const auto& [row, tick] : coordinates)
        if (itemForRow(row) != nullptr && occupied(row, tick))
            restored.push_back({ row, tick });
    setSelection(std::move(restored), true);
}

void GgdDrumGrid::selectAll()
{
    std::vector<EventRef> all;
    if (const auto* p = pattern())
    {
        for (int i = 0; i < p->getEventCount(); ++i)
        {
            const auto* event = p->getEvent(i);
            const int row = canonicalRow(event->row);
            if (row >= 0 && itemForRow(row) != nullptr)
                all.push_back({ row, event->tick });
        }
    }
    setSelection(std::move(all), true);
}

void GgdDrumGrid::clearSelection()
{
    if (!selection.empty())
        setSelection({}, true);
}

void GgdDrumGrid::copySelection(bool announce)
{
    if (selection.empty())
        return;

    int minTick = selection.front().tick;
    int maxTick = minTick;
    for (const auto& ref : selection)
    {
        minTick = juce::jmin(minTick, ref.tick);
        maxTick = juce::jmax(maxTick, ref.tick);
    }

    clipboard.clear();
    for (const auto& ref : selection)
    {
        auto state = stateOf(ref);
        state.ref.tick -= minTick;
        clipboard.push_back(state);
    }
    clipboardSpanTicks = juce::jmax(snapTicks, maxTick - minTick + snapTicks);
    if (announce)
        status("Copied " + juce::String(selection.size()) + (selection.size() == 1 ? " hit" : " hits")
               + ". Paste (V) places them after the selection.");
}

void GgdDrumGrid::duplicateSelection()
{
    if (selection.empty())
        return;
    // Same placement as copy + paste, without touching the clipboard.
    auto savedClipboard = clipboard;
    const int savedSpan = clipboardSpanTicks;
    copySelection(false);
    pasteClipboard();
    clipboard = std::move(savedClipboard);
    clipboardSpanTicks = savedSpan;
}

void GgdDrumGrid::adjustSelectedVelocity(int delta)
{
    auto* p = pattern();
    if (p == nullptr || selection.empty())
        return;
    bool changed = false;
    for (const auto& ref : selection)
        if (auto* event = p->findEventPtr(storageRow(ref.row), ref.tick))
        {
            const auto velocity = static_cast<std::uint8_t>(juce::jlimit(1, 127, event->velocity + delta));
            if (velocity != event->velocity)
            {
                event->velocity = velocity;
                changed = true;
            }
        }
    if (!changed)
        return;
    if (selection.size() == 1)
        showPopup(PopupKind::velocity, selection.front(), stateOf(selection.front()).velocity, false);
    else
        showChips(selection, false);
    commitEdit();
}

void GgdDrumGrid::shiftSelectedTiming(int deltaTicks)
{
    if (selection.empty())
        return;
    if (!moveSelectionBy(deltaTicks, 0, false))
    {
        status("Cannot shift there - a hit would leave the pattern or land on another hit");
        return;
    }
    if (selection.size() == 1)
    {
        const auto& ref = selection.front();
        showPopup(PopupKind::timing, ref, ref.tick - quantize(ref.tick), false);
    }
}

void GgdDrumGrid::pasteClipboard()
{
    if (clipboard.empty())
        return;

    const int length = patternLengthTicks();
    int anchor = 0;
    for (const auto& ref : selection)
        anchor = juce::jmax(anchor, ref.tick + snapTicks);
    anchor = (anchor / snapTicks) * snapTicks;
    if (anchor + clipboardSpanTicks > length)
        anchor = 0;

    auto fits = [&](int candidate)
    {
        for (const auto& state : clipboard)
        {
            const int tick = candidate + state.ref.tick;
            if (tick < 0 || tick >= length || occupied(state.ref.row, tick))
                return false;
        }
        return true;
    };

    while (anchor < length && !fits(anchor))
        anchor += snapTicks;
    if (anchor >= length)
    {
        status("No free space for the clipboard in this pattern");
        return;
    }

    std::vector<EventRef> pasted;
    for (const auto& state : clipboard)
    {
        const int tick = anchor + state.ref.tick;
        if (writeEvent(state, state.ref.row, tick))
            pasted.push_back({ state.ref.row, tick });
    }
    selection = std::move(pasted);
    notifySelection();
    commitEdit();
}

void GgdDrumGrid::deleteSelection()
{
    if (selection.empty())
        return;
    bool changed = false;
    for (const auto& ref : selection)
        changed |= removeEvent(ref);
    selection.clear();
    notifySelection();
    if (changed)
        commitEdit();
}

void GgdDrumGrid::setSelectedVelocity(int velocity)
{
    auto* p = pattern();
    if (p == nullptr || selection.empty())
        return;
    const auto desired = static_cast<std::uint8_t>(juce::jlimit(1, 127, velocity));
    bool changed = false;
    for (const auto& ref : selection)
        if (auto* event = p->findEventPtr(storageRow(ref.row), ref.tick))
            if (event->velocity != desired)
            {
                event->velocity = desired;
                changed = true;
            }
    if (changed)
        commitEdit();
}

void GgdDrumGrid::setSelectedProbability(int probability)
{
    auto* p = pattern();
    if (p == nullptr || selection.empty())
        return;
    const auto desired = static_cast<std::int8_t>(juce::jlimit(0, 100, probability));
    bool changed = false;
    for (const auto& ref : selection)
        if (auto* event = p->findEventPtr(storageRow(ref.row), ref.tick))
            if (event->probability != desired)
            {
                event->probability = desired;
                changed = true;
            }
    if (changed)
        commitEdit();
    notifySelection();
}

void GgdDrumGrid::quantizeSelection()
{
    if (selection.empty())
        return;

    std::vector<EventState> states;
    for (const auto& ref : selection)
        states.push_back(stateOf(ref));

    std::vector<EventRef> targets;
    for (const auto& state : states)
    {
        const EventRef target { state.ref.row, quantize(state.ref.tick) };
        if (std::find(targets.begin(), targets.end(), target) != targets.end()
            || (occupied(target.row, target.tick) && !isSelected(target)))
        {
            status("Quantize would merge hits - nothing changed");
            return;
        }
        targets.push_back(target);
    }

    for (const auto& state : states)
        removeEvent(state.ref);
    selection.clear();
    for (size_t i = 0; i < states.size(); ++i)
        if (writeEvent(states[i], targets[i].row, targets[i].tick))
            selection.push_back(targets[i]);
    notifySelection();
    commitEdit();
}

void GgdDrumGrid::humanizeSelection()
{
    if (selection.empty())
        return;

    auto& random = juce::Random::getSystemRandom();
    std::vector<EventState> states;
    for (const auto& ref : selection)
        states.push_back(stateOf(ref));
    for (const auto& state : states)
        removeEvent(state.ref);

    selection.clear();
    const int length = patternLengthTicks();
    for (auto state : states)
    {
        int tick = juce::jlimit(0, length - 1, state.ref.tick + random.nextInt(17) - 8);
        state.velocity = juce::jlimit(1, 127, state.velocity + random.nextInt(9) - 4);
        if (occupied(state.ref.row, tick))
            tick = state.ref.tick;
        if (writeEvent(state, state.ref.row, tick))
            selection.push_back({ state.ref.row, tick });
    }
    notifySelection();
    commitEdit();
}

void GgdDrumGrid::duplicateSelectionWithOffset(int deltaTicks, float velocityScale, const juce::String& what)
{
    if (selection.empty())
        return;

    const int length = patternLengthTicks();
    std::vector<EventRef> created;
    for (const auto& ref : selection)
    {
        auto state = stateOf(ref);
        const int target = ref.tick + deltaTicks;
        if (target < 0 || target >= length || occupied(ref.row, target))
            continue;
        state.velocity = juce::jlimit(1, 127, static_cast<int>(std::lround(state.velocity * velocityScale)));
        if (writeEvent(state, ref.row, target))
            created.push_back({ ref.row, target });
    }

    if (created.empty())
    {
        status(what + ": no free positions");
        return;
    }

    // The new hits become the selection so Flam/Double chain naturally.
    selection = std::move(created);
    notifySelection();
    commitEdit();
}

void GgdDrumGrid::createFlam()
{
    // 60 ticks = 1/64 at 960 PPQ (about 31 ms at 120 BPM).
    duplicateSelectionWithOffset(-(GGD_EVENT_PPQ / 16), 0.64f, "Flam");
}

void GgdDrumGrid::createDouble()
{
    duplicateSelectionWithOffset(GGD_TICKS_PER_32ND, 0.94f, "Double");
}

void GgdDrumGrid::selectRowsContainingSelection()
{
    if (selection.empty())
        return;
    std::set<int> rows;
    for (const auto& ref : selection)
        rows.insert(ref.row);

    std::vector<EventRef> result;
    if (const auto* p = pattern())
        for (int i = 0; i < p->getEventCount(); ++i)
        {
            const auto* event = p->getEvent(i);
            const int row = canonicalRow(event->row);
            if (rows.count(row) != 0)
                result.push_back({ row, event->tick });
        }
    setSelection(std::move(result), true);
}

void GgdDrumGrid::fillSelectionGaps()
{
    if (selection.size() < 2)
        return;

    std::map<int, std::vector<EventState>> byRow;
    for (const auto& ref : selection)
        byRow[ref.row].push_back(stateOf(ref));

    std::vector<EventRef> created;
    for (auto& [row, states] : byRow)
    {
        if (states.size() < 2)
            continue;
        std::sort(states.begin(), states.end(),
                  [](const EventState& a, const EventState& b) { return a.ref.tick < b.ref.tick; });

        const int start = ((states.front().ref.tick + snapTicks - 1) / snapTicks) * snapTicks;
        const int end = (states.back().ref.tick / snapTicks) * snapTicks;
        size_t right = 1;
        for (int tick = start; tick <= end; tick += snapTicks)
        {
            if (occupied(row, tick))
                continue;
            while (right < states.size() && states[right].ref.tick < tick)
                ++right;
            const auto& leftState = states[right == 0 ? 0 : right - 1];
            const auto& rightState = states[juce::jmin(right, states.size() - 1)];

            EventState state = leftState;
            if (rightState.ref.tick != leftState.ref.tick)
            {
                const float t = juce::jlimit(0.0f, 1.0f,
                    static_cast<float>(tick - leftState.ref.tick)
                        / static_cast<float>(rightState.ref.tick - leftState.ref.tick));
                state.velocity = juce::jlimit(1, 127, static_cast<int>(std::lround(
                    leftState.velocity + (rightState.velocity - leftState.velocity) * t)));
            }
            state.durationTicks = juce::jmin(state.durationTicks, snapTicks);
            if (writeEvent(state, row, tick))
                created.push_back({ row, tick });
        }
    }

    if (created.empty())
    {
        status("Fill: no gaps on the current grid between the selected hits");
        return;
    }
    selection = std::move(created);
    notifySelection();
    commitEdit();
}

void GgdDrumGrid::mirrorSelection()
{
    if (selection.size() < 2)
        return;

    int minTick = selection.front().tick;
    int maxTick = minTick;
    std::vector<EventState> states;
    for (const auto& ref : selection)
    {
        minTick = juce::jmin(minTick, ref.tick);
        maxTick = juce::jmax(maxTick, ref.tick);
        states.push_back(stateOf(ref));
    }

    std::set<std::pair<int, int>> targets;
    for (const auto& state : states)
    {
        const EventRef target { state.ref.row, minTick + maxTick - state.ref.tick };
        if (!targets.insert({ target.row, target.tick }).second
            || (occupied(target.row, target.tick) && !isSelected(target)))
        {
            status("Mirror would collide with other hits - nothing changed");
            return;
        }
    }

    for (const auto& state : states)
        removeEvent(state.ref);
    selection.clear();
    for (const auto& state : states)
    {
        const int target = minTick + maxTick - state.ref.tick;
        if (writeEvent(state, state.ref.row, target))
            selection.push_back({ state.ref.row, target });
    }
    notifySelection();
    commitEdit();
}

void GgdDrumGrid::rampSelectedVelocity(bool rising)
{
    auto* p = pattern();
    if (p == nullptr || selection.size() < 2)
        return;

    int minTick = selection.front().tick, maxTick = minTick;
    int minVelocity = 127, maxVelocity = 1;
    for (const auto& ref : selection)
    {
        const auto state = stateOf(ref);
        minTick = juce::jmin(minTick, ref.tick);
        maxTick = juce::jmax(maxTick, ref.tick);
        minVelocity = juce::jmin(minVelocity, state.velocity);
        maxVelocity = juce::jmax(maxVelocity, state.velocity);
    }

    if (maxVelocity - minVelocity < 12)
    {
        const int centre = (minVelocity + maxVelocity) / 2;
        minVelocity = juce::jmax(20, centre - 30);
        maxVelocity = juce::jmin(127, centre + 20);
    }

    bool changed = false;
    const int span = juce::jmax(1, maxTick - minTick);
    for (const auto& ref : selection)
    {
        auto* event = p->findEventPtr(storageRow(ref.row), ref.tick);
        if (event == nullptr)
            continue;
        float t = static_cast<float>(ref.tick - minTick) / static_cast<float>(span);
        if (!rising)
            t = 1.0f - t;
        const auto velocity = static_cast<std::uint8_t>(juce::jlimit(1, 127, static_cast<int>(
            std::lround(minVelocity + (maxVelocity - minVelocity) * t))));
        if (event->velocity != velocity)
        {
            event->velocity = velocity;
            changed = true;
        }
    }
    if (changed)
    {
        showChips(selection, false);
        commitEdit();
    }
}

void GgdDrumGrid::scaleSelectedVelocityRange(float factor)
{
    auto* p = pattern();
    if (p == nullptr || selection.size() < 2 || factor <= 0.0f)
        return;

    float total = 0.0f;
    int count = 0;
    for (const auto& ref : selection)
        if (const auto* event = p->findEventPtr(storageRow(ref.row), ref.tick))
        {
            total += event->velocity;
            ++count;
        }
    if (count < 2)
        return;

    const float mean = total / static_cast<float>(count);
    bool changed = false;
    for (const auto& ref : selection)
    {
        auto* event = p->findEventPtr(storageRow(ref.row), ref.tick);
        if (event == nullptr)
            continue;
        const auto velocity = static_cast<std::uint8_t>(juce::jlimit(1, 127, static_cast<int>(
            std::lround(mean + (event->velocity - mean) * factor))));
        if (event->velocity != velocity)
        {
            event->velocity = velocity;
            changed = true;
        }
    }
    if (changed)
    {
        showChips(selection, false);
        commitEdit();
    }
}

void GgdDrumGrid::thinSelection()
{
    if (selection.size() < 2)
        return;

    std::map<int, std::vector<EventRef>> byRow;
    for (const auto& ref : selection)
        byRow[ref.row].push_back(ref);

    std::vector<EventRef> keep;
    bool removed = false;
    for (auto& [row, refs] : byRow)
    {
        std::sort(refs.begin(), refs.end(), [](const EventRef& a, const EventRef& b) { return a.tick < b.tick; });
        for (size_t i = 0; i < refs.size(); ++i)
        {
            if ((i & 1) != 0)
                removed |= removeEvent(refs[i]);
            else
                keep.push_back(refs[i]);
        }
    }

    if (!removed)
        return;
    selection = std::move(keep);
    notifySelection();
    commitEdit();
}

bool GgdDrumGrid::moveSelectionBy(int deltaTicks, int deltaRows, bool duplicate)
{
    if (selection.empty() || (deltaTicks == 0 && deltaRows == 0))
        return false;

    // Vertical moves follow the visible row order of the active kit.
    std::vector<int> visibleRows;
    for (const auto& item : layout)
        if (!item.header)
            visibleRows.push_back(item.canonicalRow);
    auto visualIndex = [&visibleRows](int row)
    {
        const auto it = std::find(visibleRows.begin(), visibleRows.end(), row);
        return it == visibleRows.end() ? -1 : static_cast<int>(it - visibleRows.begin());
    };

    const int length = patternLengthTicks();
    std::vector<EventState> states;
    std::vector<EventRef> targets;
    for (const auto& ref : selection)
    {
        const int index = visualIndex(ref.row);
        const int targetIndex = index + deltaRows;
        const int tick = ref.tick + deltaTicks;
        if (index < 0 || targetIndex < 0 || targetIndex >= static_cast<int>(visibleRows.size())
            || tick < 0 || tick >= length)
            return false;
        const EventRef target { visibleRows[static_cast<size_t>(targetIndex)], tick };
        if (occupied(target.row, target.tick) && (duplicate || !isSelected(target)))
            return false;
        states.push_back(stateOf(ref));
        targets.push_back(target);
    }

    if (!duplicate)
        for (const auto& state : states)
            removeEvent(state.ref);

    std::vector<EventRef> moved;
    for (size_t i = 0; i < states.size(); ++i)
        if (writeEvent(states[i], targets[i].row, targets[i].tick))
            moved.push_back(targets[i]);

    selection = std::move(moved);
    notifySelection();
    commitEdit();
    return true;
}

void GgdDrumGrid::nudgeSelection(int horizontal, int vertical, bool duplicate)
{
    if (!moveSelectionBy(horizontal * snapTicks, vertical, duplicate) && !selection.empty())
        status("Cannot move there - the target is outside the pattern or occupied");
}

//==============================================================================
void GgdDrumGrid::paintCell(int row, int tick)
{
    if (row < 0 || tick < 0 || (row == lastPaintRow && tick == lastPaintTick))
        return;
    lastPaintRow = row;
    lastPaintTick = tick;
    if (occupied(row, tick))
        return;

    EventState state;
    state.ref = { row, tick };
    state.durationTicks = juce::jmin(GGD_DEFAULT_EVENT_DURATION_TICKS, snapTicks);
    if (writeEvent(state, row, tick))
    {
        gestureChanged = true;
        processor.publishEdit(false);
        repaint();
    }
}

void GgdDrumGrid::eraseAcross(int row, float fromX, float toX)
{
    auto* p = pattern();
    if (p == nullptr || row < 0)
        return;

    const int storage = storageRow(row);
    const float lo = juce::jmin(fromX, toX) - 4.0f;
    const float hi = juce::jmax(fromX, toX) + 4.0f;
    const float halfWidth = hitWidth() * 0.5f + 3.0f;
    bool changed = false;

    for (int i = p->getEventCount() - 1; i >= 0; --i)
    {
        const auto* event = p->getEvent(i);
        if (event->row != storage)
            continue;
        const float centre = hitCentreX(event->tick);
        if (centre + halfWidth >= lo && centre - halfWidth <= hi)
        {
            const EventRef ref { row, event->tick };
            changed |= p->removeEvent(storage, event->tick);
            selection.erase(std::remove(selection.begin(), selection.end(), ref), selection.end());
        }
    }

    if (changed)
    {
        gestureChanged = true;
        processor.publishEdit(false);
        repaint();
    }
}

void GgdDrumGrid::updateMarquee()
{
    std::vector<EventRef> result = marqueeBase;
    if (const auto* p = pattern())
    {
        const int fromTick = static_cast<int>(std::floor(tickForX(marqueeRect.getX() - cellWidth())));
        const int toTick = static_cast<int>(std::ceil(tickForX(marqueeRect.getRight())));
        for (int i = firstEventIndexAtOrAfter(*p, fromTick); i < p->getEventCount(); ++i)
        {
            const auto* event = p->getEvent(i);
            if (event->tick > toTick)
                break;
            const int row = canonicalRow(event->row);
            const int y = rowY(row);
            if (row < 0 || y < 0)
                continue;
            const juce::Point<float> centre(hitCentreX(event->tick), y + rowHeight * 0.5f);
            const EventRef ref { row, event->tick };
            if (marqueeRect.contains(centre) && std::find(result.begin(), result.end(), ref) == result.end())
                result.push_back(ref);
        }
    }
    selection = std::move(result);
    notifySelection();
    repaint();
}

void GgdDrumGrid::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragStart = e.position;
    gestureChanged = false;
    hideChips();

    if (overRuler(e.position))
        return;

    const auto* item = itemAtY(e.position.y);
    if (item == nullptr)
        return;

    if (item->header)
    {
        // Collapsing is limited to the articulation column so a stray click
        // in the timeline never folds an instrument family away.
        if (overLabelColumn(e.position))
        {
            if (collapsedGroups.count(item->groupId) != 0)
                collapsedGroups.erase(item->groupId);
            else
                collapsedGroups.insert(item->groupId);
            rebuildLayout();
            repaint();
        }
        return;
    }

    const int row = item->canonicalRow;
    if (overLabelColumn(e.position))
    {
        if (prefs.audition && layer()->getCurNote(storageRow(row)) >= 0)
        {
            processor.mIncomingData.addToFifo(SEQ_MIDI_NOTEON, GgdProject::drumLayer, storageRow(row));
            auditionRow = row;
            dragMode = DragMode::audition;
            repaint();
        }
        return;
    }

    const auto hit = hitAt(e.position);
    const auto& mods = e.mods;

    if (toolMode == ToolMode::draw)
    {
        if (mods.isRightButtonDown())
        {
            dragMode = DragMode::erase;
            lastEraseX = e.position.x;
            eraseAcross(row, e.position.x, e.position.x);
            return;
        }

        if (hit.valid() && mods.isShiftDown())
        {
            dragMode = DragMode::velocity;
            dragRef = hit;
            dragStartVelocity = stateOf(hit).velocity;
            showPopup(PopupKind::velocity, hit, dragStartVelocity, true);
            return;
        }

        if (hit.valid() && mods.isAltDown())
        {
            dragMode = DragMode::timing;
            dragRef = hit;
            dragStartTick = hit.tick;
            showPopup(PopupKind::timing, hit, hit.tick - quantize(hit.tick), true);
            return;
        }

        if (hit.valid())
        {
            // Clicking a hit removes it; keep dragging to erase more.
            dragMode = DragMode::erase;
            lastEraseX = e.position.x;
            eraseAcross(row, e.position.x, e.position.x);
            return;
        }

        dragMode = DragMode::paint;
        lastPaintRow = -1;
        lastPaintTick = -1;
        paintCell(row, cellTickForX(e.position.x));
        return;
    }

    // Select tool
    if (hit.valid())
    {
        if (mods.isShiftDown() && !mods.isAltDown() && !mods.isCommandDown())
        {
            // Shift-drag edits velocity; a plain shift-click toggles selection.
            dragMode = DragMode::selectVelocity;
            dragRef = hit;
            selectVelocityStarted = false;
            dragStates.clear();
            showPopup(PopupKind::velocity, hit, stateOf(hit).velocity, true);
            return;
        }

        if (mods.isCommandDown())
        {
            toggleSelected(hit);
            return;
        }

        if (!isSelected(hit))
            setSelection({ hit }, true);

        dragMode = DragMode::move;
        duplicateDrag = mods.isAltDown();
        dragDeltaTicks = 0;
        dragDeltaRows = 0;
        dragStates.clear();
        for (const auto& ref : selection)
            dragStates.push_back(stateOf(ref));
        repaint();
        return;
    }

    marqueeBase = mods.isShiftDown() || mods.isCommandDown() ? selection : std::vector<EventRef>();
    marqueeRect = { e.position.x, e.position.y, 0.0f, 0.0f };
    dragMode = DragMode::marquee;
    updateMarquee();
}

void GgdDrumGrid::mouseDrag(const juce::MouseEvent& e)
{
    switch (dragMode)
    {
        case DragMode::paint:
        {
            const int row = rowAtY(e.position.y);
            if (row < 0 || overLabelColumn(e.position))
                return;
            const int target = cellTickForX(e.position.x);
            if (row == lastPaintRow && lastPaintTick >= 0 && target != lastPaintTick)
            {
                // Interpolate so fast gestures never skip cells.
                const int step = target > lastPaintTick ? snapTicks : -snapTicks;
                for (int tick = lastPaintTick + step; step > 0 ? tick <= target : tick >= target; tick += step)
                    paintCell(row, tick);
            }
            else
            {
                paintCell(row, target);
            }
            return;
        }

        case DragMode::erase:
        {
            const int row = rowAtY(e.position.y);
            if (row >= 0 && !overLabelColumn(e.position))
                eraseAcross(row, lastEraseX, e.position.x);
            lastEraseX = e.position.x;
            return;
        }

        case DragMode::velocity:
        {
            auto* p = pattern();
            auto* event = p != nullptr ? p->findEventPtr(storageRow(dragRef.row), dragRef.tick) : nullptr;
            if (event == nullptr)
                return;
            const int delta = static_cast<int>(std::lround((dragStart.y - e.position.y) * 0.8f));
            const auto velocity = static_cast<std::uint8_t>(juce::jlimit(1, 127, dragStartVelocity + delta));
            if (event->velocity != velocity)
            {
                event->velocity = velocity;
                gestureChanged = true;
                processor.publishEdit(false);
            }
            showPopup(PopupKind::velocity, dragRef, velocity, true);
            return;
        }

        case DragMode::timing:
        {
            auto* p = pattern();
            if (p == nullptr)
                return;
            const int delta = static_cast<int>(std::lround((e.position.x - dragStart.x) / pixelsPerTick()));
            const int target = juce::jlimit(0, p->getLengthTicks() - 1, dragStartTick + delta);
            if (target != dragRef.tick && !occupied(dragRef.row, target)
                && p->moveEvent(storageRow(dragRef.row), dragRef.tick, storageRow(dragRef.row), target))
            {
                for (auto& ref : selection)
                    if (ref == dragRef)
                        ref.tick = target;
                dragRef.tick = target;
                gestureChanged = true;
                processor.publishEdit(false);
            }
            showPopup(PopupKind::timing, dragRef, dragRef.tick - quantize(dragRef.tick), true);
            return;
        }

        case DragMode::selectVelocity:
        {
            if (!selectVelocityStarted)
            {
                if (e.position.getDistanceFrom(dragStart) < 3.0f)
                    return;
                if (!isSelected(dragRef))
                    setSelection({ dragRef }, true);
                dragStates.clear();
                for (const auto& ref : selection)
                    dragStates.push_back(stateOf(ref));
                selectVelocityStarted = true;
            }

            auto* p = pattern();
            if (p == nullptr)
                return;
            const int delta = static_cast<int>(std::lround((dragStart.y - e.position.y) * 0.8f));
            int anchorVelocity = stateOf(dragRef).velocity;
            bool changed = false;
            for (const auto& state : dragStates)
            {
                auto* event = p->findEventPtr(storageRow(state.ref.row), state.ref.tick);
                if (event == nullptr)
                    continue;
                const auto velocity = static_cast<std::uint8_t>(juce::jlimit(1, 127, state.velocity + delta));
                if (event->velocity != velocity)
                {
                    event->velocity = velocity;
                    changed = true;
                }
                if (state.ref == dragRef)
                    anchorVelocity = velocity;
            }
            if (changed)
            {
                gestureChanged = true;
                processor.publishEdit(false);
            }
            if (selection.size() > 1)
                showChips(selection, true);
            else
                showPopup(PopupKind::velocity, dragRef, anchorVelocity, true);
            repaint();
            return;
        }

        case DragMode::marquee:
            marqueeRect = juce::Rectangle<float>(dragStart, e.position);
            updateMarquee();
            return;

        case DragMode::move:
        {
            const int ticks = static_cast<int>(std::lround(
                (e.position.x - dragStart.x) / pixelsPerTick() / snapTicks)) * snapTicks;
            const int rows = static_cast<int>(std::lround((e.position.y - dragStart.y) / rowHeight));
            if (ticks != dragDeltaTicks || rows != dragDeltaRows)
            {
                dragDeltaTicks = ticks;
                dragDeltaRows = rows;
                repaint();
            }
            return;
        }

        case DragMode::audition:
        case DragMode::none:
            return;
    }
}

void GgdDrumGrid::mouseUp(const juce::MouseEvent& e)
{
    const auto mode = dragMode;
    dragMode = DragMode::none;

    switch (mode)
    {
        case DragMode::audition:
            processor.mIncomingData.addToFifo(SEQ_MIDI_NOTEOFF, GgdProject::drumLayer, storageRow(auditionRow));
            auditionRow = -1;
            break;

        case DragMode::paint:
        case DragMode::erase:
            if (gestureChanged)
                commitEdit(); // one undo step per stroke
            break;

        case DragMode::velocity:
        case DragMode::timing:
            if (gestureChanged)
                commitEdit();
            showPopup(popupKind, dragRef, popupValue, false);
            break;

        case DragMode::selectVelocity:
            if (!selectVelocityStarted)
            {
                toggleSelected(dragRef);
                if (!e.mods.isShiftDown())
                    hidePopup();
            }
            else
            {
                if (gestureChanged)
                    commitEdit();
                if (selection.size() > 1)
                    showChips(selection, false);
                else
                    showPopup(PopupKind::velocity, dragRef, stateOf(dragRef).velocity, e.mods.isShiftDown());
            }
            selectVelocityStarted = false;
            dragStates.clear();
            break;

        case DragMode::move:
            if (dragDeltaTicks != 0 || dragDeltaRows != 0)
                if (!moveSelectionBy(dragDeltaTicks, dragDeltaRows, duplicateDrag))
                    status("Cannot move there - the target is outside the pattern or occupied");
            dragDeltaTicks = 0;
            dragDeltaRows = 0;
            dragStates.clear();
            break;

        case DragMode::marquee:
            notifySelection();
            break;

        case DragMode::none:
            break;
    }

    marqueeRect = {};
    lastPaintRow = -1;
    lastPaintTick = -1;
    gestureChanged = false;
    repaint();
}

void GgdDrumGrid::mouseMove(const juce::MouseEvent& e)
{
    int row = -1;
    int tick = -1;
    int labelRow = -1;
    juce::String headerGroup;

    if (!overRuler(e.position))
    {
        if (const auto* item = itemAtY(e.position.y))
        {
            if (item->header)
            {
                if (overLabelColumn(e.position))
                    headerGroup = item->groupId;
            }
            else if (overLabelColumn(e.position))
            {
                labelRow = item->canonicalRow;
            }
            else
            {
                row = item->canonicalRow;
                tick = cellTickForX(e.position.x);
            }
        }
    }

    if (row != hoverRow || tick != hoverTick || labelRow != hoverLabelRow || headerGroup != hoverHeaderGroup)
    {
        hoverRow = row;
        hoverTick = tick;
        hoverLabelRow = labelRow;
        hoverHeaderGroup = headerGroup;
        setMouseCursor(labelRow >= 0 || headerGroup.isNotEmpty()
                           ? juce::MouseCursor::PointingHandCursor
                           : toolMode == ToolMode::draw ? juce::MouseCursor::CrosshairCursor
                                                        : juce::MouseCursor::NormalCursor);
        repaint();
    }

    const auto& mods = e.mods;
    if (prefs.shiftHoverVelocity && mods.isShiftDown() && !mods.isAltDown() && !mods.isCommandDown())
    {
        const auto hit = hitAt(e.position);
        if (hit.valid())
        {
            showPopup(PopupKind::velocity, hit, stateOf(hit).velocity, true);
            return;
        }
    }

    if (popupKind == PopupKind::velocity && popupPinned)
        hidePopup();
}

void GgdDrumGrid::mouseExit(const juce::MouseEvent&)
{
    hoverRow = -1;
    hoverTick = -1;
    hoverLabelRow = -1;
    hoverHeaderGroup.clear();
    if (dragMode == DragMode::none)
    {
        if (popupPinned)
            hidePopup();
        if (chipsPinned)
            hideChips();
    }
    repaint();
}

void GgdDrumGrid::modifierKeysChanged(const juce::ModifierKeys& modifiers)
{
    if (!modifiers.isShiftDown() && dragMode == DragMode::none)
    {
        if (popupKind == PopupKind::velocity && popupPinned)
            hidePopup();
        if (chipsPinned)
            hideChips();
    }
}

void GgdDrumGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Alt-double-click snaps one hit to the nearest grid position.
    const auto& mods = e.mods;
    if (!mods.isAltDown() || mods.isShiftDown() || mods.isCommandDown())
        return;

    const auto hit = hitAt(e.position);
    auto* p = pattern();
    if (!hit.valid() || p == nullptr)
        return;

    const int target = quantize(hit.tick);
    if (target == hit.tick)
    {
        showPopup(PopupKind::timing, hit, 0, false);
        return;
    }
    if (occupied(hit.row, target) || !p->moveEvent(storageRow(hit.row), hit.tick, storageRow(hit.row), target))
        return;

    for (auto& ref : selection)
        if (ref == hit)
            ref.tick = target;
    commitEdit();
    showPopup(PopupKind::timing, { hit.row, target }, 0, false);
}

void GgdDrumGrid::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
        if (delta == 0.0f)
            return;
        const float anchorViewport = viewport != nullptr ? e.position.x - viewport->getViewPositionX() : e.position.x;
        const float factor = wheel.isSmooth ? std::exp(delta * 1.2f) : (delta > 0.0f ? 1.15f : 1.0f / 1.15f);
        applyZoom(zoomScale * factor, e.position.x, anchorViewport);
        return;
    }

    if (viewport == nullptr)
        return;

    const bool horizontal = e.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY);
    if (horizontal)
    {
        const float delta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
        viewport->setViewPosition(juce::jmax(0, viewport->getViewPositionX() - static_cast<int>(delta * 220.0f)),
                                  viewport->getViewPositionY());
    }
    else
    {
        viewport->setViewPosition(viewport->getViewPositionX(),
                                  juce::jmax(0, viewport->getViewPositionY() - static_cast<int>(wheel.deltaY * 180.0f)));
    }
}

//==============================================================================
bool GgdDrumGrid::isTripletGrid() const
{
    return GGD_EVENT_PPQ % snapTicks == 0 && (GGD_EVENT_PPQ / snapTicks) % 3 == 0;
}

void GgdDrumGrid::applyZoom(float scale, float anchorContentX, float anchorViewportX)
{
    const float clamped = juce::jlimit(minZoomScale, maxZoomScale, scale);
    if (std::abs(clamped - zoomScale) < 0.0005f)
        return;

    const double anchorTick = tickForX(anchorContentX);
    zoomScale = clamped;
    updateSize();

    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
    {
        const int target = juce::jmax(0, static_cast<int>(std::lround(xForTick(anchorTick) - anchorViewportX)));
        viewport->setViewPosition(target, viewport->getViewPositionY());
    }

    if (callbacks.onZoomChanged)
        callbacks.onZoomChanged(zoomScale);
    repaint();
}

void GgdDrumGrid::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    const float anchorViewport = viewport != nullptr ? e.position.x - viewport->getViewPositionX() : e.position.x;
    applyZoom(zoomScale * scaleFactor, e.position.x, anchorViewport);
}

void GgdDrumGrid::zoomToFit()
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    const int visible = viewport != nullptr ? viewport->getMaximumVisibleWidth() : getWidth();
    const float available = static_cast<float>(visible - nameWidth - 28);
    const float unscaled = static_cast<float>(patternLengthTicks()) * pixelsPerQuarter / GGD_EVENT_PPQ;
    if (available <= 0.0f || unscaled <= 0.0f)
        return;
    applyZoom(available / unscaled, static_cast<float>(nameWidth), static_cast<float>(nameWidth));
    if (viewport != nullptr)
        viewport->setViewPosition(0, viewport->getViewPositionY());
}

void GgdDrumGrid::setZoomScale(float scale)
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    if (viewport == nullptr)
    {
        zoomScale = juce::jlimit(minZoomScale, maxZoomScale, scale);
        updateSize();
        if (callbacks.onZoomChanged)
            callbacks.onZoomChanged(zoomScale);
        repaint();
        return;
    }

    const float anchorViewport = static_cast<float>(viewport->getViewWidth() + nameWidth) * 0.5f;
    applyZoom(scale, static_cast<float>(viewport->getViewPositionX()) + anchorViewport, anchorViewport);
}

void GgdDrumGrid::resetZoom()
{
    setZoomScale(defaultZoomScale);
}

//==============================================================================
void GgdDrumGrid::showPopup(PopupKind kind, const EventRef& ref, int value, bool pinned)
{
    popupKind = kind;
    popupRef = ref;
    popupValue = value;
    popupPinned = pinned;
    popupUntilMs = nowMs() + 900.0;
    repaint();
}

void GgdDrumGrid::hidePopup()
{
    if (popupKind == PopupKind::none)
        return;
    popupKind = PopupKind::none;
    popupPinned = false;
    repaint();
}

void GgdDrumGrid::showChips(const std::vector<EventRef>& refs, bool pinned)
{
    chipRefs = refs;
    chipsPinned = pinned;
    chipsUntilMs = nowMs() + 900.0;
    popupKind = PopupKind::none;
    repaint();
}

void GgdDrumGrid::hideChips()
{
    if (chipRefs.empty())
        return;
    chipRefs.clear();
    chipsPinned = false;
    repaint();
}

//==============================================================================
void GgdDrumGrid::drainPlayedEvents()
{
    const int displayed = layer()->getCurrentPattern();
    GgdEngine::PlayedEvent event;
    int drained = 0;
    while (drained++ < 512 && processor.popPlayedEvent(event))
    {
        if (!prefs.playedEffects || event.pattern != displayed)
            continue;
        PlayedPulse pulse;
        pulse.row = canonicalRow(event.row);
        pulse.tick = event.tick;
        pulse.velocity = juce::jlimit(1, 127, event.velocity);
        pulse.startMs = event.dueMs;
        if (pulse.row < 0)
            continue;
        if (pulses.size() >= 128)
            pulses.erase(pulses.begin());
        pulses.push_back(pulse);
    }
}

bool GgdDrumGrid::isAnimating() const
{
    return playTick >= 0.0 || !pulses.empty()
        || (popupKind != PopupKind::none && !popupPinned)
        || (!chipRefs.empty() && !chipsPinned);
}

void GgdDrumGrid::updatePlayback()
{
    const double now = nowMs();
    const int displayed = layer()->getCurrentPattern();

    double tick = -1.0;
    if (processor.readPlayhead(playhead) && playhead.playing && playhead.pattern == displayed)
    {
        tick = playhead.tickAt(now);
        if (tick >= 0.0 && !prefs.smoothPlayhead)
            tick = std::floor(tick / GGD_TICKS_PER_16TH) * GGD_TICKS_PER_16TH;
    }
    playTick = tick;

    drainPlayedEvents();
    if (!pulses.empty())
    {
        const double decay = juce::jmax(100, prefs.effectDecayMs);
        pulses.erase(std::remove_if(pulses.begin(), pulses.end(),
                                    [now, decay](const PlayedPulse& pulse) { return now - pulse.startMs >= decay; }),
                     pulses.end());
    }

    // Transient feedback times out on its own.
    if (popupKind != PopupKind::none && !popupPinned && now > popupUntilMs)
        hidePopup();
    if (!chipRefs.empty() && !chipsPinned && now > chipsUntilMs)
        hideChips();

    const bool effectsVisible = !pulses.empty();
    if (effectsVisible)
    {
        repaint(viewArea());
    }
    else if (playTick != lastPaintedPlayTick)
    {
        // Only the strips around the old and new playhead need repainting.
        const auto view = viewArea();
        auto stripFor = [&](double t)
        {
            if (t < 0.0)
                return juce::Rectangle<int>();
            const int x = static_cast<int>(xForTick(t));
            return juce::Rectangle<int>(x - 8, view.getY(), prefs.playheadGlow ? 72 : 16, view.getHeight());
        };
        const auto area = stripFor(playTick).getUnion(stripFor(lastPaintedPlayTick));
        if (!area.isEmpty())
            repaint(area);
    }
}

float GgdDrumGrid::getPlayheadContentX() const
{
    return playTick >= 0.0 ? xForTick(playTick) : -1.0f;
}

//==============================================================================
void GgdDrumGrid::pollKeyboard(bool active)
{
    const double now = nowMs();
    for (int i = 0; i < shortcutKeyCount; ++i)
    {
        const bool down = isShortcutKeyDown(i);
        const bool pressed = down && !keyWasDown[static_cast<size_t>(i)];
        keyWasDown[static_cast<size_t>(i)] = down;

        // Keys the host delivers as events are handled in keyPressed().
        if (!pressed || !active || keyDelivered[static_cast<size_t>(i)] || dragMode != DragMode::none)
            continue;
        if (performShortcut(shortcutKeyCode(i), juce::ModifierKeys::getCurrentModifiersRealtime()))
            keyPolledMs[static_cast<size_t>(i)] = now;
    }
}

bool GgdDrumGrid::keyPressed(const juce::KeyPress& key)
{
    const int index = shortcutIndexForKey(key.getKeyCode(), shortcutKeyCount);
    if (index < 0)
        return false;

    const auto slot = static_cast<size_t>(index);
    const bool firstDelivery = !keyDelivered[slot];
    keyDelivered[slot] = true;

    // Polling may already have acted on this very press before the host
    // got round to delivering it.
    if (firstDelivery && nowMs() - keyPolledMs[slot] < 300.0)
        return true;
    if (dragMode != DragMode::none)
        return true;

    const auto mods = key.getModifiers();
    const bool handled = performShortcut(shortcutKeyCode(index), mods);
    // Plain keys belong to the timeline. Modifier combinations it does not
    // use (Ctrl+S, Ctrl+Q...) go back to the host.
    return handled || !(mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown());
}

bool GgdDrumGrid::performShortcut(int code, const juce::ModifierKeys& mods)
{
    const bool command = mods.isCommandDown() || mods.isCtrlDown();
    const bool shift = mods.isShiftDown();
    const bool alt = mods.isAltDown();

    auto undo = [this] { if (callbacks.onUndo) callbacks.onUndo(); };
    auto redo = [this] { if (callbacks.onRedo) callbacks.onRedo(); };

    if (command)
    {
        if (alt)
            return false;
        switch (code)
        {
            case 'Z': shift ? redo() : undo(); return true;
            case 'Y': redo(); return true;
            case 'A': setToolMode(ToolMode::select); selectAll(); return true;
            case 'C': copySelection(); return true;
            case 'V': pasteClipboard(); return true;
            case 'D': duplicateSelection(); return true;
            default:  return false;
        }
    }

    const bool arrow = code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey
                    || code == juce::KeyPress::upKey || code == juce::KeyPress::downKey;
    if (arrow)
    {
        const int horizontal = code == juce::KeyPress::leftKey ? -1 : code == juce::KeyPress::rightKey ? 1 : 0;
        const int vertical = code == juce::KeyPress::upKey ? -1 : code == juce::KeyPress::downKey ? 1 : 0;
        if (shift && !alt)
        {
            // Fine edits: Shift+Up/Down velocity, Shift+Left/Right timing.
            if (vertical != 0)
                adjustSelectedVelocity(vertical < 0 ? 5 : -5);
            else
                shiftSelectedTiming(horizontal * 10);
            return true;
        }
        nudgeSelection(horizontal, vertical, alt);
        return true;
    }

    if (shift || alt)
        return false; // Shift is reserved for velocity gestures

    switch (code)
    {
        case 'D': setToolMode(ToolMode::draw); return true;
        case 'S': setToolMode(ToolMode::select); return true;
        case 'Z': undo(); return true;
        case 'Y': redo(); return true;
        case 'A': setToolMode(ToolMode::select); selectAll(); return true;
        case 'C': copySelection(); return true;
        case 'V': pasteClipboard(); return true;
        case 'Q': quantizeSelection(); return true;
        default: break;
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        deleteSelection();
        return true;
    }
    if (code == juce::KeyPress::escapeKey)
    {
        clearSelection();
        hidePopup();
        hideChips();
        return true;
    }
    return false;
}

//==============================================================================
void GgdDrumGrid::paint(juce::Graphics& g)
{
    lastPaintedPlayTick = playTick;

    const auto clip = g.getClipBounds();
    const int length = patternLengthTicks();
    const int tickFrom = juce::jmax(0, static_cast<int>(std::floor(tickForX(static_cast<float>(clip.getX()))))
                                         - snapTicks);
    const int tickTo = juce::jmin(length, static_cast<int>(std::ceil(tickForX(static_cast<float>(clip.getRight()))))
                                              + snapTicks);

    g.fillAll(tc(GgdThemeRole::outside));
    paintGridBackground(g, clip, tickFrom, tickTo);
    paintGridLines(g, clip, tickFrom, tickTo);
    paintHover(g);
    paintHits(g, clip, tickFrom, tickTo);
    paintEmptyHint(g);
    paintDragPreview(g);
    paintPlaybackEffects(g, clip);
    paintPlayhead(g);
    paintPopups(g);
    paintLabelColumn(g, clip);
    paintRuler(g, tickFrom, tickTo);
}

void GgdDrumGrid::paintGridBackground(juce::Graphics& g, juce::Rectangle<int> clip, int tickFrom, int tickTo)
{
    const int length = patternLengthTicks();
    const float left = static_cast<float>(nameWidth);
    const float right = xForTick(length);
    const int bar = barTicks();
    const int beat = beatTicks();

    for (const auto& item : layout)
    {
        if (item.y + item.height < clip.getY() || item.y > clip.getBottom())
            continue;
        const juce::Rectangle<float> band(left, static_cast<float>(item.y), right - left, static_cast<float>(item.height));

        if (item.header)
        {
            g.setColour(tc(GgdThemeRole::groupFill));
            g.fillRect(band);
            g.setColour(item.colour.withAlpha(prefs.colourByInstrument ? 0.35f : 0.0f));
            g.fillRect(band.withHeight(1.0f));
            g.setColour(tc(GgdThemeRole::borderSoft));
            g.fillRect(band.withY(band.getBottom() - 1.0f).withHeight(1.0f));
            continue;
        }

        g.setColour(item.odd ? tc(GgdThemeRole::rowAlternate) : tc(GgdThemeRole::background));
        g.fillRect(band);

        // Every other beat is shaded so long patterns stay readable.
        g.setColour(tc(GgdThemeRole::beatShade));
        const int firstBeat = juce::jmax(0, (tickFrom / beat) * beat);
        for (int tick = firstBeat; tick < juce::jmin(length, tickTo + beat); tick += beat)
        {
            const int beatInBar = (tick % bar) / beat;
            if ((beatInBar & 1) == 0)
                continue;
            const float x0 = xForTick(tick);
            const float x1 = xForTick(juce::jmin(length, tick + beat));
            g.fillRect(juce::Rectangle<float>(x0, band.getY(), x1 - x0, band.getHeight()));
        }

        g.setColour(tc(GgdThemeRole::borderSoft).withAlpha(0.55f));
        g.fillRect(band.withY(band.getBottom() - 1.0f).withHeight(1.0f));
    }
}

void GgdDrumGrid::paintGridLines(juce::Graphics& g, juce::Rectangle<int> clip, int tickFrom, int tickTo)
{
    const int length = patternLengthTicks();
    const int bar = barTicks();
    const int beat = beatTicks();
    const int major = isTripletGrid() ? GGD_TICKS_PER_8TH_TRIPLET : GGD_TICKS_PER_16TH;
    const float top = static_cast<float>(juce::jmax(rulerHeight, clip.getY()));
    const float bottom = static_cast<float>(layout.empty() ? getHeight() : layout.back().y + layout.back().height);
    if (bottom <= top)
        return;

    auto line = [&](int tick, juce::Colour colour, float width)
    {
        const float x = std::round(xForTick(tick)) - (width > 1.0f ? 0.5f : 0.0f);
        g.setColour(colour);
        g.fillRect(juce::Rectangle<float>(x, top, width, bottom - top));
    };

    const int start = (tickFrom / snapTicks) * snapTicks;
    for (int tick = start; tick <= juce::jmin(length, tickTo); tick += snapTicks)
    {
        if (tick % bar == 0 || tick % beat == 0)
            continue;
        const bool isMajor = tick % major == 0;
        line(tick, isMajor ? tc(GgdThemeRole::subdivisionLine) : tc(GgdThemeRole::fineSubdivisionLine), 1.0f);
    }

    for (int tick = (tickFrom / beat) * beat; tick <= juce::jmin(length, tickTo); tick += beat)
        if (tick % bar != 0)
            line(tick, tc(GgdThemeRole::beatLine), 1.0f);

    for (int tick = (tickFrom / bar) * bar; tick <= juce::jmin(length, tickTo); tick += bar)
        line(tick, tc(GgdThemeRole::barLine), tick == 0 || tick == length ? 2.0f : 1.5f);
}

void GgdDrumGrid::paintHover(juce::Graphics& g)
{
    if (dragMode != DragMode::none || hoverRow < 0)
        return;
    const int y = rowY(hoverRow);
    if (y < 0)
        return;

    // Row highlight across the timeline
    const float left = static_cast<float>(nameWidth);
    g.setColour(tc(GgdThemeRole::rowHover));
    g.fillRect(juce::Rectangle<float>(left, static_cast<float>(y), xForTick(patternLengthTicks()) - left,
                                      static_cast<float>(rowHeight - 1)));

    if (toolMode != ToolMode::draw || hoverTick < 0)
        return;

    const float x = xForTick(hoverTick);
    const float w = juce::jmax(5.0f, cellWidth());
    const auto target = juce::Rectangle<float>(x + 1.5f, static_cast<float>(y) + 3.0f,
                                               juce::jmax(2.0f, w - 3.0f), static_cast<float>(rowHeight) - 7.0f);
    const auto colour = colourForRow(hoverRow);
    const bool filled = occupied(hoverRow, hoverTick);
    g.setColour(colour.withAlpha(filled ? 0.06f : 0.10f));
    g.fillRoundedRectangle(target, 4.0f);
    g.setColour(colour.withAlpha(0.45f));
    g.drawRoundedRectangle(target, 4.0f, 1.0f);
}

void GgdDrumGrid::paintHits(juce::Graphics& g, juce::Rectangle<int> clip, int tickFrom, int tickTo)
{
    const auto* p = pattern();
    if (p == nullptr)
        return;

    const float w = hitWidth();
    const float radius = juce::jmin(4.5f, w * 0.32f);
    const bool moving = dragMode == DragMode::move && (dragDeltaTicks != 0 || dragDeltaRows != 0);
    const bool roomForLabels = cellWidth() >= 26.0f;

    for (int i = firstEventIndexAtOrAfter(*p, tickFrom - snapTicks); i < p->getEventCount(); ++i)
    {
        const auto* event = p->getEvent(i);
        if (event->tick > tickTo)
            break;

        const int row = canonicalRow(event->row);
        const auto* item = itemForRow(row);
        if (item == nullptr || item->y + item->height < clip.getY() || item->y > clip.getBottom())
            continue;

        const EventRef ref { row, event->tick };
        const bool selected = isSelected(ref);
        const float v = static_cast<float>(event->velocity) / 127.0f;
        const float h = 8.0f + 13.0f * v;
        const float cx = hitCentreX(event->tick);
        const float cy = static_cast<float>(item->y) + rowHeight * 0.5f - 0.5f;
        const juce::Rectangle<float> body(cx - w * 0.5f, cy - h * 0.5f, w, h);
        const auto colour = prefs.colourByInstrument ? item->colour : tc(GgdThemeRole::accent);
        const bool ghost = event->velocity <= 55;
        const int probability = juce::jlimit(0, 100, static_cast<int>(event->probability));
        const float certainty = 0.45f + 0.55f * static_cast<float>(probability) / 100.0f;
        const float dim = moving && selected && !duplicateDrag ? 0.35f : 1.0f;

        if (ghost)
        {
            g.setColour(colour.withAlpha((0.14f + 0.25f * v) * certainty * dim));
            g.fillRoundedRectangle(body, radius);
            g.setColour(colour.withAlpha(0.75f * certainty * dim));
            g.drawRoundedRectangle(body.reduced(0.6f), radius, 1.3f);
        }
        else
        {
            g.setColour(colour.withAlpha((0.42f + 0.55f * v) * certainty * dim));
            g.fillRoundedRectangle(body, radius);
            g.setColour(juce::Colours::white.withAlpha(0.18f * dim));
            g.fillRoundedRectangle(body.withHeight(juce::jmin(3.0f, h * 0.25f)).reduced(radius * 0.5f, 0.0f), 1.0f);
        }

        if (probability < 100)
        {
            // Dashed outline and a percentage when there is room.
            juce::Path outline;
            outline.addRoundedRectangle(body.expanded(2.0f), radius + 1.5f);
            juce::Path dashed;
            const float dashes[] = { 2.5f, 2.0f };
            juce::PathStrokeType(1.1f).createDashedStroke(dashed, outline, dashes, 2);
            g.setColour(colour.withAlpha(0.9f * dim));
            g.fillPath(dashed);
            if (roomForLabels)
            {
                g.setColour(tc(GgdThemeRole::text).withAlpha(0.85f * dim));
                g.setFont(GgdFonts::semibold(9.0f));
                g.drawText(juce::String(probability), body.withY(body.getBottom() + 1.0f).withHeight(10.0f).expanded(8.0f, 0.0f),
                           juce::Justification::centredTop, false);
            }
        }

        if (selected)
        {
            g.setColour(tc(GgdThemeRole::selection).withAlpha(0.14f));
            g.fillRoundedRectangle(body.expanded(4.0f), radius + 3.0f);
            g.setColour(tc(GgdThemeRole::selection).withAlpha(0.95f * juce::jmax(dim, 0.6f)));
            g.drawRoundedRectangle(body.expanded(2.5f), radius + 2.0f, 1.6f);
        }
    }
}

void GgdDrumGrid::paintEmptyHint(juce::Graphics& g)
{
    const auto* p = pattern();
    if (p == nullptr || p->getEventCount() > 0 || playTick >= 0.0 || dragMode != DragMode::none)
        return;

    const auto view = viewArea().toFloat();
    const float left = juce::jmax(view.getX() + nameWidth, static_cast<float>(nameWidth));
    const float right = juce::jmin(view.getRight(), xForTick(patternLengthTicks()));
    const float top = view.getY() + rulerHeight;
    const float bottom = juce::jmin(view.getBottom(), static_cast<float>(layout.empty() ? getHeight()
                                                                                       : layout.back().y + layout.back().height));
    if (right - left < 240.0f || bottom - top < 100.0f)
        return;

    const auto area = juce::Rectangle<float>(left, top, right - left, bottom - top);
    const auto box = juce::Rectangle<float>(juce::jmin(400.0f, area.getWidth() - 24.0f), 78.0f)
                         .withCentre({ area.getCentreX(), area.getY() + juce::jmin(area.getHeight() * 0.4f, 220.0f) });
    g.setColour(tc(GgdThemeRole::panel).withAlpha(0.92f));
    g.fillRoundedRectangle(box, 10.0f);
    g.setColour(tc(GgdThemeRole::border));
    g.drawRoundedRectangle(box.reduced(0.5f), 10.0f, 1.0f);

    auto text = box.reduced(18.0f, 14.0f);
    g.setColour(tc(GgdThemeRole::text));
    g.setFont(GgdFonts::semibold(14.0f));
    g.drawText("This pattern is empty", text.removeFromTop(22.0f), juce::Justification::centred, false);
    g.setColour(tc(GgdThemeRole::muted));
    g.setFont(GgdFonts::regular(12.5f));
    g.drawFittedText("Click a cell to add a hit and drag to paint a run,\nor double-click a groove in the library.",
                     text.toNearestInt(), juce::Justification::centred, 2, 0.9f);
}

void GgdDrumGrid::paintDragPreview(juce::Graphics& g)
{
    if (dragMode == DragMode::marquee && !marqueeRect.isEmpty())
    {
        g.setColour(tc(GgdThemeRole::accent).withAlpha(0.10f));
        g.fillRect(marqueeRect);
        g.setColour(tc(GgdThemeRole::accent).withAlpha(0.85f));
        g.drawRect(marqueeRect, 1.0f);
    }

    if (dragMode != DragMode::move || (dragDeltaTicks == 0 && dragDeltaRows == 0))
        return;

    std::vector<int> visibleRows;
    for (const auto& item : layout)
        if (!item.header)
            visibleRows.push_back(item.canonicalRow);

    const float w = hitWidth();
    const int length = patternLengthTicks();
    for (const auto& state : dragStates)
    {
        const auto it = std::find(visibleRows.begin(), visibleRows.end(), state.ref.row);
        if (it == visibleRows.end())
            continue;
        const int index = static_cast<int>(it - visibleRows.begin()) + dragDeltaRows;
        const int tick = state.ref.tick + dragDeltaTicks;
        if (index < 0 || index >= static_cast<int>(visibleRows.size()) || tick < 0 || tick >= length)
            continue;
        const int y = rowY(visibleRows[static_cast<size_t>(index)]);
        const float v = static_cast<float>(state.velocity) / 127.0f;
        const float h = 8.0f + 13.0f * v;
        const juce::Rectangle<float> body(hitCentreX(tick) - w * 0.5f, y + rowHeight * 0.5f - h * 0.5f, w, h);
        g.setColour(tc(GgdThemeRole::warm).withAlpha(0.55f));
        g.fillRoundedRectangle(body, 4.0f);
        g.setColour(tc(GgdThemeRole::warm));
        g.drawRoundedRectangle(body.expanded(1.5f), 5.0f, 1.2f);
    }
}

void GgdDrumGrid::paintPlaybackEffects(juce::Graphics& g, juce::Rectangle<int> clip)
{
    if (pulses.empty() || !prefs.playedEffects)
        return;

    const double now = nowMs();
    const double decay = juce::jmax(100, prefs.effectDecayMs);
    const auto view = viewArea();
    const float visibleLeft = static_cast<float>(view.getX() + nameWidth);
    const float visibleRight = juce::jmin(static_cast<float>(view.getRight()), xForTick(patternLengthTicks()));

    for (const auto& pulse : pulses)
    {
        const double age = now - pulse.startMs;
        if (age < 0.0 || age >= decay)
            continue; // not due yet, or finished
        const auto* item = itemForRow(pulse.row);
        if (item == nullptr || !occupied(pulse.row, pulse.tick))
            continue;
        if (item->y + item->height < clip.getY() || item->y > clip.getBottom())
            continue;

        const float progress = static_cast<float>(age / decay);
        const float envelope = 1.0f - progress;
        const float fast = envelope * envelope;
        const float slow = std::sqrt(envelope);
        const float velocityGain = prefs.velocityReactive ? 0.35f + 0.65f * pulse.velocity / 127.0f : 1.0f;
        const float gain = juce::jlimit(0.0f, 1.5f, prefs.effectIntensity * velocityGain);
        if (gain <= 0.001f)
            continue;

        const auto colour = prefs.colourByInstrument ? item->colour : tc(GgdThemeRole::accent);
        const float cx = hitCentreX(pulse.tick);
        const float cy = static_cast<float>(item->y) + rowHeight * 0.5f;
        const float base = juce::jlimit(8.0f, 20.0f, cellWidth() * 0.3f + 7.0f);

        if (prefs.playedLaneFlash)
        {
            g.setColour(colour.withAlpha(juce::jlimit(0.0f, 0.16f, 0.12f * gain * slow)));
            g.fillRect(juce::Rectangle<float>(visibleLeft, static_cast<float>(item->y),
                                              visibleRight - visibleLeft, static_cast<float>(rowHeight - 1)));
        }

        if (cx < visibleLeft - 60.0f || cx > visibleRight + 60.0f)
            continue;

        const float bloom = prefs.reduceMotion ? base : base + progress * 12.0f;
        g.setColour(colour.withAlpha(juce::jlimit(0.0f, 0.30f, 0.24f * gain * envelope)));
        g.fillEllipse(juce::Rectangle<float>(bloom * 2.0f, bloom * 2.0f).withCentre({ cx, cy }));
        g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 0.55f, 0.45f * gain * fast)));
        g.fillEllipse(juce::Rectangle<float>(base * 0.8f, base * 0.8f).withCentre({ cx, cy }));

        if (prefs.playedRipple && !prefs.reduceMotion)
        {
            const float ripple = base + 4.0f + progress * 28.0f;
            g.setColour(colour.withAlpha(juce::jlimit(0.0f, 0.6f, 0.5f * gain * envelope)));
            g.drawEllipse(juce::Rectangle<float>(ripple * 2.0f, ripple * 2.0f).withCentre({ cx, cy }),
                          1.0f + 1.2f * fast);
        }
    }
}

void GgdDrumGrid::paintPlayhead(juce::Graphics& g)
{
    if (playTick < 0.0)
        return;

    const float x = xForTick(playTick);
    const float bottom = static_cast<float>(getHeight());
    const auto colour = tc(GgdThemeRole::playhead);

    if (prefs.playheadGlow)
    {
        juce::ColourGradient glow(colour.withAlpha(0.16f), x, 0.0f, colour.withAlpha(0.0f), x + 56.0f, 0.0f, false);
        g.setGradientFill(glow);
        g.fillRect(juce::Rectangle<float>(x, static_cast<float>(rulerHeight), 56.0f, bottom - rulerHeight));
    }

    g.setColour(colour.withAlpha(0.95f));
    g.fillRect(juce::Rectangle<float>(x - 0.75f, static_cast<float>(rulerHeight), 1.5f, bottom - rulerHeight));
}

void GgdDrumGrid::paintBubble(juce::Graphics& g, juce::Rectangle<float> bubble, const juce::String& text,
                              float pointerX, bool pointerDown)
{
    g.setColour(tc(GgdThemeRole::shadow).withMultipliedAlpha(0.5f));
    g.fillRoundedRectangle(bubble.translated(0.0f, 2.0f), 6.0f);
    g.setColour(tc(GgdThemeRole::panelSoft));
    g.fillRoundedRectangle(bubble, 6.0f);
    g.setColour(tc(GgdThemeRole::accent).withAlpha(0.85f));
    g.drawRoundedRectangle(bubble.reduced(0.5f), 6.0f, 1.0f);

    juce::Path pointer;
    const float px = juce::jlimit(bubble.getX() + 8.0f, bubble.getRight() - 8.0f, pointerX);
    if (pointerDown)
        pointer.addTriangle(px - 4.0f, bubble.getBottom() - 0.5f, px + 4.0f, bubble.getBottom() - 0.5f,
                            px, bubble.getBottom() + 4.5f);
    g.setColour(tc(GgdThemeRole::panelSoft));
    g.fillPath(pointer);

    g.setColour(tc(GgdThemeRole::text));
    g.setFont(GgdFonts::semibold(11.0f));
    g.drawText(text, bubble, juce::Justification::centred, false);
}

void GgdDrumGrid::paintPopups(juce::Graphics& g)
{
    const auto view = viewArea();
    const float visibleLeft = static_cast<float>(view.getX() + nameWidth + 4);
    const float visibleRight = static_cast<float>(view.getRight() - 4);
    const float visibleTop = static_cast<float>(view.getY() + rulerHeight + 2);

    if (popupKind != PopupKind::none && popupRef.valid())
    {
        const int y = rowY(popupRef.row);
        if (y >= 0)
        {
            const auto text = popupKind == PopupKind::velocity
                ? "VEL " + juce::String(popupValue)
                : "TIME " + juce::String(popupValue > 0 ? "+" : "") + juce::String(popupValue) + "t";
            const float width = popupKind == PopupKind::velocity ? 68.0f : 84.0f;
            const float cx = hitCentreX(popupRef.tick);
            const float left = juce::jlimit(visibleLeft, juce::jmax(visibleLeft, visibleRight - width), cx - width * 0.5f);
            const float top = juce::jmax(visibleTop, static_cast<float>(y) - 25.0f);
            paintBubble(g, { left, top, width, 21.0f }, text, cx, true);
        }
    }

    for (const auto& ref : chipRefs)
    {
        const int y = rowY(ref.row);
        if (y < 0 || !occupied(ref.row, ref.tick))
            continue;
        const int velocity = stateOf(ref).velocity;
        const float width = velocity >= 100 ? 36.0f : 31.0f;
        const float cx = hitCentreX(ref.tick);
        const float left = juce::jlimit(visibleLeft, juce::jmax(visibleLeft, visibleRight - width), cx - width * 0.5f);
        const float top = juce::jmax(visibleTop, static_cast<float>(y) - 17.0f);
        const juce::Rectangle<float> chip(left, top, width, 16.0f);
        g.setColour(tc(GgdThemeRole::panelSoft));
        g.fillRoundedRectangle(chip, 4.0f);
        g.setColour(colourForRow(ref.row).withAlpha(0.85f));
        g.drawRoundedRectangle(chip.reduced(0.5f), 4.0f, 1.0f);
        g.setColour(tc(GgdThemeRole::text));
        g.setFont(GgdFonts::semibold(10.0f));
        g.drawText(juce::String(velocity), chip, juce::Justification::centred, false);
    }
}

void GgdDrumGrid::paintLabelColumn(juce::Graphics& g, juce::Rectangle<int> clip)
{
    const int x = viewPosition().x;
    if (clip.getX() > x + nameWidth)
        return;

    const auto* p = pattern();
    std::map<int, int> hitsPerRow;
    if (p != nullptr)
        for (int i = 0; i < p->getEventCount(); ++i)
            ++hitsPerRow[canonicalRow(p->getEvent(i)->row)];

    g.setColour(tc(GgdThemeRole::panel));
    g.fillRect(x, clip.getY(), nameWidth, clip.getHeight());

    for (const auto& item : layout)
    {
        if (item.y + item.height < clip.getY() || item.y > clip.getBottom())
            continue;
        const juce::Rectangle<float> band(static_cast<float>(x), static_cast<float>(item.y),
                                          static_cast<float>(nameWidth), static_cast<float>(item.height));

        if (item.header)
        {
            const bool hover = hoverHeaderGroup == item.groupId;
            g.setColour(hover ? tc(GgdThemeRole::panelSoft) : tc(GgdThemeRole::groupFill));
            g.fillRect(band);
            g.setColour(prefs.colourByInstrument ? item.colour : tc(GgdThemeRole::groupLine));
            g.fillRect(band.withWidth(3.0f));
            g.setColour(tc(GgdThemeRole::borderSoft));
            g.fillRect(band.withY(band.getBottom() - 1.0f).withHeight(1.0f));

            const bool collapsed = collapsedGroups.count(item.groupId) != 0;
            ggdDrawIcon(g, collapsed ? GgdIcon::chevronRight : GgdIcon::chevronDown,
                        { band.getX() + 12.0f, band.getCentreY() - 6.0f, 12.0f, 12.0f },
                        tc(GgdThemeRole::muted), 1.8f);

            int groupHits = 0;
            for (const auto& other : layout)
                if (!other.header && other.groupId == item.groupId)
                    groupHits += hitsPerRow[other.canonicalRow];
            if (collapsed && map != nullptr)
                for (const auto& group : map->groups)
                    if (group.id == item.groupId)
                        for (const auto& articulation : group.articulations)
                            groupHits += hitsPerRow[project.findCanonicalRow(articulation.semanticId)];

            g.setColour(tc(GgdThemeRole::text).withAlpha(0.9f));
            g.setFont(GgdFonts::bold(10.5f));
            g.drawText(item.groupLabel.toUpperCase(), band.withTrimmedLeft(32.0f).withTrimmedRight(40.0f),
                       juce::Justification::centredLeft, true);
            if (groupHits > 0)
            {
                g.setColour(tc(GgdThemeRole::faint));
                g.setFont(GgdFonts::medium(10.0f));
                g.drawText(juce::String(groupHits), band.withTrimmedRight(12.0f), juce::Justification::centredRight,
                           false);
            }
            continue;
        }

        const bool pressed = auditionRow == item.canonicalRow;
        const bool hover = hoverLabelRow == item.canonicalRow || hoverRow == item.canonicalRow;
        g.setColour(item.odd ? tc(GgdThemeRole::rowAlternate) : tc(GgdThemeRole::panel));
        g.fillRect(band);
        if (pressed)
        {
            g.setColour(item.colour.withAlpha(0.22f));
            g.fillRect(band);
        }
        else if (hover)
        {
            g.setColour(tc(GgdThemeRole::rowHover));
            g.fillRect(band);
        }

        if (prefs.colourByInstrument)
        {
            g.setColour(item.colour.withAlpha(hitsPerRow[item.canonicalRow] > 0 ? 0.95f : 0.35f));
            g.fillRoundedRectangle(band.getX() + 10.0f, band.getCentreY() - 3.0f, 6.0f, 6.0f, 3.0f);
        }

        g.setColour(tc(GgdThemeRole::borderSoft).withAlpha(0.55f));
        g.fillRect(band.withY(band.getBottom() - 1.0f).withHeight(1.0f));

        g.setColour(hitsPerRow[item.canonicalRow] > 0 ? tc(GgdThemeRole::text) : tc(GgdThemeRole::muted));
        g.setFont(GgdFonts::medium(12.0f));
        g.drawText(item.label, band.withTrimmedLeft(24.0f).withTrimmedRight(48.0f),
                   juce::Justification::centredLeft, true);

        if (item.noteName.isNotEmpty())
        {
            g.setColour(tc(GgdThemeRole::faint));
            g.setFont(GgdFonts::regular(10.5f));
            g.drawText(item.noteName, band.withTrimmedRight(12.0f), juce::Justification::centredRight, false);
        }
    }

    g.setColour(tc(GgdThemeRole::border));
    g.fillRect(x + nameWidth - 1, clip.getY(), 1, clip.getHeight());
}

void GgdDrumGrid::paintRuler(juce::Graphics& g, int tickFrom, int tickTo)
{
    const auto view = viewPosition();
    const int top = view.y;
    const int length = patternLengthTicks();
    const int bar = barTicks();
    const int beat = beatTicks();
    const auto area = juce::Rectangle<int>(view.x, top, getWidth() - view.x, rulerHeight);

    g.setColour(tc(GgdThemeRole::panel));
    g.fillRect(area);

    const float timelineEnd = xForTick(length);
    g.setColour(tc(GgdThemeRole::outside));
    g.fillRect(juce::Rectangle<float>(timelineEnd, static_cast<float>(top),
                                      juce::jmax(0.0f, static_cast<float>(getWidth()) - timelineEnd),
                                      static_cast<float>(rulerHeight)));

    // beats and subdivisions
    const int major = isTripletGrid() ? GGD_TICKS_PER_8TH_TRIPLET : GGD_TICKS_PER_16TH;
    for (int tick = (tickFrom / snapTicks) * snapTicks; tick <= juce::jmin(length, tickTo); tick += snapTicks)
    {
        if (tick % beat == 0)
            continue;
        const float x = std::round(xForTick(tick));
        const float h = tick % major == 0 ? 6.0f : 3.0f;
        g.setColour(tc(GgdThemeRole::borderStrong).withAlpha(0.6f));
        g.fillRect(juce::Rectangle<float>(x, top + rulerHeight - h - 1.0f, 1.0f, h));
    }

    g.setFont(GgdFonts::medium(10.5f));
    for (int tick = (tickFrom / beat) * beat; tick <= juce::jmin(length, tickTo); tick += beat)
    {
        if (tick % bar == 0 || tick >= length)
            continue;
        const float x = std::round(xForTick(tick));
        g.setColour(tc(GgdThemeRole::borderStrong));
        g.fillRect(juce::Rectangle<float>(x, top + rulerHeight - 11.0f, 1.0f, 10.0f));
        if (beat * pixelsPerTick() >= 22.0f)
        {
            const int barIndex = tick / bar;
            const int beatInBar = (tick % bar) / beat;
            g.setColour(tc(GgdThemeRole::faint));
            g.drawText(juce::String(barIndex + 1) + "." + juce::String(beatInBar + 1),
                       juce::Rectangle<float>(x + 4.0f, top + 20.0f, 40.0f, 12.0f),
                       juce::Justification::centredLeft, false);
        }
    }

    for (int tick = (tickFrom / bar) * bar; tick < juce::jmin(length, tickTo + 1); tick += bar)
    {
        const float x = std::round(xForTick(tick));
        g.setColour(tc(GgdThemeRole::barLine));
        g.fillRect(juce::Rectangle<float>(x - 0.5f, static_cast<float>(top + 6), 2.0f, rulerHeight - 7.0f));
        g.setColour(tc(GgdThemeRole::text));
        g.setFont(GgdFonts::bold(12.0f));
        g.drawText(juce::String(tick / bar + 1), juce::Rectangle<float>(x + 6.0f, top + 5.0f, 40.0f, 15.0f),
                   juce::Justification::centredLeft, false);
    }

    // pattern end
    g.setColour(tc(GgdThemeRole::barLine));
    g.fillRect(juce::Rectangle<float>(timelineEnd - 1.0f, static_cast<float>(top + 6), 2.0f, rulerHeight - 7.0f));

    // playhead marker
    if (playTick >= 0.0)
    {
        const float x = xForTick(playTick);
        juce::Path marker;
        marker.addTriangle(x - 6.0f, static_cast<float>(top + rulerHeight - 9), x + 6.0f,
                           static_cast<float>(top + rulerHeight - 9), x, static_cast<float>(top + rulerHeight - 1));
        g.setColour(tc(GgdThemeRole::playhead));
        g.fillPath(marker);
        g.fillRect(juce::Rectangle<float>(x - 0.75f, static_cast<float>(top), 1.5f, rulerHeight - 9.0f));
    }

    g.setColour(tc(GgdThemeRole::border));
    g.fillRect(area.getX(), top + rulerHeight - 1, area.getWidth(), 1);

    // corner over the articulation column
    const auto corner = juce::Rectangle<int>(view.x, top, nameWidth, rulerHeight);
    g.setColour(tc(GgdThemeRole::panel));
    g.fillRect(corner);
    g.setColour(tc(GgdThemeRole::border));
    g.fillRect(corner.getRight() - 1, top, 1, rulerHeight);
    g.fillRect(corner.getX(), top + rulerHeight - 1, corner.getWidth(), 1);

    const auto grid = gridLabel(snapTicks);

    g.setColour(tc(GgdThemeRole::text));
    g.setFont(GgdFonts::bold(11.0f));
    g.drawText(toolMode == ToolMode::draw ? "DRAW" : "SELECT",
               corner.toFloat().withTrimmedLeft(14.0f).withHeight(24.0f).withY(top + 5.0f),
               juce::Justification::centredLeft, false);
    g.setColour(tc(GgdThemeRole::muted));
    g.setFont(GgdFonts::medium(10.5f));
    g.drawText(grid + " grid  " + juce::String(juce::roundToInt(zoomScale * 100.0f)) + "%",
               corner.toFloat().withTrimmedLeft(14.0f).withY(top + 23.0f).withHeight(14.0f),
               juce::Justification::centredLeft, false);
}
