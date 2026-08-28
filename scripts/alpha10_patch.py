from pathlib import Path
import re


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 occurrence, found {count}")
    return text.replace(old, new, 1)


def sub_once(text, pattern, replacement, label):
    out, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 regex match, found {count}")
    return out


# -----------------------------------------------------------------------------
# Editor source
# -----------------------------------------------------------------------------
src9_path = Path('src/GgdDrumEditorAlpha9.cpp')
src10_path = Path('src/GgdDrumEditorAlpha10.cpp')
src = src9_path.read_text(encoding='utf-8')

shortcut_block = r'''    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto mods = key.getModifiers();
        const bool plain = !mods.isCtrlDown() && !mods.isCommandDown() && !mods.isShiftDown();
        if (!plain)
            return false;

        // Bitwig is not reliable about forwarding modifier combinations to plug-ins.
        // Consume the editor's single-key vocabulary here, but execute it only from
        // the edge-triggered physical-key poller below so a host/JUCE double path can
        // never produce two edits from one press.
        if (keyMatchesLetter(key, 'D') || keyMatchesLetter(key, 'S')
            || keyMatchesLetter(key, 'A') || keyMatchesLetter(key, 'C')
            || keyMatchesLetter(key, 'V') || keyMatchesLetter(key, 'Z')
            || keyMatchesLetter(key, 'Y')
            || key.getKeyCode() == juce::KeyPress::deleteKey
            || key.getKeyCode() == juce::KeyPress::backspaceKey
            || key.getKeyCode() == juce::KeyPress::escapeKey
            || key.getKeyCode() == juce::KeyPress::leftKey
            || key.getKeyCode() == juce::KeyPress::rightKey
            || key.getKeyCode() == juce::KeyPress::upKey
            || key.getKeyCode() == juce::KeyPress::downKey)
            return true;

        return false;
    }

    void pollFallbackShortcuts(bool active)
    {
        auto letterDown = [](char upperCaseLetter)
        {
            return juce::KeyPress::isKeyCurrentlyDown(static_cast<int>(upperCaseLetter))
                || juce::KeyPress::isKeyCurrentlyDown(
                    static_cast<int>(upperCaseLetter + ('a' - 'A')));
        };

        auto edge = [](bool down, bool& wasDown)
        {
            const bool pressed = down && !wasDown;
            wasDown = down;
            return pressed;
        };

        // Always update edge state, even while inactive, so clicking back into the
        // grid while holding a key cannot synthesize a phantom command.
        const bool dPressed = edge(letterDown('D'), fallbackDDown);
        const bool sPressed = edge(letterDown('S'), fallbackSDown);
        const bool aPressed = edge(letterDown('A'), fallbackADown);
        const bool cPressed = edge(letterDown('C'), fallbackCDown);
        const bool vPressed = edge(letterDown('V'), fallbackVDown);
        const bool zPressed = edge(letterDown('Z'), fallbackZDown);
        const bool yPressed = edge(letterDown('Y'), fallbackYDown);
        const bool leftPressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey), fallbackLeftDown);
        const bool rightPressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey), fallbackRightDown);
        const bool upPressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::upKey), fallbackUpDown);
        const bool downPressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::downKey), fallbackDownDown);
        const bool deletePressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::deleteKey), fallbackDeleteDown);
        const bool backspacePressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::backspaceKey), fallbackBackspaceDown);
        const bool escapePressed = edge(
            juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::escapeKey), fallbackEscapeDown);

        if (!active)
            return;

        const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
        const bool commandFree = !mods.isCtrlDown() && !mods.isCommandDown() && !mods.isShiftDown();
        const bool alt = mods.isAltDown();
        if (!commandFree)
            return;

        if (!alt)
        {
            if (dPressed)
                setToolMode(ToolMode::draw);
            else if (sPressed)
                setToolMode(ToolMode::select);
            else if (zPressed && undoCallback)
                undoCallback();
            else if (yPressed && redoCallback)
                redoCallback();
            else if (toolMode == ToolMode::select && aPressed)
                selectAllHits();
            else if (toolMode == ToolMode::select && cPressed)
                copySelectionToClipboard();
            else if (toolMode == ToolMode::select && vPressed)
                pasteClipboard();
        }

        if (toolMode != ToolMode::select)
            return;

        if (!alt && (deletePressed || backspacePressed))
            deleteSelection();
        else if (!alt && escapePressed)
            clearSelection();
        else if (leftPressed)
            nudgeSelection(-1, 0, alt);
        else if (rightPressed)
            nudgeSelection(1, 0, alt);
        else if (upPressed)
            nudgeSelection(0, -1, alt);
        else if (downPressed)
            nudgeSelection(0, 1, alt);
    }

    void paint(juce::Graphics& g) override'''

