$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\dev-env.ps1"
Push-Location $Root
try {
  # Ask CMake for the code model before the first configure: scripts/compile-commands.mjs turns it
  # into build\compile_commands.json for clangd (the VS generator does not write one).
  $query = Join-Path $Root 'build\.cmake\api\v1\query'
  New-Item -ItemType Directory -Force -Path $query | Out-Null
  Set-Content -LiteralPath (Join-Path $query 'codemodel-v2') -Value '' -NoNewline
  & cmake --preset vs
  if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
  & cmake --build --preset release --parallel
  if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
  Write-Host "BUILD SUCCESS: $Root\build\Release\ka-hgis.exe"
} finally {
  Pop-Location
}
