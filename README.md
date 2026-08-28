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
- Automatic editing resolution based on zoom:
  - straight mode: `1/16` normally, `1/32` at 350%+ zoom
  - Triplet mode: `1/8T` normally, `1/16T` at 350%+ zoom
- Per-pattern bar count and time signature across eight internal pattern slots.
- Smooth interpolated playhead and host transport sync.
- Live MIDI passthrough so the instrument remains playable through the note effect.
- Semantic drum rows that are translated through the active GGD kit map.
- High-resolution GGD MIDI groove import with velocity and performance timing retained.
- Native `.sggdp` semantic pattern files that remain portable between supported GGD libraries.
- Grooves and Patterns browser with persistent roots, loaded-item indication and live filtering.
- Draw and Select tools, marquee selection, copy/paste, move, duplicate, nudge, velocity and timing editing.
- Ghost, Accent, Humanize, Quantize, Flam and Double performance actions.
- Per-hit probability editing.
- High-resolution MIDI export through the active destination kit mapping.
- Undo/redo for pattern editing.
- Four persistent UI themes with a shared readability-first timing hierarchy.

## Appearance and timeline hierarchy

Beta 4 treats timing hierarchy as part of the editor model rather than incidental decoration. Every theme has dedicated semantic colours for:

1. **Bar boundaries**: strongest, full-height dividers.
2. **Beat boundaries**: full-height but clearly secondary.
3. **Primary subdivisions**: quieter guides for the active straight or triplet grid.
4. **Fine subdivisions**: the quietest layer, shown only at high zoom.
5. **Instrument-family headers**: dedicated bands and separators distinct from ordinary articulation rows.

The playhead has its own colour instead of reusing the hit accent, and selected hits use a separate high-contrast outline/glow so transport, content and selection remain easy to distinguish.

Current themes:

- **Graphite**: neutral dark teal default.
- **Midnight**: cool navy/cyan.
- **Ember**: warm dark amber.
- **Contrast**: maximum separation for low-light and accessibility-focused use.

Theme choice is stored locally as an appearance preference and does not alter project or pattern data.

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
4. Draw or import a groove and start playback.

The plugin outputs MIDI notes to the downstream instrument and passes live controller MIDI through.

## Pattern and groove libraries

The right-side browser has separate roots for:

- **Grooves:** `.mid` / `.midi` files
- **Patterns:** Stochas GGD `.sggdp` files

Folder locations are remembered locally. Double-click a file to load it. The filter field searches filenames and relative folder paths; clearing the filter returns to the normal collapsible folder tree. The browser follows the active UI theme and keeps loaded and selected rows visually distinct.

## MIDI import and export

MIDI import maps source notes to semantic articulations and then into the current Stochas GGD pattern. Exact event timing and velocity are retained on the 960 PPQ timeline.

MIDI export performs the reverse destination step using the currently selected GGD kit map and writes note timing, velocity and duration at the pattern PPQ resolution.

**Probability is not written to ordinary MIDI files.** Standard MIDI note events do not have a native probability property, so exported MIDI contains every mapped hit. Probability remains part of the native Stochas GGD pattern.

## Current beta boundaries

Stochas GGD is intentionally a sequencer/note effect. It does **not** contain an internal sampler, mixer, effects rack or song arranger; Kontakt/GGD and Bitwig already do those jobs better.

A rare experimental GGD Groove Player source pitch (`85`) remains intentionally unresolved rather than being assigned to an uncertain semantic articulation.

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
