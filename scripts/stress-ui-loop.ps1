# P3-1: repeat basemap toggle / layout enter-exit / save / zoom.
# P3-3: every MemoryIntervalSec, append process working set to CSV under LogDir.
# Stops with non-zero exit when KaCrashGuard leaves a new .dmp under the log dir.
param(
  [int]$Iterations = 200,
  [string]$LogDir = "",
  [int]$MemoryIntervalSec = 60,
  # Probe WorkingSet CSV without launching ka-hgis.exe (short sleep process).
  [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
. "$PSScriptRoot\dev-env.ps1"

if ($Iterations -le 0) { throw "Iterations must be > 0" }
if ($MemoryIntervalSec -le 0) { throw "MemoryIntervalSec must be > 0" }
if ([string]::IsNullOrWhiteSpace($LogDir)) {
  $LogDir = Join-Path $env:TEMP ("ka-hgis-stress-ui-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$env:KA_HGIS_LOG_DIR = $LogDir
$env:OSGEO4W_ROOT = "C:\Users\Public\ka-hgis\osgeo4w"

$MemoryCsv = Join-Path $LogDir "memory-working-set.csv"

function Count-Dumps([string]$dir) {
  if (-not (Test-Path -LiteralPath $dir)) { return 0 }
  return @(Get-ChildItem -LiteralPath $dir -Filter "*.dmp" -File -ErrorAction SilentlyContinue).Count
}

function Initialize-MemoryCsv([string]$path) {
  "timestamp_utc,elapsed_sec,pid,name,working_set_bytes,private_memory_bytes" |
    Set-Content -LiteralPath $path -Encoding utf8
}

function Write-MemorySample {
  param(
    [System.Diagnostics.Process]$Process,
    [System.Diagnostics.Stopwatch]$Stopwatch,
    [string]$CsvPath
  )
  if ($null -eq $Process) { return $false }
  try {
    $Process.Refresh()
    if ($Process.HasExited) { return $false }
  } catch {
    return $false
  }
  $ws = [int64]$Process.WorkingSet64
  $priv = [int64]$Process.PrivateMemorySize64
  $name = $Process.ProcessName
  $line = "{0},{1},{2},{3},{4},{5}" -f `
    ([datetime]::UtcNow.ToString("o")),
    ([int][math]::Floor($Stopwatch.Elapsed.TotalSeconds)),
    $Process.Id,
    $name,
    $ws,
    $priv
  Add-Content -LiteralPath $CsvPath -Value $line -Encoding utf8
  return $true
}

function Watch-ProcessMemory {
  param(
    [System.Diagnostics.Process]$Process,
    [string]$CsvPath,
    [int]$IntervalSec
  )
  Initialize-MemoryCsv $CsvPath
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  [void](Write-MemorySample -Process $Process -Stopwatch $sw -CsvPath $CsvPath)
  while (-not $Process.HasExited) {
    Start-Sleep -Seconds $IntervalSec
    if (-not (Write-MemorySample -Process $Process -Stopwatch $sw -CsvPath $CsvPath)) {
      break
    }
  }
  if (-not $Process.HasExited) {
    $Process.WaitForExit()
  }
  Write-Host "memory csv=$CsvPath rows=$((@(Get-Content -LiteralPath $CsvPath).Count - 1))"
}

if ($SelfTest) {
  Write-Host "stress-ui-loop SelfTest logDir=$LogDir intervalSec=$MemoryIntervalSec"
  $probe = Start-Process -FilePath "powershell.exe" `
    -ArgumentList @("-NoProfile", "-Command", "Start-Sleep -Seconds 5") `
    -PassThru -WindowStyle Hidden
  Watch-ProcessMemory -Process $probe -CsvPath $MemoryCsv -IntervalSec ([Math]::Min($MemoryIntervalSec, 2))
  $rows = @(Get-Content -LiteralPath $MemoryCsv)
  if ($rows.Count -lt 2) {
    Write-Host "FAIL: SelfTest expected header + at least one sample in $MemoryCsv"
    exit 1
  }
  if ($rows[0] -notmatch "working_set_bytes") {
    Write-Host "FAIL: SelfTest CSV header missing working_set_bytes"
    exit 1
  }
  Write-Host "stress-ui-loop SelfTest OK"
  exit 0
}

$before = Count-Dumps $LogDir
Write-Host "stress-ui-loop iterations=$Iterations logDir=$LogDir dumps_before=$before memoryCsv=$MemoryCsv intervalSec=$MemoryIntervalSec"

$exe = Join-Path $Root "build\Release\ka-hgis.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
  throw "Release executable not found: $exe. Build Release first."
}

$app = Start-Process -FilePath $exe -ArgumentList @("--stress-ui-loop=$Iterations") `
  -WorkingDirectory $Root -PassThru
Watch-ProcessMemory -Process $app -CsvPath $MemoryCsv -IntervalSec $MemoryIntervalSec
$appExit = $app.ExitCode

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
