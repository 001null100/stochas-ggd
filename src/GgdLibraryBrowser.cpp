#include "GgdLibraryBrowser.h"
#include "GgdPatternFile.h"
#include "GgdUiTheme.h"

#include <algorithm>
#include <vector>

namespace
{
constexpr juce::uint32 browserBg = 0xff101418;
constexpr juce::uint32 browserPanel = 0xff171d22;
constexpr juce::uint32 browserBorder = 0xff2d363f;
constexpr juce::uint32 browserText = 0xffedf2f5;
constexpr juce::uint32 browserMuted = 0xff8d99a5;
constexpr juce::uint32 browserAccent = 0xff70d6c1;
constexpr int maxFilterResults = 500;

juce::Colour colour(juce::uint32 value) { return ggdLegacyThemeColour(value); }

bool matchesFile(const juce::File& file, bool patterns)
{
    if (!file.existsAsFile())
        return false;
    if (patterns)
        return file.hasFileExtension(GgdPatternFile::extension);
    return file.hasFileExtension("mid;midi");
}

void collectLibraryFiles(const juce::File& directory,
                         bool patterns,
                         std::vector<juce::File>& results)
{
    for (const auto& child : directory.findChildFiles(
             juce::File::findFilesAndDirectories, false, "*"))
    {
        if (child.isDirectory())
            collectLibraryFiles(child, patterns, results);
        else if (matchesFile(child, patterns))
            results.push_back(child);
    }
}

class FlatRootItem final : public juce::TreeViewItem
{
public:
    bool mightContainSubItems() override { return true; }
};

class FileTreeItem final : public juce::TreeViewItem
{
public:
    using OpenCallback = std::function<void(const juce::File&)>;
    using LoadedCallback = std::function<bool(const juce::File&)>;

    FileTreeItem(juce::File source, bool patternMode, OpenCallback callback, LoadedCallback loadedCallback)
        : file(std::move(source)), patterns(patternMode), onOpen(std::move(callback)),
          isLoaded(std::move(loadedCallback))
    {
    }

    bool mightContainSubItems() override
    {
        return file.isDirectory();
    }

    void itemOpennessChanged(bool isNowOpen) override
    {
        if (!isNowOpen || populated || !file.isDirectory())
            return;

        populated = true;
        std::vector<juce::File> directories;
        std::vector<juce::File> files;

        for (const auto& child : file.findChildFiles(
                 juce::File::findFilesAndDirectories, false, "*"))
        {
            if (child.isDirectory())
                directories.push_back(child);
            else if (matchesFile(child, patterns))
                files.push_back(child);
        }

        auto sorter = [](const juce::File& a, const juce::File& b)
        {
            return a.getFileName().compareNatural(b.getFileName(), true) < 0;
        };
        std::sort(directories.begin(), directories.end(), sorter);
        std::sort(files.begin(), files.end(), sorter);

        for (const auto& child : directories)
            addSubItem(new FileTreeItem(child, patterns, onOpen, isLoaded));
        for (const auto& child : files)
            addSubItem(new FileTreeItem(child, patterns, onOpen, isLoaded));
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const bool directory = file.isDirectory();
        const bool loaded = !directory && isLoaded && isLoaded(file);
        const bool selected = isSelected();

        if (loaded)
        {
            g.setColour(colour(browserAccent).withAlpha(0.18f));
            g.fillRoundedRectangle(2.0f, 1.0f, static_cast<float>(width - 4),
                                   static_cast<float>(height - 2), 4.0f);
            g.setColour(colour(browserAccent));
            g.fillRoundedRectangle(2.0f, 3.0f, 3.0f,
                                   static_cast<float>(height - 6), 1.5f);
        }
        else if (selected)
        {
            g.setColour(colour(browserAccent).withAlpha(0.10f));
            g.fillRoundedRectangle(2.0f, 1.0f, static_cast<float>(width - 4),
                                   static_cast<float>(height - 2), 4.0f);
            g.setColour(colour(browserAccent).withAlpha(0.72f));
            g.drawRoundedRectangle(2.5f, 1.5f, static_cast<float>(width - 5),
                                   static_cast<float>(height - 3), 4.0f, 1.0f);
        }

        g.setColour(directory ? colour(browserText).withAlpha(0.94f)
                              : colour(browserText));
        g.setFont(juce::Font(directory ? 11.5f : 11.0f,
                             (directory || loaded) ? juce::Font::bold : juce::Font::plain));
        const int suffixWidth = loaded ? 58 : 6;
        g.drawText(file.getFileNameWithoutExtension(),
                   9, 0, juce::jmax(0, width - suffixWidth - 9), height,
                   juce::Justification::centredLeft, true);

        if (loaded)
        {
            g.setColour(colour(browserAccent));
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText("LOADED", juce::jmax(0, width - 55), 0, 50, height,
                       juce::Justification::centredRight, false);
        }
    }

