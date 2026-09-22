# BR PDF READER by Belajar Resolume — V1.2

Windows FFGL PDF source for Resolume.

## V1.2 fixes
This revision addresses the compile errors from the previous GitHub Actions run:
- replaces low-level WRL/ABI PDF code with C++/WinRT `Windows.Data.Pdf`
- removes the invalid `IInMemoryRandomAccessStream` ABI usage
- removes `CreateStreamOverRandomAccessStream`
- replaces legacy `GetOpenFileNameW/commdlg.h` with `IFileOpenDialog`
- keeps the current Resolume FFGL SDK + GLEW Windows build

## Features
- **BR PDF READER by Belajar Resolume**
- Choose PDF → page 1 appears immediately
- Previous / Next
- Auto Slide ON/OFF
- adjustable interval
- Stop at Last Page
- Loop to First Page
- Auto Fit ON by default, preserving page aspect ratio

## Upload/build
1. Replace the old repository contents with all files from this ZIP.
2. Keep `.github/workflows/build-windows.yml`.
3. Commit to `main`.
4. GitHub → Actions → **Build BR PDF READER** → Run workflow.
5. Download artifact `BR-PDF-READER-Windows-x64`.
6. Extract `BR_PDF_READER_FFGL.dll`.
7. Copy the DLL into your Resolume Extra Effects folder and restart Resolume.

## First runtime test
Start with a simple 2–5 page PDF. Verify:
1. plugin appears as `BR PDF READER by Belajar Resolume`
2. Choose PDF opens the Windows file picker
3. page 1 appears
4. Next / Previous works
5. Auto Slide advances
6. Stop mode stops on the last page
7. Loop mode returns to page 1
8. portrait and landscape PDFs retain their aspect ratio with Auto Fit ON
