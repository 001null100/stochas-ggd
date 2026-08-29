# Stochas GGD v0.2.0-beta.6

Beta 6 focuses on playback navigation and editor presentation. It builds directly on the Beta 5 interaction fixes and leaves the event scheduler, semantic pattern model and stored note timing unchanged.

## Real settings modal

The compact `...` control now opens a proper in-plugin modal Settings overlay instead of a popup menu. The editor behind it is dimmed and blocked until the modal closes with **Done** or **Escape**.

Settings apply live and are stored as local editor preferences rather than project/pattern data:

- **Theme** — Graphite, Midnight, Ember or Contrast.
- **Follow playhead while playing** — horizontally scroll the timeline as playback advances.
- **Smooth playhead interpolation** — higher-cadence visual interpolation between host step notifications.
- **Playhead forward glow** — enable/disable the directional light trail to the right of the cursor.
- **Follow edge margin** — choose how close the playhead may approach the right side before the viewport begins following it, from 0–30%.
- **Automatically use finer grid at high zoom** — retain the existing 350% automatic 1/32 / 1/16T switch, or keep zoom purely visual.

These preferences use the same local PropertiesFile as the existing theme choice and do not alter saved musical content.

## Follow-playhead scrolling

When Follow Playhead is enabled, the horizontal timeline viewport now keeps playback visible.

- normal vertical articulation scrolling is never changed
- once the playhead reaches the configured right-edge margin, the viewport follows continuously rather than jumping one page at a time
- pattern wrap returns the horizontal view toward the start
- follow movement is eased instead of snapping directly to the target position
- disabling Follow Playhead leaves manual horizontal scrolling completely alone

## Smoother playhead

The host notifier exposes playback position at a coarser 1/16-step cadence than the display refresh. Beta 6 improves the visual interpolation between those notifications without changing audio timing.

- smooth mode updates the editor timer at 120 Hz instead of 60 Hz
- observed step duration is filtered more gently to reduce callback jitter
- new host step notifications preserve most of the already-predicted visual phase instead of snapping the cursor back to every step boundary
- smoothing can be disabled in Settings to show the coarse host position directly

This is deliberately a presentation layer. `StochaEngineBeta.cpp` and the host-PPQ event scheduler are unchanged.

## Directional playhead glow

The playhead keeps a crisp timing edge but now optionally emits a soft fading glow approximately 52 px to its right. The glow suggests forward motion without smearing the actual cursor position. Sticky articulation labels are painted after the playhead so the effect never leaks into the name column.

## Hits centered in grid cells

Beta 5 corrected the original half-clipped tick-0 rendering by treating the event timestamp as the start of the hit body. Beta 6 takes the step-sequencer presentation one stage further: **hits are visually centered inside the grid spaces between timing markers**.

- bar/beat/subdivision lines remain the musical timing boundaries
- a hit at a grid timestamp is drawn in the middle of the following grid cell
- the first tick-0 hit remains fully visible
- hover targets, selection hit-testing, move previews and right-click erasing use the same centered geometry
- stored event ticks, MIDI import/export timing and playback timing remain unchanged

## Cell-based Draw mode

Because the note graphic is now centered in a grid space, Draw mode also treats that space as the clickable unit.

Previously the editor chose the nearest grid line, so clicking the right half of a visible cell could select the next subdivision. Beta 6 uses the cell under the mouse instead:

- click anywhere between two grid lines to edit that cell
- paint dragging still interpolates every crossed subdivision
- right-click remains an unconditional free-resolution eraser and can still delete triplet/off-grid events regardless of the active snap mode
- Shift velocity drag, Alt timing drag and Select mode continue to hit-test the actual event body

## Beta 5 workflow retained

Beta 6 keeps the previous release's workflow fixes:

- unconditional right-click erase in Draw mode
- resolution-independent erasing across straight/triplet grids
- skipped-subdivision protection during paint dragging
- articulation-name audition through the downstream GGD instrument
- click-and-type Bars editing
- live pattern-slot caption refresh
- 32-character pattern names
- Grooves / Patterns browser workflow
- high-resolution MIDI import/export
- semantic `.sggdp` pattern files
- per-hit probability, Ghost, Accent, Flam, Double, Humanize and Quantize actions

## Engine intentionally unchanged

Beta 6 does **not** modify:

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

- MIDI export remains file-based; direct MIDI drag-out into Bitwig is not implemented yet.
- Probability still uses quick presets rather than a dedicated lane/arbitrary-value editor.
- Experimental GGD Groove Player source pitch 85 remains intentionally unresolved.
- Playhead smoothing reconstructs motion from the existing coarse notifier; it is visual interpolation, not a new audio-thread timing feed.

## Release-candidate checks

The exact Beta 6 release candidate must compile and package successfully in the Windows PR workflow before merge. The release is complete only when GitHub Releases contains both:

- `Stochas.GGD.clap`
- `Stochas-GGD-v0.2.0-beta.6-Windows-x64.zip`
