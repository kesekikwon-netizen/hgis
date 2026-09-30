# Manual backup push for ka-hgis.
# Copies the current working tree (tracked + untracked, .gitignore respected) into the
# 'backup/auto-save' branch and pushes it, without touching the working tree or index.
#
# Runs only when a person asks for it: double-click scripts/run-auto-push-now.bat or run
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts/auto-git-push.ps1 -Run
# AGENTS.md forbids hidden auto-push and force-push, so there is no scheduled task any more
# (scripts/uninstall-auto-push-task.ps1 removes one installed earlier) and the push is a
# plain fast-forward: every backup commit has the previous backup as its first parent.
# A scheduled task left from the old installer calls this without -Run and does nothing.

param(
  [string]$Remote = "origin",
  [string]$BackupBranch = "backup/auto-save",
  [switch]$Run
)

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$LogDir = Join-Path $RepoRoot "logs"
if (-not (Test-Path $LogDir)) {
  New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}
$LogFile = Join-Path $LogDir "auto-git-push.log"

function Write-Log([string]$Message) {
  $time = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
  $line = "[$time] $Message"
  Write-Output $line
  try {
    Add-Content -Path $LogFile -Value $line -Encoding utf8
    # Trim log if over 100KB
    if ((Get-Item $LogFile).Length -gt 100000) {
      $lines = Get-Content $LogFile -Tail 200
      Set-Content -Path $LogFile -Value $lines -Encoding utf8
    }
  } catch {}
}

if (-not $Run) {
  Write-Log "SKIP: backup push runs only on request (-Run). A scheduled task from the old installer can be removed with scripts/uninstall-auto-push-task.ps1."
  exit 0
}

if (-not (Test-Path (Join-Path $RepoRoot ".git"))) {
  Write-Log "ERROR: Not a git repository: $RepoRoot"
  exit 1
}

# Account/key files must never leave this PC, even when .gitignore misses one.
$secretPattern = '(?i)(^|/)(secrets\.ini|ka-hgis-vworld\.ini|[^/]*-account\.ini|config/[^/]*-local\.ini|[^/]*\.env|git\.env)$'

$tempIndex = Join-Path $env:TEMP "git_autopush_idx_$PID"

try {
  $currentHead = (& git rev-parse HEAD)
  if ($LASTEXITCODE -ne 0 -or -not $currentHead) {
    Write-Log "ERROR: Failed to resolve HEAD"
    exit 1
  }
  $currentHead = $currentHead.Trim()

  $currentBranch = (& git rev-parse --abbrev-ref HEAD)
  if ($currentBranch) { $currentBranch = $currentBranch.Trim() } else { $currentBranch = "detached" }

  $env:GIT_INDEX_FILE = $tempIndex
  & git read-tree HEAD
  & git add -A
  $staged = @(& git -c core.quotepath=false ls-files)
  $blocked = @($staged | Where-Object { $_ -match $secretPattern -and $_ -ne '.env.example' })
  if ($blocked.Count -gt 0) {
    Write-Log ("ERROR: account/key files would be pushed; add them to .gitignore first: " + ($blocked -join ', '))
    exit 1
  }
  $python = Get-Command python -ErrorAction SilentlyContinue
  if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
  if ($python) {
    # secret-scan.py reads `git ls-files`, which follows GIT_INDEX_FILE: the backup tree.
    & $python.Source (Join-Path $PSScriptRoot 'secret-scan.py')
    if ($LASTEXITCODE -ne 0) {
      Write-Log "ERROR: secret scan failed on the backup tree; nothing was pushed"
      exit 1
    }
  } else {
    Write-Log "WARNING: python not found; only file names were checked for secrets"
  }
  $tree = (& git write-tree)
  if ($LASTEXITCODE -ne 0 -or -not $tree) {
    Write-Log "ERROR: Failed to write git tree"
    exit 1
  }
  $tree = $tree.Trim()

  $lastBackupTree = ""
  $lastBackupCommit = (& git rev-parse --verify --quiet "refs/heads/$BackupBranch")
  if ($LASTEXITCODE -eq 0 -and $lastBackupCommit) {
    $lastBackupCommit = $lastBackupCommit.Trim()
    $lastBackupTree = (& git rev-parse "$lastBackupCommit^{tree}")
    if ($lastBackupTree) { $lastBackupTree = $lastBackupTree.Trim() }
  } else {
    $lastBackupCommit = ""
  }

  if ($lastBackupTree -and ($lastBackupTree -eq $tree)) {
    Write-Log "No changes detected since last backup. Skipping push."
    exit 0
  }

  $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
  $msg = "auto-backup: $timestamp (base: $currentBranch @ $currentHead)"
  # First parent = previous backup, so the push below is a fast-forward (no --force).
  $parents = @()
  if ($lastBackupCommit) { $parents += @('-p', $lastBackupCommit) }
  if ($currentHead -ne $lastBackupCommit) { $parents += @('-p', $currentHead) }
  $commit = (& git commit-tree $tree @parents -m $msg)
  if ($LASTEXITCODE -ne 0 -or -not $commit) {
    Write-Log "ERROR: Failed to create backup commit"
    exit 1
  }
  $commit = $commit.Trim()

  & git update-ref "refs/heads/$BackupBranch" $commit

  $pushOutput = & git push $Remote "${BackupBranch}:${BackupBranch}" *>&1 | Out-String
  if ($LASTEXITCODE -eq 0) {
    Write-Log "SUCCESS: Backed up $commit to $Remote/$BackupBranch ($msg)"
  } else {
    Write-Log "WARNING: push was refused ($LASTEXITCODE). The remote backup branch moved; nothing was forced: $($pushOutput.Trim())"
  }
} catch {
  Write-Log "ERROR: $_"
} finally {
  $env:GIT_INDEX_FILE = $null
  if (Test-Path $tempIndex) {
    Remove-Item -Force $tempIndex -ErrorAction SilentlyContinue
  }
}
