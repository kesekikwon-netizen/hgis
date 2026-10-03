# Start this worktree's Strata build, screenshot its main window, close only what we started.
# Usage (from Bash, worktree root):
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .claude/skills/run-strata/strata-shot.ps1 -Root . -Out <png> [-WaitSec 90] [-KeepOpen]
param(
  [string]$Root = ".",
  [Parameter(Mandatory = $true)][string]$Out,
  [int]$WaitSec = 90,
  [switch]$KeepOpen
)
$ErrorActionPreference = "Continue"
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$Root = (Resolve-Path $Root).Path
$rel = Join-Path $Root "build\Release"
$exe = Join-Path $rel "ka-hgis.exe"
if (-not (Test-Path $exe)) { Write-Output "FAIL no build: $exe"; exit 2 }

Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class W {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint cmd);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int s);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static string Title(IntPtr h) { var s = new StringBuilder(512); GetWindowText(h, s, 512); return s.ToString(); }
  // Largest visible, unowned top-level window of pid (skips tooltips/popups titled "Strata").
  public static IntPtr FindMain(uint pid) {
    IntPtr best = IntPtr.Zero; long area = 0;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p);
      if (p == pid && IsWindowVisible(h) && GetWindow(h, 4) == IntPtr.Zero) { RECT r; GetWindowRect(h, out r);
        long a = (long)(r.R - r.L) * (r.B - r.T); if (a > area && a > 200000) { area = a; best = h; } }
      return true; }, IntPtr.Zero);
    return best;
  }
}
"@
[W]::SetProcessDPIAware() | Out-Null

function Get-ByPath($p) { Get-Process ka-hgis* -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $p } }
$before = @(Get-Process ka-hgis* -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
Write-Output ("before: " + (($before) -join ","))

# Single-instance rule matches the exe path: if this exe already runs, start a renamed copy.
$copy = $null
$runExe = $exe
if (Get-ByPath $exe) {
  $copy = Join-Path $rel ("ka-hgis.run-{0}.exe" -f (Get-Date -Format "HHmmss"))
  Copy-Item $exe $copy
  $runExe = $copy
  Write-Output "same exe already running (left alone); using copy $copy"
}

if (-not $copy) {
  # Same path as the desktop shortcut: start-ka-hgis.vbs -> launch.ps1 (dev-env, detached).
  Start-Process wscript.exe -ArgumentList ('"' + (Join-Path $Root "scripts\start-ka-hgis.vbs") + '"') -WorkingDirectory $Root
} else {
  # launch.ps1 only knows ka-hgis.exe, so do what it does for the copy: dev-env + detached via WMI.
  $cmd = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ". ''' + (Join-Path $Root "scripts\dev-env.ps1") + '''; Start-Process -FilePath ''' + $runExe + ''' -WorkingDirectory ''' + $rel + '''"'
  $startup = ([wmiclass]"Win32_ProcessStartup").CreateInstance()
  $startup.ShowWindow = 0  # hidden, as in launch.ps1: no console window on the person's screen
  ([wmiclass]"Win32_Process").Create($cmd, $Root, $startup) | Out-Null
}

$proc = $null; $h = [IntPtr]::Zero
$deadline = (Get-Date).AddSeconds($WaitSec)
while ((Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
  $proc = Get-ByPath $runExe | Where-Object { $before -notcontains $_.Id } | Select-Object -First 1
  if ($proc) {
    $h = [W]::FindMain([uint32]$proc.Id)
    # The small home guide (~730x430) shows first; the 1920-wide main window follows. Prefer the main window.
    if ($h -ne [IntPtr]::Zero) { $r0 = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r0) | Out-Null
      if (($r0.R - $r0.L) -ge 1000 -or (Get-Date) -gt $deadline.AddSeconds(-5)) { break } }
  }
}
$rc = 0
if ($h -eq [IntPtr]::Zero) {
  Write-Output "FAIL no main window within $WaitSec s (see build\Release\ka-hgis-launch.log)"; $rc = 3
} else {
  Start-Sleep -Seconds 4   # let the first paint finish
  $r = New-Object W+RECT
  if ([W]::DwmGetWindowAttribute($h, 9, [ref]$r, 16) -ne 0) { [W]::GetWindowRect($h, [ref]$r) | Out-Null }
  $wd = $r.R - $r.L; $ht = $r.B - $r.T
  Write-Output ("window pid={0} title='{1}' size={2}x{3} at {4},{5}" -f $proc.Id, [W]::Title($h), $wd, $ht, $r.L, $r.T)
  $bmp = New-Object System.Drawing.Bitmap $wd, $ht
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc(); $ok = [W]::PrintWindow($h, $hdc, 2); $g.ReleaseHdc($hdc)
  if (-not $ok) { $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size) }
  $g.Dispose()
  $Out = [IO.Path]::GetFullPath($Out)
  New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
  $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  Write-Output "png: $Out"
}

if ($proc -and -not $KeepOpen) {
  $proc.Refresh(); $h2 = [W]::FindMain([uint32]$proc.Id); if ($h2 -ne [IntPtr]::Zero) { $h = $h2 }
  $t = if ($h -ne [IntPtr]::Zero) { [W]::Title($h) } else { "" }
  if ($t.Contains("*")) {
    Write-Output "NOT closing pid $($proc.Id): title has * (unsaved)"; $rc = 4
  } else {
    if ($h -ne [IntPtr]::Zero) { [W]::PostMessage($h, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
    # WM_CLOSE to OUR main hwnd (not CloseMainWindow, which can hit a tooltip titled "Strata").
    if (-not $proc.WaitForExit(20000)) { Stop-Process -Id $proc.Id -Force; Write-Output "WM_CLOSE timed out; stopped our pid $($proc.Id) only" }
    else { Write-Output "closed pid $($proc.Id)" }
  }
}
if ($copy -and -not $KeepOpen) {
  for ($i = 0; $i -lt 20 -and (Test-Path $copy); $i++) { Remove-Item $copy -Force -ErrorAction SilentlyContinue; if (Test-Path $copy) { Start-Sleep -Milliseconds 500 } }
  Write-Output ("copy removed: " + (-not (Test-Path $copy)))
}
Write-Output ("after: " + ((@(Get-Process ka-hgis* -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })) -join ","))
exit $rc
