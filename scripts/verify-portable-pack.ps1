param([string]$OutDir = "")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$out = if ($OutDir) { $OutDir } else { Join-Path $root "dist\ka-hgis-portable" }
foreach ($n in @("ka-hgis.exe", "start.bat", "run.ps1", "README.txt", "LICENSE",
    "qgis_core.dll", "qgis_gui.dll", "Qt6Core.dll", "vcruntime140.dll", "msvcp140.dll",
    "curl-ca-bundle.crt", "QtWebEngineProcess.exe", "Qt6WebEngineCore.dll",
    "apps/Qt6/resources/icudtl.dat", "apps/Qt6/resources/qtwebengine_resources.pak",
    "apps/Qt6/resources/qtwebengine_resources_100p.pak", "apps/Qt6/resources/qtwebengine_resources_200p.pak",
    "apps/Qt6/resources/v8_context_snapshot.bin", "apps/Qt6/translations/qtwebengine_locales/ko.pak",
    "apps/Qt6/translations/qtwebengine_locales/en-US.pak", "apps/qgis-dev/plugins/provider_wms.dll")) {
  $p = Join-Path $out $n
  if (-not (Test-Path -LiteralPath $p)) { throw "portable missing $n" }
}
# A redistributable runtime must also validate without personal API keys/accounts.
if (-not (Test-Path (Join-Path $out "share\proj\proj.db"))) { throw "portable missing proj.db" }
if (-not (Test-Path (Join-Path $out "apps\Qt6\plugins\platforms\qwindows.dll"))) { throw "portable missing qwindows.dll" }
Write-Host "portable pack verification passed"
exit 0
