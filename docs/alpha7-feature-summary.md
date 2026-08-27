# Alpha 7 feature scope

- Deduplicated keyboard shortcut dispatch so JUCE key events and physical-key fallback cannot both execute a nudge or undo.
- Arrow nudge is one 1/16 step at ordinary zoom; the 1/32 detail grid uses half-step horizontal nudge for simple hits.
- Alt+arrow duplicates the current selection by the same nudge amount.
- Undo is debounced at the action boundary as an extra host-input guard.
- MIDI groove import parses PPQ Standard MIDI Files, preserves supported meter metadata, auto-detects a built-in GGD source mapping by note coverage (or accepts a forced source profile), converts source pitches to semantic articulations, preserves velocity and microtiming, and reports unresolved notes/collisions/truncation instead of silently hiding them.
- Import replaces the current pattern only after confirmation when it already contains hits.
