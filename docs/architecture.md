# Stochas GGD direction

## Product shape

This project is a Windows-first CLAP note-effect drum sequencer intended to sit directly before a drum instrument in Bitwig Studio.

Typical chain:

`Stochas GGD CLAP -> Kontakt / GGD -> audio`

Stochas is the engine donor. Keep the proven host sync, MIDI scheduling, sequence timing, persistence, CLAP/JUCE plumbing, probability and humanization infrastructure where useful. The long-term editor and drum data model are drum-specific rather than scale/melody-specific.

Do not add an internal sampler, mixer, effects chain or song arranger unless the project later proves a concrete need. Bitwig and the destination drum library already own those jobs.

## Core abstraction

Patterns store semantic articulations, not destination MIDI notes.

Example:

`hihat.open_2 @ beat 3.5, velocity 0.82`

The active kit map resolves that semantic articulation to a MIDI binding only at playback/export time. This lets one pattern move between P IV, P V, Modern & Massive and future libraries without rewriting the pattern.

Kit maps are versioned JSON under `maps/`. MIDI values are stored numerically; note names are display/reference data only. Current GGD screenshots use Kontakt-style naming where MIDI 60 is C3.

## Current event model

The Beta engine uses event-based hits rather than a rigid boolean step matrix. The internal timeline is 960 PPQ, which gives exact integer positions for the straight and triplet grids currently exposed by the editor.

A hit carries:

- semantic articulation row
- absolute pattern tick
- velocity
- probability
- duration

The visual grid is only an editing projection over these events. A hit does not become a 1/16 or 1/32 event because of the current zoom level. This is what allows true independent 1/32 hits, triplets, imported off-grid performances, flams and future groove operations without changing the storage model.

The editor currently chooses snap resolution from zoom:

- straight: `1/16`, switching to `1/32` at 350% zoom
- Triplet mode: `1/8T`, switching to `1/16T` at 350% zoom

Changing the grid changes painting, nudging and quantization behavior, not stored event positions.

## Pattern slots and geometry

The inherited eight Stochas pattern slots remain the working-set model, but each active event pattern owns its own meter, bar count and event length.

A 4/4 groove, a one-bar fill and an odd-meter pattern can therefore coexist without sharing one global geometry value.

## Pattern library

Three layers remain distinct:

1. **Bitwig project state**: automatically restores the current working set of patterns and plugin state with the project.
2. **Native pattern library**: reusable semantic `.sggdp` patterns.
3. **MIDI library**: external `.mid` groove packs imported through semantic source mapping.

Native patterns are independent of the selected destination kit map.

The Grooves and Patterns browser stores its two root folders in local JUCE properties rather than project state. Beta 3 keeps a lightweight in-memory index of matching files for each root. The index is rebuilt when the root changes or the user presses Refresh; live filtering then searches cached relative paths instead of recursively hitting the disk for every keystroke.

## MIDI import

Import is a two-stage mapping:

`source MIDI note -> semantic articulation -> pattern event`

Never remap directly from source note number to destination note number. That would make library switching, missing-articulation fallbacks and user-edited mappings unnecessarily fragile.

Known GGD source translations are used where available. Unknown pitches must be reported or deliberately left unresolved rather than silently assigned to a guessed articulation. Experimental GGD source pitch 85 remains intentionally unresolved for this reason.

Imported note positions and velocities are retained on the 960 PPQ event timeline rather than immediately quantized to the visible grid.

## MIDI export

Export performs the destination half of the semantic pipeline:

`semantic articulation -> active kit binding -> MIDI note`

The exporter writes the current pattern at its native PPQ resolution and preserves event position, velocity, duration and meter. Unmapped semantic hits are counted and skipped rather than translated to an arbitrary note.

Standard MIDI note events do not contain a native probability field. Export therefore writes every mapped hit; probability remains part of Stochas GGD project/native-pattern state.

## Performance editing

The editor is optimized around direct pattern manipulation rather than a separate parameter inspector.

Current operations include:

- draw/remove hits
- marquee and additive selection
- move, duplicate and grid nudge
- copy/paste across patterns
- direct velocity editing plus Ghost and Accent presets
- free timing movement and quantize
- humanize
- per-hit probability presets
- event-based Flam and Double transforms
- undo/redo

Flam and Double create independent events in the timeline. They are not encoded as inherited Stochas retrigger flags, so their generated hits can subsequently be edited like any other event.

## UI ownership

The editor shell is intentionally separate from the audio/event engine. Beta 2 restored the established Alpha editor presentation over the new Beta event model after the initial engine transition regressed several UI behaviors.

The right-side Grooves/Patterns browser owns a fixed editor column. The timeline retains the stronger hierarchy of bar, beat, primary subdivision and fine subdivision lines, plus a 60 Hz interpolated playhead derived from host step notifications.

Future visual work should preserve this separation: presentation changes should not require another event-engine rewrite.

## Longer-term editor priorities

Useful next areas include:

1. clearer visual feedback for probability and other per-hit properties
2. an optional velocity/probability lane for dense editing
3. more drum-aware transforms such as hat openness and choke pairing
4. double-kick alternation helpers
5. pattern tags/favorites and richer library organization if the simple filter proves insufficient
6. MIDI drag-out to the host if CLAP/Bitwig integration can support it cleanly
7. user-editable/custom kit maps

These should remain event/semantic operations rather than reintroducing a fixed step-cell model.

## CI and releases

GitHub Actions builds the Windows CLAP target on pull requests. PR builds produce a downloadable test artifact.

The release workflow reads `RELEASE_VERSION`, builds the same Windows CLAP target from `main`, and publishes both a raw `Stochas.GGD.clap` and a versioned Windows ZIP under GitHub Releases.

A release is not considered complete until the versioned release entry and both assets exist.