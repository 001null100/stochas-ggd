# Stochas GGD v0.1.0-alpha.9

Alpha.9 is a workflow and library-browser pass. Grooves and reusable native patterns now live beside the editor, while selection editing, history and kit navigation are substantially more practical.

## Groove and pattern browser

- Added a persistent right-side browser with separate `Grooves` and `Patterns` tabs.
- Each tab has its own configurable root folder, remembered between sessions.
- Folder hierarchies are shown recursively and sorted naturally, so existing GGD pack organization can be used directly.
- Double-click a MIDI groove to load it through the exact GGD Groove Player importer introduced in alpha.8.
- Double-click a native Stochas GGD pattern to replace the current pattern.
- Replacement is immediate when the current pattern is unchanged since its last load/save baseline. A confirmation dialog appears only when there are actual edits to protect.
- The Patterns tab can save the current pattern directly into the selected library and refreshes after saving.

## Native semantic pattern files

- Added the `.sggdp` native pattern format.
- Native patterns store semantic articulation IDs rather than one kit's raw MIDI pitches, making them portable across the built-in P V, P IV and Modern & Massive destination maps.
- Pattern files retain pattern name, meter, bar count, velocity, probability, retrigger/length and microtiming offset data.

## Editing workflow

- Added editor-level multi-step undo/redo history with a 24-state working depth.
- `U` remains the reliable plugin-local Undo shortcut and `Y` is the local Redo shortcut. The visible Undo/Redo buttons do not depend on Bitwig forwarding host shortcuts.
- Added selection copy/paste while preserving velocity, probability, retrigger and timing data.
- Paste searches to the right for a collision-free destination instead of destructively overwriting existing hits.
- The contextual bottom strip now exposes selection count, Copy, Paste, relative velocity adjustment, timing adjustment, Humanize and Delete when useful instead of showing one giant static shortcut sentence.
- Humanize applies a small per-hit velocity and timing variation to the current selection.
- Existing Alt-drag and Alt+Arrow duplication remain available for fast duplication while moving.

## Pattern-slot safety

- Added a Pattern actions menu for duplication, saving to the native pattern library and clearing the current slot.
- Duplicate now searches for an empty internal pattern slot and never silently overwrites an occupied one.
- Pattern naming remains directly editable in the top bar.

## Drum-lane organization

- Articulations are now presented in a stable drum-family order: Kick, Snare, Toms, Hi-Hat, Ride, Crashes, China, Splashes and Other.
- Family headers can be collapsed or expanded by clicking them, which makes larger kits much easier to navigate.
- Collapse indicators use plain ASCII glyphs to avoid the Windows font-fallback issue seen in earlier builds.

## Existing alpha.8 foundation

- The 1024-step engine, direct numeric bar entry, larger time signatures and exact GGD Groove Player translation path remain intact.
- The high-zoom 1/32 editor still uses the existing offset/retrigger representation rather than native 1/32 storage.
- Meter and pattern length remain inherited layer geometry, so those values are still shared by the eight internal pattern slots.