src = sub_once(
    src,
    r"    bool keyPressed\(const juce::KeyPress& key\) override\n    \{.*?\n    \}\n\n    void paint\(juce::Graphics& g\) override",
    shortcut_block,
    'replace shortcut dispatcher')

# Paint the timing bubble alongside the velocity bubble.
src = replace_once(src, '        paintVelocityPopup(g);\n',
                   '        paintVelocityPopup(g);\n        paintTimingPopup(g);\n',
                   'paint timing popup')

# Timing drag now exposes its exact value immediately and continuously.
src = replace_once(
    src,
    '''        if (mods.isAltDown() && isOn)\n        {\n            dragMode = DragMode::timing;\n            dragStartValue = layer->getOffset(storageRow, step, pattern);\n            return;\n        }''',
    '''        if (mods.isAltDown() && isOn)\n        {\n            dragMode = DragMode::timing;\n            dragStartValue = layer->getOffset(storageRow, step, pattern);\n            showTimingPopup(canonicalRow, step, secondHalf, dragStartValue, true);\n            return;\n        }''',
    'timing drag start popup')

src = replace_once(
    src,
    '''            const int offset = juce::jlimit(-50, 50, dragStartValue + delta);\n            layer->setOffset(storageRow, dragStep, static_cast<int8_t>(offset), pattern);\n            repaint();''',
    '''            const int offset = juce::jlimit(-50, 50, dragStartValue + delta);\n            layer->setOffset(storageRow, dragStep, static_cast<int8_t>(offset), pattern);\n            showTimingPopup(dragRow, dragStep, dragSecondHalf, offset, true);\n            repaint();''',
    'timing drag live popup')

src = replace_once(
    src,
    '''        if (dragMode == DragMode::velocity)\n            velocityPopupPinned = false;\n\n        dragMode = DragMode::none;''',
    '''        if (dragMode == DragMode::velocity)\n            velocityPopupPinned = false;\n        if (dragMode == DragMode::timing)\n            timingPopupPinned = false;\n\n        dragMode = DragMode::none;''',
    'unpin timing popup')

# Alt+double-click is the precise timing reset gesture requested for build 10.
mouse_double = r'''
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const auto mods = e.mods;
        if (!mods.isAltDown() || mods.isCtrlDown() || mods.isCommandDown() || mods.isShiftDown())
            return;

        int canonicalRow = -1;
        int step = -1;
        bool secondHalf = false;
        if (!displayCellAt(e.position, canonicalRow, step, secondHalf))
            return;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        const int storageRow = storageRowForCanonical(canonicalRow, canonicalRows.size());
        if (!displayHalfIsOn(layer, storageRow, step, pattern, secondHalf))
            return;

        dragMode = DragMode::none;
        const int oldOffset = layer->getOffset(storageRow, step, pattern);
        if (oldOffset != 0)
        {
            layer->setOffset(storageRow, step, 0, pattern);
            publishChange();
        }
        showTimingPopup(canonicalRow, step, false, 0);
        repaint();
    }

'''
src = replace_once(src, '    void mouseDrag(const juce::MouseEvent& e) override\n',
                   mouse_double + '    void mouseDrag(const juce::MouseEvent& e) override\n',
                   'insert alt double click timing reset')

