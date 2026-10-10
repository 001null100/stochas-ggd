#pragma once

#include "PluginProcessor.h"
#include "GgdDrumGrid.h"
#include "GgdLibraryBrowser.h"
#include "GgdPatternOps.h"
#include "GgdSettingsPanel.h"
#include "GgdUiTheme.h"

#include <array>
#include <deque>
#include <memory>
#include <vector>

// The Stochas GGD plug-in window.
//
//   +--------------------------------------------------------------+
//   | brand | pattern slots | name | Pattern | transport | output  |
//   | tools | undo | meter | bars | grid | zoom      | kit | sync   |
//   +--------------------------------------------+-----------------+
//   |                 drum timeline              |     library     |
//   +--------------------------------------------+                 |
//   | selection tools / hints / status           |                 |
//   +--------------------------------------------+-----------------+
//
// The editor edits the UI copy of the project (processor.mData) and publishes
// every committed change to the audio thread. Each pattern slot keeps its own
// undo history; meter and bar changes park hits that no longer fit so they
// come back when the pattern grows again.
class GgdDrumEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    explicit GgdDrumEditor(SeqAudioProcessor& processor);
    ~GgdDrumEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // Runs one display frame (playhead, feedback, follow). Called from the
    // display's vertical blank; public for the offscreen snapshot tool.
    void renderFrame();

    // Developer hooks used by the snapshot tool and tests.
    GgdDrumGrid& getGrid() { return *grid; }
    void openSettings();
    void closeSettings();
    void selectPatternSlot(int slot);
    void setSyncMode(GgdTimeline::SyncMode mode);
    void showStatus(const juce::String& text);

