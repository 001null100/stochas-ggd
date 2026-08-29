# Stochas GGD v0.2.0-beta.5

Beta 5 is a focused editor-correctness and workflow release on top of Beta 4. The readability-first theme/timeline system stays intact, while several interactions that felt wrong or unreliable are corrected.

## Hit alignment fixed

Hits were previously painted with their event timestamp at the **centre** of the visual note body. That made every note look half a hit early and caused the first event at tick 0 to be clipped by the left edge.

Beta 5 treats the event timestamp correctly as the **start** of the hit:

- note bodies extend to the right of the timing marker
- tick-0 hits are fully visible
- hover targets use the same left-edge convention
- move previews follow the corrected geometry
- bar / beat / subdivision lines remain the timing anchors rather than running through the middle of notes

This is a rendering/hit-testing correction only; stored event timing is unchanged.

## Right-click is always erase in Draw mode

Right-click no longer inherits the old paint/erase toggle based on the currently snapped grid cell.

- right-clicking empty timeline space never paints a note
- right-clicking an existing hit deletes it
- right-drag sweeps an eraser across actual event bodies
- erasing is based on real event positions rather than only the current snap lattice
- triplet/off-grid events can therefore be erased while a straight grid is active, and vice versa

Left-click retains the existing draw workflow and can still toggle an occupied snapped hit.

## More reliable triplet / dense paint dragging

The old paint drag only visited whichever subdivision happened to be under each sampled mouse event. Fast cursor movement could jump across narrow triplet cells without ever visiting the intermediate snap points.

Beta 5 interpolates every crossed grid position during a paint drag. If the cursor jumps across multiple 1/16T, 1/8T, 1/32 or 1/16 positions between UI events, each crossed subdivision is processed in order rather than silently skipped.

The audio/event scheduler is intentionally unchanged in this release. This fixes the concrete editor-side cause found for skipped triplet placement.

## Click articulation names to audition

The sticky articulation list is now playable.

- click an articulation/instrument row name to send its mapped GGD note
- release the mouse to send note-off
- audition follows the currently selected P V / P IV / Modern & Massive destination map
- the note is emitted through the plugin's existing MIDI output path, so it previews the downstream Kontakt/GGD instrument rather than using an internal sound

Instrument-family header clicks still collapse/expand the group instead of auditioning.

## Faster bar-count editing

The Bars field now selects its entire value when it receives focus. Click it and type a new number to replace the previous count directly, without manually selecting or deleting the old value first.

## Pattern slot names stay in sync

Renaming a pattern previously updated the dropdown items but could leave the closed pattern selector showing the old caption.

Beta 5 refreshes the selected item and reselects the same pattern ID only when its visible caption is stale, so the current slot name updates immediately without disturbing selection state.

## Longer pattern names

Pattern-name capacity increases from 14 to **32 visible characters**.

The inherited fixed-size model buffer has been widened to 33 bytes including the terminator. Project and native pattern persistence already store names as strings, so no serialization version bump is required. Older builds can still open the state but will truncate names back to their older limit.

## Cleaner top bar and compact settings

The standalone `Import MIDI` button has been removed from the visible workflow because MIDI groove loading already belongs in the dedicated Grooves browser.

Its top-bar space is repurposed as a compact `...` Settings control:

- Theme selection now lives in Settings
- the always-visible theme ComboBox is removed
- the pattern-name editor gains the freed flexible width
- top-bar order is now: kit → pattern slot → pattern actions → export → pattern name → settings

The four Beta 4 themes and their persisted appearance preference are unchanged.

## Beta 4 visual hierarchy retained

Beta 5 preserves the readability rules introduced in Beta 4:

1. bar boundaries are strongest
2. beat boundaries are clearly secondary
3. primary subdivisions are quieter
4. fine subdivisions are quietest and only shown at high resolution
5. instrument-family headers remain visually distinct from articulation rows

The Graphite, Midnight, Ember and Contrast themes all retain those semantic roles.

## Engine intentionally unchanged

Beta 5 does not modify:

- host-PPQ event scheduling
- 960 PPQ event storage
- event probability / duration playback
- transport discontinuity handling
- MIDI import/export mapping
- semantic GGD kit maps
- project persistence format
- pattern geometry
- live MIDI passthrough

The goal is to correct editor behavior without destabilizing the playback foundation that has remained stable through the previous betas.

## Current boundaries

- MIDI export remains file-based; direct MIDI drag-out into Bitwig is still not implemented.
- Probability still uses quick presets rather than a dedicated lane/arbitrary numeric editor.
- Experimental GGD Groove Player source pitch 85 remains intentionally unresolved.
- If already-existing triplet events are found to be skipped specifically during audio playback, that is separate from the paint-drag issue fixed here and should be diagnosed against the scheduler directly rather than guessed at in this UI-focused release.

## Release-candidate checks

The exact Beta 5 release candidate must compile and package successfully in the Windows PR workflow before merge. The release is complete only when GitHub Releases contains both:

- `Stochas.GGD.clap`
- `Stochas-GGD-v0.2.0-beta.5-Windows-x64.zip`
