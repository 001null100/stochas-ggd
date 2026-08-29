#include "GgdDrumGridV1.h"
#include "GgdUiTheme.h"

#include <algorithm>
#include <cmath>
#include <map>

float GgdDrumGrid::getTimelineContentWidthPixels() const
{
    return juce::jmax(0.0f,
        xForTick(static_cast<float>(patternLengthTicks())) - static_cast<float>(nameWidth));
}

std::vector<std::pair<int, int>> GgdDrumGrid::getSelectionCoordinates() const
{
    std::vector<std::pair<int, int>> result;
    result.reserve(selection.size());
    for (const auto& ref : selection)
        result.emplace_back(ref.row, ref.tick);
    return result;
}

void GgdDrumGrid::restoreSelectionCoordinates(
    const std::vector<std::pair<int, int>>& coordinates)
{
    selection.clear();
    for (const auto& coordinate : coordinates)
    {
        const int row = coordinate.first;
        const int tick = coordinate.second;
        if (row < 0 || row >= canonicalRows.size() || tick < 0)
            continue;
        if (map != nullptr && map->findArticulation(canonicalRows.getReference(row).semanticId) == nullptr)
            continue;
        if (eventOccupied(row, tick))
            selection.push_back({ row, tick });
    }
    notifySelectionChanged();
    repaint();
}

void GgdDrumGrid::setUndoRedoCallbacks(std::function<void()> undo,
                                       std::function<void()> redo)
{
    undoCallback = std::move(undo);
    redoCallback = std::move(redo);
}

void GgdDrumGrid::setSelectionCallback(std::function<void()> callback)
{
    selectionCallback = std::move(callback);
}

void GgdDrumGrid::setInteractionPreferences(bool shiftHoverVelocity,
                                            bool auditionArticulations)
{
    shiftHoverVelocityInspector = shiftHoverVelocity;
    articulationAuditionEnabled = auditionArticulations;
    if (!shiftHoverVelocityInspector && dragMode == DragMode::none)
    {
        valuePopupKind = ValuePopupKind::none;
        valuePopupPinned = false;
    }
    repaint();
}

void GgdDrumGrid::fillSelectionGapsSelectNew()
{
    if (selection.size() < 2 || snapTicks <= 0)
        return;

    std::map<int, std::vector<EventState>> byRow;
    for (const auto& ref : selection)
        byRow[ref.row].push_back(snapshotEvent(ref));

    std::vector<EventRef> created;

    for (auto& entry : byRow)
    {
        auto& states = entry.second;
        if (states.size() < 2)
            continue;

        std::sort(states.begin(), states.end(),
                  [](const EventState& a, const EventState& b)
                  {
                      return a.ref.tick < b.ref.tick;
                  });

        const int firstTick = states.front().ref.tick;
        const int lastTick = states.back().ref.tick;
        const int start = ((firstTick + snapTicks - 1) / snapTicks) * snapTicks;
        const int end = (lastTick / snapTicks) * snapTicks;
        if (start > end)
            continue;

        size_t rightIndex = 1;
        for (int tick = start; tick <= end; tick += snapTicks)
        {
            if (eventOccupied(entry.first, tick))
                continue;

            while (rightIndex < states.size() && states[rightIndex].ref.tick < tick)
                ++rightIndex;

            const size_t leftIndex = rightIndex == 0 ? 0 : rightIndex - 1;
            const size_t clampedRight = juce::jmin(rightIndex, states.size() - 1);
            const auto& leftState = states[leftIndex];
            const auto& rightState = states[clampedRight];

            EventState state = leftState;
            if (rightState.ref.tick != leftState.ref.tick)
            {
                const float t = juce::jlimit(
                    0.0f, 1.0f,
                    static_cast<float>(tick - leftState.ref.tick)
                        / static_cast<float>(rightState.ref.tick - leftState.ref.tick));
                state.velocity = juce::jlimit(
                    1, 127,
                    static_cast<int>(std::lround(
                        leftState.velocity
                        + (rightState.velocity - leftState.velocity) * t)));
            }

            state.ref = { entry.first, tick };
            state.durationTicks = juce::jmin(
                state.durationTicks, juce::jmax(1, snapTicks));
            if (writeEvent(state, entry.first, tick))
                created.push_back({ entry.first, tick });
        }
    }

    if (created.empty())
        return;

    selection = std::move(created);
    publishChange();
}