# Expose Select All and improve batch timing tools.
src = replace_once(src,
                   '    void adjustSelectedVelocityBy(int delta) { adjustSelectionVelocity(delta); }\n\n',
                   '    void selectAll() { selectAllHits(); }\n    void adjustSelectedVelocityBy(int delta) { adjustSelectionVelocity(delta); }\n\n',
                   'public select all wrapper')

src = sub_once(
    src,
    r'''    void adjustSelectedTimingBy\(int delta\)\n    \{.*?\n    \}\n\n    void humanizeSelected\(\)''',
    r'''    void adjustSelectedTimingBy(int delta)
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
            const auto& ref = selection.back();
            const int storageRow = storageRowForCanonical(ref.row, canonicalRows.size());
            showTimingPopup(ref.row, ref.step, false,
                            layer->getOffset(storageRow, ref.step, pattern));
            publishChange();
            repaint();
        }
    }

    void resetSelectedTiming()
    {
        if (selection.empty())
            return;
        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int pattern = layer->getCurrentPattern();
        bool changed = false;
        for (const auto& ref : selection)
        {
            const int storageRow = storageRowForCanonical(ref.row, canonicalRows.size());
            if (layer->getOffset(storageRow, ref.step, pattern) != 0)
            {
                layer->setOffset(storageRow, ref.step, 0, pattern);
                changed = true;
            }
        }
        const auto& ref = selection.back();
        showTimingPopup(ref.row, ref.step, false, 0);
        if (changed)
            publishChange();
        repaint();
    }

    void humanizeSelected()''',
    'batch timing tools')

# Timing popup state and reliable plain-letter shortcut edge state.
src = replace_once(
    src,
    '''    int velocityPopupRow = -1;\n    int velocityPopupStep = -1;\n    int velocityPopupValue = 0;\n    bool velocityPopupSecondHalf = false;\n    bool velocityPopupPinned = false;\n    double velocityPopupUntilMs = 0.0;\n\n    bool fallbackDDown = false;\n    bool fallbackSDown = false;\n    bool fallbackUDown = false;\n    bool fallbackYDown = false;''',
    '''    int velocityPopupRow = -1;\n    int velocityPopupStep = -1;\n    int velocityPopupValue = 0;\n    bool velocityPopupSecondHalf = false;\n    bool velocityPopupPinned = false;\n    double velocityPopupUntilMs = 0.0;\n\n    int timingPopupRow = -1;\n    int timingPopupStep = -1;\n    int timingPopupValue = 0;\n    bool timingPopupSecondHalf = false;\n    bool timingPopupPinned = false;\n    double timingPopupUntilMs = 0.0;\n\n    bool fallbackDDown = false;\n    bool fallbackSDown = false;\n    bool fallbackADown = false;\n    bool fallbackCDown = false;\n    bool fallbackVDown = false;\n    bool fallbackZDown = false;\n    bool fallbackYDown = false;''',
    'popup and fallback state')

