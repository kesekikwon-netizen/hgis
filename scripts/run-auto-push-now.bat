@echo off
setlocal
rem Manual backup push: runs only when you start it. No scheduled task, no force-push.
echo [ka-hgis] Pushing a backup of the working tree to backup/auto-save...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0auto-git-push.ps1" -Run
echo [ka-hgis] Done.
pause
