from pathlib import Path
import re

src_path = Path('src/GgdDrumEditorAlpha7.cpp')
out_path = Path('src/GgdDrumEditorAlpha9.cpp')
text = src_path.read_text(encoding='utf-8')


def replace_once(old, new, label):
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected 1 match, found {count}')
    text = text.replace(old, new, 1)


def regex_once(pattern, replacement, label):
    global text
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f'{label}: expected 1 regex match, found {count}')


replace_once(
    '#include "GgdMidiImporter.h"\n',
    '#include "GgdMidiImporter.h"\n#include "GgdPatternFile.h"\n#include "GgdLibraryBrowser.h"\n',
    'includes')

replace_once(
'''    GgdDrumGrid(SeqAudioProcessor& p,
                const juce::Array<GgdCanonicalRow>& canonical,
                std::function<void()> undoFn,
                std::function<void(float)> zoomFn,
                std::function<void(ToolMode)> toolFn)
        : processor(p),
          canonicalRows(canonical),
          undoCallback(std::move(undoFn)),
          zoomCallback(std::move(zoomFn)),
          toolCallback(std::move(toolFn))''',
'''    GgdDrumGrid(SeqAudioProcessor& p,
                const juce::Array<GgdCanonicalRow>& canonical,
                std::function<void()> undoFn,
                std::function<void()> redoFn,
                std::function<void()> publishFn,
                std::function<void(float)> zoomFn,
                std::function<void(ToolMode)> toolFn)
        : processor(p),
          canonicalRows(canonical),
          undoCallback(std::move(undoFn)),
          redoCallback(std::move(redoFn)),
          publishCallback(std::move(publishFn)),
          zoomCallback(std::move(zoomFn)),
          toolCallback(std::move(toolFn))''',
    'grid constructor')

replace_once(
'''        if ((mods.isCtrlDown() || mods.isCommandDown()) && !mods.isAltDown()
            && !mods.isShiftDown() && keyMatchesLetter(key, 'A'))
        {
            if (toolMode == ToolMode::select)
            {
                selectAllHits();
                return true;
            }
        }

        // Plain editor keys''',
'''        if ((mods.isCtrlDown() || mods.isCommandDown()) && !mods.isAltDown()
            && !mods.isShiftDown() && keyMatchesLetter(key, 'A'))
        {
            if (toolMode == ToolMode::select)
            {
                selectAllHits();
                return true;
            }
        }

        if ((mods.isCtrlDown() || mods.isCommandDown()) && !mods.isAltDown()
            && !mods.isShiftDown() && toolMode == ToolMode::select)
        {
            if (keyMatchesLetter(key, 'C'))
            {
                copySelectionToClipboard();
                return true;
            }
            if (keyMatchesLetter(key, 'V'))
            {
                pasteClipboard();
                return true;
            }
        }

        // Plain editor keys''',
    'copy paste keys')

replace_once(
"""                    || keyMatchesLetter(key, 'U')
                    || key.getKeyCode() == juce::KeyPress::deleteKey""",
"""                    || keyMatchesLetter(key, 'U') || keyMatchesLetter(key, 'Y')
                    || key.getKeyCode() == juce::KeyPress::deleteKey""",
    'consume redo key')

replace_once(
'''        const bool uPressed = edge(letterDown('U'), fallbackUDown);
        const bool leftPressed''',
'''        const bool uPressed = edge(letterDown('U'), fallbackUDown);
        const bool yPressed = edge(letterDown('Y'), fallbackYDown);
        const bool leftPressed''',
    'poll redo')

replace_once(
'''            else if (uPressed && undoCallback)
                undoCallback();
        }
''',
'''            else if (uPressed && undoCallback)
                undoCallback();
            else if (yPressed && redoCallback)
                redoCallback();
        }
''',
    'dispatch redo')

