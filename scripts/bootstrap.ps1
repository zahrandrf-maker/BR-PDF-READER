$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force external | Out-Null
if (-not (Test-Path external/ffgl/CMakeLists.txt)) {
  git clone --depth 1 --branch v2.2 https://github.com/resolume/ffgl.git external/ffgl
}
Write-Host "Resolume FFGL SDK ready."
