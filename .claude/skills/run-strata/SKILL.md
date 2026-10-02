---
name: run-strata
description: Start this worktree's Strata (ka-hgis) build and look at it. Use when you need to run or launch Strata, check a change on the real app screen, take a screenshot of the app window, or confirm a UI fix works outside the tests. Handles build check, smoke, launch via the shortcut path, screenshot to PNG, and closing only the instance it started.
---

# Run Strata and look at it

Run every command from the Bash tool at the worktree root. Never start `build/Release/ka-hgis.exe` raw.

1. **Build is current.** No `build/CMakeCache.txt` → first build: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-now.ps1` (~8 min). Otherwise build `ka-hgis` with the incremental command in CLAUDE.md (on C1060/MSB4018/MSB4166/0x800705AF add `-- /m:1 /p:CL_MPCount=2`). If the person's app runs from this same exe, rename the running exe first (CLAUDE.md).
2. **Smoke.** Run the CLAUDE.md smoke command. Exit code not 0 = failure: stop and debug, do not screenshot.
3. **Launch + screenshot + close:**
   ```
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .claude/skills/run-strata/strata-shot.ps1 -Root . -Out "<scratchpad>/strata.png"
   ```
   Then open the PNG with the Read tool and describe what is on screen.
   - Same exe path already running (the person's app): the script leaves it alone and starts a renamed copy `ka-hgis.run-<time>.exe` (single-instance matches the exe path), deletes the copy afterwards.
   - Otherwise it launches like the desktop shortcut (`scripts/start-ka-hgis.vbs` → `launch.ps1`, OSGeo env, detached).
   - It waits for the 1920-wide main window (the small home guide shows first), captures it with PrintWindow (DPI-aware, works even if covered), prints `window pid=… title=… size=…` and `png: …`.
   - It closes only the pid it started, by WM_CLOSE to that pid's main hwnd (not CloseMainWindow, which can hit a tooltip titled "Strata"). Title with `*` = unsaved: it does NOT close and exits 4; tell the person.
   - Exit codes: 0 ok, 2 no build, 3 no window (read `build/Release/ka-hgis-launch.log`), 4 left open because of `*`.
4. **Need clicks?** Use `-KeepOpen`, load computer-use, `request_access` for `ka-hgis.exe` (or the copy's basename; "Strata" does not resolve). Afterwards close your instance via its window X (choose 「저장 안 함」 for test changes; never save into the person's survey) and delete any `ka-hgis.run-*.exe` you left.
5. **Clean up.** Check `Get-Process ka-hgis*`: only the pids that were there before may remain. Never close or kill an instance you did not start.