replace_once(
'''    struct CellState
    {
        CellRef ref;
        int prob = SEQ_PROB_OFF;
        int velocity = 0;
        int length = 0;
        int offset = 0;
    };

    enum class DragMode''',
'''    struct CellState
    {
        CellRef ref;
        int prob = SEQ_PROB_OFF;
        int velocity = 0;
        int length = 0;
        int offset = 0;
    };

public:
    int getSelectionCount() const { return static_cast<int>(selection.size()); }
    bool hasClipboard() const { return !clipboard.empty(); }

    bool copySelectionToClipboard()
    {
        if (selection.empty())
            return false;

        int minStep = selection.front().step;
        int maxStep = selection.front().step;
        for (const auto& ref : selection)
        {
            minStep = juce::jmin(minStep, ref.step);
            maxStep = juce::jmax(maxStep, ref.step);
        }

        clipboard.clear();
        clipboard.reserve(selection.size());
        for (const auto& ref : selection)
        {
            auto state = snapshotCell(ref.row, ref.step);
            state.ref.step -= minStep;
            clipboard.push_back(std::move(state));
        }
        clipboardSpanSteps = juce::jmax(1, maxStep - minStep + 1);
        return true;
    }

    bool pasteClipboard()
    {
        if (clipboard.empty())
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();
        int startStep = 0;
        if (!selection.empty())
        {
            int maxSelected = selection.front().step;
            for (const auto& ref : selection)
                maxSelected = juce::jmax(maxSelected, ref.step);
            startStep = maxSelected + 1;
        }

        auto fitsAt = [&](int anchor)
        {
            for (const auto& state : clipboard)
            {
                const int step = anchor + state.ref.step;
                if (step < 0 || step >= numSteps || cellOccupied(state.ref.row, step))
                    return false;
            }
            return true;
        };

        while (startStep < numSteps && !fitsAt(startStep))
            startStep += clipboardSpanSteps;
        if (!fitsAt(startStep))
            return false;

        selection.clear();
        for (const auto& state : clipboard)
        {
            const int step = startStep + state.ref.step;
            writeCellState(state, state.ref.row, step);
            selection.push_back({ state.ref.row, step });
        }
        publishChange();
        repaint();
        return true;
    }

    void adjustSelectedVelocityBy(int delta) { adjustSelectionVelocity(delta); }

    void adjustSelectedTimingBy(int delta)
    {
        if (selection.empty() || delta == 0)
            return;
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        bool changed = false;
        for (const auto& ref : selection)
        {
            const int storageRow = storageRowForCanonical(ref.row, canonicalRows.size());
            const int oldOffset = layer->getOffset(storageRow, ref.step, pattern);
            const int newOffset = juce::jlimit(-50, 50, oldOffset + delta);
            if (newOffset != oldOffset)
            {
                layer->setOffset(storageRow, ref.step, static_cast<int8_t>(newOffset), pattern);
                changed = true;
            }
        }
        if (changed)
        {
            publishChange();
            repaint();
        }
    }

    void humanizeSelected()
    {
        if (selection.empty())
            return;
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        auto& random = juce::Random::getSystemRandom();
        for (const auto& ref : selection)
        {
            const int storageRow = storageRowForCanonical(ref.row, canonicalRows.size());
            const int velocity = juce::jlimit(1, 127,
                static_cast<int>(layer->getVel(storageRow, ref.step, pattern))
                + random.nextInt(13) - 6);
            const int offset = juce::jlimit(-50, 50,
                static_cast<int>(layer->getOffset(storageRow, ref.step, pattern))
                + random.nextInt(9) - 4);
            layer->setVel(storageRow, ref.step, static_cast<int8_t>(velocity), pattern);
            layer->setOffset(storageRow, ref.step, static_cast<int8_t>(offset), pattern);
        }
        publishChange();
        repaint();
    }

    void deleteSelected() { deleteSelection(); }

private:
    enum class DragMode''',
    'selection public API')

replace_once(
'''        juce::String groupLabel;
        juce::String label;''',
'''        juce::String groupId;
        juce::String groupLabel;
        juce::String label;''',
    'layout group id')

replace_once(
'''    std::function<void()> undoCallback;
    std::function<void(float)> zoomCallback;''',
'''    std::function<void()> undoCallback;
    std::function<void()> redoCallback;
    std::function<void()> publishCallback;
    std::function<void(float)> zoomCallback;''',
    'grid callbacks')

replace_once(
'''    std::vector<CellRef> selection;
    std::vector<CellRef> marqueeBaseSelection;
''',
'''    std::vector<CellRef> selection;
    std::vector<CellRef> marqueeBaseSelection;
    std::vector<CellState> clipboard;
    int clipboardSpanSteps = 1;
    std::set<juce::String> collapsedGroups;
''',
    'clipboard members')

replace_once(
'''    bool fallbackUDown = false;
    bool fallbackLeftDown = false;''',
'''    bool fallbackUDown = false;
    bool fallbackYDown = false;
    bool fallbackLeftDown = false;''',
    'redo fallback member')

replace_once(
'''        duplicateDrag = false;

        if (toolMode == ToolMode::select)''',
'''        duplicateDrag = false;

        if (e.position.x < static_cast<float>(currentViewX() + nameWidth))
        {
            for (const auto& item : layout)
            {
                if (!item.header || e.position.y < item.y || e.position.y >= item.y + item.height)
                    continue;
                if (collapsedGroups.count(item.groupId) != 0)
                    collapsedGroups.erase(item.groupId);
                else
                    collapsedGroups.insert(item.groupId);
                rebuildLayout();
                repaint();
                return;
            }
        }

        if (toolMode == ToolMode::select)''',
    'group collapse click')

