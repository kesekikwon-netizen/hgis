# Convert the approved PNG master into Windows icon sizes without changing its artwork.
[CmdletBinding()]
param(
    [string]$SourcePath = (Join-Path $PSScriptRoot '..\data\theme\ka-hgis-app.png'),
    [switch]$UpdateDesktopShortcut,
    # Optional thin outline along the artwork's outer edge in the .ico frames only (e.g. '#6B4226').
    # The PNG master is not changed.
    [string]$BorderColor = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sourceFile = (Resolve-Path -LiteralPath $SourcePath).Path
$sourceHash = (Get-FileHash -LiteralPath $sourceFile -Algorithm SHA256).Hash
$iconPaths = @(
    (Join-Path $repoRoot 'src\app\ka-hgis.ico'),
    (Join-Path $repoRoot 'data\theme\ka-hgis.ico')
)
$sizes = @(16, 20, 24, 32, 48, 64, 128, 256)
Add-Type -AssemblyName System.Drawing
if (-not ('KaHgis.IconDecoder' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace KaHgis {
    public static class IconDecoder {
        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern IntPtr LoadImage(IntPtr instance, string name, uint type, int cx, int cy, uint flags);
        [DllImport("user32.dll")]
        public static extern bool DestroyIcon(IntPtr icon);
        // Paints every opaque pixel that lies within `thickness` pixels of the transparent outside.
        public static void Outline(System.Drawing.Bitmap bitmap, int thickness, System.Drawing.Color color) {
            int w = bitmap.Width, h = bitmap.Height;
            bool[,] solid = new bool[w, h];
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) solid[x, y] = bitmap.GetPixel(x, y).A >= 128;
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                if (!solid[x, y]) continue;
                bool edge = false;
                for (int dy = -thickness; dy <= thickness && !edge; dy++) for (int dx = -thickness; dx <= thickness && !edge; dx++) {
                    if (dx * dx + dy * dy > thickness * thickness) continue;
                    int nx = x + dx, ny = y + dy;
                    edge = nx < 0 || ny < 0 || nx >= w || ny >= h || !solid[nx, ny];
                }
                if (edge) bitmap.SetPixel(x, y, System.Drawing.Color.FromArgb(bitmap.GetPixel(x, y).A, color));
            }
        }
    }
}
'@ -ReferencedAssemblies System.Drawing
}
$master = [Drawing.Image]::FromFile($sourceFile)
try {
    if ($master.Width -ne $master.Height -or $master.Width -lt 256) {
        throw 'The icon master must be square and at least 256 x 256 pixels.'
    }
    $alphaProbe = [Drawing.Bitmap]::new($master)
    try {
        $cornerAlpha = @(
            $alphaProbe.GetPixel(0, 0).A,
            $alphaProbe.GetPixel($alphaProbe.Width - 1, 0).A,
            $alphaProbe.GetPixel(0, $alphaProbe.Height - 1).A,
            $alphaProbe.GetPixel($alphaProbe.Width - 1, $alphaProbe.Height - 1).A
        )
        if (@($cornerAlpha | Where-Object { $_ -ne 0 }).Count -ne 0) {
            throw 'The master must have genuinely transparent outer corners; a painted checkerboard is not transparency.'
        }
    } finally { $alphaProbe.Dispose() }
    $sourceDimensions = '{0}x{1}' -f $master.Width, $master.Height
    $frames = [Collections.Generic.List[byte[]]]::new()
    foreach ($size in $sizes) {
        $bitmap = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $stream = [IO.MemoryStream]::new()
        try {
            $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.Clear([Drawing.Color]::Transparent)
            $graphics.DrawImage($master, [Drawing.Rectangle]::new(0, 0, $size, $size))
            if ($BorderColor) {
                $graphics.Flush()
                $thickness = if ($size -ge 256) { 3 } elseif ($size -ge 128) { 2 } else { 1 }
                [KaHgis.IconDecoder]::Outline($bitmap, $thickness, [Drawing.ColorTranslator]::FromHtml($BorderColor))
            }
            $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
            $frames.Add($stream.ToArray())
        } finally {
            $stream.Dispose()
            $graphics.Dispose()
            $bitmap.Dispose()
        }
    }
} finally {
    $master.Dispose()
}

