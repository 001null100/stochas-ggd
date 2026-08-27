#pragma once

#include "PluginProcessor.h"
#include "GgdKitMap.h"
#include "GgdMidiImporter.h"
#include "GgdPatternFile.h"
#include "GgdLibraryBrowser.h"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>

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
    juce::LookAndFeel_V4 lookAndFeel;

    juce::Array<GgdKitMap> maps;
    juce::Array<GgdCanonicalRow> canonicalRows;
    int activeMapIndex = 0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    int activeBars = 1;

    juce::Label productLabel;
    juce::Label transportStatus;
    juce::Label hintLabel;
    juce::Label selectionStatusLabel;
    juce::Label meterLabel;
    juce::Label meterSlash;
    juce::Label barsLabel;
    juce::Label zoomLabel;
    juce::Label zoomValueLabel;
    juce::ComboBox kitSelector;
    juce::ComboBox patternSelector;
    juce::ComboBox numeratorSelector;
    juce::ComboBox denominatorSelector;
    juce::TextEditor barsEditor;
    juce::TextEditor patternName;
    juce::TextButton drawModeButton { "Draw" };
    juce::TextButton selectModeButton { "Select" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton importMidiButton { "Import MIDI" };
    juce::TextButton fitZoomButton { "125%" };
    juce::TextButton copyButton { "Copy" };
    juce::TextButton pasteButton { "Paste" };
    juce::TextButton velocityDownButton { "Vel -" };
    juce::TextButton velocityUpButton { "Vel +" };
    juce::TextButton timingEarlierButton { "Earlier" };
    juce::TextButton timingLaterButton { "Later" };
    juce::TextButton humanizeButton { "Humanize" };
    juce::TextButton deleteSelectionButton { "Delete" };
    juce::TextButton patternActionsButton { "Pattern" };
    juce::Slider zoomSlider;
    juce::Viewport gridViewport;
    std::unique_ptr<GgdDrumGrid> grid;
    std::unique_ptr<GgdLibraryBrowser> libraryBrowser;
    std::unique_ptr<juce::FileChooser> midiFileChooser;
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
    static constexpr int bottomAreaHeight = 38;
    static constexpr int browserWidth = 310;
    static constexpr size_t maxHistoryDepth = 24;

    void timerCallback() override;
    void configureLookAndFeel();
    void initialiseDrumState();
    void applyActiveMapBindings(bool publish);
    void applyPatternGeometry(bool publish = true);
    void refreshControlsFromModel();
    void commitBarCountEditor();
    void refreshPatternSelectorLabels();
    void refreshZoomControls(float scale);
    void setActiveMap(int index);

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
    void applyMidiImport(const GgdMidiImportResult& result);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdDrumEditor)
};
