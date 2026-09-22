# BR PDF Reader V1.4.1 — Crash Fix

This hotfix keeps the V1.4 features but changes the plugin metadata so Resolume does not reuse the incompatible V1.3 parameter schema.

## Crash fixes
- FFGL unique plugin ID changed from `BRP1` to `BRP2`.
- Removed the experimental one-item Branding option parameter.
- Manual and Auto transition parameters now have unique internal serialization names:
  - Manual Mode / Manual Speed
  - Auto Mode / Auto Speed
  Resolume still displays them simply as Mode / Speed.
- Branding is now static, non-interactive text in the section header:
  `by Belajar Resolume | PDF FILE`

## Features retained
- Previous / Next
- Manual mode: NORMAL / FADE / SLIDE UP / DOWN / LEFT / RIGHT
- Manual FAST / SLOW
- Auto Slide 1–60 sec
- Stop / Loop
- Auto mode: NORMAL / FADE / SLIDE UP / DOWN / LEFT / RIGHT
- Auto FAST / SLOW
- FIT / FILL / STRETCH
- Transform: Zoom / Position X / Position Y

## IMPORTANT install test
1. Close Resolume.
2. Delete every older `BR_PDF_READER_FFGL.dll` copy from Extra Effects.
3. Put only the V1.4.1 DLL in Extra Effects.
4. Start Resolume.
5. If Resolume opens, add BR PDF Reader as a new source and test controls.

The new plugin ID is intentional: V1.4 changed parameter count/types significantly compared with V1.3, and reusing the old FFGL ID could make Resolume load stale parameter state.
