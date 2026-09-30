# Claude Code / Claude Desktop 한 번 설정 (ka-hgis C++ 개발용)
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\setup-claude.ps1
#   -SkipDesktop  : Claude Desktop MCP 설정은 건드리지 않음
#   -SkipPlugins  : Claude Code 플러그인 설치 생략
# 기존 설정은 .bak-<시각> 으로 백업하고, 기존 MCP 서버는 그대로 둔다.
param([switch]$SkipDesktop, [switch]$SkipPlugins)

$ErrorActionPreference = "Continue"  # native exe stderr must not abort on Windows PowerShell 5.1
$Repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
function Ok($m)   { Write-Host "[OK]   $m" -ForegroundColor Green }
function Warn($m) { Write-Host "[주의] $m" -ForegroundColor Yellow }
function Step($m) { Write-Host "`n== $m ==" -ForegroundColor Cyan }

# ---------------------------------------------------------------- 1. 필수 도구
Step "필수 도구 확인"
$claude = Get-Command claude -ErrorAction SilentlyContinue
if ($claude) { Ok "claude CLI: $($claude.Source)" } else { Warn "claude CLI가 없다. https://code.claude.com/docs 의 Windows 설치(PowerShell: irm https://claude.ai/install.ps1 | iex) 후 다시 실행." }
$node = Get-Command node -ErrorAction SilentlyContinue
if ($node) { Ok "node $(& node --version)" } else { Warn "node가 없다. 훅·Graft·Archify에 필요하다." }
$gitBash = @("$env:ProgramFiles\Git\bin\bash.exe", "${env:ProgramFiles(x86)}\Git\bin\bash.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($gitBash) { Ok "Git Bash: $gitBash" } else { Warn "Git Bash가 없다. Claude Code(Windows)는 Git for Windows가 필요하다." }
$clangd = Get-Command clangd -ErrorAction SilentlyContinue
if (-not $clangd -and (Test-Path "$env:ProgramFiles\LLVM\bin\clangd.exe")) { $clangd = Get-Item "$env:ProgramFiles\LLVM\bin\clangd.exe" }
if ($clangd) {
  Ok "clangd: $(if ($clangd.Source) { $clangd.Source } else { $clangd.FullName })"
  if (-not (Get-Command clangd -ErrorAction SilentlyContinue)) {
    Warn "clangd가 PATH에 없다. clangd-lsp 플러그인은 PATH의 clangd를 쓴다. 사용자 PATH에 '$env:ProgramFiles\LLVM\bin'을 추가한다."
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -notlike "*LLVM\bin*") {
      [Environment]::SetEnvironmentVariable("Path", "$userPath;$env:ProgramFiles\LLVM\bin", "User")
      Ok "사용자 PATH에 LLVM\bin 추가 (새 터미널부터 적용)"
    }
  }
} else { Warn "clangd가 없다. 'winget install LLVM.LLVM' 후 다시 실행." }
if (Test-Path (Join-Path $Repo "build\compile_commands.json")) { Ok "build/compile_commands.json 있음" } else { Warn "compile_commands.json 없음. scripts\gen-compile-commands.ps1 실행 필요." }
if (Test-Path (Join-Path $Repo "build\tooling\graft-source\dist\mcp\tools.js")) { Ok "Graft 설치됨" } else { Warn "Graft 미설치. scripts\setup-graft.ps1 실행 필요." }

# ---------------------------------------------------------------- 2. Claude Code 플러그인
if (-not $SkipPlugins -and $claude) {
  Step "Claude Code 플러그인 (claude-plugins-official)"
  & claude plugin marketplace add anthropics/claude-plugins-official 2>$null | Out-Null
  $plugins = @("clangd-lsp", "context7", "code-review", "commit-commands", "security-guidance", "claude-md-management", "skill-creator")
  Push-Location $Repo
  try {
    foreach ($p in $plugins) {
      & claude plugin install "$p@claude-plugins-official" --scope project 2>&1 | Out-Null
      if ($LASTEXITCODE -ne 0) { & claude plugin install "$p@claude-plugins-official" 2>&1 | Out-Null }
      if ($LASTEXITCODE -eq 0) { Ok "plugin $p" } else { Warn "plugin $p 설치 실패 — Claude Code에서 /plugin 으로 설치" }
    }
  } finally { Pop-Location }
}