# Add the exact signed timing bubble.
timing_popup_methods = r'''
    void showTimingPopup(int row, int step, bool secondHalf, int value,
                         bool pinned = false)
    {
        timingPopupRow = row;
        timingPopupStep = step;
        timingPopupSecondHalf = secondHalf;
        timingPopupValue = juce::jlimit(-50, 50, value);
        timingPopupPinned = pinned;
        timingPopupUntilMs = juce::Time::getMillisecondCounterHiRes() + 720.0;
        repaint();
    }

    void paintTimingPopup(juce::Graphics& g)
    {
        if (timingPopupRow < 0 || timingPopupStep < 0)
            return;

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (!timingPopupPinned && now > timingPopupUntilMs)
            return;

        const int y = rowY(timingPopupRow);
        if (y < 0)
            return;

        float x = xForStorageStepF(static_cast<float>(timingPopupStep))
                + storageStepWidth() * 0.5f;
        if (detail32Active())
            x = xForStorageStepF(static_cast<float>(timingPopupStep))
              + storageStepWidth() * (timingPopupSecondHalf ? 0.75f : 0.25f);

        juce::String valueText(timingPopupValue);
        if (timingPopupValue > 0)
            valueText = "+" + valueText;

        const float width = 58.0f;
        const float height = 23.0f;
        const float popupY = juce::jmax(static_cast<float>(rulerHeight + 2),
                                        static_cast<float>(y) - 25.0f);
        juce::Rectangle<float> bubble(x - width * 0.5f, popupY, width, height);

        g.setColour(c(outside).withAlpha(0.96f));
        g.fillRoundedRectangle(bubble, 5.0f);
        g.setColour(c(warm).withAlpha(0.94f));
        g.drawRoundedRectangle(bubble, 5.0f, 1.2f);
        g.setColour(c(text));
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText(valueText, bubble.toNearestInt(), juce::Justification::centred, false);
    }

'''
src = replace_once(src, '    bool cycleHatArticulation(int canonicalRow, int step, int direction)\n',
                   timing_popup_methods + '    bool cycleHatArticulation(int canonicalRow, int step, int direction)\n',
                   'insert timing popup methods')

# Browser callbacks remain the same, but pattern switching no longer leaves a stale loaded marker.
src = replace_once(
    src,
    '''        refreshControlsFromModel();\n        resetHistoryForCurrentPattern(false);\n        grid->repaint();''',
    '''        refreshControlsFromModel();\n        resetHistoryForCurrentPattern(false);\n        if (libraryBrowser)\n            libraryBrowser->clearLoaded();\n        grid->repaint();''',
    'clear loaded marker on slot switch')

# Text editors own keyboard focus completely while editing.
src = replace_once(src,
                   '    barsEditor.setMultiLine(false);\n',
                   '    barsEditor.setWantsKeyboardFocus(true);\n    barsEditor.setMouseClickGrabsKeyboardFocus(true);\n    barsEditor.setMultiLine(false);\n',
                   'bars focus policy')
src = replace_once(src,
                   '    patternName.setMultiLine(false);\n',
                   '    patternName.setWantsKeyboardFocus(true);\n    patternName.setMouseClickGrabsKeyboardFocus(true);\n    patternName.setMultiLine(false);\n',
                   'pattern name focus policy')

# Update local shortcut language and contextual controls.
src = src.replace('Undo through the editor history. U is the plugin-local shortcut; Bitwig may intercept Ctrl+Z.',
                  'Undo through the editor history. Z is the plugin-local shortcut.')
src = src.replace('Redo the most recently undone edit. Y is the plugin-local shortcut.',
                  'Redo the most recently undone edit. Y is the plugin-local shortcut.')

src = replace_once(src,
                   '    prepareContextButton(copyButton);\n    prepareContextButton(pasteButton);\n',
                   '    prepareContextButton(selectAllButton);\n    prepareContextButton(copyButton);\n    prepareContextButton(pasteButton);\n',
                   'prepare select all button')
src = replace_once(src,
                   '    prepareContextButton(timingEarlierButton);\n    prepareContextButton(timingLaterButton);\n',
                   '    prepareContextButton(timingEarlierButton);\n    prepareContextButton(timingResetButton);\n    prepareContextButton(timingLaterButton);\n',
                   'prepare timing reset button')
src = replace_once(src,
                   '    copyButton.onClick = [this] { if (grid) grid->copySelectionToClipboard(); };\n',
                   '    selectAllButton.onClick = [this] { if (grid) grid->selectAll(); };\n    copyButton.onClick = [this] { if (grid) grid->copySelectionToClipboard(); };\n',
                   'select all action')
