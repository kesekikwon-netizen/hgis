# 개발 환경 잠금: 기준 PC의 OSGeo4W 패키지, MSVC, Windows SDK, CMake 판을
# dev-env.lock.json 에 적고, 다른 PC가 그와 같은지 비교한다.
# qgis-dev 는 매일 새로 빌드되므로 설치한 날이 다르면 판도 다르다.
#
#   .\scripts\dev-env-lock.ps1              # 이 PC를 잠금과 비교
#   .\scripts\dev-env-lock.ps1 -Write       # 기준 PC에서 잠금을 다시 쓴다 (그 뒤 커밋)
#   .\scripts\dev-env-lock.ps1 -OsgeoOnly   # OSGeo4W 패키지만 비교
#
# 종료 코드: 0 같음, 1 다름(또는 -Write 실패), 2 잠금 파일 없음
param(
  [switch]$Write,
  [switch]$OsgeoOnly,
  [string]$OsgeoRoot = "",
  [string]$LockPath = ""
)

$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent $PSScriptRoot
if (-not $LockPath) { $LockPath = Join-Path $Repo "dev-env.lock.json" }

function Find-OsgeoRoot {
  if ($OsgeoRoot) { return $OsgeoRoot }
  # Same search order as dev-env.ps1.
  foreach ($candidate in @($env:OSGEO4W_ROOT, "C:\OSGeo4W", "D:\OSGeo4W", "A:\OSGeo4W")) {
    if ($candidate -and (Test-Path -LiteralPath $candidate)) { return $candidate }
  }
  return $null
}

function Get-OsgeoPackages([string]$osgeo) {
  $packages = [ordered]@{}
  if (-not $osgeo) { return $packages }
  $db = Join-Path $osgeo "etc\setup\installed.db"
  if (-not (Test-Path -LiteralPath $db)) { return $packages }
  $found = @{}
  foreach ($line in Get-Content -LiteralPath $db) {
    # Header "INSTALLED.DB <n>", then "<package> <archive> <flag>" per line.
    $parts = $line.Trim() -split '\s+'
    if ($parts.Count -ge 2 -and $parts[0] -ne 'INSTALLED.DB') { $found[$parts[0]] = $parts[1] }
  }
  [string[]]$names = @($found.Keys)
  [Array]::Sort($names, [StringComparer]::Ordinal)
  foreach ($name in $names) { $packages[$name] = $found[$name] }
  return $packages
}

function Get-MsvcToolset {
  $ErrorActionPreference = "Continue"
  $programFilesX86 = ${env:ProgramFiles(x86)}
  if (-not $programFilesX86) { return $null }
  $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path -LiteralPath $vswhere)) { return $null }
  # Same query as bootstrap-dev-pc.ps1.
  $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null |
    Select-Object -First 1
  if (-not $install) { return $null }
  $file = Join-Path $install "VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt"
  if (-not (Test-Path -LiteralPath $file)) { return $null }
  return (Get-Content -LiteralPath $file -TotalCount 1).Trim()
}

function Get-WindowsSdk {
  $programFilesX86 = ${env:ProgramFiles(x86)}
  if (-not $programFilesX86) { return $null }
  $include = Join-Path $programFilesX86 "Windows Kits\10\Include"
  if (-not (Test-Path -LiteralPath $include)) { return $null }
  # The Visual Studio generator uses the newest SDK unless one is named.
  $newest = Get-ChildItem -LiteralPath $include -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
    Sort-Object { [version]$_.Name } -Descending |
    Select-Object -First 1
  if ($newest) { return $newest.Name }
  return $null
}

function Get-ToolVersion([string[]]$names, [string[]]$fallbackPaths = @()) {
  $ErrorActionPreference = "Continue"
  $exe = $null
  foreach ($name in $names) {
    $command = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { $exe = $command.Source; break }
  }
  if (-not $exe) {
    foreach ($path in $fallbackPaths) {
      if ($path -and (Test-Path -LiteralPath $path)) { $exe = $path; break }
    }
  }
  if (-not $exe) { return $null }
  try {
    $text = (& $exe --version 2>&1 | Out-String)
  } catch {
    return $null
  }
  if ($text -match '(\d+\.\d+(\.\d+)?)') { return $Matches[1] }
  return $null
}

function Get-ProgramFilesPath([string]$child) {
  if (-not $env:ProgramFiles) { return $null }
  return Join-Path $env:ProgramFiles $child
}

function Get-Snapshot {
  $osgeo = Find-OsgeoRoot
  return [ordered]@{
    schema   = 1
    required = [ordered]@{
      osgeo4wPackages = Get-OsgeoPackages $osgeo
      msvcToolset     = Get-MsvcToolset
      windowsSdk      = Get-WindowsSdk
      cmake           = Get-ToolVersion @('cmake') @((Get-ProgramFilesPath "CMake\bin\cmake.exe"), "C:\CMake\bin\cmake.exe")
    }
    tools    = [ordered]@{
      git    = Get-ToolVersion @('git')
      node   = Get-ToolVersion @('node')
      python = Get-ToolVersion @('python', 'python3')
      clangd = Get-ToolVersion @('clangd') @((Get-ProgramFilesPath "LLVM\bin\clangd.exe"))
    }
    paths    = [ordered]@{
      repo        = $Repo
      osgeo4wRoot = $osgeo
    }
  }
}

