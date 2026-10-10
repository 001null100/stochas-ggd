# Stochas GGD

A drum sequencer plug-in (CLAP note effect) for programming GetGood Drums patterns directly in your DAW. It is developed and tested mainly in Bitwig Studio on Windows.

**Signal chain:**

`Stochas GGD -> Kontakt / GGD -> audio`

Stochas GGD started from the open-source Stochas sequencer. The current product is built around semantic GGD articulations, an event timeline, groove and pattern libraries, and fast drum editing.

**New in 2.0:** time-signature changes no longer throw patterns off the bar grid, plus a rebuilt editor and a new look. See [RELEASE_NOTES.md](RELEASE_NOTES.md).

## Download

Use the **Releases** page of this repository. Each release publishes:

- `Stochas.GGD.clap` for direct installation
- `Stochas-GGD-<version>-Windows-x64.zip`

## Features

- Event-based drum patterns at **960 PPQ**: straight, triplet and off-grid hits side by side.
- Eight pattern slots, each with its own time signature (1-32 over 2, 4, 8 or 16) and bar count.
- **Host-bar sync:** every pattern bar starts on a host bar line, so host time-signature changes never shift the groove. *Free run* is available for polymeter.
- Meter and bar changes keep hits in their bars. Hits that don't fit are hidden and come back if the pattern grows again.
- Semantic storage: hits are stored as articulations (`snare.hit`, `hihat.open_2`...) and the active kit map decides the MIDI note.
- Built-in maps for GGD **P V**, **P IV** and **Modern & Massive**. The grid shows only the active kit's articulations, in its own order.
- Draw and Select tools: paint, erase, box select, move, duplicate, nudge, velocity and timing editing.
- Ghost, Accent, chance (probability), Quantize, Humanize, Flam and Double, plus ramps, dynamics, fill, mirror, thin and row selection.
- Undo/redo per pattern slot that also restores the selection.
- GGD MIDI groove import with velocity and performance timing kept; native `.sggdp` pattern files that move between kits.
- Grooves and Patterns library with live filtering.
- Native **MIDI drag-out** of the current pattern, plus MIDI file export.
- Live MIDI passthrough and a per-project **Seq On / Seq Off** switch.
- Smooth playhead, follow scrolling for long patterns, and played-note feedback driven by the notes the engine actually plays.
- Four themes: Graphite, Midnight, Ember and Contrast.

## Setup in Bitwig