src = replace_once(src,
                   '    timingEarlierButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(-5); };\n    timingLaterButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(5); };\n',
                   '    timingEarlierButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(-5); };\n    timingResetButton.onClick = [this] { if (grid) grid->resetSelectedTiming(); };\n    timingLaterButton.onClick = [this] { if (grid) grid->adjustSelectedTimingBy(5); };\n',
                   'timing reset action')

src = replace_once(src,
                   '        "Draw: click/drag | right-drag erase | Shift-drag velocity | Alt-drag timing",\n',
                   '        "D draw | S select | Z undo | Y redo | Alt-drag timing | Alt-double-click timing reset",\n',
                   'initial hint shortcuts')

# Remove the delayed focus grab that could race a freshly clicked text field.
src = sub_once(
    src,
    r'''\n    juce::MessageManager::callAsync\(\n        \[safe = juce::Component::SafePointer<GgdDrumEditor>\(this\)\]\n        \{\n            if \(safe != nullptr && safe->grid != nullptr\)\n                safe->grid->grabKeyboardFocus\(\);\n        \}\);''',
    '',
    'remove async focus steal')

# Do not delegate key presses to the grid while a text editor is active.
src = replace_once(
    src,
    '''bool GgdDrumEditor::keyPressed(const juce::KeyPress& key)\n{\n    return grid != nullptr && grid->keyPressed(key);\n}''',
    '''bool GgdDrumEditor::keyPressed(const juce::KeyPress& key)\n{\n    if (patternName.hasKeyboardFocus(true) || barsEditor.hasKeyboardFocus(true))\n        return false;\n    return grid != nullptr && grid->keyPressed(key);\n}''',
    'editor key focus guard')

# Bottom strip gets explicit single-key Select All and timing reset actions.
src = replace_once(src,
                   '    place(copyButton, 48);\n    place(pasteButton, 50);\n',
                   '    place(selectAllButton, 42);\n    place(copyButton, 48);\n    place(pasteButton, 50);\n',
                   'layout select all')
src = replace_once(src,
                   '    place(timingEarlierButton, 58);\n    place(timingLaterButton, 52);\n',
                   '    place(timingEarlierButton, 58);\n    place(timingResetButton, 58);\n    place(timingLaterButton, 52);\n',
                   'layout timing reset')

# Only an explicitly focused grid may run physical shortcuts. Hover is no longer enough.
src = replace_once(
    src,
    '''    const bool shortcutSurfaceActive =\n        !textEntryActive && (grid->hasKeyboardFocus(true) || isMouseOverOrDragging(true));\n    grid->pollFallbackShortcuts(shortcutSurfaceActive);''',
    '''    const bool shortcutSurfaceActive =\n        !textEntryActive && grid->hasKeyboardFocus(true);\n    grid->pollFallbackShortcuts(shortcutSurfaceActive);''',
    'strict grid shortcut focus')

# Host UI refreshes must not overwrite a text editor while the user is typing.
src = replace_once(src,
                   '    patternName.setText(layer->getPatternName(), false);\n',
                   '    if (!patternName.hasKeyboardFocus(true))\n        patternName.setText(layer->getPatternName(), false);\n',
                   'protect pattern name typing')
src = replace_once(src,
                   '    barsEditor.setText(juce::String(activeBars), false);\n    barsEditor.setTooltip(\n        "Pattern length in bars. Current meter allows 1-" + juce::String(maxBars)\n        + " bars within the 1024-step engine capacity.");\n    refreshPatternSelectorLabels();',
                   '    if (!barsEditor.hasKeyboardFocus(true))\n        barsEditor.setText(juce::String(activeBars), false);\n    barsEditor.setTooltip(\n        "Pattern length in bars. Current meter allows 1-" + juce::String(maxBars)\n        + " bars within the 1024-step engine capacity.");\n    refreshPatternSelectorLabels();',
                   'protect length typing')

