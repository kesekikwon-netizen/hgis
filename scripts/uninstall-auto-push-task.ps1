# Removes ka-hgis-auto-git-push from Windows Task Scheduler.
# An earlier installer registered it as a hidden 30-minute force-push. That installer is gone
# (AGENTS.md: no hidden auto-push, no force-push); run this once on a PC that still has the task.
# Backups are now manual only: scripts/run-auto-push-now.bat.

$TaskName = "ka-hgis-auto-git-push"

Write-Host "Unregistering Windows Scheduled Task: $TaskName..." -ForegroundColor Cyan

& schtasks.exe /delete /tn $TaskName /f
if ($LASTEXITCODE -eq 0) {
  Write-Host "Task successfully removed." -ForegroundColor Green
} else {
  Write-Host "Task was not found or failed to delete." -ForegroundColor Yellow
}
