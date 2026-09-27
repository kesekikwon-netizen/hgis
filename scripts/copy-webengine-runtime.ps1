param(
  [Parameter(Mandatory=$true)][string]$OsgeoRoot,
  [Parameter(Mandatory=$true)][string]$Destination,
  [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$qtRuntimeRoot = Join-Path $OsgeoRoot 'apps/Qt6'
$required = @('bin/QtWebEngineProcess.exe', 'bin/Qt6WebEngineCore.dll',
  'bin/Qt6WebEngineWidgets.dll', 'resources/icudtl.dat',
  'resources/qtwebengine_resources.pak', 'resources/qtwebengine_resources_100p.pak',
  'resources/qtwebengine_resources_200p.pak', 'resources/v8_context_snapshot.bin',
  'translations/qtwebengine_locales/en-US.pak', 'translations/qtwebengine_locales/ko.pak')
foreach ($relative in $required) {
  if (-not (Test-Path -LiteralPath (Join-Path $qtRuntimeRoot $relative) -PathType Leaf)) {
    throw "Qt WebEngine runtime missing: $relative. Output was not changed."
  }
}
if ($CheckOnly) { return }
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
foreach ($relative in @('bin/QtWebEngineProcess.exe', 'bin/Qt6WebEngineCore.dll', 'bin/Qt6WebEngineWidgets.dll')) {
  Copy-Item -LiteralPath (Join-Path $qtRuntimeRoot $relative) -Destination $Destination -Force
}
foreach ($relative in @('resources', 'translations/qtwebengine_locales')) {
  $target = Join-Path $Destination ('apps/Qt6/' + $relative)
  New-Item -ItemType Directory -Force -Path $target | Out-Null
  Get-ChildItem -LiteralPath (Join-Path $qtRuntimeRoot $relative) -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $target -Force
  }
}
# Qt's own buttons (저장 / 저장 안 함 / 취소). KaApplication looks in apps/Qt6/translations
# beside apps/qgis-dev; without the file the buttons stay English.
$qtKorean = Join-Path $qtRuntimeRoot 'translations/qtbase_ko.qm'
if (Test-Path -LiteralPath $qtKorean -PathType Leaf) {
  Copy-Item -LiteralPath $qtKorean -Destination (Join-Path $Destination 'apps/Qt6/translations') -Force
} else {
  Write-Warning 'qtbase_ko.qm missing: Qt standard buttons stay English.'
}
