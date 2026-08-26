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

private:
    SeqAudioProcessor& processor;
    juce::LookAndFeel_V4 lookAndFeel;

    juce::Array<GgdKitMap> maps;
    juce::Array<GgdCanonicalRow> canonicalRows;
    int activeMapIndex = 0;

    juce::Label productLabel;
    juce::Label transportStatus;
    juce::Label hintLabel;
    juce::ComboBox kitSelector;
    juce::ComboBox patternSelector;
    juce::ComboBox barsSelector;
    juce::TextEditor patternName;
    juce::TextButton undoButton { "Undo" };
    juce::TextButton clearButton { "Clear" };
    juce::Viewport gridViewport;
    std::unique_ptr<GgdDrumGrid> grid;

    void timerCallback() override;
    void configureLookAndFeel();
    void initialiseDrumState();
    void applyActiveMapBindings(bool publish);
    void refreshControlsFromModel();
    void setActiveMap(int index);
    void publishModelChange();
    void clearCurrentPattern();
    juce::String mapPersistenceToken(const GgdKitMap& map) const;
    int mapIndexFromLayerName(const juce::String& layerName) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdDrumEditor)
};