    void itemClicked(const juce::MouseEvent&) override
    {
        setSelected(true, true);
    }

    void itemDoubleClicked(const juce::MouseEvent&) override
    {
        if (file.isDirectory())
        {
            setOpen(!isOpen());
            return;
        }

        if (onOpen)
            onOpen(file);
    }

    int getItemHeight() const override { return file.isDirectory() ? 25 : 23; }

private:
    juce::File file;
    bool patterns = false;
    bool populated = false;
    OpenCallback onOpen;
    LoadedCallback isLoaded;
};

class SearchResultItem final : public juce::TreeViewItem
{
public:
    using OpenCallback = std::function<void(const juce::File&)>;
    using LoadedCallback = std::function<bool(const juce::File&)>;

    SearchResultItem(juce::File source, juce::String relative,
                     OpenCallback callback, LoadedCallback loadedCallback)
        : file(std::move(source)), display(std::move(relative)),
          onOpen(std::move(callback)), isLoaded(std::move(loadedCallback))
    {
        display = display.replaceCharacter('\\', '/');
        display = display.upToLastOccurrenceOf(".", false, false);
    }

    bool mightContainSubItems() override { return false; }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const bool loaded = isLoaded && isLoaded(file);
        if (loaded)
        {
            g.setColour(colour(browserAccent).withAlpha(0.18f));
            g.fillRoundedRectangle(2.0f, 1.0f, static_cast<float>(width - 4),
                                   static_cast<float>(height - 2), 4.0f);
            g.setColour(colour(browserAccent));
            g.fillRoundedRectangle(2.0f, 3.0f, 3.0f,
                                   static_cast<float>(height - 6), 1.5f);
        }
        else if (isSelected())
        {
            g.setColour(colour(browserAccent).withAlpha(0.10f));
            g.fillRoundedRectangle(2.0f, 1.0f, static_cast<float>(width - 4),
                                   static_cast<float>(height - 2), 4.0f);
            g.setColour(colour(browserAccent).withAlpha(0.72f));
            g.drawRoundedRectangle(2.5f, 1.5f, static_cast<float>(width - 5),
                                   static_cast<float>(height - 3), 4.0f, 1.0f);
        }

        g.setColour(colour(browserText));
        g.setFont(juce::Font(10.5f, loaded ? juce::Font::bold : juce::Font::plain));
        g.drawText(display, 9, 0, juce::jmax(0, width - (loaded ? 62 : 8)), height,
                   juce::Justification::centredLeft, true);

        if (loaded)
        {
            g.setColour(colour(browserAccent));
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText("LOADED", juce::jmax(0, width - 55), 0, 50, height,
                       juce::Justification::centredRight, false);
        }
    }

    void itemClicked(const juce::MouseEvent&) override
    {
        setSelected(true, true);
    }

    void itemDoubleClicked(const juce::MouseEvent&) override
    {
        if (onOpen)
            onOpen(file);
    }

    int getItemHeight() const override { return 24; }

private:
    juce::File file;
    juce::String display;
    OpenCallback onOpen;
    LoadedCallback isLoaded;
};
}

