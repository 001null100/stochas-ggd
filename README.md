# Stochas GGD

A Windows-first CLAP note-effect drum sequencer for programming GetGood Drums patterns directly in Bitwig Studio.

**Signal chain:**

`Stochas GGD -> Kontakt / GGD -> audio`

Stochas GGD started from the open-source Stochas sequencer engine, but the active drum workflow is now purpose-built around semantic GGD articulations, an event-based timeline, groove/pattern libraries and fast drum editing.

## Download

Use the **Releases** page in this repository. Each beta release publishes:

- `Stochas.GGD.clap` for direct installation
- a versioned Windows x64 ZIP containing the same CLAP build

The plugin is currently developed and tested primarily as a **CLAP note effect in Bitwig on Windows**.

## What it does

- Event-based drum patterns at **960 PPQ** internal resolution.
- True independent straight and triplet hits rather than 1/16 cells with timing hacks.
- Zoom-aware editing resolution:
  - straight mode: `1/16`, optionally switching to `1/32` at 350%+ zoom
  - Triplet mode: `1/8T`, optionally switching to `1/16T` at 350%+ zoom
- Per-pattern bar count and time signature across eight internal pattern slots.
- Smooth interpolated playhead, optional centre-locked follow and host transport sync.
- Live MIDI passthrough so the instrument remains playable through the note effect.
- Semantic drum rows translated through the active GGD kit map.
- Click an articulation/instrument name to audition its mapped note through the downstream GGD instrument.
- High-resolution GGD MIDI groove import with velocity and performance timing retained.
- Native `.sggdp` semantic pattern files portable between supported GGD libraries.
- Grooves and Patterns browser with persistent roots, loaded-item indication and live filtering.
- Draw and Select tools, marquee selection, copy/paste, move, duplicate, nudge, velocity and timing editing.
- Cell-centred hit presentation and cell-based Draw-mode targeting.
- Draw-mode right-click erasing that can target off-grid/triplet hits regardless of the current snap mode.
- Interpolated paint-drag so fast mouse movement cannot silently jump over intermediate subdivisions.
- Direct Shift-drag velocity and Alt-drag timing editing with transient value readouts above the edited hit.
- Alt-double-click single-hit timing reset by quantizing to the nearest active subdivision.
- Selection transforms for row selection, repeat, timing mirror, velocity ramps, rotation and density thinning.
- Ghost, Accent, Humanize, Flam and Double performance actions.
- Per-hit probability editing.
- High-resolution MIDI export through the active destination kit mapping.
- Pattern names up to 32 visible characters.
- Undo/redo for pattern editing.
- Four persistent UI themes with a shared readability-first timing hierarchy.
- A modal Settings panel for local editor/playhead preferences.

## Settings

The compact `...` button at the right side of the top bar opens an in-plugin modal Settings overlay. These options are local workstation preferences and do not modify project or pattern data:

- **Theme** — Graphite, Midnight, Ember or Contrast.
- **Lock playhead to timeline centre while playing** — horizontally scroll the timeline so playback remains centred once enough content exists to the left.
- **Smooth playhead interpolation** — use higher-cadence visual interpolation between coarse host position notifications.
- **Playhead forward glow** — enable the fading directional glow to the right of the cursor.
- **Automatically use finer grid at high zoom** — toggle the 350% automatic straight/triplet resolution change.

The modal applies changes live, persists them locally, and closes with **Done** or **Escape**.

## Appearance and timeline hierarchy

The editor treats timing hierarchy as part of the visual model rather than incidental decoration. Every theme has dedicated semantic colours for:

1. **Bar boundaries**: strongest, full-height dividers.
2. **Beat boundaries**: full-height but clearly secondary.
3. **Primary subdivisions**: quieter guides for the active straight or triplet grid.
4. **Fine subdivisions**: the quietest layer, only shown when the fine grid is active.
5. **Instrument-family headers**: dedicated bands and separators distinct from ordinary articulation rows.

The playhead has its own colour, crisp timing edge and optional right-side glow. Selected hits use a separate high-contrast outline/glow so transport, content and selection remain easy to distinguish.

Current themes:

- **Graphite**: neutral dark teal default.
- **Midnight**: cool navy/cyan.
- **Ember**: warm dark amber.
- **Contrast**: maximum separation for low-light and accessibility-focused use.

Theme and playhead/editor preferences are stored locally and do not alter project or pattern data.

## Built-in GGD mappings

Current built-in mappings cover:

- GetGood Drums **P V**
- GetGood Drums **P IV**
- GetGood Drums **Modern & Massive**

Patterns are stored semantically. A kick event is a kick articulation, not merely a remembered destination MIDI note. Switching kit maps therefore changes the destination binding without rewriting the musical pattern.

## Basic Bitwig setup

1. Put `Stochas.GGD.clap` somewhere Bitwig scans for CLAP plugins.
2. Add Stochas GGD as a **Note FX** device before Kontakt / your GGD instrument.
3. Choose the matching GGD kit mapping in the plugin.
4. Draw or load a groove from the Grooves browser and start playback.