# Successful imports report non-modally. Collisions/fallbacks no longer interrupt browsing.
src = sub_once(
    src,
    r'''\n    if \(result\.unresolvedNotes > 0 \|\| result\.fallbackNotes > 0\n        \|\| result\.collisions > 0 \|\| result\.truncatedNotes > 0\)\n    \{\n        juce::AlertWindow::showAsync\(.*?\n    \}''',
    '',
    'remove successful import modal')

# Carry the source path through import so the browser can show the actually loaded groove.
src = replace_once(src,
                   '[safeThis, result]() mutable\n        {\n            if (auto* self = safeThis.getComponent())\n                self->applyMidiImport(result);\n        });',
                   '[safeThis, result, file]() mutable\n        {\n            if (auto* self = safeThis.getComponent())\n                self->applyMidiImport(result, file);\n        });',
                   'capture imported source file')
src = replace_once(src,
                   'void GgdDrumEditor::applyMidiImport(const GgdMidiImportResult& result)\n',
                   'void GgdDrumEditor::applyMidiImport(const GgdMidiImportResult& result, const juce::File& sourceFile)\n',
                   'apply midi signature')
src = replace_once(src,
                   '    markCurrentPatternClean();\n    refreshControlsFromModel();\n    grid->refreshSize();\n    grid->grabKeyboardFocus();\n\n    const auto summary = result.summary();',
                   '    markCurrentPatternClean();\n    if (libraryBrowser)\n        libraryBrowser->setLoadedGroove(sourceFile);\n    refreshControlsFromModel();\n    grid->refreshSize();\n    grid->grabKeyboardFocus();\n\n    const auto summary = result.summary();',
                   'mark loaded groove')

src = replace_once(src,
                   'void GgdDrumEditor::applyPatternFile(const juce::File&, const GgdPatternSnapshot& snapshot)\n',
                   'void GgdDrumEditor::applyPatternFile(const juce::File& file, const GgdPatternSnapshot& snapshot)\n',
                   'pattern source file name')
src = replace_once(src,
                   '    restorePatternSnapshot(snapshot, true);\n    markCurrentPatternClean();\n    refreshControlsFromModel();',
                   '    restorePatternSnapshot(snapshot, true);\n    markCurrentPatternClean();\n    if (libraryBrowser)\n        libraryBrowser->setLoadedPattern(file);\n    refreshControlsFromModel();',
                   'mark loaded pattern')
src = replace_once(src,
                   '                if (self->libraryBrowser)\n                    self->libraryBrowser->refresh();',
                   '                if (self->libraryBrowser)\n                {\n                    self->libraryBrowser->refresh();\n                    self->libraryBrowser->setLoadedPattern(file);\n                }',
                   'mark saved pattern loaded')

# Context strip reflects the new single-key vocabulary and timing reset control.
src = replace_once(src,
                   '    copyButton.setVisible(select && selected > 0);\n',
                   '    selectAllButton.setVisible(select);\n    copyButton.setVisible(select && selected > 0);\n',
                   'show select all')
src = replace_once(src,
                   '    timingEarlierButton.setVisible(select && selected > 0);\n    timingLaterButton.setVisible(select && selected > 0);\n',
                   '    timingEarlierButton.setVisible(select && selected > 0);\n    timingResetButton.setVisible(select && selected > 0);\n    timingLaterButton.setVisible(select && selected > 0);\n',
                   'show timing reset')
src = replace_once(src,
                   '        hintLabel.setText("click/drag draw | right-drag erase | Shift-drag velocity | Alt-drag timing",\n',
                   '        hintLabel.setText("D draw | S select | Z undo | Shift-drag velocity | Alt-drag timing | Alt-double-click timing reset",\n',
                   'draw hint')
src = replace_once(src,
                   '        hintLabel.setText("click a hit or drag empty space to select | Alt-drag duplicates",\n',
                   '        hintLabel.setText("A all | C copy | V paste | arrows move | Alt+arrows/drag duplicate | Z undo",\n',
                   'select hint')

