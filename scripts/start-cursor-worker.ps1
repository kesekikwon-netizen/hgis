# My Machines worker for cursor.com/agents. The bundled CLI node is v24 and
# cannot load better-sqlite3, so this uses the system Node 22 binary.
$ErrorActionPreference = "Stop"
$node = (Get-Command node -ErrorAction Stop).Source
$version = & $node --version
if ($version -notlike "v22.*") {
  throw "System node is $version. This worker needs Node 22 because the Cursor CLI native module is ABI 127."
}
$index = Join-Path $env:LOCALAPPDATA "cursor-agent\versions\2026.09.18-9a7762b\index.js"
if (-not (Test-Path -LiteralPath $index)) {
  throw "Cursor agent index.js is missing. Reinstall with: irm 'https://cursor.com/install?win32=true' | iex"
}
$root = Split-Path -Parent $PSScriptRoot
$logDir = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
Start-Process -FilePath $node -ArgumentList @(
  $index, "worker", "start", "--name", "ka-hgis-pc", "--worker-dir", $root
) -WorkingDirectory $root -WindowStyle Hidden `
  -RedirectStandardOutput (Join-Path $logDir "cursor-worker.out.log") `
  -RedirectStandardError (Join-Path $logDir "cursor-worker.err.log")
Write-Host "ka-hgis-pc worker started. Log: build\cursor-worker.err.log"
