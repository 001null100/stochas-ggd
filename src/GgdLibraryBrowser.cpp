#include "GgdLibraryBrowser.h"
#include "GgdPatternFile.h"
#include "GgdSettingsPanel.h"
#include "GgdUiTheme.h"

#include <algorithm>
#include <vector>

namespace
{
constexpr int maxFilterResults = 500;

juce::Colour tc(GgdThemeRole role)
{
    return ggdThemeColour(role);
}

bool matchesFile(const juce::File& file, bool patterns)
{
    if (!file.existsAsFile())
        return false;
    if (patterns)
        return file.hasFileExtension(GgdPatternFile::extension);
    return file.hasFileExtension("mid;midi");
}

void collectLibraryFiles(const juce::File& directory, bool patterns, std::vector<juce::File>& results, int depth = 0)
{
    if (depth > 12)
        return;
    for (const auto& child : directory.findChildFiles(juce::File::findFilesAndDirectories, false, "*"))
    {
        if (child.isDirectory())
            collectLibraryFiles(child, patterns, results, depth + 1);
        else if (matchesFile(child, patterns))
            results.push_back(child);
    }
}

void paintRowBackground(juce::Graphics& g, int width, int height, bool selected, bool loaded)
{
    const auto area = juce::Rectangle<float>(2.0f, 1.0f, static_cast<float>(width - 4), static_cast<float>(height - 2));
    if (loaded)
    {
        g.setColour(tc(GgdThemeRole::accent).withAlpha(0.14f));
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(tc(GgdThemeRole::accent));
        g.fillRoundedRectangle(area.getX(), area.getY() + 4.0f, 3.0f, area.getHeight() - 8.0f, 1.5f);
    }
    else if (selected)
    {
        g.setColour(tc(GgdThemeRole::panelSoft));
        g.fillRoundedRectangle(area, 5.0f);
    }
}

// Draws the "LOADED" badge at the right edge and returns its width.
float paintLoadedBadge(juce::Graphics& g, int width, int height)
{
    const auto font = GgdFonts::bold(9.0f);
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, "LOADED");
    const juce::Rectangle<float> badge(static_cast<float>(width) - textWidth - 18.0f, height * 0.5f - 8.0f,
                                       textWidth + 12.0f, 16.0f);
    g.setColour(tc(GgdThemeRole::accent));
    g.fillRoundedRectangle(badge, 4.0f);
    g.setColour(tc(GgdThemeRole::accentText));
    g.setFont(font);
    g.drawText("LOADED", badge, juce::Justification::centred, false);
    return badge.getWidth() + 10.0f;
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

    bool mightContainSubItems() override { return file.isDirectory(); }

    void itemOpennessChanged(bool isNowOpen) override
    {
        if (!isNowOpen || populated || !file.isDirectory())
            return;

        populated = true;
        std::vector<juce::File> directories;
        std::vector<juce::File> files;
        for (const auto& child : file.findChildFiles(juce::File::findFilesAndDirectories, false, "*"))
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
        paintRowBackground(g, width, height, isSelected(), loaded);

        auto area = juce::Rectangle<float>(8.0f, 0.0f, static_cast<float>(width - 8), static_cast<float>(height));
        if (loaded)
            area.removeFromRight(paintLoadedBadge(g, width, height));

        const auto icon = area.removeFromLeft(15.0f).withSizeKeepingCentre(14.0f, 14.0f);
        ggdDrawIcon(g, directory ? GgdIcon::folder : GgdIcon::file, icon,
                    directory ? tc(GgdThemeRole::muted) : tc(GgdThemeRole::faint), 1.3f);
        area.removeFromLeft(7.0f);

        g.setColour(loaded || directory ? tc(GgdThemeRole::text) : tc(GgdThemeRole::text).withAlpha(0.9f));
        g.setFont(directory || loaded ? GgdFonts::medium(12.5f) : GgdFonts::regular(12.5f));
        g.drawText(directory ? file.getFileName() : file.getFileNameWithoutExtension(), area,
                   juce::Justification::centredLeft, true);
    }

    void itemClicked(const juce::MouseEvent&) override
    {
        if (file.isDirectory())
            setOpen(!isOpen());
        setSelected(true, true);
    }

    void itemDoubleClicked(const juce::MouseEvent&) override
    {
        if (!file.isDirectory() && onOpen)
            onOpen(file);
    }

