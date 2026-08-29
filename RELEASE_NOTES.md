# Stochas GGD v1.0.0-rc.1

This is the final functional release candidate for Stochas GGD 1.0. It focuses on predictable editor state, kit-correct layouts and the last missing Bitwig workflow: dragging the current pattern out as MIDI directly from the plug-in.

The audio scheduler, 960 PPQ event model, project persistence format and semantic GGD mapping remain unchanged.

## Smarter playhead following

Centre-follow now activates only when the **musical timeline is wider than the visible timeline canvas**.

- Short patterns that already fit remain completely stationary while playing.
- Longer patterns retain centre-locked following.
- The temporary trailing runway is only added when following is actually necessary.
- Resizing or zooming until the pattern fits removes that runway and returns the timeline to its natural position.

## Selection now follows the result of an action

Addition actions hand control to the notes they created instead of leaving the source notes mixed into the selection.

- **Flam** selects only the new grace hits.
- **Double** selects only the new follow-up hits.
- **Fill** selects only the newly inserted subdivisions.
- Paste continues to select the new copy, preserving the `C`, then repeated `V` phrase-repeat workflow.

Transforms that modify existing notes, such as Mirror, Humanize and velocity transforms, keep their transformed result selected.

## Undo and redo preserve selection

Pattern history now remembers editor selection alongside pattern content.

Selection state is keyed to the pattern snapshot fingerprint, so undo/redo can restore the notes that were selected at that historical state without changing the existing pattern-history format. Selection callbacks are deliberately ignored during the destructive snapshot restore and reapplied afterward, avoiding the old behavior where undo always returned with nothing selected.

## Multi-note velocity feedback

Shift-dragging the velocity of multiple selected hits now displays a compact live velocity chip above **each affected hit**. Relative velocity differences are still preserved. The chips remain briefly after release so the final values can be read without permanently cluttering the grid.

Single-note velocity and free-timing bubbles remain unchanged.

## Active kit layouts are now exact

The editor no longer displays the union of every articulation used by every bundled GGD library.

Each selected kit now builds its visible rows from that kit's own group and articulation order, then resolves those semantic IDs back to the existing canonical storage rows. This means:

- P V only shows articulations that actually exist in P V.
- P IV keeps its own group/articulation order.
- Modern & Massive keeps its own group/articulation order.
- Stored pattern row indices and semantic persistence are unchanged.
- Collapsed-group state is respected without forcing unnecessary layout rebuilds.

## Drag MIDI directly into Bitwig

The permanent **Export MIDI** button is replaced by **Drag MIDI**.

Dragging that control creates a temporary high-resolution `.mid` file using the active GGD map and starts JUCE's native Windows file drag. The temporary file is cleaned up when the native drag completes.

The intended workflow is simply:

1. program/select the current pattern as usual
2. drag **Drag MIDI** from Stochas GGD
3. drop it into Bitwig where a MIDI clip/file is accepted

The previous file-save export remains available under **Pattern > Export MIDI file...** as a fallback.

This RC verifies that the native drag path compiles and packages correctly. Acceptance by Bitwig's drop target must still be validated in the actual host before the identical code is promoted to final 1.0.

## Settings polish

Settings retains Theme, Follow Playhead, Smooth Playhead, Playhead Glow and Auto Fine Grid, and adds:

- **Shift-hover velocity inspector** toggle
- **Articulation click audition** toggle

The modal can now be closed with **Done**, **Escape**, or by clicking the dimmed area outside the settings panel.

## Editing workflow retained

The RC keeps the stabilized Beta 8 workflow:

- cell-centred notes and cell-based Draw targeting
- right-click erase, including off-grid straight/triplet events
- interpolated paint drags across dense subdivisions
- Shift-hover velocity inspection
- Shift-click multi-selection and Shift-drag velocity editing
- Alt-drag free timing and Alt-double-click quantize-to-current-subdivision
- articulation-name audition
- group collapse only from the sticky left articulation panel
- reliable Bars text input
- 32-character pattern names
- chained keyboard copy/paste repetition
- Rows / Fill / Mirror / Ramp+ / Ramp- / Dyn- / Thin / Dyn+ transforms
- Ghost / Accent / probability / Humanize / Flam / Double performance actions
- indexed Grooves / Patterns browser
- semantic `.sggdp` pattern storage

## Engine intentionally unchanged

v1.0.0-rc.1 does **not** modify:

- host-PPQ event scheduling
- event probability or duration playback
- transport discontinuity handling
- 960 PPQ event storage
- project persistence format
- semantic kit-map storage
- live MIDI passthrough

## Remaining 1.0 gate

The only new behavior that CI cannot validate is the external drop into Bitwig itself. If the `Drag MIDI` control drops a usable MIDI clip/file into Bitwig and this RC exposes no new editor regressions, the code is ready to be promoted to `v1.0.0` without another feature pass.

## Release-candidate checks

The exact RC must compile and package successfully in the Windows PR workflow before merge. The release is complete only when GitHub Releases contains both:

- `Stochas.GGD.clap`
- `Stochas-GGD-v1.0.0-rc.1-Windows-x64.zip`
