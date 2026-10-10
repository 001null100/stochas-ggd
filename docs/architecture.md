# Stochas GGD architecture

## Product shape

Stochas GGD is a CLAP note-effect drum sequencer that sits directly before a drum instrument, developed mainly against Bitwig Studio on Windows (VST3, AU and Standalone builds come along through JUCE).

Typical chain:

`Stochas GGD CLAP -> Kontakt / GGD -> audio`

Stochas was the engine donor: host integration, persistence, the eight-slot working set and the automation parameter list are inherited. The drum data model, scheduler and editor are GGD-specific.

Do not add an internal sampler, mixer, effects chain or song arranger unless the project later proves a concrete need. The DAW and the drum library already own those jobs.

## Source map

| Area | Files |
| --- | --- |
| Timeline maths shared by engine and editor | `GgdTimeline.h/.cpp` |
| Real-time scheduler | `GgdEngine.h/.cpp`, `GgdPlayhead.h` |
| Event patterns | `GgdEventModel.h/.cpp`, `GgdPatternOps.h/.cpp` |
| Kit maps and the drum layer | `GgdKitMap.h/.cpp`, `GgdProject.h/.cpp`, `maps/ggd/*.json` |
| Project state and persistence | `SequenceData.h/.cpp`, `Persist.h/.cpp`, `GgdEventPersist.h/.cpp` |
| File formats | `GgdPatternFile.*` (`.sggdp`), `GgdMidiImporter.*`, `GgdMidiExporter.*` |
| Plug-in shell | `PluginProcessor.h/.cpp` |
| Editor | `GgdDrumEditor.*` (window, history), `GgdDrumGrid.*` (timeline), `GgdSettingsPanel.*`, `GgdLibraryBrowser.*`, `GgdUiTheme.*` |
| Tests and tools | `tests/CoreTests.cpp`, `tests/PluginTests.cpp`, `tests/TestMain.cpp` |

`CMakeLists.txt` keeps the engine, model and file formats in `STOCHAS_CORE_SOURCES` (no UI dependencies) so they can be tested without the editor.

## Core abstraction

Patterns store semantic articulations, not destination MIDI notes:

`hihat.open_2 @ tick 1440, velocity 82`

The active kit map resolves the articulation to a MIDI note only at playback and export time, so a pattern moves between P IV, P V, Modern & Massive and future libraries unchanged.

Kit maps are versioned JSON under `maps/` and are compiled in. MIDI values are numeric; note names are display data (Kontakt naming, MIDI 60 = C3).

Inside a project, every articulation known to any built-in map has a stable *canonical row*, stored on storage row `SEQ_MAX_ROWS - canonicalCount + canonical`. New built-in maps must be appended so existing indices stay valid. `GgdProject` owns this mapping, the active kit (saved as the `GGD4:<kit>` layer tag) and the rules that keep layer 0 configured as a GGD drum layer.

## Event model

A pattern slot is a `GgdEventPattern`: meter (numerator 1-32, denominator 2/4/8/16), bar count, and up to 4096 events sorted by tick. A hit carries storage row, tick at 960 PPQ, velocity, probability and duration. The maximum length is 64 bars of 4/4 (`GGD_MAX_PATTERN_TICKS`).

The visual grid is only an editing projection. A hit never becomes a 1/16 or 1/32 hit because of the grid; this is what allows straight, triplet and off-grid hits side by side, imported performances, flams and drags in ticks.

`GgdPatternOps::changeGeometry` changes meter or bar count *bar by bar*: each hit keeps its bar index and its offset from the bar line. Hits that no longer fit become *parked hits* (bar, offset, row, velocity...) stored in the pattern itself, up to 2048 per pattern. They never play, are saved with the project, travel with duplicates and undo, and come back on a later geometry change that has room for them. A live hit always wins over a parked hit on the same spot, and *Double length* discards parked hits of the bars it fills with the repeat. `.sggdp` files and MIDI exports contain only the visible hits.

## Timing model

`GgdTimeline` maps host time (quarter notes, `ppq`) onto pattern ticks. The engine and the editor's playhead use the same functions, so what is drawn is what plays.

**Host bars** (default, `SyncMode::hostBars`). The anchor is the host's current bar: its start (`ppqPositionOfLastBarStart`), its number (`barCount`) and its length from the host time signature. Pattern bar = host bar number modulo the pattern's bar count; position in the bar = `ppq - barStart`. Consequences:

- a host meter change starts the next pattern bar on the next host bar line, so nothing drifts;
- a pattern bar shorter than the host bar rests until the next bar line; a longer one is cut;
- a multi-bar pattern follows the host bar number from any start position.

If the host reports no bar start, bar lines are derived from the meter grid; if it reports no meter, the pattern's own bar length is used. While the inherited *speed* automation is set to anything other than 1x, patterns run free, since bar anchoring at another speed has no meaning.

**Free run** (`SyncMode::free`). The pattern loops at its own length from song position 0, independent of bar lines (the 1.x behaviour, useful for polymeter).

`segmentsForRange()` splits a host range into pieces that map linearly onto pattern ticks, breaking at host bar lines and loop points and skipping rests. Each segment carries a cycle number (host bar or loop count) used to seed per-occurrence randomness.

## Scheduler

`GgdEngine::processBlock` runs per block on the audio thread and never allocates or locks:

