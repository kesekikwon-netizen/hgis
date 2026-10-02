# Repeat CTest to list tests that are not 5/5, split into deterministic failures (failed
# every run: a real bug or missing data, not a flake) and flaky tests (failed some runs).
# Usage: .\scripts\ctest-flake.ps1 [-Repeat 5] [-ExcludeRegex '^save_open_window$'] [-OutDir build/qa/...]
param(
  [int]$Repeat = 5,
  [string]$BuildDir = "build",
  [string]$Config = "Release",
  [string]$OutDir = "",
  [string]$ExcludeRegex = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot
$env:PATH = "C:\Program Files\CMake\bin;" + $env:PATH
. (Join-Path $repoRoot "scripts\dev-env.ps1")

$env:TEMP = Join-Path $env:LOCALAPPDATA "Temp"
$env:TMP = $env:TEMP

$buildAbs = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir)).Replace('\', '/')
# Tests get TEMP/TMP from their CTest ENVIRONMENT property (fixed at configure time) or from
# cmake/run_qtest.cmake (build/test-tmp/<test>). Create exactly the folders CTest will hand
# out, read from the test properties, instead of guessing them from this shell's TEMP.
$tempDirs = New-Object System.Collections.Generic.List[string]
$ErrorActionPreference = "Continue"
$listing = & ctest --test-dir $BuildDir -C $Config --show-only=json-v1 2>$null | Out-String
$listingOk = $LASTEXITCODE -eq 0
$ErrorActionPreference = "Stop"
if ($listingOk -and $listing.Trim()) {
  foreach ($test in (($listing | ConvertFrom-Json).tests)) {
    foreach ($property in @($test.properties)) {
      if ($property.name -ne 'ENVIRONMENT') { continue }
      foreach ($value in @($property.value)) {
        if ($value -match '^(TEMP|TMP)=(.+)$' -and -not $tempDirs.Contains($Matches[2])) { $tempDirs.Add($Matches[2]) }
      }
    }
  }
}
foreach ($dir in $tempDirs) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
$tempRoot = "$($tempDirs.Count) per-test TEMP folders from CTest properties; others use build/test-tmp/<test>"

if (-not $OutDir) {
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  $OutDir = Join-Path $repoRoot "build\qa\ctest-flake-$stamp"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$summary = [System.Collections.Generic.List[object]]::new()
$failCounts = @{}

for ($i = 1; $i -le $Repeat; $i++) {
  $log = Join-Path $OutDir "run-$i.log"
  $args = @(
    "--test-dir", $BuildDir,
    "-C", $Config,
    "--output-on-failure",
    "--output-log", $log
  )
  if ($ExcludeRegex) { $args += @("-E", $ExcludeRegex) }
  Write-Host "=== flake run $i/$Repeat ==="
  & ctest @args
  $code = $LASTEXITCODE
  $failed = @()
  if (Test-Path $log) {
    $text = Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue
    if ($text -match "The following tests FAILED:") {
      foreach ($line in (Get-Content -LiteralPath $log)) {
        if ($line -match '^\s+\d+\s+-\s+(\S+)\s+\(') {
          $failed += $Matches[1]
        }
      }
    }
  }
  foreach ($name in $failed) {
    if (-not $failCounts.ContainsKey($name)) { $failCounts[$name] = 0 }
    $failCounts[$name]++
  }
  $summary.Add([pscustomobject]@{
    run = $i
    exitCode = $code
    failed = ($failed -join ',')
    log = $log
  })
}

$unstable = @()
$deterministic = @()
$flaky = @()
foreach ($k in ($failCounts.Keys | Sort-Object)) {
  $unstable += "$k $($failCounts[$k])/$Repeat"
  if ($failCounts[$k] -ge $Repeat) { $deterministic += "$k failed $Repeat/$Repeat" }
  else { $flaky += "$k failed $($failCounts[$k])/$Repeat" }
}

$report = Join-Path $OutDir "SUMMARY.md"
$lines = @(
  "# CTest flake $Repeat repeats",
  "",
  "BuildDir=$buildAbs",
  "TEMP isolation=$tempRoot",
  "ExcludeRegex=$ExcludeRegex",
  "",
  "## Runs"
)
foreach ($row in $summary) {
  $lines += "- run $($row.run) exit=$($row.exitCode) failed=[$($row.failed)]"
}
$lines += ""
$lines += "## Deterministic failures (every run: fix the test or the code, not a flake)"
if ($deterministic.Count -eq 0) { $lines += "- none" } else { foreach ($u in $deterministic) { $lines += "- $u" } }
$lines += ""
$lines += "## Flaky (some runs)"
if ($flaky.Count -eq 0) { $lines += "- none" } else { foreach ($u in $flaky) { $lines += "- $u" } }
Set-Content -LiteralPath $report -Value $lines -Encoding utf8
Write-Host "Wrote $report"
if ($unstable.Count -gt 0) { exit 1 }
exit 0