bool GgdDrumGridV1::activeMapLayoutNeedsRefresh() const
{
    if (map == nullptr)
        return false;

    int expectedRows = 0;
    for (int row = 0; row < canonicalRows.size(); ++row)
        if (map->findArticulation(canonicalRows.getReference(row).semanticId) != nullptr)
            ++expectedRows;

    int actualRows = 0;
    for (const auto& item : layout)
    {
        if (item.header)
            continue;
        ++actualRows;
        if (item.canonicalRow < 0 || item.canonicalRow >= canonicalRows.size())
            return true;
        if (map->findArticulation(canonicalRows.getReference(item.canonicalRow).semanticId) == nullptr)
            return true;
    }

    return actualRows != expectedRows;
}

void GgdDrumGridV1::rebuildActiveMapLayout()
{
    layout.clear();
    int y = rulerHeight;
    juce::String currentGroup;

    for (int row = 0; row < canonicalRows.size(); ++row)
    {
        const auto& canonical = canonicalRows.getReference(row);
        const GgdGroup* activeGroup = nullptr;
        const GgdArticulation* articulation = nullptr;

        if (map != nullptr)
        {
            for (const auto& group : map->groups)
            {
                for (const auto& candidate : group.articulations)
                {
                    if (candidate.semanticId == canonical.semanticId)
                    {
                        activeGroup = &group;
                        articulation = &candidate;
                        break;
                    }
                }
                if (articulation != nullptr)
                    break;
            }

            if (articulation == nullptr)
                continue;
        }

        const juce::String groupId = activeGroup != nullptr ? activeGroup->id : canonical.groupId;
        const juce::String groupLabel = activeGroup != nullptr ? activeGroup->label : canonical.groupLabel;

        if (groupId != currentGroup)
        {
            currentGroup = groupId;
            LayoutItem header;
            header.header = true;
            header.y = y;
            header.height = headerHeight;
            header.groupId = groupId;
            header.groupLabel = groupLabel;
            layout.push_back(std::move(header));
            y += headerHeight;
        }

        if (collapsedGroups.count(groupId) != 0)
            continue;

        LayoutItem item;
        item.y = y;
        item.height = rowHeight;
        item.canonicalRow = row;
        item.groupId = groupId;
        item.groupLabel = groupLabel;
        item.label = articulation != nullptr && articulation->label.isNotEmpty()
            ? articulation->label : canonical.defaultLabel;

        if (articulation != nullptr)
            if (const auto* binding = articulation->primaryNoteBinding())
                item.noteName = binding->noteName;

        layout.push_back(std::move(item));
        y += rowHeight;
    }

    const int width = static_cast<int>(std::ceil(xForTick(
        static_cast<float>(patternLengthTicks())))) + 24;
    setSize(juce::jmax(500, width), juce::jmax(200, y + 8));
}

void GgdDrumGridV1::refreshActiveMapLayout()
{
    if (activeMapLayoutNeedsRefresh())
    {
        rebuildActiveMapLayout();
        repaint();
    }
}

void GgdDrumGridV1::showMultiVelocity(const std::vector<EventRef>& refs, bool pinned)
{
    multiVelocityRefs = refs;
    multiVelocityPinned = pinned;
    multiVelocityUntilMs = juce::Time::getMillisecondCounterHiRes() + 760.0;
    valuePopupKind = ValuePopupKind::none;
    valuePopupPinned = false;
    repaint();
}

void GgdDrumGridV1::clearMultiVelocity()
{
    if (multiVelocityRefs.empty())
        return;
    multiVelocityRefs.clear();
    multiVelocityPinned = false;
    repaint();
}

