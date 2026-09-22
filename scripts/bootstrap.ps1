$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force external | Out-Null
if (Test-Path external/ffgl) {
  Remove-Item -Recurse -Force external/ffgl
}

git clone --depth 1 https://github.com/resolume/ffgl.git external/ffgl

if (-not (Test-Path external/ffgl/CMakeLists.txt)) {
  throw "FFGL SDK clone did not contain CMakeLists.txt."
}

Write-Host "Resolume FFGL SDK ready."
