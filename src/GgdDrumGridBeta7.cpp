#include "GgdDrumGridBeta7.h"
#include "GgdUiTheme.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace
{
juce::Colour bc(GgdThemeRole role)
{
    return ggdThemeColour(role);
}

int wrapPositive(int value, int modulus)
{
    if (modulus <= 0)
        return 0;
    const int wrapped = value % modulus;
    return wrapped < 0 ? wrapped + modulus : wrapped;
}
}

// GgdDrumGrid declares mouseDoubleClick so its vtable remains complete even
// when a host constructs the base grid directly. Beta 7's actual behavior lives
// in the derived interaction wrapper below.
void GgdDrumGrid::mouseDoubleClick(const juce::MouseEvent&)
{
}

int GgdDrumGrid::quantizedTickFor(int tick) const
{
    const int length = patternLengthTicks();
    if (length <= 0 || snapTicks <= 0)
        return 0;

    const int maxGridTick = ((length - 1) / snapTicks) * snapTicks;
    const int nearest = static_cast<int>(std::llround(
        static_cast<double>(tick) / static_cast<double>(snapTicks))) * snapTicks;
    return juce::jlimit(0, maxGridTick, nearest);
}

int GgdDrumGrid::timingOffsetFromGrid(int tick) const
{
    return tick - quantizedTickFor(tick);
}

void GgdDrumGrid::showValuePopup(ValuePopupKind kind,
                                 const EventRef& ref,
                                 int value,
                                 bool pinned)
{
    valuePopupKind = kind;
    valuePopupRef = ref;
    valuePopupValue = value;
    valuePopupPinned = pinned;
    valuePopupUntilMs = juce::Time::getMillisecondCounterHiRes() + 760.0;
    repaint();
}

void GgdDrumGrid::paintValuePopup(juce::Graphics& g)
{
    if (valuePopupKind == ValuePopupKind::none || valuePopupRef.tick < 0)
        return;

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (!valuePopupPinned && now > valuePopupUntilMs)
    {
        valuePopupKind = ValuePopupKind::none;
        return;
    }

    const int rowY = layoutYForCanonical(valuePopupRef.row);
    if (rowY < 0)
        return;

    const juce::String text = valuePopupKind == ValuePopupKind::velocity
        ? "VEL " + juce::String(valuePopupValue)
        : "TIME " + (valuePopupValue > 0 ? "+" : "")
            + juce::String(valuePopupValue) + "t";

    const float centreX = visualHitCenterX(valuePopupRef.tick);
    const float bubbleW = valuePopupKind == ValuePopupKind::velocity ? 72.0f : 86.0f;
    const float bubbleH = 22.0f;

    const int viewX = currentViewX();
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    const float visibleLeft = static_cast<float>(viewX + nameWidth + 5);
    const float visibleRight = viewport != nullptr
        ? static_cast<float>(viewX + viewport->getViewWidth() - 6)
        : static_cast<float>(getWidth() - 6);

    float left = centreX - bubbleW * 0.5f;
    left = juce::jlimit(visibleLeft,
                        juce::jmax(visibleLeft, visibleRight - bubbleW),
                        left);
    const float top = static_cast<float>(juce::jmax(rulerHeight + 2, rowY - 24));
    const juce::Rectangle<float> bubble(left, top, bubbleW, bubbleH);

    g.setColour(bc(GgdThemeRole::panelRaised).withAlpha(0.98f));
    g.fillRoundedRectangle(bubble, 5.0f);
    g.setColour(bc(GgdThemeRole::accentSecondary).withAlpha(0.94f));
    g.drawRoundedRectangle(bubble, 5.0f, 1.2f);

    juce::Path pointer;
    const float pointerX = juce::jlimit(left + 8.0f, left + bubbleW - 8.0f, centreX);
    pointer.addTriangle(pointerX - 4.0f, top + bubbleH,
                        pointerX + 4.0f, top + bubbleH,
                        pointerX, top + bubbleH + 4.0f);
    g.setColour(bc(GgdThemeRole::panelRaised));
    g.fillPath(pointer);

    g.setColour(bc(GgdThemeRole::text));
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText(text, bubble.toNearestInt(), juce::Justification::centred, false);
}

