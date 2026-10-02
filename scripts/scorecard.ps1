# Read existing ka-hgis measurements. Does not run CTest.
# ASCII only.
#   -LineLimitOnly   only the source line-limit gate (CI, F213): a C++ file under src/ or tests/
#                    over 300 lines fails unless docs/quality/line-limit-baseline.txt lists it,
#                    and a listed file fails when it grew past its recorded size.
#   -WriteBaseline   with -LineLimitOnly: rewrite the baseline from the current tree.
param([switch]$LineLimitOnly, [switch]$WriteBaseline)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$lineLimit = 300
$baselinePath = Join-Path $root 'docs/quality/line-limit-baseline.txt'

function Get-OversizeSources() {
  $rows = @()
  foreach ($dir in @('src', 'tests')) {
    $base = Join-Path $root $dir
    if (-not (Test-Path -LiteralPath $base)) { continue }
    # Filter by extension: Windows PowerShell 5.1 ignores -Include with -LiteralPath and counted .ico/.dxf.
    Get-ChildItem -LiteralPath $base -Recurse -File | Where-Object { $_.Extension -in '.cpp', '.h', '.hpp' } | ForEach-Object {
      # Read as UTF-8: Windows PowerShell 5.1 reads BOM-less UTF-8 as the ANSI code page (cp949) and miscounts lines.
      $count = @(Get-Content -LiteralPath $_.FullName -Encoding UTF8).Count
      if ($count -gt $lineLimit) {
        $relative = $_.FullName.Substring($root.Length).TrimStart('\', '/') -replace '\\', '/'
        $rows += [pscustomobject]@{ path = $relative; lines = $count }
      }
    }
  }
  return @($rows | Sort-Object path)
}

if ($LineLimitOnly) {
  $current = Get-OversizeSources
  if ($WriteBaseline) {
    $text = @('# C++ files already over the 300-line limit and their size when recorded.',
              '# scripts/scorecard.ps1 -LineLimitOnly fails when a file not listed here passes 300 lines',
              '# or a listed file grows. Shrinking is always fine; rewrite with -WriteBaseline after it.') +
            ($current | ForEach-Object { '{0} {1}' -f $_.lines, $_.path })
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $baselinePath) | Out-Null
    [System.IO.File]::WriteAllText($baselinePath, (($text -join "`n") + "`n"))
    Write-Host ("line-limit baseline written: {0} files" -f $current.Count)
    exit 0
  }
  $allowed = @{}
  if (Test-Path -LiteralPath $baselinePath) {
    foreach ($line in Get-Content -LiteralPath $baselinePath) {
      if ($line -match '^\s*(\d+)\s+(\S.*)$') { $allowed[$Matches[2].Trim()] = [int]$Matches[1] }
    }
  }
  $problems = @()
  foreach ($row in $current) {
    if (-not $allowed.ContainsKey($row.path)) {
      $problems += ('{0}: {1} lines (new file over {2}; split it)' -f $row.path, $row.lines, $lineLimit)
    } elseif ($row.lines -gt $allowed[$row.path]) {
      $problems += ('{0}: {1} lines, grew from {2} (hotspots must not grow)' -f $row.path, $row.lines, $allowed[$row.path])
    }
  }
  Write-Host ("line limit {0}: {1} files over, {2} problems" -f $lineLimit, $current.Count, $problems.Count)
  foreach ($p in $problems) { Write-Host "  $p" }
  if ($problems.Count -gt 0) { exit 1 }
  exit 0
}
$build = Join-Path $root 'build'
$junit = Join-Path $build 'release-tests.xml'
$failedLog = Join-Path $build 'Testing\Temporary\LastTestsFailed.log'
$costLog = Join-Path $build 'Testing\Temporary\CTestCostData.txt'
$nowPath = Join-Path $root '.codex\NOW.md'

function Count-Lines([string]$path) {
  if (-not (Test-Path -LiteralPath $path)) { return $null }
  return @(Get-Content -LiteralPath $path -Encoding UTF8).Count
}

function Count-TodoFixme() {
  $n = 0
  foreach ($dir in @('src', 'tests')) {
    $base = Join-Path $root $dir
    if (-not (Test-Path -LiteralPath $base)) { continue }
    Get-ChildItem -LiteralPath $base -Recurse -File | Where-Object { $_.Extension -in '.cpp', '.h', '.hpp' } | ForEach-Object {
      $n += @(Select-String -LiteralPath $_.FullName -Pattern 'TODO|FIXME' -AllMatches).Count
    }
  }
  return $n
}

