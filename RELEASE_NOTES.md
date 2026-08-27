# Stochas GGD v0.1.0-alpha.6

Alpha.6 is an input and selection-workflow cleanup focused on making the editor behave predictably inside Bitwig.

## Selection duplication

- Removed the ambiguous toolbar Duplicate button.
- In Select mode, hold Alt while dragging a selected hit or group to copy it to the drag destination instead of moving the original.
- Alt-drag copies velocity, probability, timing offset and retrigger data with the selected hits.
- Copy previews use a distinct tint and refuse to overwrite occupied destination cells.

## Keyboard input

- Centralized editor shortcuts through the drum grid and made letter matching case-insensitive.
- Toolbar controls no longer unnecessarily steal keyboard focus from the grid, and common actions return focus to the editor.
- Added a guarded physical-key fallback for D/S tool switching, U undo, arrow nudging, Delete/Backspace and Escape while the plugin is the active editing surface. This helps when a host does not forward normal plugin key events reliably.
- Ctrl+Z remains supported when a host forwards it, but Bitwig may intercept it first. U remains the reliable plugin-local undo shortcut.

## UI cleanup

- Replaced non-ASCII bullet/key-hint separators in the editor with plain ASCII separators to avoid broken Windows fallback-font glyphs.
- Updated the help strip to describe Alt-drag duplication instead of the removed duplicate command.

Alpha.5's Draw/Select editing, marquee selection, relative multi-hit velocity editing, exact 1-127 velocity popup, 60 Hz interpolated playhead, 25% zoom increments, 1/32 detail mode, GGD mappings, MIDI passthrough, meters and eight-bar patterns remain intact.
