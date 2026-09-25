# P3-1: repeat basemap toggle / layout enter-exit / save / zoom.
# Stops with non-zero exit when KaCrashGuard leaves a new .dmp under the log dir.
param(
  [int]$Iterations = 200,
  [string]$LogDir = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
. "$PSScriptRoot\dev-env.ps1"

if ($Iterations -le 0) { throw "Iterations must be > 0" }
if ([string]::IsNullOrWhiteSpace($LogDir)) {
  $LogDir = Join-Path $env:TEMP ("ka-hgis-stress-ui-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$env:KA_HGIS_LOG_DIR = $LogDir
$env:OSGEO4W_ROOT = "C:\Users\Public\ka-hgis\osgeo4w"

function Count-Dumps([string]$dir) {
  if (-not (Test-Path -LiteralPath $dir)) { return 0 }
  return @(Get-ChildItem -LiteralPath $dir -Filter "*.dmp" -File -ErrorAction SilentlyContinue).Count
}

$before = Count-Dumps $LogDir
Write-Host "stress-ui-loop iterations=$Iterations logDir=$LogDir dumps_before=$before"

& "$PSScriptRoot\run-ka-hgis.ps1" "--stress-ui-loop=$Iterations"
$appExit = $LASTEXITCODE

$after = Count-Dumps $LogDir
$newDumps = $after - $before
Write-Host "stress-ui-loop app_exit=$appExit dumps_after=$after new_dumps=$newDumps"

if ($newDumps -gt 0) {
  Write-Host "FAIL: KaCrashGuard dump appeared under $LogDir"
  Get-ChildItem -LiteralPath $LogDir -Filter "*.dmp" -File | ForEach-Object { Write-Host "  $($_.FullName)" }
  exit 2
}
if ($appExit -ne 0) {
  Write-Host "FAIL: app exit $appExit"
  exit $appExit
}
Write-Host "stress-ui-loop OK"
exit 0