replace_once(
'''                g.setColour(c(text).withAlpha(0.82f));
                g.setFont(juce::Font(10.5f, juce::Font::bold));
                g.drawText(item.groupLabel.toUpperCase(), stickyX + 17, item.y,
                           nameWidth - 30, item.height,
                           juce::Justification::centredLeft, false);''',
'''                g.setColour(c(text).withAlpha(0.82f));
                g.setFont(juce::Font(10.5f, juce::Font::bold));
                const bool collapsed = collapsedGroups.count(item.groupId) != 0;
                const auto groupCaption = juce::String(collapsed ? "[+] " : "[-] ")
                                        + item.groupLabel.toUpperCase();
                g.drawText(groupCaption, stickyX + 17, item.y,
                           nameWidth - 30, item.height,
                           juce::Justification::centredLeft, false);''',
    'group caption')

new_rebuild = r'''    void rebuildLayout()
    {
        layout.clear();
        int y = rulerHeight;

        if (map != nullptr)
        {
            struct Family
            {
                const char* id;
                const char* label;
                const char* prefix;
            };
            static constexpr Family families[] = {
                { "kick", "Kick", "kick." },
                { "snare", "Snare", "snare." },
                { "toms", "Toms", "tom." },
                { "hats", "Hi-Hat", "hihat." },
                { "ride", "Ride", "ride." },
                { "crashes", "Crashes", "crash." },
                { "china", "China", "china." },
                { "splashes", "Splashes", "splash." },
                { "other", "Other", "" }
            };

            std::vector<const GgdArticulation*> articulations;
            for (const auto& group : map->groups)
                for (const auto& articulation : group.articulations)
                    articulations.push_back(&articulation);

            std::set<juce::String> placed;
            for (const auto& family : families)
            {
                std::vector<const GgdArticulation*> familyItems;
                for (const auto* articulation : articulations)
                {
                    const bool ordinaryFamily = juce::String(family.id) != "other";
                    if ((ordinaryFamily && articulation->semanticId.startsWith(family.prefix))
                        || (!ordinaryFamily && placed.count(articulation->semanticId) == 0))
                    {
                        if (placed.insert(articulation->semanticId).second)
                            familyItems.push_back(articulation);
                    }
                }

                if (familyItems.empty())
                    continue;

                LayoutItem headerItem;
                headerItem.header = true;
                headerItem.y = y;
                headerItem.height = groupHeight;
                headerItem.groupId = family.id;
                headerItem.groupLabel = family.label;
                layout.push_back(headerItem);
                y += groupHeight;

                if (collapsedGroups.count(headerItem.groupId) != 0)
                    continue;

                for (const auto* articulation : familyItems)
                {
                    const int canonical = GgdKitMapLibrary::findCanonicalRow(
                        canonicalRows, articulation->semanticId);
                    if (canonical < 0)
                        continue;

                    LayoutItem row;
                    row.y = y;
                    row.height = rowHeight;
                    row.canonicalRow = canonical;
                    row.groupId = family.id;
                    row.groupLabel = family.label;
                    row.label = articulation->label;
                    if (const auto* binding = articulation->primaryNoteBinding())
                        row.noteName = binding->noteName;
                    else
                        row.noteName = "-";

                    layout.push_back(row);
                    y += rowHeight;
                }
            }
        }

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int patternRight = static_cast<int>(
            std::ceil(xForStorageStepF(static_cast<float>(layer->getNumSteps()))));
        setSize(juce::jmax(viewportWidth(), patternRight), juce::jmax(320, y + 12));
    }

'''
regex_once(r'    void rebuildLayout\(\)\n    \{.*?\n    \}\n\n    bool rowAtY', new_rebuild + '    bool rowAtY', 'rebuild layout')

replace_once(
'''    void publishChange()
    {
        processor.mData.swap();
        processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    }''',
'''    void publishChange()
    {
        if (publishCallback)
            publishCallback();
        else
        {
            processor.mData.swap();
            processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
        }
    }''',
    'grid publish callback')

replace_once(
'''        canonicalRows,
        [this] { performUndo(); },
        [this](float scale) { refreshZoomControls(scale); },''',
'''        canonicalRows,
        [this] { performUndo(); },
        [this] { performRedo(); },
        [this] { publishModelChange(); },
        [this](float scale) { refreshZoomControls(scale); },''',
    'grid constructor call')

