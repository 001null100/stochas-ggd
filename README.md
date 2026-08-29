# Stochas GGD

A Windows-first CLAP note-effect drum sequencer for programming GetGood Drums patterns directly in Bitwig Studio.

**Signal chain:**

`Stochas GGD -> Kontakt / GGD -> audio`

Stochas GGD started from the open-source Stochas sequencer engine, but the active workflow is purpose-built around semantic GGD articulations, an event timeline, groove/pattern libraries and fast drum editing.

## Download

Use the **Releases** page in this repository. Versioned releases publish:

- `Stochas.GGD.clap` for direct installation
- `Stochas-GGD-<version>-Windows-x64.zip`

The plugin is developed and tested primarily as a **CLAP note effect in Bitwig on Windows**.

## Core features

- Event-based drum patterns at **960 PPQ** internal resolution.
- Independent straight and triplet hits.
- Zoom-aware straight and triplet editing grids.
- Per-pattern bar count and time signature across eight pattern slots.
- Smooth interpolated playhead with optional glow and centre-follow navigation.
- Follow scrolling only activates when the musical timeline is wider than the visible canvas.
- Live MIDI passthrough so the downstream GGD instrument remains playable.
- Project-persistent **SEQ ON / SEQ OFF** for muting generated pattern notes while leaving Bitwig/live MIDI passthrough active.
- Semantic articulation storage translated through the active GGD kit map.
- Active-kit row layouts only show articulations that exist in that library, in that library's own order.
- Click articulation names to audition mapped notes through the downstream instrument.
- High-resolution GGD MIDI groove import with velocity and performance timing retained.
- Native `.sggdp` semantic pattern files portable between supported GGD libraries.
- Grooves and Patterns browser with persistent roots and live filtering.
- Draw and Select tools, marquee selection, copy/paste, move, duplicate, nudge, velocity and timing editing.
- Cell-centred hit presentation with cell-based Draw targeting.
- Resolution-independent right-click erase.
- Interpolated paint drag so fast gestures do not skip subdivisions.
- Shift-drag velocity editing and Alt-drag free timing editing.
- Live velocity/timing value bubbles, including multi-note velocity feedback.
- Alt-double-click quantize for a single hit.
- Ghost, Accent, Humanize, Flam and Double actions.
- Rows / Fill / Mirror / Ramp+ / Ramp- / Dyn- / Thin / Dyn+ selection transforms.
- Per-hit probability presets.
- Undo/redo restores both pattern state and the corresponding selection.
- Native MIDI drag-out for dropping the current pattern directly into Bitwig.
- File-based MIDI export remains available from the Pattern menu as a fallback.
- Played-note glow, ripple and lane feedback driven by notes the engine actually schedules.
- Four persistent UI themes and an expanded Settings panel.

## Built-in GGD mappings

Current built-in mappings cover:

- GetGood Drums **P V**
- GetGood Drums **P IV**
- GetGood Drums **Modern & Massive**

Patterns are stored semantically. A kick event is stored as a kick articulation rather than only as a destination MIDI note. Switching kit maps changes destination bindings without rewriting the musical pattern.

The visible grid belongs to the active kit. P V therefore only displays the instruments and articulations present in P V, while P IV and Modern & Massive retain their own group structure and ordering. Stable canonical storage indices remain unchanged underneath this presentation layer.

## Basic Bitwig setup

1. Put `Stochas.GGD.clap` somewhere Bitwig scans for CLAP plugins.
2. Add Stochas GGD as a **Note FX** device before Kontakt / your GGD instrument.
3. Choose the matching GGD kit mapping in Stochas GGD.
4. Draw or load a groove and start playback.

The plugin outputs mapped MIDI notes to the downstream instrument and passes live controller/timeline MIDI through.

## Editing workflow

In **Draw** mode, the visible space between timing lines is treated as a grid cell. Clicking inside a cell edits the event on that cell's leading musical timestamp while the note body is drawn visually in the middle of the cell.

Dragging paints every crossed subdivision. Right-click is always destructive and can erase actual hit bodies regardless of the currently selected straight/triplet lattice.

In **Select** mode:

- Shift-click toggles a hit in the selection.
- Shift-drag adjusts velocity.
- When several hits are selected, relative velocity differences are preserved and each affected hit displays its live velocity during the gesture.
- Alt-drag moves timing freely in ticks and displays signed timing offset.
- Alt-double-click quantizes one hit to the nearest active subdivision.
- Undo and redo restore the selection belonging to the historical pattern state.

