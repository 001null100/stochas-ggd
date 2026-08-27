# Stochas GGD v0.1.0-alpha.8

Alpha.8 removes the old short-pattern ceiling and replaces the heuristic GGD MIDI importer with the experimentally reconstructed Groove Player translation path.

## Larger pattern engine

- Expanded the inherited sequence capacity from 256 to 1024 sixteenth-note storage steps.
- Widened chain-source step indices and completed-note notification packing so positions beyond step 255 are represented correctly.
- Pattern length is now entered directly as a bar count instead of selected from a fixed dropdown.
- The maximum bar count is derived from the current meter and engine capacity. At 4/4, the editor can hold up to 64 bars.
- Time-signature numerators can now be set from 1 through 32. Denominators remain 4, 8 and 16 so every bar maps exactly to the current 1/16 storage grid.

## Exact GGD Groove Player import

- Removed the Auto/P V/P IV/Modern & Massive source-profile selector and its pitch-coverage guessing.
- Official GGD Groove Player MIDI is now translated through the pitch mappings reconstructed from the remapping experiment, then resolved into the selected destination kit's semantic articulation rows.
- Dedicated translation tables are provided for P V, P IV and Modern & Massive.
- Rare Groove Player articulations that are not exposed exactly by P V or P IV use conservative documented fallbacks rather than silently disappearing.
- Experimental source pitch 85 remains deliberately unresolved because its observed remap output was ambiguous. The importer reports it instead of inventing a drum articulation.
- Import continues to preserve velocity and sub-1/16 timing using Stochas velocity and offset data, and reports unresolved notes, fallbacks, retrigger collisions and truncation.
- Imported meter and bar count can use the expanded engine capacity, so long GGD grooves are no longer restricted to eight bars.

## Current limitations

- Time-signature changes inside a single MIDI file are not supported yet.
- Denominators other than 4, 8 and 16 are not represented by the current 1/16 storage model.
- Pattern meter and length are inherited Stochas layer geometry, so changing them still affects the shared geometry of all pattern slots on the layer.
- The high-zoom 1/32 editing view still uses the existing offset/retrigger representation rather than a native 1/32 storage grid.
- Ctrl+Z remains dependent on whether the host forwards it; U is the reliable plugin-local undo shortcut.

The existing Draw/Select workflow, marquee editing, velocity popup, smooth playhead, semantic destination maps, MIDI passthrough, ghost notes, rolls and microtiming remain intact.