void GgdDrumGrid::selectRowsContainingSelection()
{
    if (selection.empty())
        return;

    std::set<int> rows;
    for (const auto& ref : selection)
        rows.insert(ref.row);

    selection.clear();
    if (const auto* p = pattern())
    {
        for (int i = 0; i < p->getEventCount(); ++i)
        {
            const auto* event = p->getEvent(i);
            if (event == nullptr)
                continue;
            const int row = canonicalRowForStorage(event->row);
            if (rows.count(row) != 0)
                selection.push_back({ row, event->tick });
        }
    }

    notifySelectionChanged();
    repaint();
}

void GgdDrumGrid::repeatSelection()
{
    if (selection.empty())
        return;

    int minTick = selection.front().tick;
    int maxTick = minTick;
    std::vector<EventState> states;
    states.reserve(selection.size());
    for (const auto& ref : selection)
    {
        minTick = juce::jmin(minTick, ref.tick);
        maxTick = juce::jmax(maxTick, ref.tick);
        states.push_back(snapshotEvent(ref));
    }

    const int span = juce::jmax(snapTicks, maxTick - minTick + snapTicks);
    const int length = patternLengthTicks();
    std::vector<EventRef> created;
    for (const auto& state : states)
    {
        const int target = state.ref.tick + span;
        if (target < 0 || target >= length || eventOccupied(state.ref.row, target))
            continue;
        if (writeEvent(state, state.ref.row, target))
            created.push_back({ state.ref.row, target });
    }

    if (created.empty())
        return;

    // Selecting the new copy makes repeated presses chain the phrase forward.
    selection = std::move(created);
    publishChange();
}

