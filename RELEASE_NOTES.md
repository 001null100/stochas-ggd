# Stochas GGD v1.0.2

v1.0.2 fixes host-transport alignment exposed by odd-meter Bitwig projects, most visibly as a 5/4 pattern treating beat 2 as the start of the bar and producing a rough or delayed initial playback.

## Odd-meter bar alignment

Stochas GGD now uses the host's reported `ppqPositionOfLastBarStart` to normalize the PPQ coordinate before it reaches the pattern scheduler.

A shifted 5/4 host timeline can report musical bar starts at positions such as `-1, 4, 9, ...`. Feeding those absolute values directly into a five-quarter pattern makes PPQ 0 land one beat after the real bar start. v1.0.2 removes that constant host bar-origin offset first, presenting the scheduler with canonical bar starts at `0, 5, 10, ...`.

Conventional projects whose host bar starts are already canonical, such as ordinary 4/4 positions `0, 4, 8, ...`, receive an offset of zero and keep their existing timing.

## Cleaner playback start

The bar-origin correction now happens before the inherited Bitwig negative-PPQ preroll guard.

This distinction matters because a shifted musical beat 1 can otherwise look like negative preroll and be discarded. After normalization:

- the real first beat of an odd-meter bar reaches the scheduler normally
- genuine negative preroll remains negative and is still ignored until playback reaches the musical origin
- first-block scheduler state and probability seeding continue to use one consistent normalized transport coordinate

## Defensive host metadata handling

The correction is only applied when:

- the time-signature numerator and denominator are valid
- PPQ and bar-start values are finite
- the reported last bar start plausibly belongs to the current bar

If a host does not provide trustworthy bar-start metadata, Stochas GGD falls back to the existing transport behavior instead of inventing an offset.

## Engine boundary

This patch deliberately does not change:

- the 960 PPQ event model
- pattern duration/probability scheduling
- semantic GGD mappings
- MIDI drag-out or file export
- live MIDI passthrough
- pattern/project persistence
- SEQ ON / SEQ OFF
- played-note visual effects

The inherited processor implementation is kept intact as a private fallback; the GGD build adds a narrow transport-normalization layer in front of the established scheduler.

## Validation

The Windows CLAP release is built from the exact stamped source after PR CI. Runtime confirmation in Bitwig should specifically cover starting playback on beat 1 and later bars of a 5/4 project, plus a normal 4/4 project as a regression check.
