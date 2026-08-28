#pragma once

#include "PluginProcessor.h"
#include "GgdKitMap.h"
#include "GgdMidiImporter.h"
#include "GgdMidiExporter.h"
#include "GgdPatternFile.h"
#include "GgdLibraryBrowser.h"
#include "GgdUiTheme.h"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>

// The inherited constants use a separate menu item ID (2) and model value (0)
// for "MIDI respond: no". The drum editor writes the model directly.
#ifdef SEQCTL_MIDI_RESPOND_NO
#undef SEQCTL_MIDI_RESPOND_NO
#define SEQCTL_MIDI_RESPOND_NO SEQ_MIDI_RESPOND_NO
#endif

class GgdDrumGrid;

class GgdDrumEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit GgdDrumEditor(SeqAudioProcessor& processor);
    ~GgdDrumEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    SeqAudioProcessor& processor;
    GgdLookAndFeel lookAndFeel;

    juce::Array<GgdKitMap> maps;
    juce::Array<GgdCanonicalRow> canonicalRows;
    int activeMapIndex = 0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    int activeBars = 1;
    int gridTicks = GGD_TICKS_PER_16TH;
    bool tripletMode = false;
    bool beta2UiInitialised = false;
    bool beta3UiInitialised = false;
    bool beta4UiInitialised = false;

    juce::Label productLabel;
    juce::Label transportStatus;
    juce::Label hintLabel;
    juce::Label selectionStatusLabel;
    juce::Label meterLabel;
    juce::Label meterSlash;
    juce::Label barsLabel;
    juce::Label gridLabel;       // Beta 1 compatibility; hidden by Beta 2.
    juce::Label zoomLabel;
    juce::Label zoomValueLabel;
    juce::Label probabilityLabel;
    juce::ComboBox kitSelector;
    juce::ComboBox patternSelector;
    juce::ComboBox numeratorSelector;
    juce::ComboBox denominatorSelector;
    juce::ComboBox gridSelector; // Beta 1 compatibility; hidden by Beta 2.
    juce::ComboBox probabilitySelector;
    juce::ComboBox themeSelector;
    juce::TextEditor barsEditor;
    juce::TextEditor patternName;
    juce::TextButton drawModeButton { "Draw" };
    juce::TextButton selectModeButton { "Select" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton importMidiButton { "Import MIDI" };
    juce::TextButton exportMidiButton { "Export MIDI" };
    juce::TextButton tripletModeButton { "Triplet" };
    juce::TextButton fitZoomButton { "125%" };
    juce::TextButton selectAllButton { "All" };
    juce::TextButton copyButton { "Copy" };
    juce::TextButton pasteButton { "Paste" };
    juce::TextButton velocityDownButton { "Vel -" };
    juce::TextButton velocityUpButton { "Vel +" };
    juce::TextButton ghostButton { "Ghost" };
    juce::TextButton accentButton { "Accent" };
    juce::TextButton probability25Button { "P25" };
    juce::TextButton probability50Button { "P50" };
    juce::TextButton probability75Button { "P75" };
    juce::TextButton probability100Button { "P100" };
    juce::TextButton timingEarlierButton { "Earlier" };
    juce::TextButton timingResetButton { "Quantize" };
    juce::TextButton timingLaterButton { "Later" };
    juce::TextButton humanizeButton { "Humanize" };
    juce::TextButton flamButton { "Flam" };
    juce::TextButton doubleButton { "Double" };
    juce::TextButton deleteSelectionButton { "Delete" };
    juce::TextButton patternActionsButton { "Pattern" };
    juce::Slider zoomSlider;
    juce::Viewport gridViewport;
    std::unique_ptr<GgdDrumGrid> grid;
    std::unique_ptr<GgdLibraryBrowser> libraryBrowser;
    std::unique_ptr<juce::PropertiesFile> appearanceSettings;
    std::unique_ptr<juce::FileChooser> midiFileChooser;
    std::unique_ptr<juce::FileChooser> midiExportChooser;
    std::unique_ptr<juce::FileChooser> patternSaveChooser;

    std::deque<GgdPatternSnapshot> undoHistory;
    std::deque<GgdPatternSnapshot> redoHistory;
    std::optional<GgdPatternSnapshot> lastCommittedSnapshot;
    std::array<std::uint64_t, SEQ_MAX_PATTERNS> cleanPatternFingerprints {};
    std::array<bool, SEQ_MAX_PATTERNS> cleanPatternFingerprintValid {};
    bool restoringHistory = false;
    double lastUndoMs = -1000.0;
    double lastRedoMs = -1000.0;

    static constexpr int topAreaHeight = 112;
    static constexpr int bottomAreaHeight = 72;
    static constexpr int browserWidth = 310;
    static constexpr size_t maxHistoryDepth = 24;
    static constexpr int maxPatternTicks = SEQ_MAX_STEPS * GGD_TICKS_PER_16TH;

    void timerCallback() override;
    void configureLookAndFeel();
    void initialiseDrumState();
    void applyActiveMapBindings(bool publish);
    void applyPatternGeometry(bool publish = true);
    void loadGeometryFromCurrentPattern();
    void refreshControlsFromModel();
    void commitBarCountEditor();
    void refreshPatternSelectorLabels();
    void refreshZoomControls(float scale);
    void setActiveMap(int index);
    void setGridTicks(int ticks);
    void updateGridResolutionForZoom(float scale);
    juce::String currentGridText() const;
    void initialiseBeta2Ui();
    void initialiseBeta3Ui();
    void initialiseBeta4Ui();
    void applyBeta4Theme(GgdThemeId theme, bool persist);
    void updateSelectionPropertyControls();

    GgdPatternSnapshot capturePattern(int pattern) const;
    GgdPatternSnapshot captureCurrentPattern() const;
    void restorePatternSnapshot(const GgdPatternSnapshot& snapshot, bool publish = true);
    void recordCommittedPatternEdit();
    void resetHistoryForCurrentPattern(bool markClean);
    void initialiseCleanPatternFingerprints();
    void markCurrentPatternClean();
    bool currentPatternHasChanges() const;
    void performUndo();
    void performRedo();

    void chooseMidiFile();
    void importMidiFile(const juce::File& file);
    void applyMidiImport(const GgdMidiImportResult& result, const juce::File& sourceFile);
    void exportCurrentPatternMidi();
    void requestLoadGroove(const juce::File& file);
    void requestLoadPattern(const juce::File& file);
    void applyPatternFile(const juce::File& file, const GgdPatternSnapshot& snapshot);
    void saveCurrentPatternToLibrary();
    void requestPatternReplacement(const juce::String& description,
                                   std::function<void()> replacement);

    void showPatternActions();
    void duplicateCurrentPatternSlot();
    bool currentPatternSlotIsEmpty(int pattern) const;
    void updateContextStrip();

    void publishModelChange(bool recordHistory = true);
    void clearCurrentPattern();
    void updatePersistenceTag();
    juce::String mapPersistenceToken(const GgdKitMap& map) const;
    int mapIndexFromLayerName(const juce::String& layerName) const;
    bool parseMeterFromLayerName(const juce::String& layerName,
                                 int& numerator,
                                 int& denominator) const;

    // Beta 2 keeps Beta 1's event-aware editor implementation as a compatibility
    // base and replaces only the shell/presentation methods below.
    void paintLegacy(juce::Graphics& g);
    void resizedLegacy();
    bool keyPressedLegacy(const juce::KeyPress& key);
    void timerCallbackLegacy();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdDrumEditor)
};