void GgdDrumGrid::mirrorSelectedTiming()
{
    if (selection.size() < 2)
        return;

    int minTick = selection.front().tick;
    int maxTick = minTick;
    std::vector<EventState> states;
    states.reserve(selection.size());
    for (const auto& ref : selection)
    {
        minTick = juce::jmin(minTick, ref.tick);
        maxTick = juce::jmax(maxTick, ref.tick);
        states.push_back(snapshotEvent(ref));
    }

    std::set<std::pair<int, int>> targets;
    for (const auto& state : states)
    {
        const int target = minTick + maxTick - state.ref.tick;
        const EventRef targetRef { state.ref.row, target };
        if (target < 0 || target >= patternLengthTicks()
            || !targets.insert({ targetRef.row, targetRef.tick }).second
            || (eventOccupied(targetRef.row, targetRef.tick) && !isSelected(targetRef)))
            return;
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
    publishChange();
}

void GgdDrumGrid::rampSelectedVelocity(bool rising)
{
    if (selection.size() < 2)
        return;

    auto* p = pattern();
    if (p == nullptr)
        return;

    int minTick = selection.front().tick;
    int maxTick = minTick;
    int minVelocity = 127;
    int maxVelocity = 1;
    for (const auto& ref : selection)
    {
        const auto state = snapshotEvent(ref);
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
    const int tickSpan = juce::jmax(1, maxTick - minTick);
    for (const auto& ref : selection)
    {
        auto* event = p->findEventPtr(storageRowForCanonical(ref.row), ref.tick);
        if (event == nullptr)
            continue;
        float t = static_cast<float>(ref.tick - minTick) / static_cast<float>(tickSpan);
        if (!rising)
            t = 1.0f - t;
        const int velocity = juce::jlimit(
            1, 127,
            static_cast<int>(std::lround(minVelocity + (maxVelocity - minVelocity) * t)));
        if (event->velocity != velocity)
        {
            event->velocity = static_cast<std::uint8_t>(velocity);
            changed = true;
        }
    }

    if (changed)
        publishChange();
}

void GgdDrumGrid::rotateSelection(int direction)
{
    if (selection.size() < 2 || direction == 0)
        return;

    int minTick = selection.front().tick;
    int maxTick = minTick;
    std::vector<EventState> states;
    states.reserve(selection.size());
    for (const auto& ref : selection)
    {
        minTick = juce::jmin(minTick, ref.tick);
        maxTick = juce::jmax(maxTick, ref.tick);
        states.push_back(snapshotEvent(ref));
    }

    const int span = juce::jmax(snapTicks, maxTick - minTick + snapTicks);
    const int amount = (direction < 0 ? -snapTicks : snapTicks);
    std::set<std::pair<int, int>> targets;
    std::vector<EventRef> destination;
    destination.reserve(states.size());

    for (const auto& state : states)
    {
        const int target = minTick + wrapPositive(state.ref.tick - minTick + amount, span);
        const EventRef targetRef { state.ref.row, target };
        if (target < 0 || target >= patternLengthTicks()
            || !targets.insert({ targetRef.row, targetRef.tick }).second
            || (eventOccupied(targetRef.row, targetRef.tick) && !isSelected(targetRef)))
            return;
        destination.push_back(targetRef);
    }

    for (const auto& state : states)
        removeEvent(state.ref);

    selection.clear();
    for (size_t i = 0; i < states.size(); ++i)
    {
        if (writeEvent(states[i], destination[i].row, destination[i].tick))
            selection.push_back(destination[i]);
    }
    publishChange();
}

void GgdDrumGrid::thinSelection()
{
    if (selection.size() < 2)
        return;

    std::map<int, std::vector<EventRef>> byRow;
    for (const auto& ref : selection)
        byRow[ref.row].push_back(ref);

    std::vector<EventRef> remove;
    std::vector<EventRef> keep;
    for (auto& row : byRow)
    {
        auto& refs = row.second;
        std::sort(refs.begin(), refs.end(),
                  [](const EventRef& a, const EventRef& b) { return a.tick < b.tick; });
        for (size_t i = 0; i < refs.size(); ++i)
            (i & 1 ? remove : keep).push_back(refs[i]);
    }

    if (remove.empty())
        return;
    for (const auto& ref : remove)
        removeEvent(ref);
    selection = std::move(keep);
    publishChange();
}

void GgdDrumGridBeta7::paint(juce::Graphics& g)
{
    GgdDrumGrid::paint(g);
    paintValuePopup(g);
}

void GgdDrumGridBeta7::mouseDown(const juce::MouseEvent& e)
{
    // In Select mode, preserve Shift-click selection toggling while reserving
    // Shift-drag for velocity. We wait for actual pointer movement before
    // deciding which gesture the user intended.
    if (toolMode == ToolMode::select
        && e.mods.isShiftDown()
        && !e.mods.isAltDown()
        && !e.mods.isCtrlDown()
        && !e.mods.isCommandDown()
        && !e.mods.isRightButtonDown())
    {
        const auto hit = eventAt(e.position.x, e.position.y);
        if (hit.tick >= 0)
        {
            grabKeyboardFocus();
            dragStartPoint = e.position;
            selectVelocityPending = true;
            selectVelocityStarted = false;
            selectVelocityRef = hit;
            selectVelocityStartStates.clear();
            dragMode = DragMode::velocity;
            const auto state = snapshotEvent(hit);
            showValuePopup(ValuePopupKind::velocity, hit, state.velocity, true);
            return;
        }
    }

    GgdDrumGrid::mouseDown(e);

    if (dragMode == DragMode::velocity && dragEvent.tick >= 0)
    {
        const auto state = snapshotEvent(dragEvent);
        showValuePopup(ValuePopupKind::velocity, dragEvent, state.velocity, true);
    }
    else if (dragMode == DragMode::timing && dragEvent.tick >= 0)
    {
        showValuePopup(ValuePopupKind::timing,
                       dragEvent,
                       timingOffsetFromGrid(dragEvent.tick),
                       true);
    }
}

void GgdDrumGridBeta7::mouseDrag(const juce::MouseEvent& e)
{
    if (selectVelocityPending)
    {
        const float distance = e.position.getDistanceFrom(dragStartPoint);
        if (!selectVelocityStarted && distance < 3.0f)
            return;

        if (!selectVelocityStarted)
        {
            if (!isSelected(selectVelocityRef))
            {
                selection.clear();
                addSelection(selectVelocityRef);
                notifySelectionChanged();
            }

            selectVelocityStartStates.clear();
            selectVelocityStartStates.reserve(selection.size());
            for (const auto& ref : selection)
                selectVelocityStartStates.push_back(snapshotEvent(ref));
            selectVelocityStarted = true;
        }

        auto* p = pattern();
        if (p == nullptr)
            return;

        const int delta = static_cast<int>(std::lround(
            (dragStartPoint.y - e.position.y) * 0.8f));
        bool changed = false;
        int anchorVelocity = snapshotEvent(selectVelocityRef).velocity;
        for (const auto& state : selectVelocityStartStates)
        {
            auto* event = p->findEventPtr(
                storageRowForCanonical(state.ref.row), state.ref.tick);
            if (event == nullptr)
                continue;
            const int velocity = juce::jlimit(1, 127, state.velocity + delta);
            if (event->velocity != velocity)
            {
                event->velocity = static_cast<std::uint8_t>(velocity);
                changed = true;
            }
            if (sameEvent(state.ref, selectVelocityRef))
                anchorVelocity = velocity;
        }

        if (changed)
            publishChange();
        showValuePopup(ValuePopupKind::velocity,
                       selectVelocityRef,
                       anchorVelocity,
                       true);
        return;
    }

    const auto previousMode = dragMode;
    GgdDrumGrid::mouseDrag(e);

    if (previousMode == DragMode::velocity && dragEvent.tick >= 0)
    {
        showValuePopup(ValuePopupKind::velocity,
                       dragEvent,
                       snapshotEvent(dragEvent).velocity,
                       true);
    }
    else if (previousMode == DragMode::timing && dragEvent.tick >= 0)
    {
        showValuePopup(ValuePopupKind::timing,
                       dragEvent,
                       timingOffsetFromGrid(dragEvent.tick),
                       true);
    }
}

void GgdDrumGridBeta7::mouseUp(const juce::MouseEvent& e)
{
    if (selectVelocityPending)
    {
        if (!selectVelocityStarted)
        {
            toggleSelection(selectVelocityRef);
        }
        else
        {
            showValuePopup(ValuePopupKind::velocity,
                           selectVelocityRef,
                           snapshotEvent(selectVelocityRef).velocity,
                           false);
        }

        selectVelocityPending = false;
        selectVelocityStarted = false;
        selectVelocityStartStates.clear();
        selectVelocityRef = {};
        dragMode = DragMode::none;
        repaint();
        return;
    }

    const auto previousMode = dragMode;
    const auto previousRef = dragEvent;
    int popupValue = 0;
    if (previousMode == DragMode::velocity && previousRef.tick >= 0)
        popupValue = snapshotEvent(previousRef).velocity;
    else if (previousMode == DragMode::timing && previousRef.tick >= 0)
        popupValue = timingOffsetFromGrid(previousRef.tick);

    GgdDrumGrid::mouseUp(e);

    if (previousMode == DragMode::velocity && previousRef.tick >= 0)
        showValuePopup(ValuePopupKind::velocity, previousRef, popupValue, false);
    else if (previousMode == DragMode::timing && previousRef.tick >= 0)
        showValuePopup(ValuePopupKind::timing, previousRef, popupValue, false);
}

void GgdDrumGridBeta7::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto mods = e.mods;
    if (!mods.isAltDown() || mods.isShiftDown()
        || mods.isCtrlDown() || mods.isCommandDown())
        return;

    auto hit = eventAt(e.position.x, e.position.y);
    if (hit.tick < 0)
        return;

    const int target = quantizedTickFor(hit.tick);
    if (target == hit.tick)
    {
        showValuePopup(ValuePopupKind::timing, hit, 0, false);
        return;
    }

    const EventRef targetRef { hit.row, target };
    if (eventOccupied(targetRef.row, targetRef.tick))
        return;

    auto* p = pattern();
    if (p == nullptr)
        return;

    const int storage = storageRowForCanonical(hit.row);
    if (!p->moveEvent(storage, hit.tick, storage, target))
        return;

    for (auto& ref : selection)
        if (sameEvent(ref, hit))
            ref.tick = target;

    publishChange();
    showValuePopup(ValuePopupKind::timing, targetRef, 0, false);
}
