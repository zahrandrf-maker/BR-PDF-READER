# BR PDF Reader — V1.3

FFGL PDF source plugin for Resolume on Windows.

## V1.3 revisions

### Branding
- Plugin title is now **BR PDF Reader**
- A separate watermark text row shows **by Belajar Resolume**
- The plugin ignores attempts to change that watermark value

### Auto Slide
- Interval range: **1–60 seconds**
- Default: **5 seconds**
- The value is a real ranged parameter, so Resolume can expose it as a numeric/slider value
- End mode:
  - Stop at Last Page
  - Loop to First Page
- Transition:
  - **FAST FADE** ≈ 0.35 s
  - **SLOW FADE** ≈ 1.10 s

### Display
Dropdown:
- **FIT** — shows the full PDF page and preserves aspect ratio
- **FILL** — fills the Resolume frame while preserving aspect ratio; edges may crop
- **STRETCH** — fills the frame by stretching the page

Default: **FIT**

## Existing features retained
- Choose PDF
- PDF automatically starts on page 1
- Previous
- Next
- page indicator
- Windows native PDF renderer using Windows.Data.Pdf

## GitHub build
Replace the old repository files with this V1.3 package and commit to `main`.

Then:
1. GitHub → Actions
2. **Build BR PDF READER**
3. Run workflow
4. Download `BR-PDF-READER-Windows-x64`
5. Extract `BR_PDF_READER_FFGL.dll`
6. Replace the old DLL in the Resolume Extra Effects folder
7. Restart Resolume

## Test checklist
1. Confirm title says **BR PDF Reader**
2. Confirm watermark row says **by Belajar Resolume**
3. Test Interval at 5, 30 and 60 seconds
4. Test FIT / FILL / STRETCH with landscape and portrait PDFs
5. Test FAST FADE
6. Test SLOW FADE
7. Test Stop at Last Page
8. Test Loop to First Page