### Selection transforms

The context strip focuses on operations that are awkward to reproduce manually:

- **Rows**: select all hits on the instrument rows represented by the current selection.
- **Fill**: fill missing current-grid subdivisions between selected endpoints.
- **Mirror**: reflect selected timing around the temporal centre of the selection.
- **Ramp+ / Ramp-**: create rising or falling velocity contours.
- **Dyn- / Dyn+**: compress or expand velocity differences around the selection average.
- **Thin**: remove every second selected event independently on each selected articulation row.

Copy/paste is the phrase repetition workflow: `C` copies the current selection and `V` pastes after it. The pasted copy becomes the new selection so repeated `V` presses chain naturally.

## Playhead and navigation

With playhead following enabled, only horizontal timeline scrolling follows playback. Vertical articulation scrolling is preserved.

If the complete musical timeline already fits inside the visible timeline canvas, following is disabled because there is nothing useful to scroll. Once the timeline exceeds the viewport, playback is kept around the visible timeline centre, with the normal left-edge constraint at the start of the pattern.

Smooth mode reconstructs visual movement between coarse host position notifications at a higher UI update cadence. This only affects presentation.

## MIDI import, drag-out and export

MIDI import maps source notes to semantic articulations and retains event timing and velocity on the 960 PPQ timeline.

The permanent top-bar MIDI action is **Drag MIDI**. Begin dragging from it and Stochas GGD generates a temporary high-resolution `.mid` file using the active GGD destination map, then starts a native Windows file drag. This workflow is confirmed working for dropping patterns into Bitwig.

Once a dragged clip is placed on the Bitwig timeline, use **SEQ OFF** to prevent the internal pattern from doubling the timeline clip. Incoming/timeline MIDI continues to pass through to the downstream drum instrument.

A conventional **Export MIDI file...** command remains under the Pattern menu as a fallback.

**Probability is not written to ordinary MIDI files.** Standard MIDI note events have no native probability property, so exported or dragged MIDI contains every mapped hit. Probability remains part of the native Stochas GGD pattern.

## Played-note feedback

Playback effects are driven by compact notifications from events that were actually scheduled by the sequencer. Probability-skipped and muted hits therefore do not flash falsely.

Available effects include:

- played-hit glow
- expanding ripple
- articulation-lane flash
- velocity-reactive intensity
- configurable strength and decay
- Reduce Motion

The notification path is bounded and lock-free. Animation remains on the UI thread and cannot block MIDI scheduling.

## Settings

The compact `...` button opens the in-plugin Settings panel. Editor preferences are local workstation settings and do not modify pattern data.

Available options include:

- Theme: Graphite, Midnight, Ember or Contrast.
- Centre-follow playhead navigation.
- Smooth playhead interpolation.
- Playhead forward glow.
- Automatically use a finer grid at high zoom.
- Shift-hover velocity inspection.
- Articulation-name audition.
- Played-hit feedback enable/disable.
- Hit ripple.
- Lane flash.
- Velocity-reactive effect intensity.
- Effect strength.
- Effect decay.
- Reduce Motion.

Settings apply live and can be closed with **Done**, **Escape**, or by clicking outside the panel.

## Pattern and groove libraries

The right-side browser has separate roots for:

- **Grooves:** `.mid` / `.midi`
- **Patterns:** `.sggdp`

Folder locations are remembered locally. Double-click a file to load it. The filter searches filenames and relative paths.

## Release status

`v1.0.0` is the first full release. Native MIDI drag-out has been validated in Bitwig, and the final stabilization pass adds output muting, played-note feedback, expanded configuration and targeted inherited-state fixes without redesigning the proven host-PPQ scheduling model.

A rare experimental GGD Groove Player source pitch (`85`) remains intentionally unresolved rather than being assigned to an uncertain semantic articulation.

## Building on Windows

Clone with submodules, then build the CLAP target:

```powershell
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSTOCHAS_VERSION=1.0.0
cmake --build build --target stochas_CLAP --parallel
```

The GitHub Actions workflows in `.github/workflows/` build the same Windows CLAP target for pull requests and versioned releases.

## Architecture

See [`docs/architecture.md`](docs/architecture.md) for the product model, semantic mapping strategy, persistence layers and design direction.

## Upstream

This project is derived from the open-source **Stochas** sequencer. The inherited engine/source remains under its original licensing terms; Stochas GGD adds the GGD-specific event engine, semantic mapping, editor, library and workflow layers used by this fork.