void GgdDrumGridV1::paintMultiVelocity(juce::Graphics& g)
{
    if (multiVelocityRefs.empty())
        return;

    if (!multiVelocityPinned
        && juce::Time::getMillisecondCounterHiRes() > multiVelocityUntilMs)
    {
        multiVelocityRefs.clear();
        return;
    }

    const int viewX = currentViewX();
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    const float visibleLeft = static_cast<float>(viewX + nameWidth + 3);
    const float visibleRight = viewport != nullptr
        ? static_cast<float>(viewX + viewport->getViewWidth() - 3)
        : static_cast<float>(getWidth() - 3);

    for (const auto& ref : multiVelocityRefs)
    {
        const int y = layoutYForCanonical(ref.row);
        if (y < 0 || !eventOccupied(ref.row, ref.tick))
            continue;

        const int velocity = snapshotEvent(ref).velocity;
        const juce::String text = "V" + juce::String(velocity);
        const float width = velocity >= 100 ? 38.0f : 34.0f;
        const float height = 17.0f;
        float left = visualHitCenterX(ref.tick) - width * 0.5f;
        left = juce::jlimit(visibleLeft,
                            juce::jmax(visibleLeft, visibleRight - width),
                            left);
        const float top = static_cast<float>(juce::jmax(rulerHeight + 1, y - 18));
        const juce::Rectangle<float> chip(left, top, width, height);

        g.setColour(ggdThemeColour(GgdThemeRole::panelRaised).withAlpha(0.97f));
        g.fillRoundedRectangle(chip, 4.0f);
        g.setColour(ggdThemeColour(GgdThemeRole::accentSecondary).withAlpha(0.88f));
        g.drawRoundedRectangle(chip, 4.0f, 1.0f);
        g.setColour(ggdThemeColour(GgdThemeRole::text));
        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText(text, chip.toNearestInt(), juce::Justification::centred, false);
    }
}

void GgdDrumGridV1::paint(juce::Graphics& g)
{
    refreshActiveMapLayout();
    GgdDrumGridBeta7::paint(g);
    paintMultiVelocity(g);
}

void GgdDrumGridV1::mouseDown(const juce::MouseEvent& e)
{
    refreshActiveMapLayout();

    if (!articulationAuditionEnabled)
    {
        if (const auto* item = itemAtY(e.position.y))
        {
            const int stickyX = currentViewX();
            const bool overPanel = e.position.x >= stickyX
                                && e.position.x < stickyX + nameWidth;
            if (!item->header && overPanel)
            {
                grabKeyboardFocus();
                return;
            }
        }
    }

    GgdDrumGridBeta7::mouseDown(e);
}

void GgdDrumGridV1::mouseDrag(const juce::MouseEvent& e)
{
    GgdDrumGridBeta7::mouseDrag(e);

    if (selectVelocityStarted && selection.size() > 1)
        showMultiVelocity(selection, true);
    else if (!selectVelocityStarted)
        clearMultiVelocity();
}

void GgdDrumGridV1::mouseUp(const juce::MouseEvent& e)
{
    const bool multiDrag = selectVelocityPending && selectVelocityStarted && selection.size() > 1;
    const auto refs = multiDrag ? selection : std::vector<EventRef>();

    GgdDrumGridBeta7::mouseUp(e);

    if (multiDrag)
        showMultiVelocity(refs, false);
    else
        clearMultiVelocity();
}

void GgdDrumGridV1::mouseMove(const juce::MouseEvent& e)
{
    refreshActiveMapLayout();

    if (shiftHoverVelocityInspector)
    {
        GgdDrumGridBeta7::mouseMove(e);
        return;
    }

    GgdDrumGrid::mouseMove(e);
    if (dragMode == DragMode::none && !selectVelocityPending
        && valuePopupKind == ValuePopupKind::velocity)
    {
        valuePopupKind = ValuePopupKind::none;
        valuePopupPinned = false;
        repaint();
    }
}

void GgdDrumGridV1::mouseExit(const juce::MouseEvent& e)
{
    GgdDrumGridBeta7::mouseExit(e);
    if (dragMode == DragMode::none)
        clearMultiVelocity();
}

void GgdDrumGridV1::modifierKeysChanged(const juce::ModifierKeys& modifiers)
{
    GgdDrumGridBeta7::modifierKeysChanged(modifiers);
    if (!modifiers.isShiftDown() && dragMode == DragMode::none)
        clearMultiVelocity();
}