    juce::String getTooltip() override
    {
        return file.isDirectory() ? juce::String() : file.getFileName() + "\nDouble-click to load into the current slot";
    }

    int getItemHeight() const override { return 26; }

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

    SearchResultItem(juce::File source, juce::String relative, OpenCallback callback, LoadedCallback loadedCallback)
        : file(std::move(source)), onOpen(std::move(callback)), isLoaded(std::move(loadedCallback))
    {
        relative = relative.replaceCharacter('\\', '/');
        folder = relative.containsChar('/') ? relative.upToLastOccurrenceOf("/", true, false) : juce::String();
        name = file.getFileNameWithoutExtension();
    }

    bool mightContainSubItems() override { return false; }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const bool loaded = isLoaded && isLoaded(file);
        paintRowBackground(g, width, height, isSelected(), loaded);

        auto area = juce::Rectangle<float>(10.0f, 0.0f, static_cast<float>(width - 10), static_cast<float>(height));
        if (loaded)
            area.removeFromRight(paintLoadedBadge(g, width, height));

        // "folder/sub/" muted, then the file name
        auto nameFont = loaded ? GgdFonts::medium(12.5f) : GgdFonts::regular(12.5f);
        auto folderFont = GgdFonts::regular(11.5f);
        const float nameWidth = juce::GlyphArrangement::getStringWidth(nameFont, name);
        const float folderWidth = juce::jmin(juce::GlyphArrangement::getStringWidth(folderFont, folder),
                                             juce::jmax(0.0f, area.getWidth() - nameWidth - 4.0f));
        if (folderWidth > 12.0f)
        {
            g.setColour(tc(GgdThemeRole::faint));
            g.setFont(folderFont);
            g.drawText(folder, area.removeFromLeft(folderWidth), juce::Justification::centredLeft, true);
        }
        g.setColour(tc(GgdThemeRole::text));
        g.setFont(nameFont);
        g.drawText(name, area, juce::Justification::centredLeft, true);
    }

    void itemClicked(const juce::MouseEvent&) override { setSelected(true, true); }

    void itemDoubleClicked(const juce::MouseEvent&) override
    {
        if (onOpen)
            onOpen(file);
    }

    juce::String getTooltip() override { return folder + file.getFileName() + "\nDouble-click to load"; }

    int getItemHeight() const override { return 26; }

private:
    juce::File file;
    juce::String folder;
    juce::String name;
    OpenCallback onOpen;
    LoadedCallback isLoaded;
};
}

//==============================================================================
class GgdLibraryBrowser::TabButton final : public juce::Button
{
public:
    explicit TabButton(const juce::String& name) : juce::Button(name)
    {
        setMouseClickGrabsKeyboardFocus(false);
        setWantsKeyboardFocus(false);
        setClickingTogglesState(false);
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool) override
    {
        auto area = getLocalBounds().toFloat().reduced(2.0f, 3.0f);
        const bool on = getToggleState();
        if (on)
        {
            g.setColour(tc(GgdThemeRole::panelSoft));
            g.fillRoundedRectangle(area, 5.0f);
            g.setColour(tc(GgdThemeRole::border));
            g.drawRoundedRectangle(area.reduced(0.5f), 5.0f, 1.0f);
        }
        else if (highlighted)
        {
            g.setColour(tc(GgdThemeRole::panelRaised));
            g.fillRoundedRectangle(area, 5.0f);
        }
        g.setColour(on ? tc(GgdThemeRole::text) : tc(GgdThemeRole::muted));
        g.setFont(on ? GgdFonts::semibold(12.5f) : GgdFonts::medium(12.5f));
        g.drawText(getButtonText(), area, juce::Justification::centred, false);
    }
};

//==============================================================================
struct GgdLibraryBrowser::SharedSettings
{
    SharedSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Stochas GGD";
        options.filenameSuffix = "settings";
        options.folderName = ggdSettingsFolderName();
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        options.millisecondsBeforeSaving = 500;
        file = std::make_unique<juce::PropertiesFile>(options);
    }

    ~SharedSettings() { file->saveIfNeeded(); }

    std::unique_ptr<juce::PropertiesFile> file;
};

