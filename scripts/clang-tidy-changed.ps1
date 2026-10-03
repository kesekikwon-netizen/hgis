# Fail when bugprone/performance clang-tidy findings appear on changed src/tests lines.
# Existing warnings outside the diff stay warnings. Whole-tree WarningsAsErrors stays off.
param(
  [string]$Base = ""
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$db = Join-Path $Root "build\compile_commands.json"
if (-not (Test-Path -LiteralPath $db)) {
  throw "build/compile_commands.json is missing. Build once (scripts/compile-commands.mjs writes it)."
}
if (-not (Select-String -LiteralPath $db -Pattern '/vctoolsdir' -SimpleMatch -Quiet)) {
  throw "compile_commands.json does not pin the MSVC toolset. Rebuild so scripts/compile-commands.mjs rewrites it."
}
$tidy = "C:\Program Files\LLVM\bin\clang-tidy.exe"
$diffPy = "C:\Program Files\LLVM\share\clang\clang-tidy-diff.py"
if (-not (Test-Path -LiteralPath $tidy) -or -not (Test-Path -LiteralPath $diffPy)) {
  throw "LLVM clang-tidy is missing under C:\Program Files\LLVM."
}
$py = Get-Command py -ErrorAction SilentlyContinue
if (-not $py) { throw "Python launcher py.exe was not found." }

$diffArgs = @("diff", "-U0", "--no-color")
if ($Base) { $diffArgs += $Base }
$diffArgs += @("--", "src", "tests")
$diff = & git @diffArgs
if ($LASTEXITCODE -ne 0) { throw "git diff failed: $LASTEXITCODE" }
$cachedArgs = @("diff", "-U0", "--no-color", "--cached", "--", "src", "tests")
$cached = & git @cachedArgs
$patch = @($diff + $cached) -join "`n"
if ($patch -notmatch '(?m)^diff --git ') {
  Write-Host "clang-tidy: src/tests 변경 줄 없음"
  exit 0
}
$patch | & py.exe -3 $diffPy -p1 -path (Join-Path $Root "build") `
  -clang-tidy-binary $tidy `
  -warnings-as-errors "bugprone-*,performance-*"
if ($LASTEXITCODE -ne 0) { throw "clang-tidy failed on changed lines: $LASTEXITCODE" }
Write-Host "clang-tidy: 변경 줄 통과"
exit 0