$tests = [ordered]@{
  source = 'missing'
  total = $null
  passed = $null
  failed = $null
  failedNames = @()
}
if (Test-Path -LiteralPath $junit) {
  [xml]$xml = Get-Content -LiteralPath $junit -Raw
  $suites = @($xml.SelectNodes('//testsuite'))
  $failedNames = New-Object System.Collections.Generic.List[string]
  $total = 0
  $failed = 0
  foreach ($suite in $suites) {
    $cases = @($suite.SelectNodes('testcase'))
    if ($cases.Count -eq 0) {
      $total += [int]$suite.tests
      $failed += [int]$suite.failures + [int]$suite.errors
      continue
    }
    foreach ($case in $cases) {
      $total++
      $bad = @($case.SelectNodes('failure')).Count + @($case.SelectNodes('error')).Count
      if ($bad -gt 0) {
        $failed++
        $name = [string]$case.name
        if (-not $name) { $name = [string]$case.classname }
        if ($name) { $failedNames.Add($name) }
      }
    }
  }
  $tests.source = 'release-tests.xml'
  $tests.total = $total
  $tests.failed = $failed
  $tests.passed = $total - $failed
  $tests.failedNames = @($failedNames)
} else {
  $lastTest = Join-Path $build 'Testing\Temporary\LastTest.log'
  $lastTestNewer = $false
  if (Test-Path -LiteralPath $lastTest) {
    $lastTestTime = (Get-Item -LiteralPath $lastTest).LastWriteTimeUtc
    $failedTime = [datetime]::MinValue
    if (Test-Path -LiteralPath $failedLog) {
      $failedTime = (Get-Item -LiteralPath $failedLog).LastWriteTimeUtc
    }
    $lastTestNewer = $lastTestTime -ge $failedTime
  }
  if ($lastTestNewer) {
    $raw = Get-Content -LiteralPath $lastTest -Raw
    $found = [regex]::Matches($raw, '(\d+) tests failed out of (\d+)')
    if ($found.Count -gt 0) {
      $sum = $found[$found.Count - 1]
      $failed = [int]$sum.Groups[1].Value
      $total = [int]$sum.Groups[2].Value
      $failedNames = New-Object System.Collections.Generic.List[string]
      if ($failed -gt 0) {
        $tail = $raw.Substring($sum.Index)
        foreach ($m in [regex]::Matches($tail, '(?m)^\s+\d+ - (\S+) \(Failed\)')) {
          $failedNames.Add($m.Groups[1].Value)
        }
      }
      $tests.source = 'Testing/Temporary/LastTest.log'
      $tests.total = $total
      $tests.failed = $failed
      $tests.passed = $total - $failed
      $tests.failedNames = @($failedNames)
    }
  }
}
if ($tests.source -eq 'missing' -and ((Test-Path -LiteralPath $failedLog) -or (Test-Path -LiteralPath $costLog))) {
  $failedNames = New-Object System.Collections.Generic.List[string]
  $staleFailed = $false
  if ((Test-Path -LiteralPath $costLog) -and (Test-Path -LiteralPath $failedLog)) {
    $staleFailed = (Get-Item -LiteralPath $costLog).LastWriteTimeUtc -gt (Get-Item -LiteralPath $failedLog).LastWriteTimeUtc
  }
  if ((Test-Path -LiteralPath $failedLog) -and -not $staleFailed) {
    foreach ($line in Get-Content -LiteralPath $failedLog) {
      if ($line -match '^\d+:(\S+)$') { $failedNames.Add($Matches[1]) }
    }
  }
  $total = $null
  if (Test-Path -LiteralPath $costLog) {
    $total = @(Get-Content -LiteralPath $costLog | Where-Object { $_ -match '^\S+ \d+ ' }).Count
  }
  if ($staleFailed) { $tests.source = 'Testing/Temporary/CTestCostData.txt' }
  else { $tests.source = 'Testing/Temporary/LastTestsFailed.log' }
  $tests.total = $total
  $tests.failed = $failedNames.Count
  if ($null -ne $total) { $tests.passed = $total - $failedNames.Count }
  $tests.failedNames = @($failedNames)
}

$flake = $null
$qa = Join-Path $build 'qa'
if (Test-Path -LiteralPath $qa) {
  $latest = Get-ChildItem -LiteralPath $qa -Directory -Filter 'ctest-flake-*' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
  if ($latest) {
    $summary = Join-Path $latest.FullName 'SUMMARY.md'
    $flake = [ordered]@{
      dir = $latest.Name
      summary = $(if (Test-Path -LiteralPath $summary) { 'present' } else { 'missing' })
    }
  }
}

$hotspots = [ordered]@{
  'src/app/MainWindow.cpp' = Count-Lines (Join-Path $root 'src\app\MainWindow.cpp')
  'src/core/LayerOps.cpp' = Count-Lines (Join-Path $root 'src\core\LayerOps.cpp')
}
$nowBytes = $null
if (Test-Path -LiteralPath $nowPath) { $nowBytes = (Get-Item -LiteralPath $nowPath).Length }

$card = [ordered]@{
  schema = 1
  readOnly = $true
  generatedUtc = [DateTime]::UtcNow.ToString('o')
  tests = $tests
  flake = $flake
  hotspots = $hotspots
  nowMdBytes = $nowBytes
  todoFixme = Count-TodoFixme
  sourcesOver300Lines = @(Get-OversizeSources).Count
}

New-Item -ItemType Directory -Force -Path $build | Out-Null
$out = Join-Path $build 'scorecard.json'
($card | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath $out -Encoding ascii

Write-Host 'ka-hgis scorecard (no CTest run)'
Write-Host ("tests.source  {0}" -f $tests.source)
Write-Host ("tests         passed={0} failed={1} total={2}" -f $tests.passed, $tests.failed, $tests.total)
if ($tests.failedNames.Count -gt 0) {
  Write-Host ("failed        {0}" -f ($tests.failedNames -join ', '))
}
Write-Host ("MainWindow.cpp lines {0}" -f $hotspots['src/app/MainWindow.cpp'])
Write-Host ("LayerOps.cpp lines   {0}" -f $hotspots['src/core/LayerOps.cpp'])
Write-Host ("NOW.md bytes         {0}" -f $nowBytes)
Write-Host ("TODO|FIXME           {0}" -f $card.todoFixme)
Write-Host ("C++ files >300 lines {0}" -f $card.sourcesOver300Lines)
if ($flake) { Write-Host ("flake              {0} summary={1}" -f $flake.dir, $flake.summary) }
else { Write-Host 'flake              none' }
Write-Host ("wrote              {0}" -f $out)
if ($null -eq $tests.total -or $null -eq $tests.failed) { exit 2 }
exit 0
