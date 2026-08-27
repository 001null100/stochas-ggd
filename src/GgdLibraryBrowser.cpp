#include "GgdLibraryBrowser.h"
#include "GgdPatternFile.h"

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

juce::Colour colour(juce::uint32 value) { return juce::Colour(value); }

bool matchesFile(const juce::File& file, bool patterns)
{
    if (!file.existsAsFile())
        return false;
    if (patterns)
        return file.hasFileExtension(GgdPatternFile::extension);
    return file.hasFileExtension("mid;midi");
}

class FileTreeItem final : public juce::TreeViewItem
{
public:
    using OpenCallback = std::function<void(const juce::File&)>;

    FileTreeItem(juce::File source, bool patternMode, OpenCallback callback)
        : file(std::move(source)), patterns(patternMode), onOpen(std::move(callback))
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
            addSubItem(new FileTreeItem(child, patterns, onOpen));
        for (const auto& child : files)
            addSubItem(new FileTreeItem(child, patterns, onOpen));
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const bool directory = file.isDirectory();
        g.setColour(directory ? colour(browserText).withAlpha(0.90f)
                              : colour(browserText));
        g.setFont(juce::Font(directory ? 11.5f : 11.0f,
                             directory ? juce::Font::bold : juce::Font::plain));
        g.drawText(file.getFileNameWithoutExtension(),
                   4, 0, juce::jmax(0, width - 6), height,
                   juce::Justification::centredLeft, true);
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

    int getItemHeight() const override { return file.isDirectory() ? 24 : 22; }

private:
    juce::File file;
    bool patterns = false;
    bool populated = false;
    OpenCallback onOpen;
};
}

class GgdLibraryBrowser::BrowserPane final : public juce::Component
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
        refreshButton.onClick = [this] { rebuildTree(); };
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

        rootLabel.setColour(juce::Label::textColourId, colour(browserMuted));
        rootLabel.setFont(10.0f);
        rootLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(rootLabel);

        emptyLabel.setColour(juce::Label::textColourId, colour(browserMuted));
        emptyLabel.setFont(11.0f);
        emptyLabel.setJustificationType(juce::Justification::centred);
        emptyLabel.setText(patterns
            ? "Choose a pattern folder to build your library."
            : "Choose your GGD MIDI groove folder.",
            juce::dontSendNotification);
        addAndMakeVisible(emptyLabel);

        tree.setRootItemVisible(false);
        tree.setDefaultOpenness(true);
        tree.setColour(juce::TreeView::backgroundColourId, colour(browserBg));
        tree.setColour(juce::TreeView::linesColourId, colour(browserBorder));
        tree.setColour(juce::TreeView::dragAndDropIndicatorColourId, colour(browserAccent));
        addAndMakeVisible(tree);

        rebuildTree();
    }

    ~BrowserPane() override
    {
        tree.setRootItem(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(colour(browserBg));
        g.setColour(colour(browserBorder));
        g.drawRect(getLocalBounds(), 1);
        g.setColour(colour(browserPanel));
        g.fillRect(0, 0, getWidth(), 69);
        g.setColour(colour(browserBorder));
        g.drawHorizontalLine(68, 0.0f, static_cast<float>(getWidth()));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(7);
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
        rootLabel.setBounds(bounds.removeFromTop(25));
        bounds.removeFromTop(5);
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

    juce::File getRoot() const { return root; }

    void rebuildTree()
    {
        tree.setRootItem(nullptr);
        treeRoot.reset();

        const bool valid = root.isDirectory();
        emptyLabel.setVisible(!valid);
        tree.setVisible(valid);
        rootLabel.setText(valid ? root.getFullPathName() : "No folder selected",
                          juce::dontSendNotification);
        rootLabel.setTooltip(valid ? root.getFullPathName() : juce::String());

        if (!valid)
            return;

        treeRoot = std::make_unique<FileTreeItem>(root, patterns, onOpen);
        tree.setRootItem(treeRoot.get());
        treeRoot->setOpen(true);
        tree.repaint();
    }

private:
    juce::PropertiesFile& properties;
    juce::String key;
    bool patterns = false;
    juce::File root;
    FileCallback onOpen;
    VoidCallback onSave;

    juce::TextButton chooseButton;
    juce::TextButton refreshButton;
    juce::TextButton saveButton;
    juce::Label rootLabel;
    juce::Label emptyLabel;
    juce::TreeView tree;
    std::unique_ptr<FileTreeItem> treeRoot;
    std::unique_ptr<juce::FileChooser> chooser;

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
                    self->rebuildTree();
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
    groovePane->rebuildTree();
    patternPane->rebuildTree();
}
