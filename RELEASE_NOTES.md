# Stochas GGD v0.1.0-alpha.2

This alpha is a visual and interaction pass focused on making the drum editor feel intentional rather than merely functional.

## Zoom and timeline

- Rebuilt zoom around a fitted overview. At Fit, the complete pattern uses the available timeline width whether it is 1 bar or 8 bars.
- Added smooth continuous zoom with Ctrl+wheel, anchored beneath the pointer.
- Added a visible zoom slider, current zoom/grid readout, and one-click Fit control.
- Editing resolution moves musically from quarter notes to eighth notes to sixteenth notes as you zoom in.
- Bar-count changes and editor resizing now recalculate the fitted timeline instead of leaving stale empty canvas space.

## Visual polish

- Cleaner two-row toolbar and stronger control hierarchy.
- Refined ruler, bar boundaries, alternating bar shading, row grouping, sticky articulation lane and playhead.
- Added cell hover feedback and clearer hit rendering.
- Ghost notes are visually distinct from normal hits and retriggers have a dedicated accent treatment.
- Tightened spacing, row heights and the shortcut hint strip to reduce visual noise.

## Release packaging

- Versioned releases are now driven by `RELEASE_VERSION` rather than ad-hoc build numbers.
- Pull-request builds stay in GitHub Actions artifacts and no longer create Releases.
- A release ZIP contains the Windows x64 CLAP, installation instructions and GPL license, with the raw CLAP attached separately as well.

This is still an alpha. Existing GGD map support and the previous drum-editing gestures remain intact while the editor shell continues to evolve.
