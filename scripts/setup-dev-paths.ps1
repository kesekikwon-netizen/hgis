# Point Graft at this checkout. Paths stay on this computer.
# Versions stay in dev-env.lock.json. Does not install OSGeo4W, CMake, or Node.
#
#   .\scripts\setup-dev-paths.ps1
param()

$ErrorActionPreference = "Stop"
$Repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) { throw "node not found. Graft needs the Node major in dev-env.lock.json." }

$lockPath = Join-Path $Repo "dev-env.lock.json"
if (Test-Path -LiteralPath $lockPath) {
  try {
    $lock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $lockNode = [string]$lock.tools.node
    $hereNode = (& node --version).Trim().TrimStart("v")
    $lockMajor = ($lockNode -split '\.')[0]
    $hereMajor = ($hereNode -split '\.')[0]
    if ($lockMajor -and $hereMajor -and $lockMajor -ne $hereMajor) {
      Write-Host ("[경고] Node 주 판이 잠금과 다르다: 잠금 {0} / 이 PC {1}. Graft는 잠금 주 판이 필요하다." -f $lockNode, $hereNode) -ForegroundColor Yellow
    }
  } catch {
    Write-Host ("[경고] dev-env.lock.json 의 Node 판을 읽지 못했다: {0}" -f $_.Exception.Message) -ForegroundColor Yellow
  }
}

function ConvertTo-TomlLiteral([string]$value) {
  return "'" + ($value.Replace("'", "''")) + "'"
}

$tomlDir = Join-Path $Repo ".codex"
New-Item -ItemType Directory -Force -Path $tomlDir | Out-Null
$graftScript = Join-Path $Repo "scripts\graft-mcp.mjs"
$toml = @"
# This checkout's Graft MCP. Written by scripts/setup-dev-paths.ps1. Not committed.
[mcp_servers.hgis_graft]
command = $(ConvertTo-TomlLiteral $node.Source)
args = [$(ConvertTo-TomlLiteral $graftScript)]
cwd = $(ConvertTo-TomlLiteral $Repo)
startup_timeout_sec = 30
tool_timeout_sec = 120
enabled_tools = ['graft_find_code', 'graft_file_api', 'graft_find_all', 'graft_check_freshness']

[mcp_servers.hgis_graft.env]
DO_NOT_TRACK = '1'
CI = '1'
"@
$tomlPath = Join-Path $tomlDir "config.toml"
[System.IO.File]::WriteAllText($tomlPath, $toml.Replace("`r`n", "`n").TrimEnd() + "`n", (New-Object System.Text.UTF8Encoding($false)))

$cursorDir = Join-Path $env:USERPROFILE ".cursor"
New-Item -ItemType Directory -Force -Path $cursorDir | Out-Null
$mcpPath = Join-Path $cursorDir "mcp.json"
$jsPath = Join-Path $env:TEMP "ka-hgis-setup-dev-paths.js"
$js = @'
const fs = require("fs");
const repo = process.argv[2];
const mcpPath = process.argv[3];
const graftScript = process.argv[4];
let mcp = { mcpServers: {} };
if (fs.existsSync(mcpPath)) {
  mcp = JSON.parse(fs.readFileSync(mcpPath, "utf8"));
}
if (!mcp.mcpServers || typeof mcp.mcpServers !== "object" || Array.isArray(mcp.mcpServers)) {
  mcp.mcpServers = {};
}
mcp.mcpServers.hgis_graft = {
  type: "stdio",
  command: "node",
  args: [graftScript],
  cwd: repo,
  env: { DO_NOT_TRACK: "1", CI: "1" }
};
fs.writeFileSync(mcpPath, JSON.stringify(mcp, null, 2) + "\n");
'@
[System.IO.File]::WriteAllText($jsPath, $js, (New-Object System.Text.UTF8Encoding($false)))
try {
  & node $jsPath $Repo $mcpPath $graftScript
  if ($LASTEXITCODE -ne 0) { throw "Failed to write $mcpPath" }
} finally {
  Remove-Item -LiteralPath $jsPath -Force -ErrorAction SilentlyContinue
}

Write-Host ("Graft cwd: {0}" -f $Repo)
Write-Host ("Cursor MCP: {0}" -f $mcpPath)
Write-Host ("Codex config: {0}" -f $tomlPath)