# Clearing the current pattern also clears any browser loaded-source marker.
src = replace_once(src,
                   '    publishModelChange();\n    grid->repaint();\n    updateContextStrip();\n}',
                   '    publishModelChange();\n    if (libraryBrowser)\n        libraryBrowser->clearLoaded();\n    grid->repaint();\n    updateContextStrip();\n}',
                   'clear loaded marker on clear')

src10_path.write_text(src, encoding='utf-8')

# -----------------------------------------------------------------------------
# Editor header
# -----------------------------------------------------------------------------
header_path = Path('src/GgdDrumEditor.h')
header = header_path.read_text(encoding='utf-8')
header = replace_once(header,
                      '    juce::TextButton copyButton { "Copy" };\n',
                      '    juce::TextButton selectAllButton { "All" };\n    juce::TextButton copyButton { "Copy" };\n',
                      'header select all button')
header = replace_once(header,
                      '    juce::TextButton timingEarlierButton { "Earlier" };\n    juce::TextButton timingLaterButton { "Later" };\n',
                      '    juce::TextButton timingEarlierButton { "Earlier" };\n    juce::TextButton timingResetButton { "Timing 0" };\n    juce::TextButton timingLaterButton { "Later" };\n',
                      'header timing reset button')
header = replace_once(header,
                      '    void applyMidiImport(const GgdMidiImportResult& result);\n',
                      '    void applyMidiImport(const GgdMidiImportResult& result, const juce::File& sourceFile);\n',
                      'header midi import signature')
header_path.write_text(header, encoding='utf-8')

# -----------------------------------------------------------------------------
# Browser header
# -----------------------------------------------------------------------------
browser_header_path = Path('src/GgdLibraryBrowser.h')
bh = browser_header_path.read_text(encoding='utf-8')
bh = replace_once(bh,
                  '    void setSavePatternCallback(VoidCallback callback);\n\n',
                  '    void setSavePatternCallback(VoidCallback callback);\n    void setLoadedGroove(const juce::File& file);\n    void setLoadedPattern(const juce::File& file);\n    void clearLoaded();\n\n',
                  'browser loaded api')
browser_header_path.write_text(bh, encoding='utf-8')

# -----------------------------------------------------------------------------
# Browser source
# -----------------------------------------------------------------------------
browser_path = Path('src/GgdLibraryBrowser.cpp')
b = browser_path.read_text(encoding='utf-8')

b = replace_once(b,
                 '    using OpenCallback = std::function<void(const juce::File&)>;\n\n    FileTreeItem(juce::File source, bool patternMode, OpenCallback callback)\n        : file(std::move(source)), patterns(patternMode), onOpen(std::move(callback))\n',
                 '    using OpenCallback = std::function<void(const juce::File&)>;\n    using LoadedCallback = std::function<bool(const juce::File&)>;\n\n    FileTreeItem(juce::File source, bool patternMode, OpenCallback callback, LoadedCallback loadedCallback)\n        : file(std::move(source)), patterns(patternMode), onOpen(std::move(callback)),\n          isLoaded(std::move(loadedCallback))\n',
                 'browser item loaded callback')

b = replace_once(b,
                 '        for (const auto& child : directories)\n            addSubItem(new FileTreeItem(child, patterns, onOpen));\n        for (const auto& child : files)\n            addSubItem(new FileTreeItem(child, patterns, onOpen));\n',
                 '        for (const auto& child : directories)\n            addSubItem(new FileTreeItem(child, patterns, onOpen, isLoaded));\n        for (const auto& child : files)\n            addSubItem(new FileTreeItem(child, patterns, onOpen, isLoaded));\n',
                 'browser child callbacks')

