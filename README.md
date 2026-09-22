# BR PDF Reader V1.4.4 — Classic Transform + Thumbnail

V1.4.4 keeps the embedded FFGL thumbnail from V1.4.3, but restores the Transform behavior to the earlier stable version.

## Transform restored
- Zoom: 25%–400%
- Position X: -1.00 to +1.00
- Position Y: -1.00 to +1.00
- Same classic transform behavior as before the Dynamic Pan experiment

## Thumbnail retained
The BR PDF Reader artwork remains embedded directly inside the DLL as a 160x120 FFGL thumbnail.

No external PNG file is required after build.

## Other features retained
- BR PDF Reader
- static `by Belajar Resolume` branding
- Previous / Next
- NORMAL / FADE / SLIDE UP / DOWN / LEFT / RIGHT
- manual FAST / SLOW
- Auto Slide 1–60 sec
- Stop / Loop
- independent Auto Slide transition + speed
- FIT / FILL / STRETCH

## Build
Replace repository contents with this package and run the existing GitHub Action.

Output:
`BR_PDF_READER_FFGL.dll`
