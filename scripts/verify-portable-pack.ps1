param([string]$OutDir = "")
# Checks a portable folder. Runs on the build PC (default: dist\ka-hgis-portable) and on the
# receiving PC: make-portable.ps1 copies this script into the folder, where it checks itself.
# F178: compares ka-hgis.exe with the SHA256 in PORTABLE-MANIFEST.json and prints whether that
# EXE passed scripts/verify-release.ps1 (verified) or not (unverified). A hash mismatch fails.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$out = if ($OutDir) {
  $OutDir
} elseif (Test-Path -LiteralPath (Join-Path $PSScriptRoot "ka-hgis.exe")) {
  $PSScriptRoot
} else {
  Join-Path $root "dist\ka-hgis-portable"
}
foreach ($n in @("ka-hgis.exe", "start.bat", "run.ps1", "README.txt", "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "qgis_core.dll", "qgis_gui.dll", "Qt6Core.dll", "vcruntime140.dll", "msvcp140.dll",
    "curl-ca-bundle.crt", "QtWebEngineProcess.exe", "Qt6WebEngineCore.dll",
    "apps/Qt6/resources/icudtl.dat", "apps/Qt6/resources/qtwebengine_resources.pak",
    "apps/Qt6/resources/qtwebengine_resources_100p.pak", "apps/Qt6/resources/qtwebengine_resources_200p.pak",
    "apps/Qt6/resources/v8_context_snapshot.bin", "apps/Qt6/translations/qtwebengine_locales/ko.pak",
    "apps/Qt6/translations/qtwebengine_locales/en-US.pak", "apps/Qt6/translations/qtbase_ko.qm",
    "apps/qgis-dev/plugins/provider_wms.dll")) {
  $p = Join-Path $out $n
  if (-not (Test-Path -LiteralPath $p)) { throw "portable missing $n" }
}
# A redistributable runtime must also validate without personal API keys/accounts.
if (-not (Test-Path (Join-Path $out "share\proj\proj.db"))) { throw "portable missing proj.db" }
if (-not (Test-Path (Join-Path $out "apps\Qt6\plugins\platforms\qwindows.dll"))) { throw "portable missing qwindows.dll" }

# Licence material added with F138. Older packs lack it: warn, do not fail.
foreach ($n in @("COPYING", "licenses/BUNDLED-COMPONENTS.txt", "source/ka-hgis-source.zip")) {
  if (-not (Test-Path -LiteralPath (Join-Path $out $n))) { Write-Warning "portable has no $n (made before F138?)" }
}

$manifestPath = Join-Path $out "PORTABLE-MANIFEST.json"
$exeHash = (Get-FileHash -LiteralPath (Join-Path $out "ka-hgis.exe") -Algorithm SHA256).Hash
if (Test-Path -LiteralPath $manifestPath) {
  $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
  if ($manifest.executableSha256 -ne $exeHash) {
    throw ("ka-hgis.exe does not match PORTABLE-MANIFEST.json (manifest {0}, file {1}). " +
      "The EXE was replaced or damaged after packaging.") -f $manifest.executableSha256, $exeHash
  }
  Write-Host ("ka-hgis.exe SHA256 {0} matches the manifest" -f $exeHash)
  Write-Host ("release status: {0} - {1}" -f $manifest.releaseStatus, $manifest.releaseStatusNote)
  if ($manifest.releaseStatus -ne 'verified') {
    Write-Warning "This EXE did not pass scripts/verify-release.ps1 (unverified). It may still work; treat it as a test build."
  }
  if ($manifest.credentialsIncluded) {
    Write-Warning "This is a personal portable: config holds API keys/account passwords. Do not pass it on."
  }
} else {
  Write-Warning "No PORTABLE-MANIFEST.json (made before F178). ka-hgis.exe SHA256 $exeHash"
}
Write-Host "portable pack verification passed"
exit 0