class GgdLibraryBrowser::BrowserPane final : public juce::Component,
                                             private juce::Timer
{
public:
    BrowserPane(juce::PropertiesFile& propertyStore,
                juce::String propertyKey,
                bool patternMode)
        : properties(propertyStore), key(std::move(propertyKey)), patterns(patternMode)
    {
        root = juce::File(properties.getValue(key));

        chooseButton.setButtonText("Folder");
        chooseButton.setTooltip(patterns
            ? "Choose the root folder containing Stochas GGD pattern files"
            : "Choose the root folder containing GGD MIDI grooves");
        chooseButton.onClick = [this] { chooseRoot(); };
        addAndMakeVisible(chooseButton);

        refreshButton.setButtonText("Refresh");
        refreshButton.setTooltip("Rescan the library folder and rebuild the search index");
        refreshButton.onClick = [this] { refresh(); };
        addAndMakeVisible(refreshButton);

        if (patterns)
        {
            saveButton.setButtonText("Save Pattern");
            saveButton.setTooltip("Save the current pattern into the pattern library");
            saveButton.onClick = [this]
            {
                if (onSave)
                    onSave();
            };
            addAndMakeVisible(saveButton);
        }

        rootLabel.setFont(10.0f);
        rootLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(rootLabel);

        filterEditor.setTooltip("Filter the cached library by filename or folder path");
        filterEditor.onTextChange = [this]
        {
            stopTimer();
            startTimer(120);
        };
        addAndMakeVisible(filterEditor);

        emptyLabel.setFont(11.0f);
        emptyLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(emptyLabel);

        tree.setRootItemVisible(false);
        tree.setDefaultOpenness(false);
        addAndMakeVisible(tree);

        applyTheme();
        refresh();
    }

    ~BrowserPane() override
    {
        stopTimer();
        tree.setRootItem(nullptr);
    }

    void applyTheme()
    {
        rootLabel.setColour(juce::Label::textColourId, colour(browserMuted));
        emptyLabel.setColour(juce::Label::textColourId, colour(browserMuted));
        filterEditor.setTextToShowWhenEmpty(patterns ? "Filter patterns..." : "Filter grooves...",
                                            colour(browserMuted).withAlpha(0.72f));
        filterEditor.setColour(juce::TextEditor::backgroundColourId, colour(browserBg));
        filterEditor.setColour(juce::TextEditor::outlineColourId, colour(browserBorder));
        filterEditor.setColour(juce::TextEditor::focusedOutlineColourId,
                               colour(browserAccent).withAlpha(0.85f));
        filterEditor.setColour(juce::TextEditor::textColourId, colour(browserText));
        tree.setColour(juce::TreeView::backgroundColourId, colour(browserBg));
        tree.setColour(juce::TreeView::linesColourId, colour(browserBorder).withAlpha(0.62f));
        tree.setColour(juce::TreeView::dragAndDropIndicatorColourId, colour(browserAccent));
        repaint();
        tree.repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(colour(browserBg));
        g.setColour(ggdThemeColour(GgdThemeRole::borderStrong).withAlpha(0.52f));
        g.drawRect(getLocalBounds(), 1);
        g.setColour(colour(browserPanel));
        g.fillRect(0, 0, getWidth(), 99);
        g.setColour(colour(browserBorder).withAlpha(0.90f));
        g.drawHorizontalLine(98, 0.0f, static_cast<float>(getWidth()));
        g.setColour(colour(browserAccent).withAlpha(0.24f));
        g.fillRect(0, 98, getWidth(), 1);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(8);
        auto buttons = bounds.removeFromTop(29);
        chooseButton.setBounds(buttons.removeFromLeft(64));
        buttons.removeFromLeft(5);
        refreshButton.setBounds(buttons.removeFromLeft(68));
        if (patterns)
        {
            buttons.removeFromLeft(5);
            saveButton.setBounds(buttons);
        }

        bounds.removeFromTop(3);
        rootLabel.setBounds(bounds.removeFromTop(21));
        bounds.removeFromTop(2);
        filterEditor.setBounds(bounds.removeFromTop(28));
        bounds.removeFromTop(7);
        tree.setBounds(bounds);
        emptyLabel.setBounds(bounds.reduced(12));
    }

    void setOpenCallback(FileCallback callback)
    {
        onOpen = std::move(callback);
        rebuildTree();
    }

    void setSaveCallback(VoidCallback callback)
    {
        onSave = std::move(callback);
    }

    void setLoadedFile(const juce::File& file)
    {
        loadedFile = file;
        tree.repaint();
    }

    juce::File getRoot() const { return root; }

    void refresh()
    {
        rebuildIndex();
        rebuildTree();
    }

    void rebuildTree()
    {
        tree.setRootItem(nullptr);
        treeRoot.reset();

        const bool valid = root.isDirectory();
        if (!valid)
        {
            tree.setVisible(false);
            emptyLabel.setVisible(true);
            emptyLabel.setText(patterns
                ? "Choose a pattern folder to build your library."
                : "Choose your GGD MIDI groove folder.",
                juce::dontSendNotification);
            rootLabel.setText("No folder selected", juce::dontSendNotification);
            rootLabel.setTooltip(juce::String());
            return;
        }

        auto loaded = [this](const juce::File& candidate)
        {
            return loadedFile != juce::File() && candidate == loadedFile;
        };

        const auto filter = filterEditor.getText().trim();
        if (filter.isEmpty())
        {
            emptyLabel.setVisible(false);
            tree.setVisible(true);
            rootLabel.setText(
                root.getFileName() + "  |  " + juce::String(static_cast<int>(indexedFiles.size())) + " files",
                juce::dontSendNotification);
            rootLabel.setTooltip(root.getFullPathName());
            treeRoot = std::make_unique<FileTreeItem>(root, patterns, onOpen, loaded);
            tree.setRootItem(treeRoot.get());
            treeRoot->setOpen(true);
            tree.repaint();
            return;
        }

        std::vector<juce::File> results;
        results.reserve(static_cast<size_t>(juce::jmin(
            maxFilterResults, static_cast<int>(indexedFiles.size()))));
        for (const auto& file : indexedFiles)
        {
            if (static_cast<int>(results.size()) >= maxFilterResults)
                break;
            if (file.getRelativePathFrom(root).containsIgnoreCase(filter))
                results.push_back(file);
        }

        rootLabel.setText(
            juce::String(static_cast<int>(results.size()))
                + (results.size() == 1 ? " match  |  " : " matches  |  ")
                + root.getFileName(),
            juce::dontSendNotification);
        rootLabel.setTooltip(root.getFullPathName());

        if (results.empty())
        {
            tree.setVisible(false);
            emptyLabel.setVisible(true);
            emptyLabel.setText("No matches for \"" + filter + "\"", juce::dontSendNotification);
            return;
        }

        emptyLabel.setVisible(false);
        tree.setVisible(true);
        auto flatRoot = std::make_unique<FlatRootItem>();
        for (const auto& file : results)
            flatRoot->addSubItem(new SearchResultItem(
                file, file.getRelativePathFrom(root), onOpen, loaded));
        treeRoot = std::move(flatRoot);
        tree.setRootItem(treeRoot.get());
        treeRoot->setOpen(true);
        tree.repaint();
    }

private:
    juce::PropertiesFile& properties;
    juce::String key;
    bool patterns = false;
    juce::File root;
    juce::File loadedFile;
    FileCallback onOpen;
    VoidCallback onSave;

    juce::TextButton chooseButton;
    juce::TextButton refreshButton;
    juce::TextButton saveButton;
    juce::Label rootLabel;
    juce::TextEditor filterEditor;
    juce::Label emptyLabel;
    juce::TreeView tree;
    std::unique_ptr<juce::TreeViewItem> treeRoot;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::File> indexedFiles;

    void timerCallback() override
    {
        stopTimer();
        rebuildTree();
    }

    void rebuildIndex()
    {
        indexedFiles.clear();
        if (!root.isDirectory())
            return;

        collectLibraryFiles(root, patterns, indexedFiles);
        std::sort(indexedFiles.begin(), indexedFiles.end(), [this](const juce::File& a, const juce::File& b)
        {
            return a.getRelativePathFrom(root).compareNatural(
                b.getRelativePathFrom(root), true) < 0;
        });
    }

    void chooseRoot()
    {
        chooser = std::make_unique<juce::FileChooser>(
            patterns ? "Choose pattern library folder" : "Choose groove library folder",
            root.isDirectory() ? root : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));

        auto safe = juce::Component::SafePointer<BrowserPane>(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectDirectories,
            [safe](const juce::FileChooser& result)
            {
                if (auto* self = safe.getComponent())
                {
                    const auto chosen = result.getResult();
                    if (!chosen.isDirectory())
                        return;

                    self->root = chosen;
                    self->properties.setValue(self->key, chosen.getFullPathName());
                    self->properties.saveIfNeeded();
                    self->refresh();
                }
            });
    }
};

