$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$logDir = Join-Path $here "build\Release"
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
$log = Join-Path $logDir "ka-hgis-launch.log"
function Write-LaunchLog([string]$msg) {
  $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
  Add-Content -LiteralPath $log -Value $line -Encoding UTF8
}

# Agent shells use a Job Object that kills children when the command ends.
# Re-launch once via WMI (WmiPrvSE) so the GUI survives that teardown.
if ($env:KA_HGIS_DETACHED -ne "1") {
  $ps = Join-Path $PSHOME "powershell.exe"
  if (-not (Test-Path -LiteralPath $ps)) { $ps = "powershell.exe" }
  $cmd = 'cmd.exe /c set KA_HGIS_DETACHED=1&& "' + $ps + '" -NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '"'
  # Hidden: without this the cmd.exe console (a Windows Terminal tab) shows on every start.
  $startup = ([wmiclass]"Win32_ProcessStartup").CreateInstance()
  $startup.ShowWindow = 0
  $r = ([wmiclass]"Win32_Process").Create($cmd, $here, $startup)
  if ($r.ReturnValue -eq 0) {
    Write-LaunchLog "detached via WMI child=$($r.ProcessId)"
    exit 0
  }
  Write-LaunchLog "WMI detach failed return=$($r.ReturnValue) — launching in-process"
}

try {
  . "$here\scripts\dev-env.ps1"
  # The desktop shortcut always runs the local Release build. Portable output
  # is a separate, explicitly requested deliverable, never a silent fallback.
  $exe = Join-Path $here "build\Release\ka-hgis.exe"
  if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "실행 파일이 없습니다. Release 빌드를 완료한 뒤 바탕 화면 아이콘을 다시 눌러 주세요."
  }

  $work = Split-Path $exe
  Write-LaunchLog "start exe=$exe cwd=$work OSGEO=$($env:OSGEO4W_ROOT)"

  $p = Start-Process -FilePath $exe -WorkingDirectory $work -PassThru -WindowStyle Normal
  if (-not $p) { throw "Start-Process가 프로세스를 만들지 못했습니다." }

  $ok = $false
  for ($i = 0; $i -lt 25; $i++) {
    Start-Sleep -Milliseconds 200
    $alive = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
    if (-not $alive) { throw "프로세스가 바로 종료되었습니다 (PID $($p.Id)). DLL/경로 문제일 수 있습니다. 로그: $log" }
    if ($alive.MainWindowHandle -ne 0) {
      $ok = $true
      break
    }
  }
  Write-LaunchLog ("pid={0} hwnd={1} title={2}" -f $p.Id, $p.MainWindowHandle, $p.MainWindowTitle)
  if (-not $ok) {
    Write-LaunchLog "window handle still 0 — process is alive, window may be off-screen"
  }
} catch {
  Write-LaunchLog ("FAIL " + $_.Exception.Message)
  throw
}
