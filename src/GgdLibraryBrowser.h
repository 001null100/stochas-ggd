#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

class GgdLibraryBrowser : public juce::Component
{
public:
    using FileCallback = std::function<void(const juce::File&)>;
    using VoidCallback = std::function<void()>;

    GgdLibraryBrowser();
    ~GgdLibraryBrowser() override;

    void resized() override;

    void setGrooveOpenCallback(FileCallback callback);
    void setPatternOpenCallback(FileCallback callback);
    void setSavePatternCallback(VoidCallback callback);

    juce::File getGrooveRoot() const;
    juce::File getPatternRoot() const;
    void refresh();

private:
    class BrowserPane;

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<BrowserPane> groovePane;
    std::unique_ptr<BrowserPane> patternPane;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GgdLibraryBrowser)
};
