#pragma once

#include "PluginProcessor.h"
#include "GgdKitMap.h"

#include <functional>
#include <set>
#include <vector>

class GgdDrumGrid : public juce::Component
{
public:
    enum class ToolMode { draw, select };

    GgdDrumGrid(SeqAudioProcessor& processor,
                const juce::Array<GgdCanonicalRow>& canonicalRows,
                std::function<void()> undoCallback,
                std::function<void()> redoCallback,
                std::function<void()> publishCallback,
                std::function<void(float)> zoomCallback,
                std::function<void(ToolMode)> toolCallback,
                std::function<void()> selectionCallback);

    void setMap(const GgdKitMap* map);
    void setMeter(int numerator, int denominator, int bars);
    void setSnapTicks(int ticks);
    int getSnapTicks() const { return snapTicks; }
    void refreshSize();
    void patternChanged();

    void setToolMode(ToolMode mode);
    ToolMode getToolMode() const { return toolMode; }

    void setPlayPosition(int stepPosition);
    float getZoomScale() const { return zoomScale; }
    void setZoomScale(float scale);
    void resetZoom();

    int getSelectedCount() const { return static_cast<int>(selection.size()); }
    int getSelectedProbability() const;
    void pollFallbackShortcuts(bool active);

    void selectAll();
    void copySelectionToClipboard();
    void pasteClipboard();
    void adjustSelectedVelocityBy(int delta);
    void setSelectedVelocity(int velocity);
    void setSelectedProbability(int probability);
    void adjustSelectedTimingBy(int deltaTicks);
    void quantizeSelected();
    void humanizeSelected();
    void createFlamFromSelection();
    void createDoubleFromSelection();
    void deleteSelected();

    bool keyPressed(const juce::KeyPress& key) override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;

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
    };

    struct EventRef
    {
        int row = -1;
        int tick = -1;
    };

    struct EventState
    {
        EventRef ref;
        int velocity = 100;
        int probability = SEQ_PROB_ON;
        int durationTicks = GGD_DEFAULT_EVENT_DURATION_TICKS;
    };

    enum class DragMode
    {
        none,
        paint,
        // The Beta 2 compatibility translation temporarily macro-renames the
        // token `paint`; keep an enum alias so DragMode::paint references in the
        // inherited interaction implementation remain valid under that include.
        paintLegacy = paint,
        velocity,
        timing,
        marquee,
        move
    };

    SeqAudioProcessor& processor;
    const juce::Array<GgdCanonicalRow>& canonicalRows;
    std::function<void()> undoCallback;
    std::function<void()> redoCallback;
    std::function<void()> publishCallback;
    std::function<void(float)> zoomCallback;
    std::function<void(ToolMode)> toolCallback;
    std::function<void()> selectionCallback;
    const GgdKitMap* map = nullptr;

    std::vector<LayoutItem> layout;
    std::set<juce::String> collapsedGroups;
    std::vector<EventRef> selection;
    std::vector<EventRef> marqueeBaseSelection;
    std::vector<EventState> clipboard;
    int clipboardSpanTicks = GGD_TICKS_PER_16TH;

    ToolMode toolMode = ToolMode::draw;
    DragMode dragMode = DragMode::none;
    int meterNumerator = 4;
    int meterDenominator = 4;
    int meterBars = 1;
    int snapTicks = GGD_TICKS_PER_16TH;
    float zoomScale = 1.25f;
    int playStepPosition = -1;
    double playStepStartMs = 0.0;
    double playStepMs = 125.0;
    bool hasPlayStepEstimate = false;

    int hoverCanonicalRow = -1;
    int hoverTick = -1;
    bool paintErase = false;
    int lastPaintRow = -1;
    int lastPaintTick = -1;

    EventRef dragEvent;
    int dragStartVelocity = 100;
    int dragStartTick = 0;
    juce::Point<float> dragStartPoint;
    std::vector<EventState> dragSelectionStates;
    int dragDeltaTicks = 0;
    int dragDeltaRows = 0;
    bool duplicateDrag = false;

    juce::Point<float> marqueeStart;
    juce::Rectangle<float> marqueeRect;
    bool marqueeAdditive = false;

    bool fallbackDDown = false;
    bool fallbackSDown = false;
    bool fallbackADown = false;
    bool fallbackCDown = false;
    bool fallbackVDown = false;
    bool fallbackZDown = false;
    bool fallbackYDown = false;
    bool fallbackLeftDown = false;
    bool fallbackRightDown = false;
    bool fallbackUpDown = false;
    bool fallbackDownDown = false;
    bool fallbackDeleteDown = false;
    bool fallbackBackspaceDown = false;
    bool fallbackEscapeDown = false;

    static constexpr int nameWidth = 188;
    static constexpr int rulerHeight = 42;
    static constexpr int rowHeight = 31;
    static constexpr int headerHeight = 25;
    static constexpr float pixelsPerQuarter = 128.0f;
    static constexpr float minZoomScale = 0.5f;
    static constexpr float maxZoomScale = 4.0f;

    SequenceLayer* layer() const;
    GgdEventPattern* pattern() const;
    int storageRowForCanonical(int canonicalRow) const;
    int canonicalRowForStorage(int storageRow) const;
    int patternLengthTicks() const;
    int ticksPerBar() const;

    float xForTick(float tick) const;
    float tickForX(float x) const;
    int snappedTickForX(float x) const;
    int currentViewX() const;
    int canonicalRowAtY(float y) const;
    int layoutYForCanonical(int row) const;
    const LayoutItem* itemAtY(float y) const;

    void rebuildLayout();
    void notifySelectionChanged();
    void publishChange();
    void repaintAndResize();

    bool sameEvent(const EventRef& a, const EventRef& b) const;
    bool isSelected(const EventRef& ref) const;
    void addSelection(const EventRef& ref);
    void removeSelection(const EventRef& ref);
    void toggleSelection(const EventRef& ref);
    void clearSelection(bool notify = true);
    void selectAllHits();

    EventRef eventAt(float x, float y) const;
    EventState snapshotEvent(const EventRef& ref) const;
    bool writeEvent(const EventState& state, int row, int tick);
    bool removeEvent(const EventRef& ref);
    bool eventOccupied(int row, int tick) const;

    void paintAt(int row, int tick);
    void updateMarqueeSelection();
    bool moveSelectionBy(int deltaTicks, int deltaRows, bool duplicate);
    bool nudgeSelection(int horizontal, int vertical, bool duplicate);
    void adjustSelectionVelocity(int delta);
    void duplicateSelectionWithOffset(int deltaTicks, float velocityScale);
    void deleteSelection();
    void beginSelectionMove(const juce::MouseEvent& e, bool duplicate);
    void finishSelectionMove();

    float snapZoom(float scale) const;
    void applyZoom(float scale, float anchorContent, float anchorViewport);
    void notifyZoomChanged();
    float interpolatedPlayTick() const;

    // Beta 2 compiles the Beta 1 implementation as a compatibility base and
    // replaces only presentation/playhead methods. These declarations are the
    // renamed legacy definitions used by that translation unit.
    void setPlayPositionLegacy(int stepPosition);
    void paintLegacy(juce::Graphics& g);
};