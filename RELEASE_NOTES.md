# Stochas GGD v0.1.0-alpha.10

Alpha.10 is a performance-editing and browser UX pass. It removes several sources of friction found while rapidly auditioning GGD grooves and makes the editor's keyboard and microtiming workflows much more dependable inside Bitwig.

## Calmer groove and pattern browsing

- Successful MIDI imports no longer open a modal import report. Retrigger collisions, fallbacks and other import statistics remain available in the non-modal status text/tooltip instead of interrupting every audition.
- Actual import failures still show a warning dialog.
- Groove and pattern folders now start collapsed. Only the invisible library root opens automatically, so large GGD libraries no longer explode into one enormous tree.
- Single-clicked browser items get an obvious selection highlight.
- The file actually loaded into the current pattern is distinguished separately with a stronger accent, left marker and `LOADED` label.
- Loading a groove, loading/saving a native pattern, switching internal pattern slots and clearing the current pattern keep that loaded-source marker accurate.

## Reliable single-key editing shortcuts

- Removed dependence on Ctrl/Cmd combinations for core grid commands because Bitwig may consume those before a plug-in receives them.
- `A` selects all hits in Select mode.
- `C` copies the current selection.
- `V` pastes the clipboard, including after switching to another internal pattern slot for cross-pattern copying.
- `Z` is the reliable plug-in-local Undo shortcut.
- `Y` is Redo.
- `D` / `S`, Delete, arrows and Alt+arrows remain on the same edge-triggered physical-key path.
- Shortcut polling now runs only while the grid explicitly owns keyboard focus. Hovering the plug-in is no longer enough to fire editor commands.

## Text-entry focus fixes

- Pattern Name and Length explicitly own keyboard focus while being edited.
- Host/UI refreshes no longer overwrite either text field while it has focus.
- Removed the delayed constructor focus grab that could steal focus from a text field shortly after opening the editor.
- Grid shortcut dispatch is suspended while either text field is active.

## Microtiming editing

- Alt-drag timing now shows an exact signed `-50 ... +50` popup, mirroring the feedback already provided for velocity.
- The value updates live while dragging and remains briefly visible after release.
- Alt+double-click a hit to reset its microtiming directly to `0`.
- Selected hits expose a `Timing 0` action for batch reset.
- Earlier/Later selection adjustments also show an exact timing value after the change.

## Existing workflow retained

- Cross-pattern selection copy/paste preserves velocity, probability, retrigger/length and timing data.
- Alt-drag and Alt+Arrow duplication remain the fast local duplication tools.
- Multi-level editor undo/redo, semantic `.sggdp` patterns, exact GGD Groove Player import, collapsible drum families and the 1024-step engine remain intact.

## Current limitations

- The high-zoom 1/32 editor still projects onto the 1/16 storage model with offset/retrigger data rather than using native independent 1/32 events.
- Pattern meter and length remain shared layer geometry across the eight inherited Stochas pattern slots.
- The rare experimental Groove Player source pitch 85 remains deliberately unresolved rather than being assigned an invented articulation.
