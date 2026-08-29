# Stochas GGD v1.0.1

v1.0.1 is a visual-polish patch for the 1.0 playback feedback system. It fixes the broken Played Note Effects settings layout reported after release and makes the note-trigger effects substantially more expressive without touching MIDI timing or scheduling.

## Settings layout fix

The explanatory Played Note Effects text no longer collides with the Effect strength and Effect decay controls.

- the right settings card now reserves the real height of all effect controls before drawing help text
- the explanatory copy sits in a separated footer below the sliders
- the modal has slightly more breathing room at normal Bitwig plug-in sizes
- click-outside, Escape and Done closing behavior is unchanged

## Stronger played-note effects

Played-note feedback is still driven only by notes that the sequencer actually schedules after mute and probability decisions, but the rendering is now richer and easier to read at a glance.

- stronger full-lane flash
- local accent streak centered on the played hit
- layered outer and inner bloom
- brighter velocity-reactive impact core
- short four-direction impact flare
- larger primary expanding ripple
- delayed secondary ripple for a more fluid aftershock

The existing Ripple, Lane flash, Velocity-reactive intensity and Reduce Motion controls continue to govern the effect stack.

## Wider effect controls

- Effect strength now ranges from 0% to 150% instead of stopping at 100%.
- Effect decay now ranges from 100 ms to 1400 ms instead of stopping at 900 ms.
- New installations default to 100% strength and 420 ms decay.
- Existing local preference values remain valid and are preserved.

Reduce Motion still disables expanding/impact motion while retaining static bloom and lane feedback.

## Engine boundary

This patch does not change:

- host-PPQ scheduling
- 960 PPQ event storage
- MIDI drag-out
- MIDI passthrough
- kit mappings
- pattern/project persistence
- SEQ ON / SEQ OFF behavior

The audio-thread-safe played-event FIFO remains the source of playback feedback. Visual events may be dropped if the UI falls behind rather than ever interfering with MIDI timing.