b = sub_once(
    b,
    r'''    void paintItem\(juce::Graphics& g, int width, int height\) override\n    \{.*?\n    \}\n\n    void itemDoubleClicked''',
    r'''    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const bool directory = file.isDirectory();
        const bool loaded = !directory && isLoaded && isLoaded(file);
        const bool selected = isSelected();

        if (loaded)
        {
            g.setColour(colour(browserAccent).withAlpha(0.25f));
            g.fillRect(0, 0, width, height);
            g.setColour(colour(browserAccent));
            g.fillRect(0, 0, 4, height);
        }
        else if (selected)
        {
            g.setColour(colour(browserAccent).withAlpha(0.11f));
            g.fillRect(0, 0, width, height);
            g.setColour(colour(browserAccent).withAlpha(0.75f));
            g.drawRect(0, 0, width, height, 1);
        }

        g.setColour(directory ? colour(browserText).withAlpha(0.90f)
                              : colour(browserText));
        g.setFont(juce::Font(directory ? 11.5f : 11.0f,
                             (directory || loaded) ? juce::Font::bold : juce::Font::plain));
        const int suffixWidth = loaded ? 58 : 6;
        g.drawText(file.getFileNameWithoutExtension(),
                   7, 0, juce::jmax(0, width - suffixWidth - 7), height,
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

    void itemDoubleClicked''',
    'browser item paint')

b = replace_once(b,
                 '    OpenCallback onOpen;\n',
                 '    OpenCallback onOpen;\n    LoadedCallback isLoaded;\n',
                 'browser loaded member')

b = replace_once(b,
                 '        tree.setRootItemVisible(false);\n        tree.setDefaultOpenness(true);\n',
                 '        tree.setRootItemVisible(false);\n        tree.setDefaultOpenness(false);\n',
                 'collapsed folders default')

# BrowserPane gets a loaded file and repaint-only setter.
b = replace_once(b,
                 '    void setSaveCallback(VoidCallback callback)\n    {\n        onSave = std::move(callback);\n    }\n\n    juce::File getRoot() const { return root; }\n',
                 '    void setSaveCallback(VoidCallback callback)\n    {\n        onSave = std::move(callback);\n    }\n\n    void setLoadedFile(const juce::File& file)\n    {\n        loadedFile = file;\n        tree.repaint();\n    }\n\n    juce::File getRoot() const { return root; }\n',
                 'browser pane loaded setter')

b = replace_once(b,
                 '        treeRoot = std::make_unique<FileTreeItem>(root, patterns, onOpen);\n',
                 '        auto loaded = [this](const juce::File& candidate)\n        {\n            return loadedFile != juce::File() && candidate == loadedFile;\n        };\n        treeRoot = std::make_unique<FileTreeItem>(root, patterns, onOpen, loaded);\n',
                 'browser root loaded callback')

b = replace_once(b,
                 '    juce::File root;\n    FileCallback onOpen;\n',
                 '    juce::File root;\n    juce::File loadedFile;\n    FileCallback onOpen;\n',
                 'browser loaded file member')

# Public browser loaded APIs clear the opposite tab so only the source for the
# current editor pattern is highlighted.
b += '''\nvoid GgdLibraryBrowser::setLoadedGroove(const juce::File& file)\n{\n    groovePane->setLoadedFile(file);\n    patternPane->setLoadedFile({});\n}\n\nvoid GgdLibraryBrowser::setLoadedPattern(const juce::File& file)\n{\n    patternPane->setLoadedFile(file);\n    groovePane->setLoadedFile({});\n}\n\nvoid GgdLibraryBrowser::clearLoaded()\n{\n    groovePane->setLoadedFile({});\n    patternPane->setLoadedFile({});\n}\n'''

browser_path.write_text(b, encoding='utf-8')

# -----------------------------------------------------------------------------
# Build target now compiles the alpha 10 editor source.
# -----------------------------------------------------------------------------
cmake_path = Path('CMakeLists.txt')
cmake = cmake_path.read_text(encoding='utf-8')
cmake = replace_once(cmake,
                     '    src/GgdDrumEditorAlpha9.cpp\n',
                     '    src/GgdDrumEditorAlpha10.cpp\n',
                     'cmake alpha10 editor')
cmake_path.write_text(cmake, encoding='utf-8')

print('Alpha 10 source generated successfully')