1. Put `Stochas.GGD.clap` where Bitwig scans for CLAP plug-ins (on Windows `C:\Program Files\Common Files\CLAP\`).
2. Add Stochas GGD as a **Note FX** before Kontakt / your GGD instrument.
3. Pick the matching kit in the toolbar (P V, P IV or Modern & Massive).
4. Draw a pattern or double-click a groove in the library, then start the transport.

Stochas GGD sends mapped notes to the instrument and passes your own MIDI (controllers, clips) through.

## The window

- **Header:** pattern slots 1-8, the pattern name, the *Pattern* menu, the transport display (bar.beat and host meter), *Seq On/Off*, *Drag MIDI*, the library toggle and settings.
- **Toolbar:** Draw/Select, undo/redo, meter, bars, grid, zoom, kit and sync mode.
- **Timeline:** articulation rows grouped by instrument. Click a group header to fold it; click a row name to audition its note.
- **Context strip:** tools for the current selection, or the mouse gestures of the current tool.
- **Library:** grooves (`.mid`) and patterns (`.sggdp`).

## Time signatures and sync

Each slot has its own meter and bar count. Change them in the toolbar; hits keep their bar and their position in the bar. If a hit no longer fits (a 4/4 bar becomes 3/4, or bars are removed), it is hidden rather than deleted. The toolbar shows how many are hidden, and they come back when the pattern grows again. Undo works across all of this.

*Sync* (per project) decides how patterns line up with the host:

- **Host bars** (default): bar 1 of the pattern starts on a host bar line, and multi-bar patterns follow the host's bar number. When the host changes meter, the next host bar simply starts the next pattern bar. If a pattern bar is shorter than the host bar it rests until the next bar line; if it is longer it is cut short. The transport display warns when the meters differ.
- **Free run**: the pattern loops on its own from the song start, regardless of host bar lines. Use it for deliberate polymeter. This is how 1.x behaved.

## Editing

### Draw tool

| Gesture | Action |
| --- | --- |
| Click an empty cell | Add a hit |
| Drag | Paint every cell you cross |
| Click a hit / drag from a hit | Remove it / erase along the row |
| Right-drag | Erase |
| Shift-drag a hit | Velocity |
| Alt-drag a hit | Timing, in ticks (off the grid) |
| Alt-double-click a hit | Snap it to the grid |

### Select tool

| Gesture | Action |
| --- | --- |
| Drag on empty space | Box select (Shift or Ctrl adds) |
| Click / Ctrl-click a hit | Select / toggle |
| Drag the selection | Move (Alt-drag duplicates) |
| Shift-click a hit | Add it to or remove it from the selection |
| Shift-drag a hit | Velocity of the whole selection |
| Right-click | Context menu |

A move preview turns red if the drop would land on another hit or leave the pattern.

### Keyboard

| Keys | Action |
| --- | --- |
| D / S | Draw / Select tool |
| Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y | Undo, redo (Z and Y alone also work) |
| Ctrl+A | Select all |
| Ctrl+C, Ctrl+V | Copy; paste after the selection (repeat V to chain) |
| Ctrl+D | Duplicate the selection after itself |
| Q | Quantize the selection |
| Delete / Backspace | Delete the selection |
| Esc | Clear the selection |
| Arrows | Move the selection one grid step or one row |
| Alt+Arrows | Duplicate one step or row over |
| Shift+Up / Shift+Down | Velocity +5 / -5 |
| Shift+Left / Shift+Right | Timing -10 / +10 ticks |
| Ctrl+mouse wheel | Zoom |

On macOS use Cmd instead of Ctrl.

### Grid and zoom

The grid (1/8, 1/16, 1/32, 1/8T, 1/16T, 1/32T) controls painting, nudging and quantize. It never changes stored positions. With *Finer grid when zoomed in* on, the grid halves at 350% zoom. Zoom with the slider, Ctrl+wheel or a trackpad pinch; *Fit* shows the whole pattern.

## Pattern slots

Click a slot to edit it; this also makes it the pattern that plays (pattern automation can still override it). Dots mark slots with content; a bar under a slot shows the one the engine is playing. Right-click a slot to open it, copy the current pattern into it, or clear it. Each slot keeps its own undo history while the window is open.

The *Pattern* menu has: duplicate to an empty slot, double length, save to the pattern library, import a MIDI groove, export a MIDI file, and clear.

## MIDI import, drag-out and export

Imported grooves are mapped to semantic articulations and keep their timing and velocity at 960 PPQ. Loading a groove or pattern replaces the current slot and can be undone.

Drag from **Drag MIDI** to drop the current pattern into your DAW as a MIDI clip, written with the active kit's notes. Once the clip is on the timeline, switch **Seq Off** so the internal pattern doesn't double it; your MIDI still passes through. *Pattern > Export MIDI file* is the fallback.

**Chance (probability) is not written to MIDI files.** Standard MIDI has no probability field, so exported or dragged MIDI contains every mapped hit.

## Played-note feedback

Hits light up as the sequencer plays them: lane flash, streak, bloom, impact flare and ripples, tinted by instrument. Feedback follows the notes that were really scheduled, so muted hits and hits skipped by their chance setting stay dark. Strength, decay and each effect can be set in Settings, and *Reduce motion* keeps the feedback static.

## Settings

The gear button opens Settings. Preferences belong to the computer and apply to every Stochas GGD window:

- Theme (Graphite, Midnight, Ember, Contrast) and colour-by-instrument
- Follow the playhead, smooth playhead motion, playhead glow
- Finer grid when zoomed in, Shift to inspect velocity, click a row name to audition
- Played-note feedback: on/off, ripple, lane flash, scale with velocity, reduce motion, strength and decay

Sync mode and Seq On/Off are saved with each project.

## Libraries

The library panel has two tabs, **Grooves** (`.mid`, `.midi`) and **Patterns** (`.sggdp`), each with its own root folder, remembered on this computer. Double-click a file to load it into the current slot. The filter matches file and folder names. Hide the panel with the folder button in the header.

## Building

Clone with submodules first:

```
git submodule update --init --recursive
```

Windows (Visual Studio developer prompt):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target stochas_CLAP --parallel
```

Linux needs the usual JUCE dependencies (ALSA, JACK, FreeType, fontconfig, X11 development packages):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target stochas_CLAP stochas_VST3 stochas_Standalone
```

### Tests and snapshots

```bash
cmake -S . -B build -G Ninja -DSTOCHAS_BUILD_TESTS=ON
cmake --build build --target stochas_tests
build/stochas_tests                        # engine, persistence, processor and editor tests
build/stochas_tests --snapshot snapshots   # renders the editor to PNG files
```

On a headless Linux machine run them under `xvfb-run`. `stochas_core_tests` runs only the engine, timeline and persistence tests, without UI code.

The GitHub Actions workflows in `.github/workflows/` build the Windows CLAP for pull requests and releases, and run the tests on Linux for pull requests.

## Architecture

See [`docs/architecture.md`](docs/architecture.md) for the data model, the timing model, threading and the editor structure.

## Upstream

This project is derived from the open-source **Stochas** sequencer. The inherited source remains under its original licensing terms (GPL v3, see `COPYING`). Stochas GGD adds the GGD-specific event engine, semantic mapping, editor, library and workflow layers. The Inter typeface is embedded under the SIL Open Font License (`resources/fonts/OFL.txt`).
