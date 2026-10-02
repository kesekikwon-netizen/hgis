# Quick end-to-end smoke on the Windows build PC. ASCII only.
#   default          rule file + hard-coded path checks, unit test exe, app --smoke-quit
#   -SkipTests       skip test executables (CI already ran them through ctest)
#   -SkipSmoke       skip --smoke-quit (CI starts ka-hgis.exe --smoke-quit itself and checks its exit code)
#   -IncludeWorkflow also run ka_workflow_tests.exe (about 70 s; ctest runs it too)
# Evidence goes to build/e2e-smoke (the old .omo/evidence path belonged to a removed harness).
param([switch]$SkipTests, [switch]$SkipSmoke, [switch]$IncludeWorkflow)
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root
. "$root\scripts\dev-env.ps1"
$ev = Join-Path $root "build\e2e-smoke"
New-Item -ItemType Directory -Force -Path $ev | Out-Null
$log = Join-Path $ev "e2e.txt"
function Log([string]$m) { Add-Content -LiteralPath $log -Value $m; Write-Host $m }
Set-Content -LiteralPath $log -Value "=== ka-hgis e2e smoke ===" -Encoding utf8
Log "root=$root"

# Checklist rules: every rule needs an id, a known severity and a Korean message, ids are
# unique, and the set never shrinks below the 21 rules of 2026-09-29 (error rules block submit).
$rules = Join-Path $root "data\rules\drawing_checklist.v1.json"
if (-not (Test-Path $rules)) { Log "FAIL rules missing"; exit 1 }
$j = Get-Content $rules -Raw -Encoding UTF8 | ConvertFrom-Json
$ids = @($j.rules | ForEach-Object { $_.id })
$bad = @($j.rules | Where-Object { -not $_.id -or -not $_.message_ko -or @('error', 'warn') -notcontains $_.severity })
if ($bad.Count -gt 0) { Log "FAIL rules without id/message_ko/severity(error|warn): $(@($bad | ForEach-Object { $_.id }) -join ', ')"; exit 1 }
$dupes = @($ids | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })
if ($dupes.Count -gt 0) { Log "FAIL duplicate rule ids: $($dupes -join ', ')"; exit 1 }
if ($ids.Count -lt 21) { Log "FAIL rule_count=$($ids.Count) < 21"; exit 1 }
Log "OK rules count=$($ids.Count) errors=$(@($j.rules | Where-Object { $_.severity -eq 'error' }).Count)"

if (-not (Test-Path "$root\docs\architecture\data-flow.md")) { Log "FAIL graph"; exit 1 }
Log "OK graph"

# Product code must not depend on one developer's folders. Public/placeholder paths are fine.
$personal = Get-ChildItem "$root\src" -Recurse -Include *.cpp,*.h |
  Select-String -Pattern '[A-Za-z]:[\\/]+Users[\\/]+(?!Public\b|AppData\b|<)[^\\/"<>]+[\\/]', 'kyi25' |
  Where-Object { $_.Line -notmatch '^\s*//' }
if ($personal) {
  $personal | ForEach-Object { Log ("FAIL personal path {0}:{1}" -f $_.Path, $_.LineNumber) }
  exit 1
}
Log "OK no personal paths"
$devTree = Get-ChildItem "$root\src" -Recurse -Include *.cpp,*.h | Select-String -Pattern "D:/qgis" -SimpleMatch
foreach ($hit in $devTree) { Log ("WARN dev checkout path {0}:{1}" -f $hit.Path, $hit.LineNumber) }

if (-not $SkipTests) {
  $testExe = Join-Path $root "build\Release\ka_hgis_tests.exe"
  if (-not (Test-Path $testExe)) { Log "FAIL tests missing"; exit 1 }
  $p = Start-Process -FilePath $testExe -WorkingDirectory $root -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $ev "t-out.txt") -RedirectStandardError (Join-Path $ev "t-err.txt")
  if ($p.ExitCode -ne 0) { Log "FAIL unit $($p.ExitCode)"; exit $p.ExitCode }
  Log "OK unit tests"
  if ($IncludeWorkflow) {
    $wf = Join-Path $root "build\Release\ka_workflow_tests.exe"
    if (-not (Test-Path $wf)) { Log "FAIL workflow tests missing"; exit 1 }
    $pw = Start-Process -FilePath $wf -WorkingDirectory $root -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $ev "wf-out.txt") -RedirectStandardError (Join-Path $ev "wf-err.txt")
    if ($pw.ExitCode -ne 0) { Log "FAIL workflow $($pw.ExitCode)"; exit $pw.ExitCode }
    Log "OK workflow tests"
  }
} else {
  Log "SKIP test executables (-SkipTests)"
}

if (-not $SkipSmoke) {
  $app = Join-Path $root "build\Release\ka-hgis.exe"
  $p2 = Start-Process -FilePath $app -ArgumentList "--smoke-quit" -WorkingDirectory (Split-Path $app) -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $ev "a-out.txt") -RedirectStandardError (Join-Path $ev "a-err.txt")
  if ($p2.ExitCode -ne 0) { Log "FAIL smoke $($p2.ExitCode)"; exit $p2.ExitCode }
  Log "OK smoke-quit"
} else {
  Log "SKIP smoke-quit (-SkipSmoke)"
}
Log "E2E PASS"
exit 0
