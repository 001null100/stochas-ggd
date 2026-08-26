# Stochas GGD direction

## Product shape

This project is a Windows-first CLAP note-effect drum sequencer intended to sit directly before a drum instrument in Bitwig Studio.

Typical chain:

`Stochas GGD CLAP -> Kontakt / GGD -> audio`

Stochas is the engine donor. Keep the proven host sync, MIDI scheduling, sequence timing, persistence, CLAP/JUCE plumbing, probability and humanization infrastructure where useful. The long-term editor and drum data model are drum-specific rather than scale/melody-specific.

## Core abstraction

Patterns should store semantic articulations, not destination MIDI notes.

Example:

`hihat.open_2 @ beat 3.5, velocity 0.82`

The active kit map resolves that semantic articulation to one or more MIDI bindings only at playback/export time. This lets one pattern move between P IV, P V, Modern & Massive and future libraries without rewriting the pattern.

Kit maps are versioned JSON under `maps/`. MIDI values are stored numerically; note names are display/reference data only. Current GGD screenshots use Kontakt-style naming where MIDI 60 is C3.

## Native pattern model

The editor should eventually use event-based hits rather than a rigid boolean step matrix. A hit needs at least:

- semantic articulation ID
- beat position
- velocity
- microtiming offset
- probability

The visual grid is an editing projection over those events. This keeps off-grid hits, flams, triplets, groove templates and imported performances possible without changing the storage model.

## Pattern library

Three layers should remain distinct:

1. **Bitwig project state**: automatically restores the current working set of patterns and editor state with the project.
2. **Native pattern library**: reusable semantic patterns with folders/tags/favorites/search.
3. **MIDI library**: browsable external `.mid` groove packs that are imported through a source mapping profile and then become ordinary semantic events.

Native patterns should be independent of any selected destination kit map.

## MIDI import/remapping

Import is a two-stage mapping:

`source MIDI note -> semantic articulation -> destination kit binding`

Never remap directly from source note number to destination note number. That would make library switching, missing-articulation fallbacks and user-edited mappings unnecessarily fragile.

Source profiles should support General MIDI, known GGD groove-pack formats, known library maps and custom mappings. Unknown notes must be preserved/reported rather than discarded.

Automatic detection can score profiles against the pitches present in a file, but the UI should expose the chosen source profile and confidence because historical groove packs are not guaranteed to use one universal vocabulary.

## Initial editor priorities

1. Drum-grouped articulation rows.
2. Fast hit painting/removal.
3. Velocity editing directly on hits plus an optional velocity lane.
4. Microtiming editing.
5. Selection/copy/duplicate.
6. Flexible grid including triplets.
7. Variable pattern lengths and time signatures.
8. Native pattern save/load and browsing.
9. MIDI import and auditioning.
10. Drum-aware actions such as flam, ghost, hat-openness changes, choke pairing and double-kick alternation.

Do not add an internal sampler, mixer, effects chain or song arranger unless the project later proves a concrete need. Bitwig and the destination drum library already own those jobs.

## CI

GitHub Actions builds only the Windows CLAP target. Pull requests produce a downloadable artifact for testing in Bitwig; pushes to `main` and version tags also produce Windows CLAP artifacts.
