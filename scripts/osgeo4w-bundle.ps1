# 기준 PC의 OSGeo4W 폴더를 통째로 옮겨 다른 PC도 같은 qgis-dev 판을 쓰게 한다.
# install-deps.ps1 은 설치한 날의 qgis-dev 를 받으므로 PC마다 판이 달라진다.
# 지난 판은 다시 받을 수 없어서 설치 대신 복사한다.
#
#   기준 PC: .\scripts\osgeo4w-bundle.ps1 -Export E:\ka-hgis-sdk
#   다른 PC: .\scripts\osgeo4w-bundle.ps1 -Import E:\ka-hgis-sdk [-Root A:\OSGeo4W]
#
# 내보내기 전에 커밋된 dev-env.lock.json 과 같은지 확인한다(-Force 로 건너뜀).
# 가져올 자리에 폴더가 있으면 지우지 않고 <폴더>.before-<시각> 으로 이름만 바꿔 둔다.
param(
  [string]$Export = "",
  [string]$Import = "",
  [string]$Root = "",
  [switch]$Force
)

$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent $PSScriptRoot
$LockScript = Join-Path $PSScriptRoot "dev-env-lock.ps1"
$RepoLock = Join-Path $Repo "dev-env.lock.json"

if ((-not $Export -and -not $Import) -or ($Export -and $Import)) {
  Write-Host "사용법: -Export <번들 폴더> 또는 -Import <번들 폴더> 중 하나를 준다." -ForegroundColor Red
  exit 1
}

function Invoke-Mirror([string]$from, [string]$to) {
  & robocopy $from $to /MIR /COPY:DAT /DCOPY:DAT /MT:16 /R:1 /W:1 /NFL /NDL /NP /NJH
  # robocopy: 0-7 means success (1 = files copied); 8 and above is a failure.
  $code = $LASTEXITCODE
  if ($code -ge 8) { throw ("robocopy 실패 (종료 코드 {0}): {1} -> {2}" -f $code, $from, $to) }
  $global:LASTEXITCODE = 0
}

# Which root dev-env.ps1 would pick on this PC.
function Get-DevEnvRoot {
  foreach ($candidate in @($env:OSGEO4W_ROOT, "C:\OSGeo4W", "D:\OSGeo4W", "A:\OSGeo4W")) {
    if ($candidate -and (Test-Path -LiteralPath $candidate)) { return $candidate }
  }
  return $null
}

if ($Export) {
  $source = if ($Root) { $Root } else { Get-DevEnvRoot }
  if (-not $source -or -not (Test-Path -LiteralPath (Join-Path $source "etc\setup\installed.db"))) {
    Write-Host ("OSGeo4W 를 찾지 못했다: {0}" -f $source) -ForegroundColor Red
    exit 1
  }
  & $LockScript -OsgeoOnly -OsgeoRoot $source
  $lockCode = $LASTEXITCODE
  if ($lockCode -ne 0 -and -not $Force) {
    Write-Host "번들은 커밋된 잠금과 같은 판이어야 한다. 먼저 .\scripts\dev-env-lock.ps1 -Write 로 잠금을 갱신해 커밋한다." -ForegroundColor Red
    exit 1
  }
  $target = Join-Path $Export "OSGeo4W"
  New-Item -ItemType Directory -Force -Path $Export | Out-Null
  Write-Host ("복사: {0} -> {1}" -f $source, $target) -ForegroundColor Cyan
  Invoke-Mirror $source $target
  if (Test-Path -LiteralPath $RepoLock) {
    Copy-Item -LiteralPath $RepoLock -Destination (Join-Path $Export "dev-env.lock.json") -Force
  }
  Write-Host ("내보냈다: {0}" -f $Export) -ForegroundColor Green
  Write-Host "다른 PC에서: .\scripts\osgeo4w-bundle.ps1 -Import <이 폴더>"
  exit 0
}

# --- Import ---
$bundleSdk = Join-Path $Import "OSGeo4W"
if (-not (Test-Path -LiteralPath (Join-Path $bundleSdk "etc\setup\installed.db"))) {
  Write-Host ("번들에 OSGeo4W 가 없다: {0}" -f $bundleSdk) -ForegroundColor Red
  exit 1
}
$target = $Root
if (-not $target) {
  # Default to the reference PC's location so every path stays the same.
  $bundleLock = Join-Path $Import "dev-env.lock.json"
  if (Test-Path -LiteralPath $bundleLock) {
    $target = [string](Get-Content -LiteralPath $bundleLock -Raw -Encoding UTF8 | ConvertFrom-Json).paths.osgeo4wRoot
  }
}
if (-not $target) {
  Write-Host "가져올 위치를 정하지 못했다. -Root C:\OSGeo4W 처럼 지정한다." -ForegroundColor Red
  exit 1
}
$pathRoot = [System.IO.Path]::GetPathRoot($target)
if ($pathRoot -and -not (Test-Path -LiteralPath $pathRoot)) {
  Write-Host ("{0} 드라이브가 이 PC에 없다: {1}" -f $pathRoot, $target) -ForegroundColor Red
  Write-Host "  같은 경로를 쓰려면 드라이브를 만든다(예: subst A: D:\drive-a), 아니면 -Root 로 다른 위치를 준다."
  exit 1
}

$target = $target.TrimEnd('\', '/')
if ((Test-Path -LiteralPath $target) -and (Get-ChildItem -LiteralPath $target -Force | Select-Object -First 1)) {
  $asideName = "{0}.before-{1}" -f (Split-Path -Leaf $target), (Get-Date -Format "yyyyMMdd-HHmmss")
  Rename-Item -LiteralPath $target -NewName $asideName
  Write-Host ("기존 폴더는 지우지 않고 옮겨 두었다: {0}" -f (Join-Path (Split-Path -Parent $target) $asideName)) -ForegroundColor Yellow
}

Write-Host ("복사: {0} -> {1}" -f $bundleSdk, $target) -ForegroundColor Cyan
Invoke-Mirror $bundleSdk $target

& $LockScript -OsgeoOnly -OsgeoRoot $target
$lockCode = $LASTEXITCODE

$picked = Get-DevEnvRoot
if (-not $picked -or ([System.IO.Path]::GetFullPath($picked).TrimEnd('\', '/') -ine [System.IO.Path]::GetFullPath($target))) {
  if ($picked) {
    Write-Host ("dev-env.ps1 은 지금 {0} 을(를) 쓴다. 가져온 폴더를 쓰게 하려면:" -f $picked) -ForegroundColor Yellow
  } else {
    Write-Host "dev-env.ps1 은 OSGEO4W_ROOT, C:\OSGeo4W, D:\OSGeo4W, A:\OSGeo4W 만 찾는다. 가져온 폴더를 쓰게 하려면:" -ForegroundColor Yellow
  }
  Write-Host ("  [Environment]::SetEnvironmentVariable('OSGEO4W_ROOT', '{0}', 'User')" -f $target)
  Write-Host "  (새 PowerShell 창부터 적용된다)"
}

if ($lockCode -eq 1) {
  Write-Host "가져온 판이 커밋된 잠금과 다르다. 번들을 만든 뒤 잠금이 바뀌었는지 확인한다." -ForegroundColor Red
  exit 1
}
Write-Host "가져왔다. 다음: .\scripts\dev-env-lock.ps1 로 전체 비교, 그 뒤 .\scripts\bootstrap-dev-pc.ps1" -ForegroundColor Green
exit 0
