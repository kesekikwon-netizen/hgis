# ka-hgis: 저장소 밖 파일에서 GitHub 토큰을 읽어 현재 세션에만 넣는다.
#
# 사용법 (점 소스로 실행해야 환경변수가 세션에 남는다):
#   . .\scripts\load-git-env.ps1
#   . .\scripts\load-git-env.ps1 -Check        # 값은 보지 않고 상태만 점검
#
# 기본 파일: %USERPROFILE%\.config\ka-hgis\git.env
# 다른 위치를 쓰려면 KA_HGIS_ENV_FILE 환경변수나 -EnvFile 로 지정한다.
[CmdletBinding()]
param(
  [string]$EnvFile,
  [switch]$Check
)

$ErrorActionPreference = 'Stop'

if (-not $EnvFile) {
  if ($env:KA_HGIS_ENV_FILE) { $EnvFile = $env:KA_HGIS_ENV_FILE }
  else { $EnvFile = Join-Path $env:USERPROFILE '.config\ka-hgis\git.env' }
}

if (-not (Test-Path -LiteralPath $EnvFile)) {
  throw "환경 파일이 없습니다: $EnvFile`n.env.example 을 이 위치로 복사한 뒤 값을 채우세요."
}

$resolved = (Resolve-Path -LiteralPath $EnvFile).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if ($resolved.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase)) {
  throw "환경 파일이 저장소 안에 있습니다: $resolved`n커밋될 위험이 있으니 저장소 밖으로 옮기세요."
}

# 권한 점검: 본인·SYSTEM·Administrators 외에 접근 권한이 있으면 알린다.
$allowed = @("$env:USERDOMAIN\$env:USERNAME", 'NT AUTHORITY\SYSTEM', 'BUILTIN\Administrators')
$others = (Get-Acl -LiteralPath $resolved).Access |
  Where-Object { $allowed -notcontains $_.IdentityReference.Value } |
  ForEach-Object { $_.IdentityReference.Value } | Sort-Object -Unique
if ($others) {
  Write-Warning "다른 계정에 접근 권한이 있습니다: $($others -join ', ')"
  Write-Warning "잠그려면: icacls `"$resolved`" /inheritance:r /grant:r `"$env:USERNAME`:F`""
}

$loaded = @()
foreach ($raw in Get-Content -LiteralPath $resolved) {
  $line = $raw.Trim()
  if (-not $line -or $line.StartsWith('#')) { continue }
  $split = $line.IndexOf('=')
  if ($split -lt 1) { continue }
  $name = $line.Substring(0, $split).Trim()
  $value = $line.Substring($split + 1).Trim().Trim('"')
  if ($name -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') { continue }
  if (-not $value) { continue }
  Set-Item -Path "Env:$name" -Value $value
  $loaded += $name
}

# 값은 절대 출력하지 않는다. 이름과 길이만 알린다.
foreach ($name in $loaded) {
  $len = (Get-Item -Path "Env:$name").Value.Length
  Write-Host ("불러옴: {0} (길이 {1}자, 값 감춤)" -f $name, $len)
}
if (-not $loaded) { Write-Warning '값이 채워진 항목이 없습니다.' }

if ($Check) {
  Write-Host "파일: $resolved"
  if ($env:GH_TOKEN) {
    if ($env:GH_TOKEN -match '^(ghp|gho|ghu|ghs|github_pat)_') { Write-Host '토큰 형식: 정상' }
    else { Write-Warning '토큰 형식이 GitHub 토큰과 다릅니다.' }
  }
  $url = (git -C $repoRoot remote get-url origin 2>$null)
  if ($url -match '@github\.com') { Write-Warning '원격 URL 에 인증 정보가 박혀 있습니다. 토큰을 뺀 주소로 바꾸세요.' }
  elseif ($url) { Write-Host "원격 URL: $url" }
}
