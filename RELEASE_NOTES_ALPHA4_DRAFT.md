# Stochas GGD v0.1.0-alpha.4

This alpha is a grid-readability and high-zoom editing pass.

## Grid and zoom

- 125% is now the default working zoom and the reset target.
- 1/16 remains the normal editing grid across ordinary zoom levels.
- At 350% and above, a true playable 1/32 detail grid appears.
- Second-half 1/32 hits use the existing +50% timing offset, while adjacent pairs use the existing x2 retrigger timing. This keeps the underlying 1/16 engine and existing project data compatible.
- 1/32 pairs are drawn as two separate hits in detail mode instead of a generic x2 badge.

## Visual hierarchy

- Bar boundaries are the strongest timeline dividers.
- Beat boundaries are full-height and clearly stronger than subdivisions.
- 1/16 subdivisions use short local ticks instead of competing with beat lines.
- 1/32 midpoint ticks only appear in detail mode and are intentionally lighter.
- Drum-family blocks have taller headers and stronger top/bottom separators.

Existing velocity, microtiming, ghost-note, roll, hat-articulation, scrolling and pointer-anchored zoom gestures remain intact.
