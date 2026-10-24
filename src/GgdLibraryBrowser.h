#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

// Library panel with two tabs: GGD MIDI grooves and saved Stochas GGD
// patterns. Each tab browses one root folder (remembered on this computer)
// as a tree, or as a flat list while the filter has text.
class GgdLibraryBrowser : public juce::Component
{
public:
    using FileCallback = std::function<void(const juce::File&)>;
    using VoidCallback = std::function<void()>;

    GgdLibraryBrowser();
    ~GgdLibraryBrowser() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setGrooveOpenCallback(FileCallback callback);
    void setPatternOpenCallback(FileCallback callback);
    void setSavePatternCallback(VoidCallback callback);
    void setLoadedGroove(const juce::File& file);
    void setLoadedPattern(const juce::File& file);
    void clearLoaded();

    juce::File getGrooveRoot() const;
    juce::File getPatternRoot() const;
    void refresh();
    void themeChanged();

    // 0 = grooves, 1 = patterns
    void showTab(int index);
    int getCurrentTab() const { return currentTab; }

    // The library settings file, shared by every open window so one window
    // never overwrites another's folder choice.
    struct SharedSettings;

private:
    class BrowserPane;
    class TabButton;

    juce::SharedResourcePointer<SharedSettings> settings;
    std::unique_ptr<BrowserPane> groovePane;
    std::unique_ptr<BrowserPane> patternPane;
    std::unique_ptr<TabButton> grooveTab;
    std::unique_ptr<TabButton> patternTab;
    int currentTab = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdLibraryBrowser)
};
