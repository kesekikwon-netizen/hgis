# Update an existing portable folder only when the user explicitly requests it.
# The desktop icon uses scripts/start-ka-hgis.vbs -> launch.ps1 -> build/Release.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$src = Join-Path $root "build\Release\ka-hgis.exe"
$dstDir = Join-Path $root "dist\ka-hgis-portable"
$dst = Join-Path $dstDir "ka-hgis.exe"
if (-not (Test-Path -LiteralPath $src)) { throw "Release ka-hgis.exe missing. Build first." }
if (-not (Test-Path -LiteralPath $dstDir)) { throw "dist\ka-hgis-portable missing." }

& (Join-Path $PSScriptRoot 'verify-release.ps1') -CheckOnly
if ($LASTEXITCODE -ne 0) { throw 'Release verification did not pass. Portable was not changed.' }

if (Get-Process -Name ka-hgis -ErrorAction SilentlyContinue) {
  throw "HGIS is still running. Close it after saving, then publish again. No process was stopped."
}
. (Join-Path $PSScriptRoot 'dev-env.ps1')
$caBundle = Join-Path $env:OSGEO4W_ROOT 'bin/curl-ca-bundle.crt'
if (-not (Test-Path -LiteralPath $caBundle -PathType Leaf)) {
  throw 'OSGeo4W curl-ca-bundle.crt missing. Portable was not changed.'
}
Copy-Item -LiteralPath $caBundle -Destination $dstDir -Force
& (Join-Path $PSScriptRoot 'copy-webengine-runtime.ps1') -OsgeoRoot $env:OSGEO4W_ROOT -Destination $dstDir
Copy-Item -LiteralPath $src -Destination $dst -Force
$qssSrc = Join-Path $root "data\theme\ka-hgis.qss"
$qssDstDir = Join-Path $dstDir "data\theme"
if (Test-Path -LiteralPath $qssSrc) {
  New-Item -ItemType Directory -Force -Path $qssDstDir | Out-Null
  Copy-Item -LiteralPath $qssSrc -Destination (Join-Path $qssDstDir "ka-hgis.qss") -Force
}
$dongSrc = Join-Path $root "data\korea_dongs.json"
$dongDstDir = Join-Path $dstDir "data"
if (Test-Path -LiteralPath $dongSrc) {
  New-Item -ItemType Directory -Force -Path $dongDstDir | Out-Null
  Copy-Item -LiteralPath $dongSrc -Destination (Join-Path $dongDstDir "korea_dongs.json") -Force
}
$pdb = Join-Path $root "build\Release\ka-hgis.pdb"
if (Test-Path -LiteralPath $pdb) { Copy-Item -LiteralPath $pdb -Destination $dstDir -Force }

foreach ($notice in @('LICENSE', 'COPYING', 'THIRD_PARTY_NOTICES.md')) {
  $noticeSrc = Join-Path $root $notice
  if (Test-Path -LiteralPath $noticeSrc) { Copy-Item -LiteralPath $noticeSrc -Destination $dstDir -Force }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'verify-portable-pack.ps1') -Destination $dstDir -Force

$a = Get-Item -LiteralPath $src
$b = Get-Item -LiteralPath $dst
if ($a.Length -ne $b.Length) { throw "portable exe size mismatch after copy" }
if ((Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash) {
  throw "portable exe hash mismatch after copy"
}
# The EXE changed: record its hash and verify-release status for the receiving PC (F178).
& (Join-Path $PSScriptRoot 'make-portable.ps1') -OutDir $dstDir -RefreshManifestOnly
Write-Host ("published {0} bytes -> {1}" -f $b.Length, $dst)
exit 0
