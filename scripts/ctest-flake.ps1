# Repeat CTest to list tests that are not 5/5.
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
$sha = [System.Security.Cryptography.SHA256]::Create()
$hashBytes = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($buildAbs))
$hashHex = ([BitConverter]::ToString($hashBytes) -replace '-', '').ToLowerInvariant().Substring(0, 12)
$tempRoot = Join-Path $env:TEMP "ka-hgis-tests-$hashHex"

$isolated = @(
  "checklist_engine","workflow_engine","georef_engine","feature_presets",
  "buffer_ring","measure_tape","dem_trench_engine","theme_qss","recent_surveys",
  "section_layout_engine","section_studio_engine","terrain_3d_engine",
  "save_open_window","save_open_topo","save_open_drawing","save_open_edit",
  "save_open_roundtrip","save_open_open","save_open_open_invalid",
  "save_open_commit","save_open_saveas",
  "e2e_opaque_suite","network_services","storage_safety",
  "reference_download","region_locator_popup","layer_state_regressions",
  "map_render","above_labels","above_labels_125","above_labels_150",
  "above_labels_200","dem_presentation","export_survey_areas","perf_engine",
  "parallel_render","catch_log","cadastral"
)
foreach ($n in $isolated) {
  New-Item -ItemType Directory -Force -Path (Join-Path $tempRoot $n) | Out-Null
}

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
foreach ($k in ($failCounts.Keys | Sort-Object)) {
  $unstable += "$k $($failCounts[$k])/$Repeat"
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
$lines += "## Not 5/5"
if ($unstable.Count -eq 0) {
  $lines += "- none"
} else {
  foreach ($u in $unstable) { $lines += "- $u" }
}
Set-Content -LiteralPath $report -Value $lines -Encoding utf8
Write-Host "Wrote $report"
if ($unstable.Count -gt 0) { exit 1 }
exit 0