The plugin outputs MIDI notes to the downstream instrument and passes live controller MIDI through. Clicking an articulation name in the sticky left column also sends that mapped note for quick auditioning.

## Pattern and groove libraries

The right-side browser has separate roots for:

- **Grooves:** `.mid` / `.midi` files
- **Patterns:** Stochas GGD `.sggdp` files

Folder locations are remembered locally. Double-click a file to load it. The filter field searches filenames and relative folder paths; clearing the filter returns to the normal collapsible folder tree. The browser follows the active UI theme and keeps loaded and selected rows visually distinct.

The dedicated top-bar Import MIDI button was removed because the Grooves browser already owns that workflow.

## Editing workflow

In **Draw** mode, the visible space between two timing lines is treated as one grid cell. Left-click anywhere inside the cell to place/toggle its hit. The hit graphic is centered inside that space while the stored event timestamp remains on the cell's leading timing boundary.

Dragging paints every crossed subdivision even when the cursor moves faster than the UI event rate. Right-click is always destructive: click or drag over hits to erase them. Right-click hit-testing follows actual event bodies, so a triplet can be erased while a straight grid is selected and vice versa.

For detailed hit editing:

- **Shift-drag** a hit vertically to adjust velocity. A `VEL` bubble appears above it with the current value.
- **Alt-drag** a hit horizontally to move it freely in ticks. A `TIME` bubble shows its signed offset from the nearest current subdivision.
- **Alt-double-click** a hit to quantize that one hit to the nearest current subdivision.
- In **Select** mode, Shift-click still toggles selection. If the gesture actually becomes a Shift-drag, it adjusts velocity instead; a multi-selection moves together while preserving relative velocity differences.

Because cell centering is presentation-only, the 960 PPQ event timestamp, playback timing, MIDI import/export timing and native pattern persistence are unchanged.

The Bars field explicitly grabs keyboard focus, selects its complete value when focused, and supports click-and-type replacement. Return commits the new count and returns focus to the grid.

### Selection transforms

The context strip reserves permanent buttons for operations that do more than duplicate keyboard shortcuts:

- **Rows** — select all hits on the instrument rows represented by the current selection.
- **Repeat** — copy the selected phrase immediately after its span; the new copy stays selected for repeated chaining.
- **Mirror** — reflect selected timing around the selection's temporal centre.
- **Ramp+ / Ramp-** — create rising or falling velocity contours across the selected hits.
- **Rot < / Rot >** — rotate selected events by one current subdivision while wrapping inside the selection span.
- **Thin** — remove every second selected hit independently on each selected instrument row.

Keyboard shortcuts remain available for the basic editing operations: `A` select all, `C` copy, `V` paste, arrows move/nudge, and Alt+arrows duplicate while moving.

## Playhead and navigation

With **Lock playhead to timeline centre while playing** enabled, only the timeline's horizontal position follows playback. Vertical articulation scrolling stays where you put it.

At the beginning of a pattern the view naturally stays at the left edge. Once enough content exists to the left, the playhead remains fixed at the visible timeline centre while the grid moves underneath it. Temporary trailing scroll space during playback lets the final half-screen remain centred as well; that extra runway disappears when transport stops.

Smooth mode reconstructs display motion between the existing coarse host step notifications and uses a higher visual update cadence. This is a UI interpolation feature only; the audio/event scheduler is unchanged.

## MIDI import and export

MIDI import maps source notes to semantic articulations and then into the current Stochas GGD pattern. Exact event timing and velocity are retained on the 960 PPQ timeline.

MIDI export performs the reverse destination step using the currently selected GGD kit map and writes note timing, velocity and duration at the pattern PPQ resolution.

**Probability is not written to ordinary MIDI files.** Standard MIDI note events do not have a native probability property, so exported MIDI contains every mapped hit. Probability remains part of the native Stochas GGD pattern.

## Current beta boundaries

Stochas GGD is intentionally a sequencer/note effect. It does **not** contain an internal sampler, mixer, effects rack or song arranger; Kontakt/GGD and Bitwig already do those jobs better.

A rare experimental GGD Groove Player source pitch (`85`) remains intentionally unresolved rather than being assigned to an uncertain semantic articulation.

MIDI export is currently file-based. Direct drag-out of a generated MIDI clip into Bitwig is not implemented yet.

The project is still in beta, so project-state compatibility and editor workflows should be treated as evolving until the first stable release.

## Building on Windows

Clone with submodules, then build the CLAP target:

```powershell
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSTOCHAS_VERSION=0.2.0
cmake --build build --target stochas_CLAP --parallel
```

The GitHub Actions workflows in `.github/workflows/` build the same Windows CLAP target for pull requests and versioned releases.

## Architecture

See [`docs/architecture.md`](docs/architecture.md) for the product model, semantic mapping strategy, persistence layers and longer-term design direction.

## Upstream

This project is derived from the open-source **Stochas** sequencer. The inherited engine/source remains under its original licensing terms; Stochas GGD adds the GGD-specific event engine, semantic mapping, editor, library and workflow layers used by this fork.
