# BR PDF Reader V1.4.3 — Embedded Thumbnail

V1.4.3 keeps all V1.4.2 features and adds a native FFGL embedded thumbnail.

## Thumbnail
The supplied BR PDF Reader artwork is embedded directly into the DLL as a 160x120 RGBA thumbnail.

Expected behavior in Resolume:
- BR PDF Reader has a recognizable thumbnail in the Sources browser / clip representation
- when the source is dragged into a clip/layer, Resolume can use the embedded FFGL thumbnail instead of a generic blank/default source image
- no external PNG file is required after build; the image is compiled into the DLL

The original artwork was center-cropped to Resolume-friendly 4:3 thumbnail dimensions.

## Existing features retained
- BR PDF Reader
- static `by Belajar Resolume` branding
- Previous / Next
- NORMAL / FADE / SLIDE UP / DOWN / LEFT / RIGHT
- manual FAST / SLOW
- Auto Slide 1–60 sec
- Stop / Loop
- independent Auto Slide transition + speed
- FIT / FILL / STRETCH
- Zoom 25–400%
- Dynamic Position X / Y based on zoom and document aspect ratio

## Build
Replace the repository files with this package and run the existing GitHub Action.

The resulting `BR_PDF_READER_FFGL.dll` contains the thumbnail internally.
