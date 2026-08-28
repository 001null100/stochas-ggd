# Stochas GGD v0.2.0-beta.4

Beta 4 is a readability-first visual release built directly on Beta 3. The event scheduler, 960 PPQ pattern model, persistence, MIDI import/export and drum-transform logic are deliberately unchanged. This release concentrates on making the editor easier to parse at a glance and more satisfying to use for long sessions.

## Shared theme system

Beta 4 introduces four persistent appearance themes:

- **Graphite** — neutral dark teal and the new default baseline.
- **Midnight** — cool navy with cyan/blue accents.
- **Ember** — warm charcoal with amber accents.
- **Contrast** — near-black surfaces with deliberately strong structural separation.

Theme selection is stored as a local appearance preference rather than inside project or pattern data. The editor, timeline and Grooves / Patterns browser use the same shared palette instead of maintaining unrelated colour schemes.

## Timing hierarchy is now explicit

Bar, beat and subdivision lines no longer depend on small brightness variations of one generic border colour. They have dedicated semantic roles that preserve the same hierarchy in every theme:

1. **Bar boundaries** are the strongest full-height dividers and use a two-pixel line.
2. **Beat boundaries** remain full-height but are clearly secondary to bars.
3. **Primary subdivisions** are quieter editing guides for the active straight or triplet grid.
4. **Fine subdivisions** are the quietest layer and only appear when the high-resolution grid is active.

Alternating bar shading remains intentionally subtle so it supports the structure without competing with hits.

## Clearer instrument groups

Instrument-family separators now read as actual sections rather than slightly different rows.

- Family headers use their own fill and separator colours.
- A strong top edge and dedicated side rail distinguish headers from articulations.
- Collapse state uses a clean triangle indicator instead of ASCII `[-]` / `[+]` markers.
- Ordinary articulation rows retain alternating shading and lighter horizontal separators.
- The sticky articulation column has a stronger boundary from the timeline so horizontal scrolling never visually merges labels with the grid.

## Hits, selection and playback

Pattern content, editing state and transport now have separate visual identities.

- Regular hits gain a restrained accent halo and velocity-dependent body.
- Ghost notes use a lighter filled/outlined treatment instead of looking like ordinary hits at lower opacity.
- Selected hits use a dedicated high-contrast selection glow and outline rather than relying on the hit accent.
- Draw-mode hover targets have a subtle fill plus outline so the pending insertion point is easier to judge.
- Marquee selection remains translucent but has a stronger readable edge.
- The playhead now has its own theme role, soft vertical glow, strong centre line and a small ruler marker. It no longer competes with note-hit colour.

The existing smooth playhead interpolation is unchanged.

## More tactile controls

Beta 4 adds a shared JUCE look-and-feel for the editor chrome.

- Buttons have consistent rounded surfaces, borders and distinct hover / pressed / active states.
- Toggle buttons make their active state substantially clearer.
- Combo boxes use the same surface/border language and a cleaner dropdown indicator.
- The zoom slider has separate background, active track, thumb and hover feedback.
- Popup menus, text editors and scrollbars follow the selected theme.
- The two-row performance strip is grouped into two quiet visual shelves instead of floating controls on a flat background.

The goal is feedback that feels responsive without adding animation or decoration that interferes with rhythmic reading.

## Browser visual pass

The Grooves / Patterns browser now follows the active theme and updates immediately when the theme changes.

- Loaded items use a rounded highlight and accent rail.
- Selected items use a separate quieter outline/fill state.
- Search fields, tree lines, empty-state text and browser chrome use shared palette roles.
- Browser header separation and padding are slightly cleaner.
- Existing indexed live search, folder roots, double-click loading and loaded-file tracking are unchanged.

## Readable minimum layout

The minimum editor width is now **1400 px**. Beta 4 intentionally prefers preserving the pattern-name field, theme selector, dedicated browser column and timeline readability rather than allowing the host to compress the interface into a technically valid but poor layout.

## Beta 3 workflow retained

Everything added in Beta 3 remains available:

- per-hit probability presets
- Ghost / Accent velocity actions
- event-based Flam / Double transforms
- high-resolution MIDI export
- indexed Grooves / Patterns filtering
- the two-row selection/performance strip
- high-resolution MIDI import
- native `.sggdp` patterns
- per-pattern meter and bar count
- zoom-driven straight/triplet editing resolution

## Engine intentionally unchanged

Beta 4 does **not** modify:

- host-PPQ event scheduling
- 960 PPQ event storage
- event duration / probability playback
- transport discontinuity handling
- project persistence
- Alpha-to-Beta migration
- MIDI import translation
- MIDI export mapping
- semantic GGD kit mappings
- pattern geometry
- live MIDI passthrough

This is intentionally a visual layer on top of the stable Beta 3 behavior.

## Current boundaries

- Themes are supplied palettes rather than a full user colour editor.
- Theme preference is global/local appearance state, not per-project state.
- MIDI export remains file-based; direct drag-out into Bitwig is still a later feature.
- Probability has quick presets but no dedicated lane or arbitrary-value editor yet.
- Experimental GGD Groove Player source pitch 85 remains intentionally unresolved.

## Release-candidate checks

The exact Beta 4 release candidate must compile and package successfully in the Windows PR workflow before merge. The release is complete only when GitHub Releases contains both:

- `Stochas.GGD.clap`
- `Stochas-GGD-v0.2.0-beta.4-Windows-x64.zip`
