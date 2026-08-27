# Stochas GGD v0.1.0-alpha.3

This alpha replaces the fit-relative timeline from alpha.2 with a stable absolute musical scale.

## Zoom and timeline

- Pattern length no longer changes beat width. One quarter note has the same physical width whether the pattern is 1 bar or 8 bars.
- 100% zoom is now a fixed 128 pixels per quarter note. A 4/4 bar is therefore always 512 pixels at 100%.
- Zoom now ranges from 50% to 400% and scales only the timeline. Longer patterns grow horizontally and scroll instead of being squeezed into the viewport.
- Removed automatic Fit behavior. The former Fit control is now a 100% reset.
- Ctrl+wheel zoom remains pointer-anchored, but the anchor calculation no longer depends on viewport width or pattern length, eliminating the feedback loop that made zoom-out drift and feel inaccurate.
- Resizing the editor or changing the bar count no longer alters musical scale.
- Zoom and editing resolution are decoupled. The editor keeps 1/16 placement precision at every zoom level instead of changing quantization while zooming.

## Readability

- Hit bodies have a larger, more consistent relationship to their 1/16 cells.
- The area after the end of a short pattern is visually distinct from the active timeline rather than stretching the pattern to fill it.
- Added a clear pattern-end marker.
- Zoom readouts now use familiar percentages instead of a Fit/multiplier hybrid.

The existing GGD mappings, velocity/timing gestures, ghost notes, retriggers, hat articulation switching, pattern controls and project-state behavior are unchanged.