//==============================================================================
class GgdLibraryBrowser::BrowserPane final : public juce::Component,
                                             private juce::Timer,
                                             private juce::ChangeListener
{
public:
    BrowserPane(juce::PropertiesFile& propertyStore, juce::String propertyKey, bool patternMode)
        : properties(propertyStore), key(std::move(propertyKey)), patterns(patternMode)
    {
        root = juce::File(properties.getValue(key));
        properties.addChangeListener(this);

        folderButton.setTooltip(patterns ? "Choose the folder that holds your Stochas GGD patterns"
                                         : "Choose the folder that holds your GGD MIDI grooves");
        folderButton.onClick = [this] { chooseRoot(); };
        addAndMakeVisible(folderButton);

        refreshButton.setStyle(GgdButton::Style::quiet);
        refreshButton.setTooltip("Rescan the folder");
        refreshButton.onClick = [this] { refresh(); };
        addAndMakeVisible(refreshButton);

        if (patterns)
        {
            saveButton.setTooltip("Save the current pattern into this library");
            saveButton.onClick = [this]
            {
                if (onSave)
                    onSave();
            };
            addAndMakeVisible(saveButton);
        }

        emptyButton.setStyle(GgdButton::Style::primary);
        emptyButton.onClick = [this] { chooseRoot(); };
        addChildComponent(emptyButton);

        filterEditor.setTooltip("Filter by file or folder name");
        filterEditor.setFont(GgdFonts::regular(12.5f));
        filterEditor.setIndents(28, 0);
        filterEditor.setJustification(juce::Justification::centredLeft);
        filterEditor.onTextChange = [this]
        {
            stopTimer();
            startTimer(120);
        };
        filterEditor.onEscapeKey = [this] { filterEditor.clear(); rebuildTree(); };
        addAndMakeVisible(filterEditor);

        tree.setRootItemVisible(false);
        tree.setDefaultOpenness(false);
        tree.setIndentSize(16);
        tree.setWantsKeyboardFocus(false);
        addAndMakeVisible(tree);

        applyTheme();
        refresh();
    }

    ~BrowserPane() override
    {
        properties.removeChangeListener(this);
        stopTimer();
        tree.setRootItem(nullptr);
    }

    void applyTheme()
    {
        filterEditor.setTextToShowWhenEmpty(patterns ? "Filter patterns" : "Filter grooves",
                                            tc(GgdThemeRole::faint));
        filterEditor.setColour(juce::TextEditor::backgroundColourId, tc(GgdThemeRole::outside));
        filterEditor.setColour(juce::TextEditor::outlineColourId, tc(GgdThemeRole::border));
        filterEditor.setColour(juce::TextEditor::focusedOutlineColourId, tc(GgdThemeRole::accent));
        filterEditor.setColour(juce::TextEditor::textColourId, tc(GgdThemeRole::text));
        filterEditor.applyColourToAllText(tc(GgdThemeRole::text));
        tree.setColour(juce::TreeView::backgroundColourId, tc(GgdThemeRole::panel));
        tree.setColour(juce::TreeView::linesColourId, juce::Colours::transparentBlack);
        tree.setColour(juce::TreeView::selectedItemBackgroundColourId, juce::Colours::transparentBlack);
        tree.setColour(juce::TreeView::dragAndDropIndicatorColourId, tc(GgdThemeRole::accent));
        repaint();
        tree.repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(tc(GgdThemeRole::panel));

        // folder caption
        g.setColour(tc(GgdThemeRole::muted));
        g.setFont(GgdFonts::regular(11.5f));
        g.drawText(caption, captionArea, juce::Justification::centredLeft, true);

        if (emptyArea.isEmpty())
            return;

        auto area = emptyArea;
        auto iconArea = area.removeFromTop(44).toFloat();
        ggdDrawIcon(g, patterns ? GgdIcon::file : GgdIcon::folder, iconArea.withSizeKeepingCentre(30.0f, 30.0f),
                    tc(GgdThemeRole::faint), 1.2f);
        g.setColour(tc(GgdThemeRole::text));
        g.setFont(GgdFonts::semibold(13.0f));
        g.drawText(emptyTitle, area.removeFromTop(22), juce::Justification::centred, true);
        g.setColour(tc(GgdThemeRole::muted));
        g.setFont(GgdFonts::regular(12.0f));
        g.drawFittedText(emptyMessage, area.removeFromTop(40), juce::Justification::centredTop, 3);
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        ggdDrawIcon(g, GgdIcon::search,
                    filterEditor.getBounds().toFloat().removeFromLeft(28.0f).withSizeKeepingCentre(14.0f, 14.0f),
                    tc(GgdThemeRole::faint), 1.4f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(12, 8);
        auto buttons = bounds.removeFromTop(30);
        folderButton.setBounds(buttons.removeFromLeft(patterns ? 92 : 120));
        buttons.removeFromLeft(4);
        refreshButton.setBounds(buttons.removeFromLeft(30));
        if (patterns)
            saveButton.setBounds(buttons.removeFromRight(juce::jmin(112, buttons.getWidth() - 4)));

        bounds.removeFromTop(6);
        captionArea = bounds.removeFromTop(18);
        bounds.removeFromTop(4);
        filterEditor.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(8);
        tree.setBounds(bounds);

        if (showingEmptyState)
        {
            auto empty = bounds.withSizeKeepingCentre(bounds.getWidth(), 180).withY(bounds.getY() + 40);
            emptyButton.setBounds(empty.removeFromBottom(32).withSizeKeepingCentre(150, 32));
            emptyArea = empty;
        }
        else
        {
            emptyArea = {};
        }
    }

    void setOpenCallback(FileCallback callback)
    {
        onOpen = std::move(callback);
        rebuildTree();
    }

    void setSaveCallback(VoidCallback callback) { onSave = std::move(callback); }

    void setLoadedFile(const juce::File& file)
    {
        if (loadedFile != file)
        {
            loadedFile = file;
            tree.repaint();
        }
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

        if (!root.isDirectory())
        {
            setEmptyState(true, patterns ? "No pattern folder yet" : "No groove folder yet",
                          patterns ? "Choose a folder to keep your saved patterns in. Save Pattern stores the "
                                     "current pattern there."
                                   : "Point Stochas GGD at the MIDI folder of your GetGood Drums library.",
                          "Choose folder...");
            caption = "No folder chosen";
            return;
        }

        auto loaded = [this](const juce::File& candidate)
        {
            return loadedFile != juce::File() && candidate == loadedFile;
        };

        const auto filter = filterEditor.getText().trim();
        if (filter.isEmpty())
        {
            if (indexedFiles.empty())
                setEmptyState(true, patterns ? "No patterns here yet" : "No MIDI files found",
                              patterns ? "Saved patterns (.sggdp) appear here."
                                       : "This folder has no .mid files. Choose another folder?",
                              "Choose folder...");
            else
                setEmptyState(false);
            caption = root.getFileName() + "  ·  " + juce::String(static_cast<int>(indexedFiles.size()))
                    + (indexedFiles.size() == 1 ? " file" : " files");
            treeRoot = std::make_unique<FileTreeItem>(root, patterns, onOpen, loaded);
            tree.setRootItem(treeRoot.get());
            treeRoot->setOpen(true);
            repaint();
            return;
        }

        std::vector<juce::File> results;
        for (const auto& file : indexedFiles)
        {
            if (static_cast<int>(results.size()) >= maxFilterResults)
                break;
            if (file.getRelativePathFrom(root).containsIgnoreCase(filter))
                results.push_back(file);
        }

        caption = juce::String(static_cast<int>(results.size()))
                + (results.size() == 1 ? " match" : " matches")
                + (results.size() >= static_cast<size_t>(maxFilterResults) ? " (first 500)" : "");

        if (results.empty())
        {
            setEmptyState(true, "No matches", "Nothing in " + root.getFileName() + " matches \"" + filter + "\".",
                          {});
            return;
        }

        setEmptyState(false);
        auto flatRoot = std::make_unique<FlatRootItem>();
        for (const auto& file : results)
            flatRoot->addSubItem(new SearchResultItem(file, file.getRelativePathFrom(root), onOpen, loaded));
        treeRoot = std::move(flatRoot);
        tree.setRootItem(treeRoot.get());
        treeRoot->setOpen(true);
        repaint();
    }

private:
    juce::PropertiesFile& properties;
    juce::String key;
    bool patterns = false;
    juce::File root;
    juce::File loadedFile;
    FileCallback onOpen;
    VoidCallback onSave;

    GgdButton folderButton { "Folder", GgdIcon::folder };
    GgdButton refreshButton { {}, GgdIcon::refresh };
    GgdButton saveButton { "Save pattern", GgdIcon::save };
    GgdButton emptyButton { "Choose folder..." };
    juce::TextEditor filterEditor;
    juce::TreeView tree;
    std::unique_ptr<juce::TreeViewItem> treeRoot;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::File> indexedFiles;

    juce::String caption;
    juce::Rectangle<int> captionArea;
    bool showingEmptyState = false;
    juce::Rectangle<int> emptyArea;
    juce::String emptyTitle;
    juce::String emptyMessage;

    void setEmptyState(bool empty, const juce::String& title = {}, const juce::String& message = {},
                       const juce::String& buttonText = {})
    {
        showingEmptyState = empty;
        emptyTitle = title;
        emptyMessage = message;
        tree.setVisible(!empty);
        emptyButton.setVisible(empty && buttonText.isNotEmpty());
        if (buttonText.isNotEmpty())
            emptyButton.setButtonText(buttonText);
        resized();
        repaint();
    }

    void timerCallback() override
    {
        stopTimer();
        rebuildTree();
    }

    // Another window chose a different folder.
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        const juce::File chosen(properties.getValue(key));
        if (chosen != root)
        {
            root = chosen;
            refresh();
        }
    }

    void rebuildIndex()
    {
        indexedFiles.clear();
        if (!root.isDirectory())
            return;

        collectLibraryFiles(root, patterns, indexedFiles);
        std::sort(indexedFiles.begin(), indexedFiles.end(), [this](const juce::File& a, const juce::File& b)
        {
            return a.getRelativePathFrom(root).compareNatural(b.getRelativePathFrom(root), true) < 0;
        });
    }

    void chooseRoot()
    {
        chooser = std::make_unique<juce::FileChooser>(
            patterns ? "Choose the pattern library folder" : "Choose the groove library folder",
            root.isDirectory() ? root : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));

        auto safe = juce::Component::SafePointer<BrowserPane>(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
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

//==============================================================================
GgdLibraryBrowser::GgdLibraryBrowser()
{
    groovePane = std::make_unique<BrowserPane>(*settings->file, "grooveRoot", false);
    patternPane = std::make_unique<BrowserPane>(*settings->file, "patternRoot", true);
    addChildComponent(*groovePane);
    addChildComponent(*patternPane);

    grooveTab = std::make_unique<TabButton>("Grooves");
    patternTab = std::make_unique<TabButton>("Patterns");
    grooveTab->setTooltip("GGD MIDI grooves. Double-click one to load it into the current slot.");
    patternTab->setTooltip("Your saved Stochas GGD patterns");
    grooveTab->onClick = [this] { showTab(0); };
    patternTab->onClick = [this] { showTab(1); };
    addAndMakeVisible(*grooveTab);
    addAndMakeVisible(*patternTab);

    showTab(juce::jlimit(0, 1, settings->file->getIntValue("libraryTab", 0)));
}

GgdLibraryBrowser::~GgdLibraryBrowser() = default;

void GgdLibraryBrowser::showTab(int index)
{
    currentTab = juce::jlimit(0, 1, index);
    grooveTab->setToggleState(currentTab == 0, juce::dontSendNotification);
    patternTab->setToggleState(currentTab == 1, juce::dontSendNotification);
    groovePane->setVisible(currentTab == 0);
    patternPane->setVisible(currentTab == 1);
    auto& file = *settings->file;
    if (file.getIntValue("libraryTab", 0) != currentTab)
        file.setValue("libraryTab", currentTab);
}

void GgdLibraryBrowser::paint(juce::Graphics& g)
{
    g.fillAll(tc(GgdThemeRole::panel));
    auto header = getLocalBounds().removeFromTop(48);
    auto title = header.reduced(14, 0);
    g.setColour(tc(GgdThemeRole::muted));
    g.setFont(GgdFonts::bold(10.5f));
    g.drawText("LIBRARY", title.removeFromLeft(70), juce::Justification::centredLeft, false);
    g.setColour(tc(GgdThemeRole::borderSoft));
    g.fillRect(0, header.getBottom() - 1, getWidth(), 1);
}

void GgdLibraryBrowser::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(48).reduced(12, 9);
    header.removeFromLeft(62);
    auto tabs = header.withSizeKeepingCentre(juce::jmin(header.getWidth(), 200), header.getHeight())
                    .withRightX(header.getRight());
    grooveTab->setBounds(tabs.removeFromLeft(tabs.getWidth() / 2));
    patternTab->setBounds(tabs);
    groovePane->setBounds(bounds);
    patternPane->setBounds(bounds);
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
    groovePane->applyTheme();
    patternPane->applyTheme();
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
