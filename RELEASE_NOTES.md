# Stochas GGD v0.1.0-alpha.4

This alpha focuses on making the timeline grid read more naturally at normal zoom while giving the extreme zoom range a useful purpose.

## Working scale and detail grid

- 125% is now the default working zoom and the reset target.
- 1/16 remains the normal editing grid through ordinary zoom levels.
- At 350% and above, the editor switches to a playable 1/32 detail grid.
- A hit on the second 1/32 uses the engine's existing +50% step offset.
- Two adjacent 1/32 hits use the existing x2 retrigger timing, so the feature does not require a new sequencer clock or project-data migration.
- In detail mode those x2 events are drawn as two concrete hit bodies rather than an abstract x2 badge.

## Grid hierarchy

- Bar boundaries are the strongest vertical markers.
- Beat boundaries are full-height and clearly stronger than subdivisions.
- 1/16 subdivisions use short local ticks so they remain visible without turning the grid into graph paper.
- 1/32 midpoint ticks only appear in detail mode and are deliberately lighter.
- The ruler uses the same bar > beat > 1/16 > 1/32 hierarchy.

## Drum-family hierarchy

- Instrument-family headers are taller and visually heavier.
- Group tops use a stronger accent boundary and the bottoms use a clearer separator.
- Sticky labels mirror the same hierarchy, making Kick, Snare, Toms, Hats and cymbal families easier to scan while scrolling.

Existing velocity, microtiming, ghost-note, roll, hat-articulation, scrolling and pointer-anchored zoom gestures remain intact.