regex_once(
    r'''        \[this\]\(GgdDrumGrid::ToolMode mode\)\n        \{\n            const bool draw = mode == GgdDrumGrid::ToolMode::draw;.*?\n        \}\);''',
'''        [this](GgdDrumGrid::ToolMode mode)
        {
            const bool draw = mode == GgdDrumGrid::ToolMode::draw;
            drawModeButton.setToggleState(draw, juce::dontSendNotification);
            selectModeButton.setToggleState(!draw, juce::dontSendNotification);
            updateContextStrip();
        });''',
    'tool callback')

replace_once(
'''    addAndMakeVisible(gridViewport);

    productLabel.setText''',
'''    addAndMakeVisible(gridViewport);

    libraryBrowser = std::make_unique<GgdLibraryBrowser>();
    libraryBrowser->setGrooveOpenCallback(
        [this](const juce::File& file) { requestLoadGroove(file); });
    libraryBrowser->setPatternOpenCallback(
        [this](const juce::File& file) { requestLoadPattern(file); });
    libraryBrowser->setSavePatternCallback(
        [this] { saveCurrentPatternToLibrary(); });
    addAndMakeVisible(*libraryBrowser);

    productLabel.setText''',
    'library browser construction')

replace_once(
'''        refreshControlsFromModel();
        grid->repaint();
    };''',
'''        refreshControlsFromModel();
        resetHistoryForCurrentPattern(false);
        grid->repaint();
    };''',
    'pattern slot history reset')

replace_once(
'''    undoButton.setTooltip("Undo the most recent sequence edit. U is the plugin-local shortcut; Bitwig may intercept Ctrl+Z.");''',
'''    patternActionsButton.setTooltip("Pattern slot operations");
    patternActionsButton.setMouseClickGrabsKeyboardFocus(false);
    patternActionsButton.onClick = [this] { showPatternActions(); };
    addAndMakeVisible(patternActionsButton);

    undoButton.setTooltip("Undo through the editor history. U is the plugin-local shortcut; Bitwig may intercept Ctrl+Z.");''',
    'pattern actions button')

replace_once(
'''    addAndMakeVisible(undoButton);

    clearButton.setTooltip''',
'''    addAndMakeVisible(undoButton);

    redoButton.setTooltip("Redo the most recently undone edit. Y is the plugin-local shortcut.");
    redoButton.setMouseClickGrabsKeyboardFocus(false);
    redoButton.onClick = [this]
    {
        performRedo();
        if (grid != nullptr)
            grid->grabKeyboardFocus();
    };
    addAndMakeVisible(redoButton);

    clearButton.setTooltip''',
    'redo button')

replace_once(
'''    hintLabel.setText(
        "Draw: click/drag | Erase: right-drag | Velocity: Shift-drag or Alt+wheel | Timing: Alt-drag | D/S tools | U undo",
        juce::dontSendNotification);''',
'''    selectionStatusLabel.setFont(juce::Font(10.5f, juce::Font::bold));
    selectionStatusLabel.setColour(juce::Label::textColourId, c(text));
    addAndMakeVisible(selectionStatusLabel);

    auto prepareContextButton = [this](juce::TextButton& button)
    {
        button.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(button);
    };
    prepareContextButton(copyButton);
    prepareContextButton(pasteButton);
    prepareContextButton(velocityDownButton);
    prepareContextButton(velocityUpButton);
    prepareContextButton(timingEarlierButton);
    prepareContextButton(timingLaterButton);
    prepareContextButton(humanizeButton);
    prepareContextButton(deleteSelectionButton);

    copyButton.onClick = [this] { if (grid) grid->copySelectionToClipboard(); };
    pasteButton.onClick = [this] { if (grid) grid->pasteClipboard(); };
    velocityDownButton.onClick = [this] { if (grid) grid->adjustSelectedVelocityBy(-5); };
    velocityUpButton.onClick = [this] { if (grid) grid->adjustSelectedVelocityBy(5); };
    timingEarlierButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(-5); };
    timingLaterButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(5); };
    humanizeButton.onClick = [this] { if (grid) grid->humanizeSelected(); };
    deleteSelectionButton.onClick = [this] { if (grid) grid->deleteSelected(); };

    hintLabel.setText(
        "Draw: click/drag | right-drag erase | Shift-drag velocity | Alt-drag timing",
        juce::dontSendNotification);''',
    'context controls')

replace_once(
'''    if (!maps.isEmpty())
        grid->setMap(&maps.getReference(activeMapIndex));

    setResizable(true, true);
    setResizeLimits(900, 560, 1900, 1400);
    setSize(1160, 780);''',
'''    if (!maps.isEmpty())
        grid->setMap(&maps.getReference(activeMapIndex));

    initialiseCleanPatternFingerprints();
    resetHistoryForCurrentPattern(false);
    updateContextStrip();

    setResizable(true, true);
    setResizeLimits(1080, 600, 2200, 1500);
    setSize(1420, 820);''',
    'initial history and size')