# ICO directory followed by eight PNG-compressed 32-bit frames (Windows Vista+).
$iconStream = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($iconStream)
try {
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($index = 0; $index -lt $sizes.Count; $index++) {
        $dimension = if ($sizes[$index] -eq 256) { 0 } else { $sizes[$index] }
        $writer.Write([byte]$dimension)
        $writer.Write([byte]$dimension)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$frames[$index].Length)
        $writer.Write([uint32]$offset)
        $offset += $frames[$index].Length
    }
    foreach ($frame in $frames) { $writer.Write($frame) }
    $writer.Flush()
    $iconBytes = $iconStream.ToArray()
} finally {
    $writer.Dispose()
    $iconStream.Dispose()
}
foreach ($iconPath in $iconPaths) {
    [IO.File]::WriteAllBytes($iconPath, $iconBytes)
    $iconHandle = [KaHgis.IconDecoder]::LoadImage([IntPtr]::Zero, $iconPath, 1, 256, 256, 0x10)
    if ($iconHandle -eq [IntPtr]::Zero) { throw "Windows could not load the icon: $iconPath" }
    $decoded = [Drawing.Icon]::FromHandle($iconHandle)
    try {
        if ($decoded.Width -ne 256 -or $decoded.Height -ne 256) {
            throw "Windows could not decode the 256-pixel icon: $iconPath"
        }
    } finally {
        $decoded.Dispose()
        [KaHgis.IconDecoder]::DestroyIcon($iconHandle) | Out-Null
    }
}
if ((Get-FileHash -LiteralPath $sourceFile -Algorithm SHA256).Hash -ne $sourceHash) {
    throw 'The master image changed during conversion.'
}

$shortcutInfo = $null
if ($UpdateDesktopShortcut) {
    $launcher = Join-Path $repoRoot 'scripts\start-ka-hgis.vbs'
    foreach ($requiredFile in @($launcher, (Join-Path $repoRoot 'launch.ps1'), (Join-Path $repoRoot 'build\Release\ka-hgis.exe'))) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "Desktop launch chain is incomplete: $requiredFile"
        }
    }
    $desktop = [Environment]::GetFolderPath('DesktopDirectory')
    $commonDesktop = [Environment]::GetFolderPath('CommonDesktopDirectory')
    $shell = New-Object -ComObject WScript.Shell
    $shortcutPath = $null
    foreach ($directory in @($desktop, $commonDesktop)) {
        if (-not (Test-Path -LiteralPath $directory -PathType Container)) { continue }
        foreach ($item in (Get-ChildItem -LiteralPath $directory -Filter '*.lnk' | Sort-Object Name)) {
            $candidate = $shell.CreateShortcut($item.FullName)
            $isAppTarget = $candidate.TargetPath -in @($launcher, (Join-Path $repoRoot 'build\Release\ka-hgis.exe'))
            if ($item.BaseName -eq '고고학 전용 HGIS' -or $isAppTarget) {
                $shortcutPath = $item.FullName
                break
            }
        }
        if ($shortcutPath) { break }
    }
    if (-not $shortcutPath) {
        $shortcutPath = Join-Path $desktop '고고학 전용 HGIS.lnk'
    }
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $launcher
    $shortcut.Arguments = ''
    $shortcut.WorkingDirectory = $repoRoot
    $shortcut.Description = '필드고고학GIS v2'
    $shortcut.IconLocation = $iconPaths[1] + ',0'
    $shortcut.Save()
    $saved = $shell.CreateShortcut($shortcutPath)
    if ($saved.TargetPath -ne $launcher -or $saved.IconLocation -ne ($iconPaths[1] + ',0')) {
        throw 'Desktop shortcut verification failed.'
    }
    if (-not ('KaHgis.IconShellNotify' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace KaHgis {
    public static class IconShellNotify {
        [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
        public static extern void SHChangeNotify(uint eventId, uint flags, string item1, IntPtr item2);
    }
}
'@
    }
    [KaHgis.IconShellNotify]::SHChangeNotify(0x00002000, 0x0005, $shortcutPath, [IntPtr]::Zero)
    $shortcutInfo = [ordered]@{ path = $shortcutPath; target = $saved.TargetPath; icon = $saved.IconLocation }
}

[ordered]@{
    source = $sourceFile
    sourceDimensions = $sourceDimensions
    sourceSha256 = $sourceHash
    sizes = $sizes
    icons = @($iconPaths | ForEach-Object { [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash; bytes = (Get-Item -LiteralPath $_).Length } })
    desktopShortcut = $shortcutInfo
} | ConvertTo-Json -Depth 5