private:
    friend struct GgdEditorTestAccess;

    class SlotStrip;
    class BarsStepper;
    class TransportDisplay;
    class MidiDragButton;

    // One entry of a slot's undo history.
    struct PatternState
    {
        int numerator = 4;
        int denominator = 4;
        int bars = 1;
        std::vector<GgdSequenceEvent> events;
        juce::String name;
        std::vector<GgdParkedHit> parked;

        // Not part of the content: restored alongside it.
        std::vector<std::pair<int, int>> selection;
        juce::File source;
        bool sourceIsPattern = false;

        bool sameContent(const PatternState& other) const;
    };

    struct SlotHistory
    {
        std::deque<PatternState> undo;
        std::deque<PatternState> redo;
        PatternState committed;
        bool valid = false;
        juce::File source;
        bool sourceIsPattern = false;
    };

    SeqAudioProcessor& processor;
    const GgdProject& project;
    GgdLookAndFeel lookAndFeel;
    juce::SharedResourcePointer<GgdPreferenceStore> preferenceStore;
    GgdPreferences prefs;
    juce::TooltipWindow tooltips { this, 650 };

    // header, row 1
    std::unique_ptr<SlotStrip> slotStrip;
    juce::TextEditor patternName;
    GgdButton patternMenuButton { "Pattern", GgdIcon::menu };
    std::unique_ptr<TransportDisplay> transport;
    GgdButton outputButton { "Seq On", GgdIcon::power };
    std::unique_ptr<MidiDragButton> dragButton;
    GgdButton libraryButton { {}, GgdIcon::folder };
    GgdButton settingsButton { {}, GgdIcon::settings };

    // header, row 2
    GgdButton drawButton { "Draw", GgdIcon::pencil };
    GgdButton selectButton { "Select", GgdIcon::pointer };
    GgdButton undoButton { {}, GgdIcon::undo };
    GgdButton redoButton { {}, GgdIcon::redo };
    juce::ComboBox numeratorBox;
    juce::ComboBox denominatorBox;
    std::unique_ptr<BarsStepper> barsStepper;
    GgdButton parkedButton { {}, GgdIcon::warning };
    juce::ComboBox gridBox;
    juce::Slider zoomSlider;
    GgdButton zoomValueButton { "125%" };
    GgdButton fitButton { {}, GgdIcon::fit };
    juce::ComboBox kitBox;
    juce::ComboBox syncBox;

    // timeline
    juce::Viewport viewport;
    std::unique_ptr<GgdDrumGrid> grid;

    // context strip
    GgdButton ghostButton { "Ghost" };
    GgdButton accentButton { "Accent" };
    std::array<GgdButton, 4> chanceButtons;
    GgdButton quantizeButton { "Quantize" };
    GgdButton humanizeButton { "Humanize" };
    GgdButton flamButton { "Flam" };
    GgdButton doubleButton { "Double" };
    GgdButton moreButton { "More" };
    GgdButton deleteButton { "Delete" };

    std::unique_ptr<GgdLibraryBrowser> browser;
    std::unique_ptr<GgdSettingsPanel> settingsPanel;
    std::unique_ptr<juce::FileChooser> chooser;

    std::array<SlotHistory, SEQ_MAX_PATTERNS> history;
    bool restoringHistory = false;
    int nameFieldSlot = -1; // slot whose name the name field shows
    int appliedTheme = -1;

    std::unique_ptr<juce::VBlankAttachment> vblank;
    double lastFrameMs = 0.0;
    bool focusGivenToGrid = false;

    // playhead follow
    int followSetViewX = -1;
    double followPausedUntilMs = 0.0;
    bool followPadded = false;

    // transport / host
    bool transportPlaying = false;
    int playingSlot = -1;
    bool hostMeterKnown = false;
    int hostNumerator = 4;
    int hostDenominator = 4;

    juce::String statusText;
    double statusUntilMs = 0.0;
    juce::Rectangle<int> brandArea;
    juce::Rectangle<int> meterSlashArea;
    juce::Rectangle<int> contextArea;
    juce::Rectangle<int> hintArea;
    juce::Rectangle<int> selectionInfoArea;

    static constexpr int row1Height = 52;
    static constexpr int row2Height = 46;
    static constexpr int headerHeight = row1Height + row2Height;
    static constexpr int contextHeight = 50;
    static constexpr size_t maxHistory = 64;

    // model access
    SequenceData& ui() const { return *processor.mData.getUISeqData(); }
    SequenceLayer& layer() const { return *ui().getLayer(GgdProject::drumLayer); }
    int currentSlot() const { return layer().getCurrentPattern(); }
    GgdEventPattern& currentPattern() const { return *layer().getEventPattern(currentSlot()); }
    const GgdKitMap* activeMap() const { return project.getActiveMap(ui()); }

    // frame and housekeeping
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void reloadFromProcessor();
    void updateTransport();
    void updatePlayheadFollow();

    // preferences
    void applyPreferences(const GgdPreferences& preferences, bool save);
    int effectiveGridTicks() const;
    void setBaseGrid(int ticks);

    // edits and history
    PatternState capture(int slot) const;
    bool liveMatches(const PatternState& state, int slot) const;
    void restore(int slot, const PatternState& state);
    void ensureHistory(int slot);
    void commitEdit();
    void commitSlotEdit(int slot);
    void applyHistoryState(int slot, const PatternState& state);
    void handleSelectionChanged();
    void undo();
    void redo();
    void resetHistories();

    // pattern operations
    void setMeter(int numerator, int denominator);
    void setBars(int bars);
    void discardParkedHits();
    void reportGeometryChange(const GgdPatternOps::GeometryChange& change);
    void renamePattern();
    void clearPattern();
    void doublePatternLength();
    void duplicateToEmptySlot();
    void copyPatternToSlot(int target);
    void clearSlot(int slot);
    void toggleOutput();
    void setKit(int index);
    void showPatternMenu();
    void showMoreMenu();
    void showSlotMenu(int slot);

    // files
    void loadGroove(const juce::File& file);
    void loadPatternFile(const juce::File& file);
    void chooseMidiToImport();
    void savePatternToLibrary();
    void exportMidiFile();
    void beginMidiDrag(juce::Component* source);
    juce::String currentPatternFileName() const;
    void showAlert(const juce::String& title, const juce::String& message);
    void focusGrid();
    void updateBrowserHighlight();

    // presentation
    void refreshAll();
    void refreshHeader();
    void refreshSlots();
    void refreshHistoryButtons();
    void refreshContextStrip();
    void refreshZoomControls();
    void layoutContextStrip();
    void layoutViewport();
    void paintHeader(juce::Graphics& g);
    void paintContextStrip(juce::Graphics& g);
    juce::String slotTooltip(int slot) const;
    bool slotHasContent(int slot) const;
    static juce::String shortcut(const juce::String& keys);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdDrumEditor)
};