new_resized = r'''void GgdDrumEditor::resized()
{
    const int pad = 12;
    const int gap = 7;
    const int editorWidth = juce::jmax(680, getWidth() - browserWidth);

    auto first = juce::Rectangle<int>(pad, 7, editorWidth - pad * 2, 43);
    productLabel.setBounds(first.removeFromLeft(142));
    first.removeFromLeft(3);
    transportStatus.setBounds(first.removeFromLeft(72).reduced(0, 8));
    first.removeFromLeft(gap + 2);

    const int kitWidth = juce::jlimit(166, 220, editorWidth / 5);
    kitSelector.setBounds(first.removeFromLeft(kitWidth).reduced(0, 5));
    first.removeFromLeft(gap);
    patternSelector.setBounds(first.removeFromLeft(142).reduced(0, 5));
    first.removeFromLeft(gap);
    patternActionsButton.setBounds(first.removeFromLeft(76).reduced(0, 5));
    first.removeFromLeft(gap);
    patternName.setBounds(first.reduced(0, 5));

    auto second = juce::Rectangle<int>(pad, 60, editorWidth - pad * 2, 43);

    auto actions = second.removeFromRight(181);
    clearButton.setBounds(actions.removeFromRight(56).reduced(0, 5));
    actions.removeFromRight(5);
    redoButton.setBounds(actions.removeFromRight(57).reduced(0, 5));
    actions.removeFromRight(5);
    undoButton.setBounds(actions.removeFromRight(58).reduced(0, 5));
    second.removeFromRight(10);

    drawModeButton.setBounds(second.removeFromLeft(55).reduced(0, 5));
    second.removeFromLeft(4);
    selectModeButton.setBounds(second.removeFromLeft(62).reduced(0, 5));
    second.removeFromLeft(10);

    meterLabel.setBounds(second.removeFromLeft(43));
    numeratorSelector.setBounds(second.removeFromLeft(48).reduced(0, 5));
    meterSlash.setBounds(second.removeFromLeft(15));
    denominatorSelector.setBounds(second.removeFromLeft(55).reduced(0, 5));
    second.removeFromLeft(9);

    barsLabel.setBounds(second.removeFromLeft(45));
    barsEditor.setBounds(second.removeFromLeft(58).reduced(0, 5));
    second.removeFromLeft(8);

    zoomLabel.setBounds(second.removeFromLeft(36));
    fitZoomButton.setBounds(second.removeFromRight(54).reduced(0, 5));
    second.removeFromRight(4);
    zoomValueLabel.setBounds(second.removeFromRight(82));
    second.removeFromRight(5);
    zoomSlider.setBounds(second.reduced(0, 7));

    const int contentHeight = getHeight() - topAreaHeight;
    gridViewport.setBounds(0, topAreaHeight, editorWidth,
                           contentHeight - bottomAreaHeight);
    if (libraryBrowser)
        libraryBrowser->setBounds(editorWidth, topAreaHeight,
                                  getWidth() - editorWidth, contentHeight);

    auto strip = juce::Rectangle<int>(8, getHeight() - bottomAreaHeight + 2,
                                      editorWidth - 16, bottomAreaHeight - 4);
    selectionStatusLabel.setBounds(strip.removeFromLeft(110));
    strip.removeFromLeft(5);

    auto place = [&](juce::TextButton& button, int width)
    {
        button.setBounds(strip.removeFromLeft(width).reduced(0, 4));
        strip.removeFromLeft(4);
    };
    place(copyButton, 48);
    place(pasteButton, 50);
    place(velocityDownButton, 50);
    place(velocityUpButton, 50);
    place(timingEarlierButton, 58);
    place(timingLaterButton, 52);
    place(humanizeButton, 68);
    place(deleteSelectionButton, 54);
    hintLabel.setBounds(strip);

    if (grid != nullptr)
        grid->refreshSize();
}

'''
regex_once(r'void GgdDrumEditor::resized\(\)\n\{.*?\n\}\n\nvoid GgdDrumEditor::timerCallback', new_resized + 'void GgdDrumEditor::timerCallback', 'resized')

replace_once(
'''    grid->repaint();
}

void GgdDrumEditor::initialiseDrumState()''',
'''    updateContextStrip();
    grid->repaint();
}

void GgdDrumEditor::initialiseDrumState()''',
    'timer context update')

replace_once('    publishModelChange();\n}\n\nvoid GgdDrumEditor::applyActiveMapBindings',
             '    publishModelChange(false);\n}\n\nvoid GgdDrumEditor::applyActiveMapBindings',
             'initial publish without history')

