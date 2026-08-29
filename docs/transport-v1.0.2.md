# v1.0.2 transport normalization

The GGD processor normalizes the host PPQ coordinate against `ppqPositionOfLastBarStart` before the inherited Bitwig negative-PPQ preroll guard runs.

For a 5/4 host whose musical bars are reported at PPQ `-1, 4, 9, ...`, the signed bar-origin remainder is `-1`, so playback is presented to the scheduler as `0, 5, 10, ...`. A conventional 4/4 host reporting `0, 4, 8, ...` produces an offset of zero and remains unchanged.

The normalization is accepted only when the host bar-start metadata is finite and plausibly belongs to the current bar. Genuine negative preroll remains negative after normalization and continues to use the inherited preroll guard.
