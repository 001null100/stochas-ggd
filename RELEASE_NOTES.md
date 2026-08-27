# Stochas GGD v0.1.0-alpha.7

Alpha.7 fixes the remaining shortcut double-input behavior and adds the first proper MIDI groove-import workflow.

## Deterministic keyboard editing

- Plain editor shortcuts now have one execution path instead of being independently handled by both JUCE key events and the Bitwig physical-key fallback.
- Arrow nudging at normal zoom always moves exactly one 1/16 step horizontally or one visible articulation row vertically.
- In the 1/32 detail view, simple hits can be nudged horizontally by one 1/32 position.
- Alt+Arrow duplicates the current selection by the same nudge amount instead of moving it.
- Alt+drag duplication remains available in Select mode.
- Undo actions are debounced at the action boundary to protect against duplicate host input. U remains the plugin-local undo shortcut, while Ctrl+Z still depends on whether the host forwards it.

## MIDI groove import

- Added an Import MIDI action for Standard MIDI Files using PPQ timing.
- Added a source-profile selector with Auto plus the built-in P V, P IV and Modern & Massive mappings.
- Auto mode chooses the source profile with the highest note-mapping coverage.
- Imported source MIDI pitches are converted to semantic drum articulations before being written to the currently selected destination GGD kit, so source and target mappings can differ.
- Supported time-signature metadata is imported and pattern length is derived automatically up to the current 8-bar limit.
- Velocity and sub-1/16 timing are preserved using the existing Stochas velocity and offset data.
- Same-cell flam/retrigger collisions are compacted into the existing retrigger representation where possible.
- Import reports mapping coverage, unresolved MIDI pitches, collisions and truncated notes instead of silently hiding them.
- Importing over a non-empty current pattern requires confirmation and does not overwrite other pattern slots.

## Current import limitations

- Auto detection currently uses the three built-in GGD kit maps as source profiles. Pack-specific historical MIDI profiles are a later expansion.
- Time-signature changes inside one MIDI file are not supported yet.
- Unresolved source pitches are reported but are not retained as editable unresolved events.
- At 1/32 detail zoom, retrigger/roll cells cannot yet be losslessly half-step nudged.
- Pattern meter and length are still inherited Stochas layer geometry, so changing them affects the shared pattern-slot geometry.

The existing Draw/Select editor, marquee editing, velocity popup, smooth playhead, semantic GGD destination mappings, MIDI passthrough, ghost notes, rolls, microtiming and 125% default zoom remain intact.
