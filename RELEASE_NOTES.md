# Stochas GGD v0.2.0-beta.1

Beta 1 is the engine transition release. The GGD drum path no longer stores musical hits as inherited Stochas 1/16 cells with offsets and retrigger encodings. Patterns now use an event timeline with exact musical positions, and playback schedules those events directly against the host PPQ timeline.

The visual redesign is deliberately deferred. This release is about replacing the foundation without also moving every piece of furniture.

## Event-based pattern engine

- Drum patterns now store independent events rather than a fixed cell matrix.
- Event timing uses a 960 PPQ internal timeline, giving exact integer positions for straight and triplet subdivisions.
- Events retain velocity, probability, duration and semantic drum-row identity.
- The event store is fixed-capacity and pointer-free inside the existing SequenceData double-buffer snapshots, so the audio thread still reads immutable state without realtime allocation.
- The inherited Stochas cell scheduler remains available only as a compatibility fallback for legacy state that has not yet been migrated.

## Real subdivisions

- Native `1/16` grid.
- Native `1/32` grid. These are now genuinely independent hits rather than half-step offsets packed into a 1/16 cell.
- Native `1/8T` triplet grid.
- Native `1/16T` triplet grid.
- Grid changes affect quantisation/edit placement rather than changing the pattern's underlying storage resolution.
- Imported off-grid and triplet timing can remain at its exact event tick instead of being flattened into the closest cell plus an offset.

## Direct event playback

- Beta patterns are scheduled directly from host PPQ positions inside each audio block.
- Playback no longer needs to wait for inherited sequencer-cell boundaries before deciding which drum events occur.
- Pattern loops use each pattern's own musical length.
- Existing MIDI output, note-off queueing, host transport following and CLAP note-effect routing are retained.

## Per-pattern geometry

- Each of the eight internal patterns now owns its own time signature and length.
- Switching pattern slots restores that pattern's meter and bar count rather than sharing one layer-wide geometry value.
- A short fill can therefore live beside a multi-bar groove without padding both to the same inherited step count.

## MIDI import

- GGD Groove Player import now produces event hits at high-resolution musical positions.
- Multiple hits that previously collided inside one 1/16 cell remain separate events.
- Triplets and 1/32 timing survive import as actual timing rather than retrigger/offset approximations.
- Existing semantic translation for P V, P IV and Modern & Massive remains in place.
- The rare experimental source pitch 85 remains deliberately unresolved rather than being assigned an invented articulation.

## Native pattern files

- `.sggdp` pattern files are now version 2.
- Version 2 stores semantic articulation, exact event tick, duration, velocity and probability together with meter and pattern length.
- Alpha version 1 `.sggdp` files still load and are translated into event positions.

## Project compatibility

- Beta event state is persisted as its own versioned block alongside the inherited Stochas project XML.
- Existing alpha projects without event state keep their legacy cell data available for one-time migration when opened in Beta.
- New Beta state round-trips exact event positions and per-pattern geometry instead of recreating the old cell representation.

## Editor changes kept intentionally small

- The editor now exposes explicit `1/16`, `1/32`, `1/8T` and `1/16T` grid choices.
- Timing reset is now conceptually Quantize: selected events snap to the active grid rather than resetting a cell-offset field.
- The existing drum-family layout, semantic kit mapping, pattern browser, selection workflow, velocity editing, undo/redo and library workflow remain the baseline for this release.
- Broader UX and visual changes are postponed until the event engine has been tested in real Bitwig sessions.

## What to test in Beta 1

This release changes the most timing-sensitive part of the plug-in. The useful tests are therefore behavioral rather than cosmetic:

- Create alternating 1/16 and 1/32 hits and verify that every hit plays distinctly and stays locked over long loops.
- Create 1/8T and 1/16T patterns and verify that triplets remain phase-locked to Bitwig over repeated looping and transport restarts.
- Put patterns with different meters and lengths in separate internal slots, switch between them, and verify that each restores and loops at its own geometry.
- Import GGD grooves containing fast doubles, 1/32 notes or triplets and compare playback against the source MIDI.
- Save/reopen a Bitwig project and verify exact timing, pattern lengths and active pattern state survive.
- Open a project created with Alpha 10 and verify its existing patterns migrate without moving or losing hits.
- Exercise stop/start, Bitwig loop regions and transport jumps to catch duplicate or missed events at block/loop boundaries.

## Known Beta 1 boundaries

- This is the first release on the event engine, so transport edge cases and migration behavior deserve more scrutiny than the established Alpha editor path.
- The editor still visually inherits much of Alpha 10's workflow and styling; the larger UX pass comes later.
- MIDI drag-and-drop export into Bitwig is not part of Beta 1 yet. The event model is now suitable for implementing it without lossy reconstruction, which was the prerequisite for doing that feature properly.
