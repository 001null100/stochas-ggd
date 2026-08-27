#pragma once

#include "PluginProcessor.h"
#include "GgdKitMap.h"

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
    juce::Label meterLabel;
    juce::Label meterSlash;
    juce::Label barsLabel;
    juce::Label zoomLabel;
    juce::Label zoomValueLabel;
    juce::ComboBox kitSelector;
    juce::ComboBox patternSelector;
    juce::ComboBox numeratorSelector;
    juce::ComboBox denominatorSelector;
    juce::ComboBox barsSelector;
    juce::TextEditor patternName;
    juce::TextButton drawModeButton { "Draw" };
    juce::TextButton selectModeButton { "Select" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton fitZoomButton { "125%" };
    juce::Slider zoomSlider;
    juce::Viewport gridViewport;
    std::unique_ptr<GgdDrumGrid> grid;

    static constexpr int topAreaHeight = 112;
    static constexpr int bottomAreaHeight = 32;

    void timerCallback() override;
    void configureLookAndFeel();
    void initialiseDrumState();
    void applyActiveMapBindings(bool publish);
    void applyPatternGeometry(bool publish = true);
    void refreshControlsFromModel();
    void rebuildBarsSelector();
    void refreshPatternSelectorLabels();
    void refreshZoomControls(float scale);
    void setActiveMap(int index);
    void performUndo();
    void publishModelChange();
    void clearCurrentPattern();
    void updatePersistenceTag();
    juce::String mapPersistenceToken(const GgdKitMap& map) const;
    int mapIndexFromLayerName(const juce::String& layerName) const;
    bool parseMeterFromLayerName(const juce::String& layerName,
                                 int& numerator,
                                 int& denominator) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdDrumEditor)
};
