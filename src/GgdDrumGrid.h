#pragma once

#include "PluginProcessor.h"
#include "GgdKitMap.h"
#include "GgdPlayhead.h"

#include <functional>
#include <set>
#include <utility>
#include <vector>

// The drum timeline: articulation rows against musical time.
//
// The grid edits the UI copy of the current pattern directly and reports
// every committed change through Callbacks::onEdit, which publishes it to the
// audio thread and records undo history. Hits are addressed by canonical row
// and tick; storage rows are an implementation detail of GgdProject.
//
// The articulation column and the ruler are sticky: they are painted at the
// viewport's current scroll position so they never scroll out of view.
class GgdDrumGrid final : public juce::Component
{
public:
    enum class ToolMode { draw, select };

    struct Callbacks
    {
        std::function<void()> onEdit;
        std::function<void()> onSelectionChanged;
        std::function<void(ToolMode)> onToolChanged;
        std::function<void(float)> onZoomChanged;
        std::function<void()> onUndo;
        std::function<void()> onRedo;
        std::function<void(const juce::String&)> onStatus;
    };

    struct Preferences
    {
        bool smoothPlayhead = true;
        bool playheadGlow = true;
        bool shiftHoverVelocity = true;
        bool audition = true;
        bool colourByInstrument = true;
        bool playedEffects = true;
        bool playedRipple = true;
        bool playedLaneFlash = true;
        bool velocityReactive = true;
        bool reduceMotion = false;
        float effectIntensity = 1.0f;
        int effectDecayMs = 420;
    };

    GgdDrumGrid(SeqAudioProcessor& processor, const GgdProject& project, Callbacks callbacks);

    void setKitMap(const GgdKitMap* map);
    void setPreferences(const Preferences& preferences);

    // The current pattern's geometry changed (meter, bars, slot switch).
    void patternGeometryChanged();
    // The current pattern's content was replaced (load, undo, clear).
    void patternReplaced();

    void setSnapTicks(int ticks);
    int getSnapTicks() const { return snapTicks; }

    void setToolMode(ToolMode mode);
    ToolMode getToolMode() const { return toolMode; }

    float getZoomScale() const { return zoomScale; }
    void setZoomScale(float scale);
    void resetZoom();
    // Zooms so the whole pattern fits the visible timeline.
    void zoomToFit();
    static constexpr float defaultZoomScale = 1.25f;
    static constexpr float minZoomScale = 0.5f;
    static constexpr float maxZoomScale = 4.0f;

    // Playback presentation. Call once per display frame.
    void updatePlayback();
    bool isAnimating() const;
    float getPlayheadContentX() const;
    int getTimelineInsetPixels() const { return nameWidth; }
    float getTimelineContentWidthPixels() const;
    void setTrailingPadding(int pixels);
    // Outer size of the enclosing viewport. The grid always fills it, so the
    // empty area right of and below the pattern is painted consistently.
    void setViewportSize(int width, int height, int scrollbarThickness);

    // Area of one grid cell in component coordinates (empty if the row is
    // not visible in the current kit).
    juce::Rectangle<float> getCellBounds(int canonicalRow, int tick) const;

    // Selection queries and transforms used by the context strip.
    int getSelectedCount() const { return static_cast<int>(selection.size()); }
    int getSelectedProbability() const; // -1 when mixed or empty
    int getSelectedVelocity() const;    // average, -1 when empty
    std::vector<std::pair<int, int>> getSelectionCoordinates() const;
    void restoreSelectionCoordinates(const std::vector<std::pair<int, int>>& coordinates);

    void selectAll();
    void clearSelection();
    void copySelection(bool announce = true);
    void pasteClipboard();
    void duplicateSelection();
    void adjustSelectedVelocity(int delta);
    void shiftSelectedTiming(int deltaTicks);
    void deleteSelection();
    void setSelectedVelocity(int velocity);
    void setSelectedProbability(int probability);
    void quantizeSelection();
    void humanizeSelection();
    void createFlam();
    void createDouble();
    void selectRowsContainingSelection();
    void fillSelectionGaps();
    void mirrorSelection();
    void rampSelectedVelocity(bool rising);
    void scaleSelectedVelocityRange(float factor);
    void thinSelection();
    void nudgeSelection(int horizontal, int vertical, bool duplicate);
    void showContextMenu(juce::Point<float> position);