1. Resolve the anchor for the block and publish a playhead snapshot.
2. Build the scheduling window. Windows are chained: if the block starts where the last window ended (within max(0.05 quarter, two blocks)), the new window continues from there, so tempo automation or position jitter can never drop or double a hit. Anything larger is a relocation (loop, seek): pending note-ons are dropped and the window restarts a hair (2 ticks) before the block, so a start exactly on a hit still plays it. Windows never reach before song position 0 (pre-roll).
3. For every segment, find the events in its tick range, apply probability (a stateless hash of seed, cycle, tick and row, so decisions are repeatable within a pass and fresh on the next), swing (relative to the bar), humanize (the inherited position, velocity and length variance) and automation overrides, and queue note-on/note-off pairs in a fixed 256-entry queue.
4. Hand due notes to the processor. Note-offs are delivered before note-ons at the same sample, so a retriggered note is never cut short. Stopping the transport flushes every pending note-off.

The random seed changes whenever the transport starts or jumps backwards, unless the project pins one.

## Threading

- **Project state.** `SeqDataBuffer` holds two `SequenceData` copies (about 2.4 MB each). The editor edits the UI copy and calls `processor.publishEdit()`, which flips the published index, waits for any block still reading the old copy (the audio thread announces the copy it reads in `AudioReadScope`) and then refreshes the old copy. The audio thread never sees a half-written pattern and never waits.
- **Playhead.** The engine publishes a `GgdPlayheadSnapshot` (anchor, shape, block position, tempo, wall-clock time) through a seqlock. The editor extrapolates the position to the current frame with the same timeline maths.
- **Played-note feedback.** A single-producer single-consumer FIFO carries the notes that were actually scheduled (after mute and probability), with the wall-clock time they are due. Feedback can drop events if the UI falls behind; it can never block the audio thread.
- **Automation.** Overrides from host parameters are atomics read by the engine.
- **Audition.** Row-name clicks send note on/off through a small FIFO into the next block.

## Persistence

Three layers stay distinct:

1. **Project state** (saved by the host): every slot's events and geometry, names, kit, sync mode, mute and the inherited Stochas settings. `GgdEventPersist` writes the events (version 1, with a `sync` attribute); `Persist` writes the inherited XML and migrates alpha-era grid cells of GGD-tagged layers into events on load.
2. **Native patterns** (`.sggdp`): semantic, kit-independent snapshots of one slot.
3. **MIDI grooves**: external `.mid` files imported through semantic source mapping.

Pattern names are UTF-8 and are truncated on character boundaries.

## MIDI import and export

Import is a two-stage mapping, `source note -> semantic articulation -> event`, never source note to destination note directly. Unknown pitches are reported rather than guessed (experimental GGD source pitch 85 stays unresolved on purpose). Positions and velocities keep their 960 PPQ timing.

Export (file or drag-out) runs the destination half, `articulation -> active kit binding -> note`, at native resolution with meter, velocity and duration. Unmapped hits are counted and skipped. Standard MIDI has no probability, so every mapped hit is written.

## Editor

`GgdDrumEditor` owns the window: header, toolbar, context strip, library panel and settings sheet. It is the only place that turns UI actions into model edits:

- **Commit and history.** Every completed edit ends in `commitSlotEdit()`: publish to the audio thread, then compare the slot with its last committed state. If it changed, the old state goes onto that slot's undo stack (64 steps). States hold geometry, events, name and parked hits. The selection and the library file the slot was loaded from are stored alongside and restored with undo/redo but don't count as changes. Each of the eight slots has its own history.
- **Selection memory.** While nothing is pending, selection changes update the committed state's selection, so undo brings back exactly what was selected before an edit.
- **Frames.** A `VBlankAttachment` drives `renderFrame()` (playhead, feedback, follow, keyboard polling) at the display rate; a 10 Hz timer takes over if vertical blanks stop arriving.
- **Follow.** While the transport runs and the pattern is wider than the view, the view keeps the playhead centred and pads the end so the last bar can reach the centre. A manual scroll pauses following for 2.5 seconds.
- **Preferences** live in a `GgdPreferenceStore` shared by all open windows (same file as 1.x, so settings carry over). Layout choices (zoom, grid, library visibility, window size) are per window; everything in the settings sheet is shared live.

`GgdDrumGrid` is the timeline component inside a `Viewport`. It edits the current slot directly, publishes live during gestures so you hear the changes, and calls back once per completed gesture (one undo step per paint stroke or drag). The articulation column and the ruler are painted at the viewport's scroll position, so they stay in place. Painting is clipped to the visible area, and the playhead repaints only the strips it moved through unless feedback is animating.

Keyboard shortcuts are handled from key events whenever the host delivers them. Any shortcut key a host keeps to itself is polled while the timeline has keyboard focus. A press that was already handled by polling is not handled again when its key event arrives late.

`GgdUiTheme` defines four palettes with semantic roles, instrument colours, the embedded Inter fonts, a small vector icon set, `GgdButton` (icon, style, segmented edges) and `GgdLookAndFeel`.

## Testing

`stochas_core_tests` (engine, timeline, pattern operations, persistence) needs no UI. `stochas_tests` adds processor tests (state round-trip, meter changes through a fake host play head, passthrough, stop), editor tests (history, parked hits, per-slot undo, rename, duplicate, kits, shortcuts) and simulated mouse editing. Engine tests compare every note-on against an analytically computed expectation: each expected hit exactly once, within a sample, and nothing else.

`stochas_tests --snapshot <folder>` renders the editor offscreen in representative states (draw, select, settings, playback, every theme, odd meters, hidden hits, narrow window, mid-gesture) for visual review.

## CI and releases

Pull requests build the Windows CLAP (downloadable artifact) and run the tests on Linux. The release workflow reads `RELEASE_VERSION`, builds the Windows CLAP from `main` and publishes `Stochas.GGD.clap` plus a versioned ZIP, with `RELEASE_NOTES.md` as the release text.

## Possible next steps

1. A velocity and probability lane for dense editing.
2. Drum-aware transforms such as hat openness and choke pairing, double-kick alternation.
3. Tags or favourites in the library.
4. User-editable kit maps.
5. Keeping undo history in the processor so it survives closing the editor.