# ---------------------------------------------------------------- 3. 사용자 전역 CLAUDE.md
Step "사용자 전역 ~/.claude/CLAUDE.md"
$userClaudeDir = Join-Path $env:USERPROFILE ".claude"
New-Item -ItemType Directory -Force -Path $userClaudeDir | Out-Null
$userMd = Join-Path $userClaudeDir "CLAUDE.md"
if (Test-Path $userMd) {
  Ok "이미 있음 — 건드리지 않음: $userMd"
} else {
  @"
# 전역 선호 (모든 프로젝트)

- 한국어로 답한다. 코드·주석·커밋 메시지·식별자는 영어를 유지해도 된다.
- 결론을 먼저 쓰고 근거(파일:줄, 실행한 명령과 결과)를 붙인다. 확인하지 못한 내용은 따로 표시한다.
- 요청이 모호하면 합리적 가정을 밝히고 진행한 뒤 확인 질문을 한다. 되돌리기 어려운 작업(커밋·푸시·삭제·배포)은 먼저 묻는다.
- C++ 작업: 프로젝트의 CLAUDE.md·빌드 설정·컴파일러가 이 파일보다 우선한다. 최소 변경, 기존 패턴 우선, 새 의존성은 요청 시에만.
- 디버깅은 원인 분석 → 해결 → 재발 방지 순서로 보고한다.
"@ | Set-Content -LiteralPath $userMd -Encoding UTF8
  Ok "작성: $userMd"
}

# ---------------------------------------------------------------- 4. Claude Desktop MCP
if (-not $SkipDesktop) {
  Step "Claude Desktop MCP (claude_desktop_config.json)"
  $candidates = @(Join-Path $env:APPDATA "Claude\claude_desktop_config.json")
  $candidates += Get-ChildItem "$env:LOCALAPPDATA\Packages" -Directory -Filter "Claude_*" -ErrorAction SilentlyContinue |
    ForEach-Object { Join-Path $_.FullName "LocalCache\Roaming\Claude\claude_desktop_config.json" }
  $cfgPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $cfgPath) { $cfgPath = $candidates[0]; New-Item -ItemType Directory -Force -Path (Split-Path $cfgPath) | Out-Null; '{}' | Set-Content $cfgPath -Encoding UTF8 }
  Copy-Item -LiteralPath $cfgPath -Destination "$cfgPath.bak-$stamp"
  Ok "백업: $cfgPath.bak-$stamp"

  $js = @'
const fs = require("fs");
const [cfgPath, repo] = process.argv.slice(2);
const cfg = JSON.parse(fs.readFileSync(cfgPath, "utf8").replace(/^﻿/, "") || "{}");
cfg.mcpServers ??= {};
const add = (name, def) => { if (!cfg.mcpServers[name]) { cfg.mcpServers[name] = def; console.log("added " + name); } else console.log("kept " + name); };
add("hgis_graft", { command: "node", args: [repo + "\\scripts\\graft-mcp.mjs"], env: { DO_NOT_TRACK: "1", CI: "1" } });
add("context7", { command: "cmd", args: ["/c", "npx", "-y", "@upstash/context7-mcp"] });
add("microsoft-learn", { command: "cmd", args: ["/c", "npx", "-y", "mcp-remote", "https://learn.microsoft.com/api/mcp"] });
fs.writeFileSync(cfgPath, JSON.stringify(cfg, null, 2) + "\n");
'@
  $tmp = Join-Path $env:TEMP "ka-hgis-desktop-mcp-$stamp.js"
  [IO.File]::WriteAllText($tmp, $js, (New-Object Text.UTF8Encoding($false)))
  try { & node $tmp $cfgPath $Repo } finally { Remove-Item $tmp -Force -ErrorAction SilentlyContinue }
  Ok "Claude Desktop을 완전히 종료(트레이 포함) 후 다시 열면 적용된다."
}

# ---------------------------------------------------------------- 5. 확인
Step "확인"
if ($node) {
  Push-Location $Repo
  try { & node --test scripts/graft-mcp.test.mjs *> $null; if ($LASTEXITCODE -eq 0) { Ok "graft-mcp 테스트 통과" } else { Warn "graft-mcp 테스트 실패: node --test scripts/graft-mcp.test.mjs" } } finally { Pop-Location }
}
if ($claude) { Push-Location $Repo; try { & claude mcp list } finally { Pop-Location } }
Write-Host "`n끝. 저장소에서 'claude' 실행 → 프로젝트 MCP(.mcp.json) 승인 → /plugin, /mcp, /hooks, /context 로 확인." -ForegroundColor Cyan
