# BR PDF READER by Belajar Resolume

**V1.1 build fix:** uses the current Resolume FFGL SDK layout (the older `v2.2` tag does not contain a root `CMakeLists.txt`) and installs GLEW automatically in GitHub Actions.

A focused FFGL source plugin for loading and presenting PDF documents directly inside Resolume on Windows.

## V1 controls

**PDF FILE**
- Choose PDF — select a PDF and immediately display page 1.

**SLIDE CONTROL**
- Previous
- Next
- Page indicator is shown on the Previous/Next parameter display.

**AUTO SLIDE**
- Auto Slide — ON/OFF
- Interval Seconds — default 5 seconds
- When Finished:
  - Stop at Last Page
  - Loop to First Page

**DISPLAY**
- Auto Fit — ON by default.
- Preserves the PDF page aspect ratio; portrait and landscape documents fit the Resolume output without stretching.

## Requirements

- Windows 10/11 x64
- Resolume Arena/Avenue compatible with current FFGL SDK
- GitHub Actions can build the DLL; no Visual Studio setup is required on your own PC.

## Upload to GitHub

1. Create a new empty repository, for example `BR-PDF-READER`.
2. Extract this ZIP.
3. Upload **everything inside the extracted folder**, including `.github`.
4. Commit to the `main` branch.
5. Open **Actions**.
6. Choose **Build BR PDF READER**.
7. Click **Run workflow**.
8. When the run is green, download artifact **BR-PDF-READER-Windows-x64**.
9. Extract `BR-PDF-READER-Windows-x64.zip` to get `BR_PDF_READER_FFGL.dll`.

## Install in Resolume

Copy:

`BR_PDF_READER_FFGL.dll`

to:

`Documents\Resolume\Extra Effects\`

Then restart Resolume.

## First test

1. Add **BR PDF READER by Belajar Resolume** as a Source.
2. Click **Choose PDF**.
3. Select a small PDF (2–5 pages is ideal for the first test).
4. Page 1 should appear automatically.
5. Test **Next** and **Previous**.
6. Turn on **Auto Slide** with 2–5 seconds.
7. Test **Stop at Last Page**.
8. Change **When Finished** to **Loop to First Page** and test again.
9. Keep **Auto Fit** ON and test both portrait and landscape PDFs.

## V1 scope

This version deliberately focuses on PDF playback stability. Preview/Take, PPT/PPTX support, transitions and other presentation features are not included yet.
