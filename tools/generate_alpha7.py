from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
alpha6_path = ROOT / "src" / "GgdDrumEditorAlpha6.cpp"
alpha7_path = ROOT / "src" / "GgdDrumEditorAlpha7.cpp"
header_path = ROOT / "src" / "GgdDrumEditor.h"
cmake_path = ROOT / "CMakeLists.txt"
importer_cpp_path = ROOT / "src" / "GgdMidiImporter.cpp"
importer_h_path = ROOT / "src" / "GgdMidiImporter.h"


def replace_between(text: str, start: str, end: str, replacement: str) -> str:
    a = text.find(start)
    if a < 0:
        raise RuntimeError(f"missing start marker: {start!r}")
    b = text.find(end, a)
    if b < 0:
        raise RuntimeError(f"missing end marker: {end!r}")
    return text[:a] + replacement + text[b:]


def replace_once(text: str, old: str, new: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing replacement marker: {old[:120]!r}")
    return text.replace(old, new, 1)


src = alpha6_path.read_text(encoding="utf-8")
src = replace_once(src, '#include "GgdDrumEditor.h"\n', '#include "GgdDrumEditor.h"\n#include "GgdMidiImporter.h"\n')

shortcut_block = r'''    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto mods = key.getModifiers();

        // Ctrl/Cmd+A and Ctrl/Cmd+Z do not have a physical-key fallback.
        if (isHostUndoKey(key) && undoCallback)
        {
            undoCallback();
            return true;
        }

        if ((mods.isCtrlDown() || mods.isCommandDown()) && !mods.isAltDown()
            && !mods.isShiftDown() && keyMatchesLetter(key, 'A'))
        {
            if (toolMode == ToolMode::select)
            {
                selectAllHits();
                return true;
            }
        }

        // Plain editor keys are deliberately consumed here but executed only by
        // pollFallbackShortcuts(). Some hosts send both JUCE key events and expose
        // the same physical key state, which previously caused double nudges/undo.
        const bool noCommand = !mods.isCtrlDown() && !mods.isCommandDown() && !mods.isShiftDown();
        if (noCommand)
        {
            if (!mods.isAltDown()
                && (keyMatchesLetter(key, 'D') || keyMatchesLetter(key, 'S')
                    || keyMatchesLetter(key, 'U')
                    || key.getKeyCode() == juce::KeyPress::deleteKey
                    || key.getKeyCode() == juce::KeyPress::backspaceKey
                    || key.getKeyCode() == juce::KeyPress::escapeKey))
                return true;

            if (key.getKeyCode() == juce::KeyPress::leftKey
                || key.getKeyCode() == juce::KeyPress::rightKey
                || key.getKeyCode() == juce::KeyPress::upKey
                || key.getKeyCode() == juce::KeyPress::downKey)
                return true;
        }

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

        const bool dPressed = edge(letterDown('D'), fallbackDDown);
        const bool sPressed = edge(letterDown('S'), fallbackSDown);
        const bool uPressed = edge(letterDown('U'), fallbackUDown);
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
            else if (uPressed && undoCallback)
                undoCallback();
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

'''
src = replace_between(
    src,
    "    bool handleShortcut(const juce::KeyPress& key)\n",
    "    void paint(juce::Graphics& g) override\n",
    shortcut_block,
)

nudge_block = r'''    bool duplicateSelectionByDelta(int deltaSteps, int deltaRows)
    {
        if ((deltaSteps == 0 && deltaRows == 0) || selection.empty())
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();
        std::set<std::pair<int, int>> targets;

        for (const auto& ref : selection)
        {
            const int targetRow = shiftedCanonicalRow(ref.row, deltaRows);
            const int targetStep = ref.step + deltaSteps;
            if (targetRow < 0 || targetStep < 0 || targetStep >= numSteps)
                return false;
            if (!targets.emplace(targetRow, targetStep).second)
                return false;
            if (cellOccupied(targetRow, targetStep))
                return false;
        }

        std::vector<CellRef> duplicated;
        duplicated.reserve(selection.size());
        for (const auto& ref : selection)
        {
            const auto state = snapshotCell(ref.row, ref.step);
            const int targetRow = shiftedCanonicalRow(ref.row, deltaRows);
            const int targetStep = ref.step + deltaSteps;
            writeCellState(state, targetRow, targetStep);
            duplicated.push_back({ targetRow, targetStep });
        }

        selection = std::move(duplicated);
        publishChange();
        repaint();
        return true;
    }

    bool nudgeSelectionHalfStep(int direction, bool duplicate)
    {
        if (selection.empty() || (direction != -1 && direction != 1))
            return false;

        auto* layer = processor.mData.getUISeqData()->getLayer(0);
        const int numSteps = layer->getNumSteps();

        struct Destination
        {
            CellState state;
            int step = 0;
            int offset = 0;
        };

        std::vector<Destination> destinations;
        destinations.reserve(selection.size());
        std::set<std::pair<int, int>> targetKeys;

        for (const auto& ref : selection)
        {
            auto state = snapshotCell(ref.row, ref.step);
            if (state.length < 0)
                return false; // retrigger/roll cells cannot be split losslessly yet

            const int phase = state.offset == 50 ? 1 : 0;
            const int halfIndex = ref.step * 2 + phase + direction;
            if (halfIndex < 0)
                return false;

            const int targetStep = halfIndex / 2;
            const int targetPhase = halfIndex % 2;
            if (targetStep < 0 || targetStep >= numSteps)
                return false;

            // Duplicating to the other half of the same 1/16 cell is exactly an x2 retrigger.
            if (duplicate && targetStep == ref.step)
            {
                if (selection.size() != 1)
                    return false;
                state.length = -1;
                state.offset = 0;
                writeCellState(state, ref.row, ref.step);
                publishChange();
                repaint();
                return true;
            }

            const auto key = std::make_pair(ref.row, targetStep);
            if (!targetKeys.emplace(key).second)
                return false;

            if (cellOccupied(ref.row, targetStep))
            {
                if (duplicate || !isSelected(ref.row, targetStep))
                    return false;
            }

            state.offset = targetPhase == 0 ? 0 : 50;
            destinations.push_back({ state, targetStep, state.offset });
        }

        if (!duplicate)
            for (const auto& ref : selection)
                clearCellRaw(ref.row, ref.step);

        std::vector<CellRef> moved;
        moved.reserve(destinations.size());
        for (auto& destination : destinations)
        {
            destination.state.offset = destination.offset;
            writeCellState(destination.state, destination.state.ref.row, destination.step);
            moved.push_back({ destination.state.ref.row, destination.step });
        }

        selection = std::move(moved);
        publishChange();
        repaint();
        return true;
    }

    bool nudgeSelection(int horizontalDirection, int verticalDirection, bool duplicate)
    {
        if (selection.empty())
            return false;

        if (verticalDirection != 0)
            return duplicate
                ? duplicateSelectionByDelta(0, verticalDirection)
                : moveSelection(0, verticalDirection);

        if (horizontalDirection == 0)
            return false;

        if (detail32Active())
            return nudgeSelectionHalfStep(horizontalDirection, duplicate);

        return duplicate
            ? duplicateSelectionByDelta(horizontalDirection, 0)
            : moveSelection(horizontalDirection, 0);
    }

'''
move_marker = "    bool moveSelection(int deltaSteps, int deltaRows)\n"
move_at = src.find(move_marker)
if move_at < 0:
    raise RuntimeError("could not find moveSelection marker")
src = src[:move_at] + nudge_block + src[move_at:]

# Add source-profile and import controls after the pattern selector is constructed.
ui_anchor = "    addAndMakeVisible(patternSelector);\n"
ui_insert = r'''

    sourceMapSelector.addItem("Source: Auto", 1);
    for (int i = 0; i < maps.size(); ++i)
        sourceMapSelector.addItem("Source: " + maps.getReference(i).library, i + 2);
    sourceMapSelector.setSelectedId(1, juce::dontSendNotification);
    sourceMapSelector.setTooltip("MIDI groove source mapping. Auto chooses the built-in profile with the highest pitch coverage.");
    sourceMapSelector.setWantsKeyboardFocus(false);
    addAndMakeVisible(sourceMapSelector);

    importMidiButton.setTooltip("Import a .mid/.midi groove into the current pattern with semantic GGD remapping");
    importMidiButton.setWantsKeyboardFocus(false);
    importMidiButton.onClick = [this] { chooseMidiFile(); };
    addAndMakeVisible(importMidiButton);
'''
src = replace_once(src, ui_anchor, ui_anchor + ui_insert)

layout_old = "    patternSelector.setBounds(first.removeFromLeft(152).reduced(0, 5));\n    first.removeFromLeft(gap);\n    patternName.setBounds(first.reduced(0, 5));"
layout_new = "    patternSelector.setBounds(first.removeFromLeft(142).reduced(0, 5));\n    first.removeFromLeft(gap);\n    sourceMapSelector.setBounds(first.removeFromLeft(126).reduced(0, 5));\n    first.removeFromLeft(5);\n    importMidiButton.setBounds(first.removeFromLeft(78).reduced(0, 5));\n    first.removeFromLeft(gap);\n    patternName.setBounds(first.reduced(0, 5));"
src = replace_once(src, layout_old, layout_new)

# Route top-level editor key events through the same consume-only grid path.
src = replace_between(
    src,
    "bool GgdDrumEditor::keyPressed(const juce::KeyPress& key)\n",
    "void GgdDrumEditor::configureLookAndFeel()\n",
    r'''bool GgdDrumEditor::keyPressed(const juce::KeyPress& key)
{
    return grid != nullptr && grid->keyPressed(key);
}

''',
)

# Do not poll shortcuts while typing a pattern name. Preserve the existing active expression.
src, count = re.subn(
    r"grid->pollFallbackShortcuts\(([^;]+)\);",
    r"grid->pollFallbackShortcuts((\1) && !patternName.hasKeyboardFocus(true));",
    src,
    count=1,
)
if count != 1:
    raise RuntimeError("could not patch fallback shortcut activation")

# Debounce undo at the action boundary as an extra guard against hosts dispatching duplicate key paths.
src = replace_between(
    src,
    "void GgdDrumEditor::performUndo()\n",
    "void GgdDrumEditor::publishModelChange()\n",
    r'''void GgdDrumEditor::performUndo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastUndoMs < 110.0)
        return;
    lastUndoMs = now;

    processor.mData.undo();
    processor.mIncomingData.addToFifo(SEQ_NOTIFY_HOST, 0, 0);
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();
}

void GgdDrumEditor::chooseMidiFile()
{
    midiFileChooser = std::make_unique<juce::FileChooser>(
        "Import drum groove MIDI",
        juce::File(),
        "*.mid;*.midi");

    const int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles;
    auto safeThis = juce::Component::SafePointer<GgdDrumEditor>(this);
    midiFileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
    {
        if (auto* self = safeThis.getComponent())
        {
            const auto file = chooser.getResult();
            if (file.existsAsFile())
                self->importMidiFile(file);
        }
    });
}

void GgdDrumEditor::importMidiFile(const juce::File& file)
{
    const int selectedSource = sourceMapSelector.getSelectedId();
    const int forcedSource = selectedSource <= 1 ? -1 : selectedSource - 2;
    auto result = GgdMidiImporter::parseFile(
        file, maps, canonicalRows, forcedSource, maxUserBars);

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

    auto* layer = processor.mData.getUISeqData()->getLayer(0);
    const int pattern = layer->getCurrentPattern();
    bool occupied = false;
    for (int row = 0; row < canonicalRows.size() && !occupied; ++row)
    {
        const int storageRow = storageRowForCanonical(row, canonicalRows.size());
        for (int step = 0; step < layer->getNumSteps(); ++step)
        {
            if (layer->getProb(storageRow, step, pattern) >= 0)
            {
                occupied = true;
                break;
            }
        }
    }

    auto safeThis = juce::Component::SafePointer<GgdDrumEditor>(this);
    auto apply = [safeThis, result](int response) mutable
    {
        if (response != 1)
            return;
        if (auto* self = safeThis.getComponent())
            self->applyMidiImport(result);
    };

    if (occupied)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Replace current pattern?")
                .withMessage("Importing '" + file.getFileName() + "' will replace the current pattern. Other pattern slots are untouched.")
                .withButton("Replace")
                .withButton("Cancel"),
            std::move(apply));
    }
    else
    {
        apply(1);
    }
}

void GgdDrumEditor::applyMidiImport(const GgdMidiImportResult& result)
{
    timeSigNumerator = result.numerator;
    timeSigDenominator = result.denominator;
    activeBars = result.bars;
    applyPatternGeometry(false);

    auto* seq = processor.mData.getUISeqData();
    auto* layer = seq->getLayer(0);
    const int pattern = layer->getCurrentPattern();
    seq->clearPattern(0, pattern);

    for (const auto& cell : result.cells)
    {
        if (cell.canonicalRow < 0 || cell.canonicalRow >= canonicalRows.size()
            || cell.step < 0 || cell.step >= layer->getNumSteps())
            continue;

        const int storageRow = storageRowForCanonical(cell.canonicalRow, canonicalRows.size());
        layer->setProb(storageRow, cell.step, SEQ_PROB_ON, pattern);
        layer->setVel(storageRow, cell.step,
                      static_cast<int8_t>(juce::jlimit(1, 127, cell.velocity)), pattern);
        layer->setLength(storageRow, cell.step,
                         static_cast<int8_t>(cell.retriggerLength), pattern);
        layer->setOffset(storageRow, cell.step,
                         static_cast<int8_t>(juce::jlimit(-50, 50, cell.offset)), pattern);
    }

    const auto importedName = result.fileName.substring(0, SEQ_PATTERN_NAME_MAXLEN - 1);
    layer->setPatternName(importedName.toRawUTF8(), pattern);
    publishModelChange();
    refreshControlsFromModel();
    grid->refreshSize();
    grid->grabKeyboardFocus();

    const auto summary = result.summary();
    hintLabel.setText(summary.substring(0, 220), juce::dontSendNotification);
    hintLabel.setTooltip(summary);

    if (result.unresolvedNotes > 0 || result.collisions > 0
        || result.truncatedNotes > 0 || result.sourceConfidence < 0.85f)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("MIDI import report")
                .withMessage(summary)
                .withButton("OK"),
            nullptr);
    }
}

''',
)

src = src.replace("Copy: Alt-drag", "Copy: Alt-drag / Alt+arrow")
alpha7_path.write_text(src, encoding="utf-8")

# Header additions.
header = header_path.read_text(encoding="utf-8")
if '#include "GgdMidiImporter.h"' not in header:
    header = header.replace('#include "GgdKitMap.h"\n', '#include "GgdKitMap.h"\n#include "GgdMidiImporter.h"\n', 1)
if "sourceMapSelector" not in header:
    header = header.replace(
        "    juce::ComboBox barsSelector;\n",
        "    juce::ComboBox barsSelector;\n    juce::ComboBox sourceMapSelector;\n",
        1,
    )
    header = header.replace(
        '    juce::TextButton clearButton { "Clear" };\n',
        '    juce::TextButton clearButton { "Clear" };\n    juce::TextButton importMidiButton { "Import MIDI" };\n',
        1,
    )
    header = header.replace(
        "    std::unique_ptr<GgdDrumGrid> grid;\n",
        "    std::unique_ptr<GgdDrumGrid> grid;\n    std::unique_ptr<juce::FileChooser> midiFileChooser;\n    double lastUndoMs = -1000.0;\n",
        1,
    )
    header = header.replace(
        "    void performUndo();\n",
        "    void performUndo();\n    void chooseMidiFile();\n    void importMidiFile(const juce::File& file);\n    void applyMidiImport(const GgdMidiImportResult& result);\n",
        1,
    )
header_path.write_text(header, encoding="utf-8")

# Build the generated alpha7 source and importer.
cmake = cmake_path.read_text(encoding="utf-8")
cmake = cmake.replace(
    "    src/GgdDrumEditorAlpha6.cpp\n",
    "    src/GgdDrumEditorAlpha7.cpp\n    src/GgdMidiImporter.cpp\n",
    1,
)
cmake_path.write_text(cmake, encoding="utf-8")

# Small include hygiene for the importer.
imp = importer_cpp_path.read_text(encoding="utf-8")
if "#include <limits>" not in imp:
    imp = imp.replace("#include <map>\n", "#include <map>\n#include <limits>\n", 1)
importer_cpp_path.write_text(imp, encoding="utf-8")

imp_h = importer_h_path.read_text(encoding="utf-8")
if "#include <vector>" not in imp_h:
    imp_h = imp_h.replace("#include <JuceHeader.h>\n", "#include <JuceHeader.h>\n#include <vector>\n", 1)
importer_h_path.write_text(imp_h, encoding="utf-8")

print("Generated alpha7 editor and build wiring")