# Leading version components, so "14.44.35207" and "14.44.35211" match at 2.
function Get-VersionPrefix([string]$version, [int]$parts) {
  if (-not $version) { return $null }
  return (($version -split '\.') | Select-Object -First $parts) -join '.'
}

function Get-NormalizedPath([string]$path) {
  if (-not $path) { return $null }
  try { $path = [System.IO.Path]::GetFullPath($path) } catch { }
  return $path.TrimEnd('\', '/')
}

$snapshot = Get-Snapshot

if ($Write) {
  $missing = @()
  if ($snapshot.required.osgeo4wPackages.Count -eq 0) {
    $missing += "OSGeo4W 패키지 목록 (<OSGeo4W>\etc\setup\installed.db)"
  }
  if (-not $snapshot.required.msvcToolset) { $missing += "VS 2022 C++ 도구 (vswhere)" }
  if (-not $snapshot.required.windowsSdk) { $missing += "Windows SDK" }
  if (-not $snapshot.required.cmake) { $missing += "CMake" }
  if ($missing.Count -gt 0) {
    Write-Host "잠금 파일을 쓰지 않았다. 이 PC에서 찾지 못한 것:" -ForegroundColor Red
    foreach ($item in $missing) { Write-Host "  - $item" -ForegroundColor Red }
    exit 1
  }
  $json = $snapshot | ConvertTo-Json -Depth 6
  [System.IO.File]::WriteAllText($LockPath, $json + "`n", (New-Object System.Text.UTF8Encoding($false)))
  Write-Host ("잠금 파일을 썼다: {0}" -f $LockPath) -ForegroundColor Green
  Write-Host ("  OSGeo4W 패키지 {0}개 ({1})" -f $snapshot.required.osgeo4wPackages.Count, $snapshot.paths.osgeo4wRoot)
  Write-Host ("  MSVC {0}, Windows SDK {1}, CMake {2}" -f $snapshot.required.msvcToolset, $snapshot.required.windowsSdk, $snapshot.required.cmake)
  Write-Host "이 파일을 커밋해야 다른 PC가 같은 기준으로 비교한다."
  exit 0
}

if (-not (Test-Path -LiteralPath $LockPath)) {
  Write-Host ("잠금 파일이 없다: {0}" -f $LockPath) -ForegroundColor Yellow
  Write-Host "기준 PC에서 .\scripts\dev-env-lock.ps1 -Write 를 실행하고 dev-env.lock.json 을 커밋한다."
  exit 2
}

$lock = Get-Content -LiteralPath $LockPath -Raw -Encoding UTF8 | ConvertFrom-Json
$failed = $false

Write-Host ("개발 환경 비교: {0}" -f $LockPath) -ForegroundColor Cyan
Write-Host ("  이 PC의 OSGeo4W: {0}" -f $(if ($snapshot.paths.osgeo4wRoot) { $snapshot.paths.osgeo4wRoot } else { "찾지 못함" }))

# --- OSGeo4W packages: exact archive names ---
$lockPackages = @{}
if ($lock.required.osgeo4wPackages) {
  foreach ($property in $lock.required.osgeo4wPackages.PSObject.Properties) {
    $lockPackages[$property.Name] = [string]$property.Value
  }
}
$herePackages = $snapshot.required.osgeo4wPackages
$allNames = New-Object System.Collections.Generic.HashSet[string]
foreach ($name in $lockPackages.Keys) { [void]$allNames.Add($name) }
foreach ($name in $herePackages.Keys) { [void]$allNames.Add($name) }
[string[]]$sortedNames = @($allNames)
[Array]::Sort($sortedNames, [StringComparer]::Ordinal)
$packageLines = New-Object System.Collections.Generic.List[string]
foreach ($name in $sortedNames) {
  $inLock = $lockPackages.ContainsKey($name)
  $inHere = $herePackages.Contains($name)
  if ($inLock -and -not $inHere) {
    $packageLines.Add(("    없음  {0}  (잠금 {1})" -f $name, $lockPackages[$name]))
  } elseif ($inHere -and -not $inLock) {
    $packageLines.Add(("    추가  {0}  (이 PC {1})" -f $name, $herePackages[$name]))
  } elseif ($lockPackages[$name] -ne $herePackages[$name]) {
    $packageLines.Add(("    다름  {0}  잠금 {1} / 이 PC {2}" -f $name, $lockPackages[$name], $herePackages[$name]))
  }
}
if ($packageLines.Count -eq 0) {
  Write-Host ("[같음] OSGeo4W 패키지 {0}개" -f $lockPackages.Count) -ForegroundColor Green
} else {
  $failed = $true
  Write-Host ("[다름] OSGeo4W 패키지 {0}곳이 잠금과 다르다" -f $packageLines.Count) -ForegroundColor Red
  $limit = 40
  $packageLines | Select-Object -First $limit | ForEach-Object { Write-Host $_ }
  if ($packageLines.Count -gt $limit) { Write-Host ("    ... 외 {0}곳" -f ($packageLines.Count - $limit)) }
  Write-Host "    같은 판은 설치로 다시 받을 수 없다. 기준 PC의 폴더를 옮긴다: .\scripts\osgeo4w-bundle.ps1"
}

function Compare-Entry([string]$label, $lockValue, $hereValue, [int]$parts, [switch]$WarnOnly) {
  $lockText = [string]$lockValue
  $hereText = [string]$hereValue
  if (-not $lockText) {
    Write-Host ("[건너뜀] {0}: 잠금에 값이 없다" -f $label) -ForegroundColor DarkGray
    return $true
  }
  $display = if ($hereText) { $hereText } else { "없음" }
  if ($hereText -and ((Get-VersionPrefix $lockText $parts) -eq (Get-VersionPrefix $hereText $parts))) {
    Write-Host ("[같음] {0} {1}" -f $label, $display) -ForegroundColor Green
    return $true
  }
  $tag = if ($WarnOnly) { "[경고]" } else { "[다름]" }
  $color = if ($WarnOnly) { "Yellow" } else { "Red" }
  Write-Host ("{0} {1}: 잠금 {2} / 이 PC {3}" -f $tag, $label, $lockText, $display) -ForegroundColor $color
  return [bool]$WarnOnly
}

if (-not $OsgeoOnly) {
  # Compiler and SDK decide the build; patch-level MSVC and CMake updates do not.
  if (-not (Compare-Entry "MSVC 도구 모음" $lock.required.msvcToolset $snapshot.required.msvcToolset 2)) { $failed = $true }
  if (-not (Compare-Entry "Windows SDK" $lock.required.windowsSdk $snapshot.required.windowsSdk 4)) { $failed = $true }
  if (-not (Compare-Entry "CMake" $lock.required.cmake $snapshot.required.cmake 2)) { $failed = $true }

  # Helper tools only warn: Graft needs the same Node major, the rest rarely matter.
  [void](Compare-Entry "Git" $lock.tools.git $snapshot.tools.git 2 -WarnOnly)
  [void](Compare-Entry "Node" $lock.tools.node $snapshot.tools.node 1 -WarnOnly)
  [void](Compare-Entry "Python" $lock.tools.python $snapshot.tools.python 2 -WarnOnly)
  [void](Compare-Entry "clangd" $lock.tools.clangd $snapshot.tools.clangd 1 -WarnOnly)

  # Cursor loads Graft from the USER-level MCP config, which holds this PC's paths.
  $userHome = [Environment]::GetFolderPath('UserProfile')
  $mcpFile = Join-Path $userHome ".cursor\mcp.json"
  $graftCwd = $null
  if (Test-Path -LiteralPath $mcpFile) {
    try {
      $mcp = Get-Content -LiteralPath $mcpFile -Raw -Encoding UTF8 | ConvertFrom-Json
      if ($mcp.mcpServers -and $mcp.mcpServers.hgis_graft) { $graftCwd = [string]$mcp.mcpServers.hgis_graft.cwd }
    } catch { }
  }
  if ($graftCwd -and ((Get-NormalizedPath $graftCwd) -ieq (Get-NormalizedPath $Repo))) {
    Write-Host ("[같음] Cursor MCP hgis_graft -> {0}" -f $graftCwd) -ForegroundColor Green
  } else {
    if ($graftCwd) {
      Write-Host ("[경고] Cursor MCP hgis_graft 가 다른 폴더를 가리킨다: {0}" -f $graftCwd) -ForegroundColor Yellow
    } else {
      Write-Host ("[경고] Cursor MCP 에 hgis_graft 가 없다: {0}" -f $mcpFile) -ForegroundColor Yellow
    }
    Write-Host "    이 체크아웃 경로로 넣는다: .\scripts\setup-dev-paths.ps1"
  }

  if ($lock.paths -and $lock.paths.osgeo4wRoot -and
      ((Get-NormalizedPath $lock.paths.osgeo4wRoot) -ine (Get-NormalizedPath $snapshot.paths.osgeo4wRoot))) {
    Write-Host ("[정보] OSGeo4W 위치가 기준 PC와 다르다: 잠금 {0} / 이 PC {1}" -f $lock.paths.osgeo4wRoot, $snapshot.paths.osgeo4wRoot) -ForegroundColor DarkGray
  }
}

if ($failed) {
  Write-Host "결과: 기준 PC와 다르다. 위 [다름] 항목을 맞춘 뒤 다시 실행한다." -ForegroundColor Red
  exit 1
}
Write-Host "결과: 기준 PC와 같다." -ForegroundColor Green
exit 0