    // Keyboard shortcuts. Key events are used whenever the host delivers
    // them; keys a host keeps to itself are polled instead while the grid
    // owns keyboard focus.
    void pollKeyboard(bool active);
    bool keyPressed(const juce::KeyPress& key) override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;
    void modifierKeysChanged(const juce::ModifierKeys& modifiers) override;

private:
    struct LayoutItem
    {
        bool header = false;
        int canonicalRow = -1;
        int y = 0;
        int height = 0;
        juce::String groupId;
        juce::String groupLabel;
        juce::String label;
        juce::String noteName;
        juce::Colour colour;
        bool odd = false;
    };

    struct EventRef
    {
        int row = -1;
        int tick = -1;
        bool valid() const { return row >= 0 && tick >= 0; }
        bool operator==(const EventRef& other) const { return row == other.row && tick == other.tick; }
    };

    struct EventState
    {
        EventRef ref;
        int velocity = 100;
        int probability = SEQ_PROB_ON;
        int durationTicks = GGD_DEFAULT_EVENT_DURATION_TICKS;
    };

    enum class DragMode { none, paint, erase, velocity, timing, marquee, move, selectVelocity, audition };
    enum class PopupKind { none, velocity, timing };

    struct PlayedPulse
    {
        int row = -1; // canonical
        int tick = -1;
        int velocity = 100;
        double startMs = 0.0;
    };

    SeqAudioProcessor& processor;
    const GgdProject& project;
    Callbacks callbacks;
    Preferences prefs;
    const GgdKitMap* map = nullptr;

    std::vector<LayoutItem> layout;
    std::set<juce::String> collapsedGroups;
    std::vector<EventRef> selection;
    std::vector<EventRef> marqueeBase;
    std::vector<EventState> clipboard;
    int clipboardSpanTicks = GGD_TICKS_PER_16TH;

    ToolMode toolMode = ToolMode::draw;
    DragMode dragMode = DragMode::none;
    int snapTicks = GGD_TICKS_PER_16TH;
    float zoomScale = defaultZoomScale;
    int trailingPadding = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
    int viewportScrollbar = 0;

    // hover
    int hoverRow = -1;
    int hoverTick = -1;
    int hoverLabelRow = -1;
    juce::String hoverHeaderGroup;

    // gestures
    juce::Point<float> dragStart;
    EventRef dragRef;
    int dragStartVelocity = 100;
    int dragStartTick = 0;
    int lastPaintRow = -1;
    int lastPaintTick = -1;
    float lastEraseX = 0.0f;
    bool eraseStarted = false;
    int dragStartVisualRow = 0;
    std::vector<EventState> dragStates;
    int dragDeltaTicks = 0;
    int dragDeltaRows = 0;
    bool duplicateDrag = false;
    bool gestureChanged = false;
    juce::Rectangle<float> marqueeRect;
    bool selectVelocityStarted = false;
    int auditionRow = -1;

    // value feedback
    PopupKind popupKind = PopupKind::none;
    EventRef popupRef;
    int popupValue = 0;
    double popupUntilMs = 0.0;
    bool popupPinned = false;
    std::vector<EventRef> chipRefs;
    double chipsUntilMs = 0.0;
    bool chipsPinned = false;

    // playback
    GgdPlayheadSnapshot playhead;
    double playTick = -1.0;
    double lastPaintedPlayTick = -1.0;
    std::vector<PlayedPulse> pulses;

    // keyboard: per shortcut key, whether it is held, whether the host has
    // ever delivered it as a key event, and when polling last acted on it
    static constexpr int shortcutKeyCount = 15;
    std::array<bool, shortcutKeyCount> keyWasDown {};
    std::array<bool, shortcutKeyCount> keyDelivered {};
    std::array<double, shortcutKeyCount> keyPolledMs {};

