# Stochas GGD v0.2.0-beta.8

Beta 8 is intentionally a **1.0 release-candidate cleanup pass** rather than another expansion release. It removes redundant UI actions, fixes the remaining Shift-velocity feedback rough edge, makes instrument-family dividers safer to interact with, and replaces novelty transforms with practical drum-editing tools. The scheduler, stored event timing, MIDI mapping and project persistence remain unchanged.

## Copy / Paste is the phrase-repeat workflow

The dedicated **Repeat** transform has been removed. It duplicated the existing clipboard behavior.

The intended workflow is now simply:

1. select a phrase
2. press `C` once
3. press `V` to paste immediately after the current selected phrase
4. the pasted copy becomes the selection, so additional `V` presses naturally walk the phrase forward

This keeps repetition on the keyboard instead of spending a permanent context-strip slot on it.

## Shift velocity inspection and editing

The Beta 7 Shift-click path could leave the velocity bubble pinned after merely changing selection. Beta 8 separates **inspection**, **selection** and **editing** more cleanly.

- **Hold Shift and hover a hit** to inspect its velocity.
- **Shift-click** still toggles that hit in the selection.
- **Shift-drag** still edits velocity; a multi-selection moves together while preserving relative differences.
- Releasing Shift or leaving the grid clears the hover velocity bubble.
- Velocity/timing bubbles during active edits remain available exactly where they are useful.

## Instrument groups only collapse from the articulation panel

Instrument-family headers remain full-width visual dividers across the timeline, but the timeline portion is now deliberately inert.

A group can only be collapsed or expanded by clicking its header in the sticky **left articulation panel**. This prevents accidental folding when clicking an otherwise useful empty portion of a group divider in the grid.

## Context-strip transforms refined

The transform strip now focuses on operations that are awkward to reproduce manually:

### Rows
Select every event on each instrument row represented by the current selection.

### Fill
Fill missing **current-grid subdivisions** between selected endpoints on each row. Existing hits are never moved or quantized. New hits interpolate velocity between the surrounding selected hits, making Fill useful for building rolls, hats and denser variations without flattening the dynamics.

### Mirror
Reflect selected hit timing around the temporal centre of the selection while keeping each hit on its current articulation row.

### Ramp+ / Ramp-
Create rising or falling velocity contours across selected hits.

### Dyn- / Dyn+
Compress or expand velocity differences around the selection's average velocity. `Dyn-` tightens an uneven performance; `Dyn+` exaggerates the existing accents and ghosts without replacing them with fixed values.

### Thin
Remove every second selected event independently on each articulation row for quick density reduction.

The previous Rotate actions have been removed.

## Editing workflow retained

Beta 8 keeps the editing fixes from previous betas:

- cell-centred hit presentation
- cell-based Draw targeting
- unconditional right-click erase in Draw mode
- resolution-independent erasing across straight and triplet hits
- paint-drag interpolation across skipped subdivisions
- Alt-drag free timing
- Alt-double-click quantize-to-nearest-current-subdivision
- velocity and timing value bubbles
- articulation-name audition through the downstream GGD instrument
- reliable Bars click/select/type behavior
- 32-character pattern names
- live pattern-slot caption refresh

## Playback and presentation retained

Beta 8 keeps:

- centre-locked playhead following
- optional 120 Hz playhead interpolation
- optional forward playhead glow
- modal Settings
- Graphite / Midnight / Ember / Contrast themes
- bar > beat > primary subdivision > fine subdivision hierarchy
- clear instrument-family divider hierarchy
- fixed Grooves / Patterns browser

## Engine intentionally unchanged

Beta 8 does **not** modify:

- host-PPQ event scheduling
- 960 PPQ event storage
- event probability or duration playback
- transport discontinuity handling
- MIDI import/export mapping
- semantic GGD kit maps
- project persistence format
- pattern geometry
- live MIDI passthrough

That is deliberate. At this stage, the priority is stabilising the interaction surface rather than reopening the engine without a concrete playback bug.

## Current boundaries before 1.0

- MIDI export remains file-based; direct MIDI drag-out into Bitwig is still not implemented.
- Probability uses quick presets rather than a dedicated lane/arbitrary-value editor.
- Experimental GGD Groove Player source pitch 85 remains intentionally unresolved.
- Playhead smoothing remains display interpolation reconstructed from the existing host notifier rather than a new audio-thread timing feed.

None of those currently block the core drum-sequencing workflow, so Beta 8 is intended to be tested as the final prerelease candidate before a 1.0 decision.

## Release-candidate checks

The exact Beta 8 candidate must compile and package successfully in the Windows PR workflow before merge. The release is complete only when GitHub Releases contains both:

- `Stochas.GGD.clap`
- `Stochas-GGD-v0.2.0-beta.8-Windows-x64.zip`
