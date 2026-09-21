# Generate real CMake flags and preserve MSVC's implicit includes for editor clangd.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\dev-env.ps1"

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vsWhere)) { throw "VS 2022 Build Tools / vswhere.exe not found." }
$vsInstall = & $vsWhere -latest -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) { throw "VS 2022 C++ tools not found." }
$devShell = Join-Path $vsInstall 'Common7\Tools\Launch-VsDevShell.ps1'
& $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
if (-not $env:INCLUDE) { throw "MSVC/Windows SDK INCLUDE paths were not initialized." }
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
  $ninjaDir = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
  if (Test-Path -LiteralPath (Join-Path $ninjaDir 'ninja.exe')) { $env:PATH = "$ninjaDir;$env:PATH" }
}

Push-Location $Root
try {
  & cmake --preset compiledb
  if ($LASTEXITCODE -ne 0) { throw "CMake compile database configure failed." }
  $dbFile = Join-Path $Root 'build-clangd\compile_commands.json'
  $outDir = Join-Path $Root 'build'
  New-Item -ItemType Directory -Force -Path $outDir | Out-Null
  $outFile = Join-Path $outDir 'compile_commands.json'
  # Windows PowerShell 5 ConvertFrom-Json collapses this array. Python keeps every entry.
  $py = @'
import json, os, sys
src, dst = sys.argv[1], sys.argv[2]
includes = [p for p in os.environ.get("INCLUDE", "").split(";") if p and os.path.isdir(p)]
extra = " ".join('/I"' + p.replace("\\", "/") + '"' for p in includes)
with open(src, encoding="utf-8") as handle:
    entries = json.load(handle)
if not entries:
    raise SystemExit("empty compiler database")
for entry in entries:
    entry["command"] = entry.get("command", "") + " " + extra
with open(dst, "w", encoding="utf-8", newline="\n") as handle:
    json.dump(entries, handle, ensure_ascii=False)
print(len(entries))
'@
  $count = $py | & py.exe -3 - $dbFile $outFile
  if ($LASTEXITCODE -ne 0) { throw "Failed to add MSVC include paths to compile_commands.json." }
  Write-Host "CMake compiler database with MSVC/SDK includes: $outFile ($count entries)"
} finally {
  Pop-Location
}
