$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Split-Path -Parent $here

# Automated flags must stay in-process (ctest / smoke).
$wait = $false
foreach ($a in $args) {
  if ($a -eq "--smoke-quit" -or $a -eq "--qa-phase1" -or $a.StartsWith("--open-gpkg") -or
      $a -eq "--stress-ui-loop" -or $a.StartsWith("--stress-ui-loop=")) { $wait = $true }
}

if (-not $wait) {
  & (Join-Path $repo "launch.ps1")
  exit $LASTEXITCODE
}

. (Join-Path $here "dev-env.ps1")
$exe = Join-Path $repo "build\Release\ka-hgis.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
  throw "Release executable not found: $exe. Build Release first."
}
# Qgs* writes plugin-miss warnings to stderr; do not treat that as a script failure.
$ErrorActionPreference = "Continue"
& $exe @args
exit $LASTEXITCODE
