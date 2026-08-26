# Groove pack analysis

Initial analysis of the supplied `GGD.zip` archive. This document records format facts useful for the MIDI-library/import implementation; the MIDI files themselves are not committed.

## Archive summary

- 2,165 `.mid` files total.
- 2,149 unique MIDI files by SHA-256.
- 16 exact duplicates: the small top-level `Anup Midi Pack` is duplicated inside the larger `GetGood Drums ANUP Grooves Midi Pack`.
- All 2,165 files are Standard MIDI File type 1.
- All 2,165 files use 480 PPQ.
- 1,496 files contain two tracks; 669 contain one track.
- Note events are overwhelmingly on MIDI channel 1 (zero-based channel 0). Only a handful of note events use channel 10.
- Time signatures include ordinary 4/4 plus 3/4, 5/4, 6/4, 7/4, 5/8, 6/8, 7/8, 12/8 and 15/16. Import must therefore preserve MIDI time-signature metadata instead of assuming 4/4.

## Supplied top-level packs

| Folder | MIDI files | Observed note numbers |
| --- | ---: | --- |
| Anup Midi Pack | 16 | 36, 38, 41, 43, 44, 48, 49, 50, 52, 55, 57, 70, 74 |
| GetGood Drums ANUP Grooves Midi Pack | 177 | 25, 36, 38, 41, 42, 43, 44, 48, 49, 50, 52, 53, 55, 57, 58, 64, 70, 74 |
| GetGood Drums Beyond Prog Grooves by Simen Sandnes & Baard Kolstad Midi Pack | 370 | 10, 36, 37, 38, 42, 43, 44, 46, 48, 49, 50, 51, 53, 57, 64 |
| GetGood Drums GGD Crazy Fills Vol.1 Midi Pack MiDi | 230 | 36, 38, 41, 42, 43, 44, 46, 48, 49, 50, 52, 53, 54, 55, 57 |
| GetGood Drums Halpern Grooves 2 Midi Pack | 194 | 36, 38, 41, 42, 43, 44, 46, 48, 49, 51, 52, 53, 54, 55, 57, 58 |
| GetGood Drums Modern Metal by Ali Richardson Midi Pack | 220 | 36, 38, 39, 41, 42, 43, 44, 46, 48, 49, 51, 52, 53, 55, 57, 58, 59 |
| GetGood Drums Rudi Groove Midi Pack | 231 | 10, 24, 36, 38, 41, 43, 44, 48, 49, 50, 51, 53, 55, 57, 62, 64, 74, 95 |
| GGD_Free_Grooves_Vol1 | 238 | 36, 38, 42, 43, 44, 46, 48, 49, 50, 51, 52, 53, 54, 55, 57, 85 |
| GGD_Free_Grooves_Vol2 | 246 | 36, 38, 41, 42, 43, 46, 49, 50, 51, 52, 53, 54, 55, 57 |
| GGD_Krimh_MIDI_Pack | 228 | 36, 38, 41, 42, 43, 46, 48, 49, 50, 51, 52, 53, 54, 55, 57, 58 |
| Matt MIDI Pack | 15 | 36, 38, 41, 42, 44, 46, 48, 49, 50, 51, 53, 57 |

## Import implications

The files are technically straightforward to parse, but the pitch vocabularies are not identical across packs. Kick 36 and snare 38 are extremely common, while several packs use extended/outlier notes such as 10, 24, 25, 62, 64, 70, 74, 85 and 95.

Therefore:

- Do not assume every historical GGD pack uses one exact source map.
- Preserve the original folder hierarchy in the library browser.
- Allow a source mapping profile to be assigned at folder/pack level so hundreds of files do not need individual configuration.
- Auto-detection can use the set/frequency of pitches as a signature, but must expose ambiguity rather than silently guessing.
- Unknown pitches should survive import as unresolved events until the user maps or discards them.
- Deduplicate exact MIDI files in the browser by content hash while retaining aliases/locations if useful.

A practical first implementation can import a file into raw MIDI events, identify the most likely source profile, translate recognized pitches to semantic articulation IDs, and show a compact unresolved-articulation report before committing the pattern.
