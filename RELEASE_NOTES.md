# Stochas GGD v2.0.0

Version 2 is a complete pass over Stochas GGD. It fixes playback when time signatures change, rebuilds the editor around one clean code path, and adds a new look.

## Time signatures: fixed

In 1.x, patterns were counted from the start of the song in quarter notes. As soon as the host changed time signature, or a pattern's meter didn't match the host's, every following bar was shifted: hits landed mid-bar, fills started late and the groove drifted for the rest of the song. Changing a pattern's own meter in the editor also slid hits across bar lines.

Version 2 fixes both:

- **Patterns follow the host's bar lines.** Bar 1 of a pattern always starts on a host bar line, and the pattern's bar count follows the host's bar number. A meter change in the host never shifts the groove. A pattern bar shorter than the host bar rests until the next bar line; a longer one is cut at the bar line.
- **Sync mode per project.** *Sync: Host bars* is the new default. *Sync: Free run* keeps the 1.x behaviour (the pattern loops on its own from the song start) for deliberate polymeter.
- **Editing the meter keeps hits in their bars.** Every hit keeps its bar and its offset from the bar line. Hits that no longer fit (beat 4 of a bar that becomes 3/4, or bars beyond a shorter bar count) are hidden, not deleted, and return when the pattern grows again, even after the project is saved and reopened. The toolbar shows how many are hidden, with an option to discard them.
- The transport display shows the host meter and warns when it differs from the pattern's, with a tooltip explaining what will happen.

The scheduler was rewritten along with it: block windows chain seamlessly, so tempo automation and host position jitter can no longer drop or double a hit, nothing plays during host pre-roll, and a loop jump restarts cleanly.

## New editor

- **Pattern slots** are now a strip of eight buttons. Dots mark slots with content and a bar marks the slot the engine is playing. Right-click a slot to open, copy into or clear it.
- **Undo for every slot.** Each slot keeps its own history (64 steps) when you switch between slots. Undo brings back the selection too, as well as hidden hits after meter changes. Loading a groove or pattern is now undoable as well, so the "Replace edited pattern?" prompt is gone.
- **Grid selector:** 1/8, 1/16, 1/32, 1/8T, 1/16T and 1/32T, with optional finer grid when zoomed in.
- **Zoom** with the slider, Ctrl + mouse wheel, trackpad pinch, or *Fit* to show the whole pattern.
- **Context strip** for the selection: Ghost / Accent, chance 25-100%, Quantize, Humanize, Flam, Double, and a *More* menu with ramps, dynamics, fill, mirror, thin and row selection. In Draw mode it lists the mouse gestures.
- **Right-click menu** in Select mode with the common actions.
- **Keyboard:** Ctrl+D duplicates the selection, Shift+Up/Down nudges velocity, Shift+Left/Right nudges timing, Esc clears the selection. Keys are handled as normal key events wherever the host delivers them and polled only where it doesn't, so quick taps and key repeat work. Combinations the editor doesn't use, such as Ctrl+S, go to the host.
- Move previews turn red when the drop would collide or leave the pattern.
- Empty patterns show a short hint on how to start.
- Long patterns scroll with the playhead, but scrolling by hand pauses following for a moment instead of fighting you.

## New look

- Embedded Inter typeface and a consistent set of vector icons.
- Redesigned header: brand, slot strip, pattern name, transport and host meter display, output switch, MIDI drag, library and settings.
- Hits are coloured by instrument family (kick, snare, toms, hats, cymbals). Probability shows as a dashed outline with its percentage.
- Sticky ruler and articulation column with collapsible instrument groups and hit counts.
- Played-note feedback keeps the full 1.0.1 effect stack, now tinted per instrument and clipped to the pattern.
- New settings sheet with live theme previews and grouped switches.
- Restyled library with tabs, empty states, folder and file icons, and path-aware search results.
- The library panel can be hidden, and the window remembers its size.

## Under the hood

- The editor and timeline are now one class each (`GgdDrumEditor`, `GgdDrumGrid`) instead of ten layered Beta/V1/Final files compiled through macro renaming.
- Project state shrank from about 28 MB to 2.4 MB. The UI/audio double buffer waits for the audio thread before reusing a buffer, so the audio thread never reads a half-written pattern.
- Playhead and played-note feedback reach the editor through lock-free channels, and the playhead is drawn from the host position extrapolated to the display's refresh.
- Pattern names are stored as proper UTF-8.
- Automation overrides are atomic. The audio thread no longer asks the host to refresh its display.
- New test suite: engine timing across meter changes, odd meters, tempo ramps, loops and pre-roll; persistence; processor; editor; and simulated mouse editing. An offscreen snapshot tool renders the editor for visual review.

## Compatibility

- Projects, `.sggdp` patterns and preferences from 1.x load unchanged.
- Projects from 1.x open in *Sync: Host bars*. A 4/4 pattern in a 4/4 song plays exactly as before. Choose *Sync: Free run* to get back the 1.x timing in songs with mixed meters.
- The automation parameter list is unchanged, so existing automation lanes keep working.
- MIDI drag-out, MIDI export and SEQ ON / SEQ OFF behave as before.
