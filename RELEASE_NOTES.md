# Stochas GGD v0.1.0-alpha.5

Alpha.5 turns the grid into a more complete drum editor rather than only a drawing surface.

## Draw and Select/Edit modes

- Added explicit Draw and Select tools, switchable with D and S while the editor has keyboard focus.
- Select mode supports click selection, Shift-add/toggle, marquee selection, dragging selections in time or between visible articulation rows, arrow-key nudging, Delete/Backspace, Ctrl+A and Ctrl+D when the host forwards those keys.
- Selected hits receive a clear outline and drag moves show a destination preview.
- Moves preserve velocity, probability, timing offsets and retrigger data and refuse to overwrite unrelated occupied cells.
- Alt+wheel over a selected hit adjusts the whole selection's velocity relatively, preserving the dynamics between hits.

## Editing feedback

- Velocity editing now shows the exact 1-127 value in a compact popup above the hit during Shift-drag and after Alt+wheel adjustments.
- Ctrl+Z is still accepted if a host forwards it, but Bitwig intercepts it before the plugin. Plain U is now the plugin-local undo shortcut alongside the Undo button.

## Transport

- Replaced the subdivision-jumping playback highlight with a thin interpolated playhead rendered at 60 Hz.
- The editor measures the engine's step-transition interval and interpolates between reported positions, keeping the audio engine untouched while producing visibly smoother motion.

## Zoom

- Zoom now snaps to 25% increments from 50% to 400%.
- Ctrl+wheel moves one 25% interval per gesture.
- 125% remains the default and the existing 350%+ 1/32 detail mode is unchanged.

The existing GGD semantic mappings, MIDI passthrough, arbitrary meter support, eight-bar patterns, ghost notes, rolls, microtiming and hat-articulation workflow remain intact.