replace_once(
'''    if (publish)
        publishModelChange();
}

void GgdDrumEditor::applyPatternGeometry''',
'''    if (publish)
        publishModelChange(false);
}

void GgdDrumEditor::applyPatternGeometry''',
    'map publish without history')

new_undo = r'''void GgdDrumEditor::performUndo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastUndoMs < 110.0 || undoHistory.empty())
        return;
    lastUndoMs = now;

    redoHistory.push_back(captureCurrentPattern());
    if (redoHistory.size() > maxHistoryDepth)
        redoHistory.pop_front();

    auto target = undoHistory.back();
    undoHistory.pop_back();
    restoringHistory = true;
    restorePatternSnapshot(target, false);
    restoringHistory = false;
    lastCommittedSnapshot = target;

    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();
}

void GgdDrumEditor::performRedo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastRedoMs < 110.0 || redoHistory.empty())
        return;
    lastRedoMs = now;

    undoHistory.push_back(captureCurrentPattern());
    if (undoHistory.size() > maxHistoryDepth)
        undoHistory.pop_front();

    auto target = redoHistory.back();
    redoHistory.pop_back();
    restoringHistory = true;
    restorePatternSnapshot(target, false);
    restoringHistory = false;
    lastCommittedSnapshot = target;

    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();
}

'''
regex_once(r'void GgdDrumEditor::performUndo\(\)\n\{.*?\n\}\n\nvoid GgdDrumEditor::chooseMidiFile', new_undo + 'void GgdDrumEditor::chooseMidiFile', 'undo redo')

new_import = r'''void GgdDrumEditor::importMidiFile(const juce::File& file)
{
    if (maps.isEmpty() || activeMapIndex < 0 || activeMapIndex >= maps.size())
        return;

    auto result = GgdMidiImporter::parseFile(
        file, maps.getReference(activeMapIndex), canonicalRows, SEQ_MAX_STEPS);

    if (!result.ok)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("MIDI import failed")
                .withMessage(result.error)
                .withButton("OK"),
            nullptr);
        return;
    }

    auto safeThis = juce::Component::SafePointer<GgdDrumEditor>(this);
    requestPatternReplacement(
        "Load groove '" + file.getFileName() + "'?",
        [safeThis, result]() mutable
        {
            if (auto* self = safeThis.getComponent())
                self->applyMidiImport(result);
        });
}

'''
regex_once(r'void GgdDrumEditor::importMidiFile\(const juce::File& file\)\n\{.*?\n\}\n\nvoid GgdDrumEditor::applyMidiImport', new_import + 'void GgdDrumEditor::applyMidiImport', 'smart midi replacement')

replace_once(
'''    publishModelChange();
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();

    const auto summary = result.summary();''',
'''    publishModelChange();
    markCurrentPatternClean();
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();

    const auto summary = result.summary();''',
    'import marks clean')

