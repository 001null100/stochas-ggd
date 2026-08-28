# Stochas GGD v0.2.0-beta.2

Beta 2 repairs the UI regressions introduced by the Beta 1 engine transition. The Beta 1 event engine, persistence, MIDI import and per-pattern geometry remain intact; this release puts the established Alpha 10 editing presentation back on top of that engine.

## Restored timeline hierarchy

- Bars, beats and subdivisions are visually distinct again instead of being drawn with nearly equal weight.
- Bar boundaries use the strongest full-height divider and alternating bar shading.
- Beat boundaries are clearly stronger than subdivision ticks.
- Primary subdivisions use shorter/lighter markers, with fine subdivisions shown only when the high-resolution grid is active.
- Beat numbers and stronger drum-family separators are restored.
- Velocity-dependent hit sizing and selection outlines remain on the event editor.

## Smooth playhead

- The playhead once again interpolates between the engine's 1/16 position notifications at the editor's 60 Hz refresh rate.
- Observed step duration is smoothed so movement remains fluid while following tempo changes.
- The playhead uses an Alpha-style accent line with a subtle timeline glow rather than stepping one cell at a time.

## Grooves / Patterns browser restored

- The right-side library browser is explicitly assigned its own 310 px editor column again.
- Its Grooves and Patterns roots, loaded-item state, folder persistence, save/load callbacks and existing browser implementation are unchanged.
- The browser is explicitly kept visible and above the timeline viewport after resizing so host-restored window sizes cannot silently bury it.
- The editor minimum size has been raised enough to preserve the browser and top controls instead of allowing them to collapse into each other.

## Zoom-driven grid resolution

The four-way manual grid selector from Beta 1 is removed from the visible workflow. Resolution follows zoom again, while one Triplet toggle selects the rhythmic family.

Straight mode:
- below 350% zoom: `1/16`
- 350% zoom and above: `1/32`

Triplet mode:
- below 350% zoom: `1/8T`
- 350% zoom and above: `1/16T`

The current zoom and effective grid are shown together. Changing zoom updates drawing, painting, arrow-key nudging and Quantize resolution; existing event positions remain independent of the display grid.

## Engine unchanged from Beta 1

Beta 2 intentionally does not modify:

- the 960 PPQ event store
- direct host-PPQ event scheduling
- true independent 1/32 and triplet events
- per-pattern meter and bar count
- semantic GGD mappings
- high-resolution MIDI groove import
- `.sggdp` v2 pattern files
- Beta project persistence and Alpha migration

## What to verify

- The Grooves / Patterns browser is visible immediately and retains its configured roots.
- Bar, beat, 1/16 and high-zoom 1/32 divisions are easy to distinguish at a glance.
- Triplet mode shows 1/8T normally and switches to 1/16T at 350% zoom.
- The playhead moves continuously rather than jumping once per 1/16 step.
- Existing Beta 1 patterns play identically after opening Beta 2.
- MIDI import, per-pattern lengths/meters, selection editing and project save/reopen behave exactly as they did in Beta 1.