    static constexpr int nameWidth = 196;
    static constexpr int rulerHeight = 44;
    static constexpr int rowHeight = 29;
    static constexpr int headerHeight = 26;
    static constexpr float pixelsPerQuarter = 128.0f;

    // model access
    SequenceLayer* layer() const;
    GgdEventPattern* pattern() const;
    int storageRow(int canonicalRow) const { return project.storageRowForCanonical(canonicalRow); }
    int canonicalRow(int storageRow) const { return project.canonicalRowForStorage(storageRow); }
    int patternLengthTicks() const;
    int barTicks() const;
    int beatTicks() const;
    int meterNumerator() const;

    bool performShortcut(int keyCode, const juce::ModifierKeys& mods);

    // geometry
    float xForTick(double tick) const;
    double tickForX(float x) const;
    float pixelsPerTick() const { return pixelsPerQuarter * zoomScale / GGD_EVENT_PPQ; }
    float cellWidth() const { return static_cast<float>(snapTicks) * pixelsPerTick(); }
    float hitCentreX(int tick) const { return xForTick(tick) + cellWidth() * 0.5f; }
    float hitWidth() const;
    int cellTickForX(float x) const;
    int quantize(int tick) const;
    juce::Point<int> viewPosition() const;
    juce::Rectangle<int> viewArea() const;
    bool overLabelColumn(juce::Point<float> position) const;
    bool overRuler(juce::Point<float> position) const;
    const LayoutItem* itemAtY(float y) const;
    int rowAtY(float y) const;
    int rowY(int canonical) const;
    const LayoutItem* itemForRow(int canonical) const;
    juce::Colour colourForRow(int canonical) const;

    void rebuildLayout();
    void updateSize();

    // events
    EventRef hitAt(juce::Point<float> position) const;
    EventState stateOf(const EventRef& ref) const;
    bool occupied(int row, int tick) const;
    bool cellHasHit(int row, int cellTick) const;
    int visualRowNearestY(float y) const;
    void removeHitForGesture(const EventRef& hit);
    bool writeEvent(const EventState& state, int row, int tick);
    bool removeEvent(const EventRef& ref);
    bool isSelected(const EventRef& ref) const;
    void setSelection(std::vector<EventRef> refs, bool notify = true);
    void toggleSelected(const EventRef& ref);
    void commitEdit();
    void notifySelection();
    void status(const juce::String& text);

    // gestures
    void paintCell(int row, int tick);
    void eraseAcross(int row, float fromX, float toX);
    bool moveSelectionBy(int deltaTicks, int deltaRows, bool duplicate);
    void duplicateSelectionWithOffset(int deltaTicks, float velocityScale, const juce::String& what);
    void updateMarquee();
    void applyZoom(float scale, float anchorContentX, float anchorViewportX);
    bool isTripletGrid() const;

    // feedback
    void showPopup(PopupKind kind, const EventRef& ref, int value, bool pinned);
    void hidePopup();
    void showChips(const std::vector<EventRef>& refs, bool pinned);
    void hideChips();
    void drainPlayedEvents();

    // painting
    void paintGridBackground(juce::Graphics& g, juce::Rectangle<int> clip, int tickFrom, int tickTo);
    void paintGridLines(juce::Graphics& g, juce::Rectangle<int> clip, int tickFrom, int tickTo);
    void paintHover(juce::Graphics& g);
    void paintHits(juce::Graphics& g, juce::Rectangle<int> clip, int tickFrom, int tickTo);
    void paintEmptyHint(juce::Graphics& g);
    void paintDragPreview(juce::Graphics& g);
    void paintPlaybackEffects(juce::Graphics& g, juce::Rectangle<int> clip);
    void paintPlayhead(juce::Graphics& g);
    void paintPopups(juce::Graphics& g);
    void paintLabelColumn(juce::Graphics& g, juce::Rectangle<int> clip);
    void paintRuler(juce::Graphics& g, int tickFrom, int tickTo);
    void paintBubble(juce::Graphics& g, juce::Rectangle<float> bubble, const juce::String& text,
                     float pointerX, bool pointerDown);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdDrumGrid)
};
