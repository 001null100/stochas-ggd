# Stochas GGD v0.2.0-beta.3

Beta 3 is the first feature-focused release on top of the stabilized Beta event engine and restored Beta 2 editor. The scheduler, event store, persistence and import foundation are deliberately left alone; this release concentrates on making drum programming faster and the surrounding workflow less cumbersome.

## Per-hit probability editing

- Selected hits now expose real event probability through fast `25%`, `50%`, `75%` and `100%` presets.
- The selection status reports the shared probability when selected hits match, or `Pmix` when the selection contains different values.
- Probability remains an event property, so it survives native pattern/project persistence and moves with copied or transformed hits.
- Probability editing participates in the existing pattern undo history through the normal event publication path.

## Faster velocity workflow

Two common drum-performance values now have dedicated actions:

- **Ghost** sets selected hits to velocity 35.
- **Accent** sets selected hits to velocity 120.

The existing relative velocity controls, direct velocity dragging and imported velocities remain unchanged.

## Real Flam and Double transforms

The new event timeline is finally being used for drum-aware transforms rather than only storage.

- **Flam** creates a quieter grace hit 1/64 before each selected hit.
- **Double** creates a slightly softer independent 1/32 follow-up hit.
- Generated hits are ordinary events with their own velocity, probability, duration and editable position.
- Collisions and transforms outside the pattern boundary are skipped safely.
- These are not inherited Stochas retrigger flags, so the generated notes can be selected, moved, copied, quantized or deleted independently afterward.

## High-resolution MIDI export

Beta 3 adds `Export MIDI` beside the existing import action.

- Exports the current pattern using the **active GGD destination kit map**.
- Resolves semantic articulations to destination MIDI notes only at export time.
- Preserves the pattern PPQ resolution, exact event timing, velocity, duration and time signature.
- Unmapped semantic articulations are counted and skipped instead of being assigned guessed MIDI notes.
- The default filename follows the current pattern name and prefers the configured pattern-library folder when available.

Standard MIDI note events have no native probability field. MIDI export therefore writes every mapped hit; probability remains part of Stochas GGD native/project state.

## Searchable Grooves / Patterns browser

The right-side browser now has a live filter in both tabs.

- Searches both filenames and relative folder paths.
- Matching results temporarily flatten into one quick result list for large groove packs.
- Clearing the filter returns to the normal collapsible folder hierarchy.
- Loaded-file highlighting remains visible in filtered results.
- Search results retain the existing double-click load behavior.
- Each library now maintains a cached file index. The disk is scanned when the root changes or Refresh is pressed, while typing filters the in-memory index instead of recursively walking the folder on every keystroke.
- Filter output is capped at 500 visible matches to keep the tree responsive on extremely large libraries.

## Editor workflow polish

The selection/action area has been expanded into a two-row performance strip so the new tools do not compress the timeline or browser.

First row:
- select all / copy / paste
- relative velocity
- Ghost / Accent
- probability presets
- Delete

Second row:
- Earlier / Quantize / Later
- Humanize
- Flam / Double
- contextual shortcut hint

The top toolbar now keeps pattern-level operations together, including Import MIDI and Export MIDI.

The editor minimum size remains large enough to preserve the dedicated browser column and usable timeline area.

## Browser and project documentation cleanup

- Replaced the stale upstream Stochas README that still linked to the Surge Stochas releases and described generic AU/AAX/Projucer workflows.
- The repository README now documents the actual Windows CLAP note-effect workflow, supported GGD maps, event engine, library behavior, MIDI import/export and current beta boundaries.
- `docs/architecture.md` now reflects the implemented 960 PPQ event model, independent per-pattern geometry, semantic import/export pipeline, browser index, performance transforms and release process.

## Engine intentionally unchanged

Beta 3 does **not** change the parts that proved stable in Beta 1/Beta 2:

- 960 PPQ event storage
- host-PPQ playback scheduling
- true independent 1/32 and triplet events
- smooth interpolated playhead
- zoom-driven straight/triplet editing resolution
- per-pattern meter and bar count
- live MIDI passthrough
- semantic GGD destination maps
- high-resolution MIDI import
- `.sggdp` v2 pattern files
- Beta project persistence / Alpha migration

The goal is to build upward from the stable foundation rather than reopen it every release.

## Current boundaries

- Probability has fast presets but no dedicated lane or arbitrary numeric editor yet.
- Flam uses a musical 1/64 grace offset rather than a millisecond-based flam-time control.
- Double creates a fixed 1/32 follow-up; more elaborate roll tools are left for later.
- MIDI export is file-based. Dragging MIDI directly from the plugin into Bitwig is not implemented yet.
- Experimental GGD Groove Player source pitch 85 remains intentionally unresolved.
- Browser search is filename/path filtering rather than a tags/favorites database.

## Release-candidate checks

The Beta 3 branch is expected to preserve Beta 2 playback behavior while adding the new editor workflows. The Windows PR build must compile and package successfully before merge, and the final release is complete only when GitHub Releases contains both:

- `Stochas.GGD.clap`
- `Stochas-GGD-v0.2.0-beta.3-Windows-x64.zip`
