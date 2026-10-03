# One-shot: env + build + ctest + smoke + e2e. Portable delivery is explicit only.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

. "$PSScriptRoot\dev-env.ps1"

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) { throw "cmake.exe not found. Install CMake or add it to PATH." }

& cmake --preset vs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

& cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed" }

& ctest --test-dir build -C Release --output-on-failure --parallel
if ($LASTEXITCODE -ne 0) { throw "ctest failed" }

& "$PSScriptRoot\run-ka-hgis.ps1" --smoke-quit
if ($LASTEXITCODE -ne 0) { throw "smoke-quit failed: $LASTEXITCODE" }

& "$PSScriptRoot\e2e-smoke.ps1"
if ($LASTEXITCODE -ne 0) { throw "e2e failed" }

$ver = (Get-Content VERSION -Raw).Trim()
Write-Host "build-all OK  version=$ver"
exit 0