GgdLibraryBrowser::GgdLibraryBrowser()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Stochas GGD";
    options.filenameSuffix = "settings";
    options.folderName = "Stochas GGD";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    settings = std::make_unique<juce::PropertiesFile>(options);

    groovePane = std::make_unique<BrowserPane>(*settings, "grooveRoot", false);
    patternPane = std::make_unique<BrowserPane>(*settings, "patternRoot", true);

    tabs.setColour(juce::TabbedComponent::backgroundColourId, colour(browserBg));
    tabs.setColour(juce::TabbedComponent::outlineColourId, colour(browserBorder));
    tabs.addTab("Grooves", colour(browserBg), groovePane.get(), false);
    tabs.addTab("Patterns", colour(browserBg), patternPane.get(), false);
    addAndMakeVisible(tabs);
}

GgdLibraryBrowser::~GgdLibraryBrowser() = default;

void GgdLibraryBrowser::resized()
{
    tabs.setBounds(getLocalBounds());
}

void GgdLibraryBrowser::setGrooveOpenCallback(FileCallback callback)
{
    groovePane->setOpenCallback(std::move(callback));
}

void GgdLibraryBrowser::setPatternOpenCallback(FileCallback callback)
{
    patternPane->setOpenCallback(std::move(callback));
}

void GgdLibraryBrowser::setSavePatternCallback(VoidCallback callback)
{
    patternPane->setSaveCallback(std::move(callback));
}

