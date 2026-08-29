# Stochas GGD v0.2.0-beta.7

Beta 7 is an editing-workflow pass on top of Beta 6. It restores high-information interactions that were lost during the visual rewrite, makes playhead following truly centre-locked, hardens bar-count text entry, and replaces redundant context-strip buttons with pattern transforms. The audio scheduler and stored event timing are unchanged.

## Velocity and timing feedback restored

Velocity and free-timing edits once again show a compact value bubble directly above the note being edited.

- **Shift-drag velocity** shows `VEL <value>` while dragging.
- **Alt-drag timing** shows the signed tick offset from the nearest active subdivision, for example `TIME +18t`.
- The bubble remains briefly after release so the final value is readable without permanently cluttering the grid.
- The popup follows the note while timing is moved and respects the horizontally scrolled sticky articulation column.

## Alt-double-click timing reset

Alt-double-clicking a hit now performs the event-model equivalent of resetting timing: it quantizes that individual hit to the **nearest currently active subdivision**.

This works with the visible editing resolution, including straight and triplet grids. If the destination is already occupied by another event on the same row, the editor leaves the hit unchanged rather than destroying or merging notes.

## Shift-drag velocity in Select mode

Select mode now supports direct velocity editing without sacrificing Shift-click multi-selection.

- **Shift-click** still toggles a hit in or out of the selection.
- Once a Shift gesture actually moves a few pixels, it becomes a velocity drag instead.
- If the dragged hit belongs to a multi-selection, every selected hit changes by the same relative amount while preserving the velocity differences between them.
- If the dragged hit was not selected, it becomes the selection before the velocity gesture begins.

## Bars keyboard input hardened

The Bars field now explicitly behaves as a focusable, editable JUCE TextEditor rather than relying on inherited host focus behavior.

- mouse click explicitly grabs keyboard focus
- the entire value remains selected on focus
- typing replaces the old value immediately
- Return commits the new bar count and gives keyboard focus back to the grid
- the field accepts up to four numeric digits
- model refresh still refuses to overwrite the field while the user is editing it

## Centre-locked playhead following

Follow Playhead no longer chases a configurable right-edge margin. When enabled, playback is locked to the **centre of the visible timeline** as soon as enough content exists to scroll there.

- only horizontal timeline position moves; articulation-row vertical scroll is untouched
- the beginning naturally remains left-aligned until the playhead reaches the centre position
- while playing, temporary trailing viewport runway is added so the final half-screen of the pattern can remain centred too
- stopping playback removes that temporary runway and restores the normal content width
- the old Follow Edge Margin control has been removed from Settings because centre-lock makes it obsolete

Beta 6 smooth interpolation and optional forward glow remain available.

## Redundant strip buttons replaced with transforms

The permanent **All / Copy / Paste**, **Vel +/-**, and **Earlier / Quantize / Later** buttons duplicated operations already faster from the keyboard or direct manipulation. Their slots now expose pattern-level transforms instead.

### Rows
Select every event on each instrument row represented by the current selection. Useful for grabbing an entire kick, snare, hat or articulation line after selecting one representative hit.

### Repeat
Copy the selected phrase immediately after its current temporal span. The new copy becomes selected, so repeated clicks can walk a phrase forward through the pattern.

### Mirror
Reflect selected event timing around the temporal centre of the selection while keeping each event on its current instrument row.

### Ramp+ / Ramp-
Create rising or falling velocity contours across selected hits. Existing velocity spread is respected; nearly-flat selections are expanded into a useful dynamic range before the ramp is applied.

### Rot < / Rot >
Rotate selected events left or right by one current subdivision, wrapping inside the selection's own temporal span.

### Thin
Remove every second selected event independently on each instrument row, providing a quick density-reduction operation.

The original keyboard workflow remains:

- `A` select all
- `C` copy
- `V` paste
- arrow keys move/nudge selection
- Alt + arrows duplicate while moving
- direct Shift-drag handles velocity
- direct Alt-drag handles timing

Ghost, Accent, probability presets, Humanize, Flam and Double remain in the strip because they provide distinct performance operations rather than shortcut duplicates.

## Beta 6 presentation retained

Beta 7 keeps:

- modal local Settings
- Graphite / Midnight / Ember / Contrast themes
- 120 Hz optional playhead interpolation
- optional directional playhead glow
- cell-centred hit presentation
- cell-based Draw targeting
- readability hierarchy of bars > beats > primary subdivisions > fine subdivisions
- clear instrument-family separation

## Previous workflow fixes retained

Beta 7 also preserves:

- unconditional right-click erase in Draw mode
- free-resolution erasing across straight/triplet hits
- interpolation across skipped subdivisions during paint drags
- articulation-name audition through the downstream GGD instrument
- live pattern-slot caption refresh
- 32-character pattern names
- indexed Grooves / Patterns browser
- high-resolution MIDI import/export
- semantic `.sggdp` pattern storage
- per-hit probability, Ghost, Accent, Flam, Double and Humanize actions

## Engine intentionally unchanged

Beta 7 does **not** modify:

- host-PPQ event scheduling
- 960 PPQ event storage
- event probability or duration playback
- transport discontinuity handling
- MIDI import/export mapping
- semantic GGD kit maps
- project persistence format
- pattern geometry
- live MIDI passthrough

## Current boundaries

- MIDI export remains file-based; direct MIDI drag-out into Bitwig is still not implemented.
- Probability still uses quick presets rather than a dedicated lane/arbitrary-value editor.
- Experimental GGD Groove Player source pitch 85 remains intentionally unresolved.
- Playhead smoothing remains display interpolation reconstructed from the existing host notifier rather than a new audio-thread timing feed.

## Release-candidate checks

The exact Beta 7 release candidate must compile and package successfully in the Windows PR workflow before merge. The release is complete only when GitHub Releases contains both:

- `Stochas.GGD.clap`
- `Stochas-GGD-v0.2.0-beta.7-Windows-x64.zip`