new_publish_and_methods = r'''GgdPatternSnapshot GgdDrumEditor::capturePattern(int pattern) const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    return GgdPatternFile::capture(*layer, canonicalRows, pattern,
                                   timeSigNumerator, timeSigDenominator, activeBars);
}

GgdPatternSnapshot GgdDrumEditor::captureCurrentPattern() const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    return capturePattern(layer->getCurrentPattern());
}

void GgdDrumEditor::recordCommittedPatternEdit()
{
    const auto current = captureCurrentPattern();
    if (restoringHistory)
    {
        lastCommittedSnapshot = current;
        return;
    }

    if (lastCommittedSnapshot.has_value()
        && GgdPatternFile::fingerprint(*lastCommittedSnapshot)
            != GgdPatternFile::fingerprint(current))
    {
        undoHistory.push_back(*lastCommittedSnapshot);
        if (undoHistory.size() > maxHistoryDepth)
            undoHistory.pop_front();
        redoHistory.clear();
    }
    lastCommittedSnapshot = current;
}

void GgdDrumEditor::resetHistoryForCurrentPattern(bool markClean)
{
    undoHistory.clear();
    redoHistory.clear();
    lastCommittedSnapshot = captureCurrentPattern();
    if (markClean)
        markCurrentPatternClean();
}

void GgdDrumEditor::initialiseCleanPatternFingerprints()
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    for (int pattern = 0; pattern < SEQ_MAX_PATTERNS; ++pattern)
    {
        cleanPatternFingerprints[static_cast<size_t>(pattern)] =
            GgdPatternFile::fingerprint(capturePattern(pattern));
        cleanPatternFingerprintValid[static_cast<size_t>(pattern)] = true;
    }
    lastCommittedSnapshot = capturePattern(layer->getCurrentPattern());
}

void GgdDrumEditor::markCurrentPatternClean()
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const int pattern = layer->getCurrentPattern();
    cleanPatternFingerprints[static_cast<size_t>(pattern)] =
        GgdPatternFile::fingerprint(captureCurrentPattern());
    cleanPatternFingerprintValid[static_cast<size_t>(pattern)] = true;
}

bool GgdDrumEditor::currentPatternHasChanges() const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const int pattern = layer->getCurrentPattern();
    if (!cleanPatternFingerprintValid[static_cast<size_t>(pattern)])
        return true;
    return cleanPatternFingerprints[static_cast<size_t>(pattern)]
        != GgdPatternFile::fingerprint(captureCurrentPattern());
}

void GgdDrumEditor::restorePatternSnapshot(const GgdPatternSnapshot& snapshot, bool publish)
{
    timeSigNumerator = snapshot.numerator;
    timeSigDenominator = snapshot.denominator;
    const int stepsPerBar = stepsPerBarForMeter(timeSigNumerator, timeSigDenominator);
    activeBars = juce::jlimit(1, juce::jmax(1, SEQ_MAX_STEPS / stepsPerBar), snapshot.bars);
    applyPatternGeometry(false);

    auto* seq = processor.mData.getUISeqData();
    const int pattern = seq->getLayer(0)->getCurrentPattern();
    GgdPatternFile::restore(snapshot, *seq, canonicalRows, 0, pattern);
    updatePersistenceTag();

    if (publish)
        publishModelChange();
}

void GgdDrumEditor::requestPatternReplacement(const juce::String& description,
                                               std::function<void()> replacement)
{
    if (!currentPatternHasChanges())
    {
        replacement();
        return;
    }

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Replace edited pattern?")
            .withMessage(description + " The current pattern has changes since it was loaded or saved.")
            .withButton("Replace")
            .withButton("Cancel"),
        [safe, replacement = std::move(replacement)](int result) mutable
        {
            if (result == 1 && safe != nullptr)
                replacement();
        });
}

void GgdDrumEditor::requestLoadGroove(const juce::File& file)
{
    importMidiFile(file);
}

void GgdDrumEditor::requestLoadPattern(const juce::File& file)
{
    GgdPatternSnapshot snapshot;
    juce::String error;
    if (!GgdPatternFile::read(file, snapshot, error))
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Pattern load failed")
                .withMessage(error)
                .withButton("OK"), nullptr);
        return;
    }

    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    requestPatternReplacement(
        "Load pattern '" + file.getFileNameWithoutExtension() + "'?",
        [safe, file, snapshot]() mutable
        {
            if (auto* self = safe.getComponent())
                self->applyPatternFile(file, snapshot);
        });
}

void GgdDrumEditor::applyPatternFile(const juce::File&, const GgdPatternSnapshot& snapshot)
{
    restorePatternSnapshot(snapshot, true);
    markCurrentPatternClean();
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();
    hintLabel.setText("Loaded pattern: " + snapshot.name, juce::dontSendNotification);
}

void GgdDrumEditor::saveCurrentPatternToLibrary()
{
    if (!libraryBrowser)
        return;
    const auto root = libraryBrowser->getPatternRoot();
    if (!root.isDirectory())
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("Choose a pattern folder")
                .withMessage("Open the Patterns tab and choose a library folder first.")
                .withButton("OK"), nullptr);
        return;
    }

    auto snapshot = captureCurrentPattern();
    juce::String safeName = snapshot.name.trim();
    if (safeName.isEmpty() || safeName == SEQ_DEFAULT_PAT_NAME)
        safeName = "Pattern";
    safeName = juce::File::createLegalFileName(safeName);
    const auto suggested = root.getChildFile(safeName + GgdPatternFile::extension);

    patternSaveChooser = std::make_unique<juce::FileChooser>(
        "Save Stochas GGD pattern", suggested, "*" + juce::String(GgdPatternFile::extension));
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    patternSaveChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe, snapshot](const juce::FileChooser& chooser) mutable
        {
            if (auto* self = safe.getComponent())
            {
                auto file = chooser.getResult();
                if (file == juce::File())
                    return;
                juce::String error;
                if (!GgdPatternFile::write(file, snapshot, error))
                {
                    juce::AlertWindow::showAsync(
                        juce::MessageBoxOptions()
                            .withIconType(juce::MessageBoxIconType::WarningIcon)
                            .withTitle("Pattern save failed")
                            .withMessage(error)
                            .withButton("OK"), nullptr);
                    return;
                }
                self->markCurrentPatternClean();
                if (self->libraryBrowser)
                    self->libraryBrowser->refresh();
            }
        });
}

bool GgdDrumEditor::currentPatternSlotIsEmpty(int pattern) const
{
    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    for (int row = 0; row < canonicalRows.size(); ++row)
    {
        const int storageRow = storageRowForCanonical(row, canonicalRows.size());
        for (int step = 0; step < layer->getNumSteps(); ++step)
            if (layer->getProb(storageRow, step, pattern) >= 0)
                return false;
    }
    return true;
}

void GgdDrumEditor::duplicateCurrentPatternSlot()
{
    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const int source = layer->getCurrentPattern();
    int target = -1;
    for (int i = 1; i < SEQ_MAX_PATTERNS; ++i)
    {
        const int candidate = (source + i) % SEQ_MAX_PATTERNS;
        if (currentPatternSlotIsEmpty(candidate))
        {
            target = candidate;
            break;
        }
    }

    if (target < 0)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("No empty pattern slot")
                .withMessage("All eight internal pattern slots contain notes. Nothing was overwritten.")
                .withButton("OK"), nullptr);
        return;
    }

    seq->copyPatternData(0, target, 0, source);
    juce::String copyName(layer->getPatternName(source));
    if (copyName.isEmpty() || copyName == SEQ_DEFAULT_PAT_NAME)
        copyName = "Pattern " + juce::String(source + 1);
    copyName = (copyName + " copy").substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(copyName.toRawUTF8(), target);
    layer->setCurrentPattern(target);
    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    refreshControlsFromModel();
    markCurrentPatternClean();
    resetHistoryForCurrentPattern(false);
    grid->refreshSize();
}

void GgdDrumEditor::showPatternActions()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Duplicate to empty slot");
    menu.addItem(2, "Save to pattern library");
    menu.addSeparator();
    menu.addItem(3, "Clear current pattern");
    auto safe = juce::Component::SafePointer<GgdDrumEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(patternActionsButton),
        [safe](int result)
        {
            if (auto* self = safe.getComponent())
            {
                if (result == 1) self->duplicateCurrentPatternSlot();
                else if (result == 2) self->saveCurrentPatternToLibrary();
                else if (result == 3) self->clearCurrentPattern();
            }
        });
}

void GgdDrumEditor::updateContextStrip()
{
    if (!grid)
        return;
    const bool select = grid->getToolMode() == GgdDrumGrid::ToolMode::select;
    const int selected = grid->getSelectionCount();
    selectionStatusLabel.setText(select ? juce::String(selected) + " selected" : "DRAW MODE",
                                 juce::dontSendNotification);

    copyButton.setVisible(select && selected > 0);
    pasteButton.setVisible(select && grid->hasClipboard());
    velocityDownButton.setVisible(select && selected > 0);
    velocityUpButton.setVisible(select && selected > 0);
    timingEarlierButton.setVisible(select && selected > 0);
    timingLaterButton.setVisible(select && selected > 0);
    humanizeButton.setVisible(select && selected > 0);
    deleteSelectionButton.setVisible(select && selected > 0);

    hintLabel.setVisible(!select || selected == 0);
    if (!select)
        hintLabel.setText("click/drag draw | right-drag erase | Shift-drag velocity | Alt-drag timing",
                          juce::dontSendNotification);
    else if (selected == 0)
        hintLabel.setText("click a hit or drag empty space to select | Alt-drag duplicates",
                          juce::dontSendNotification);
}

void GgdDrumEditor::publishModelChange(bool recordHistory)
{
    if (recordHistory)
        recordCommittedPatternEdit();
    processor.mData.swap();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
}

'''
regex_once(r'void GgdDrumEditor::publishModelChange\(\)\n\{.*?\n\}\n\n', new_publish_and_methods, 'history browser methods')

# Make clear an undoable action and refresh selection UI.
replace_once(
'''    publishModelChange();
    grid->repaint();
}

void GgdDrumEditor::updatePersistenceTag''',
'''    publishModelChange();
    grid->repaint();
    updateContextStrip();
}

void GgdDrumEditor::updatePersistenceTag''',
    'clear refresh context')

out_path.write_text(text, encoding='utf-8')

cmake = Path('CMakeLists.txt').read_text(encoding='utf-8')
cmake = cmake.replace('src/GgdDrumEditorAlpha7.cpp', 'src/GgdDrumEditorAlpha9.cpp')
needle = '    src/GgdMidiImporter.cpp\n    src/GgdKitMap.cpp\n'
replacement = ('    src/GgdMidiImporter.cpp\n'
               '    src/GgdPatternFile.cpp\n'
               '    src/GgdLibraryBrowser.cpp\n'
               '    src/GgdKitMap.cpp\n')
if needle not in cmake:
    raise RuntimeError('CMake source insertion point missing')
cmake = cmake.replace(needle, replacement, 1)
Path('CMakeLists.txt').write_text(cmake, encoding='utf-8')

print('Generated alpha 9 editor and updated CMake source list.')