juce::File GgdLibraryBrowser::getGrooveRoot() const
{
    return groovePane->getRoot();
}

juce::File GgdLibraryBrowser::getPatternRoot() const
{
    return patternPane->getRoot();
}

void GgdLibraryBrowser::refresh()
{
    groovePane->refresh();
    patternPane->refresh();
}

void GgdLibraryBrowser::themeChanged()
{
    const int currentTab = tabs.getCurrentTabIndex();
    groovePane->applyTheme();
    patternPane->applyTheme();

    // Rebuild just the tab buttons so their explicit background colours follow
    // the new palette. Pane objects and library indexes remain untouched.
    tabs.clearTabs();
    tabs.setColour(juce::TabbedComponent::backgroundColourId, colour(browserBg));
    tabs.setColour(juce::TabbedComponent::outlineColourId, colour(browserBorder));
    tabs.addTab("Grooves", colour(browserBg), groovePane.get(), false);
    tabs.addTab("Patterns", colour(browserBg), patternPane.get(), false);
    tabs.setCurrentTabIndex(juce::jlimit(0, 1, currentTab), false);
    repaint();
}

void GgdLibraryBrowser::setLoadedGroove(const juce::File& file)
{
    groovePane->setLoadedFile(file);
    patternPane->setLoadedFile({});
}

void GgdLibraryBrowser::setLoadedPattern(const juce::File& file)
{
    patternPane->setLoadedFile(file);
    groovePane->setLoadedFile({});
}

void GgdLibraryBrowser::clearLoaded()
{
    groovePane->setLoadedFile({});
    patternPane->setLoadedFile({});
}
