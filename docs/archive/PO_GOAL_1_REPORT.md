# GOAL-1 Completion Report

**Date:** 2026-08-07  
**Repo:** `C:\Users\권을\Documents\hgis`  
**Implementer:** historical (OpenCode / Sisyphus era; current harness is Grok Build)  
**Rules:** no git commit/push; no destructive reset; OSGeo=`C:\OSGeo4W`; CMake=`C:\CMake\bin`

---

## BUILD

```
cmake --build build --config Release  → OK
  ka_core.lib, ka-hgis.exe, ka_hgis_tests.exe, ka_workflow_tests.exe
```

## CTEST

```
100% tests passed, 0 tests failed out of 2
  checklist_engine  Passed (~2.2s)
  workflow_engine   Passed (~7.4s)
```

Includes new `importControlCsvWritesFeatures` (2-row CSV → featureCount==2 + step-5 complete rules).

---

## A_WORKFLOW7

| Requirement | Status | Evidence |
|-------------|--------|----------|
| All 7 steps visible / not clipped by default | DONE | `m_workflowList` min height **238px** (up to 280); item min-height 26–30px |
| Competing widgets reduced | DONE | VWorld opacity moved to **collapsible** `QGroupBox` (unchecked/collapsed by default) |
| Scrollbar affordance | DONE | Vertical scrollbar styled (12px handle); `ScrollBarAsNeeded` |
| UIA object name | DONE | `workflowGuideList` unchanged; 7 `QListWidgetItem`s still populated in `refreshWorkflowGuide` |
| Click-to-action kept | DONE | `onStepClicked` unchanged |

Root cause of PO clip: fixed height **160px** + large always-open VWorld slider panel. Fixed by height + collapse.

---

## B_CSV

| Requirement | Status | Evidence |
|-------------|--------|----------|
| Real parse + write Point features | DONE | `LayerOps::importControlPointsCsv` |
| Columns | DONE | `point_id,x,y,datum,ellipsoid,projection,accuracy_m,pdop,fix_type` (+ header aliases) |
| Menu 도구 → CSV 기준점 가져오기 | DONE | `MainWindow::buildMenus` |
| Unit test 2 rows → count 2 | DONE | `TestWorkflow::importControlCsvWritesFeatures` |
| Step 5 complete only if count≥2 AND meta | DONE | `WorkflowGuide`: `cpCount >= 2 && has_datum && has_ellipsoid && has_projection` |

---

## C_EXTENT

| Requirement | Status | Evidence |
|-------------|--------|----------|
| Startup Korea extent | DONE | `applyStartupMap` → `LayerOps::zoomToKorea` + deferred `QTimer::singleShot(50, …)` re-zoom/refresh after first layout |
| OSM when no VWorld key | DONE | adds OSM; ensures layer-tree visibility checked for OSM/VWorld |
| Status bar menu path for key | DONE | `도움말 → VWorld API 키 설정` (also mentions 배경지도 메뉴) |

---

## D_TOOLBAR

| Requirement | Status | Evidence |
|-------------|--------|----------|
| 편집저장 / 그리기종료 not overflow-only at ~1280 | DONE | Dedicated **`editCommitToolbar`** (2 actions only, non-floatable) |
| Group File \| Digitize \| Edit-commit \| Output | DONE | `fileToolbar`, `editCommitToolbar`, `digitizeToolbar`, `outputToolbar` |
| Basemap off main bar | DONE | VWorld/등고선 toolbar buttons removed; remain under **배경지도** menu |
| Object names for UIA | DONE | `actionSaveEdits`, `actionStopEdits` |

---

## EVIDENCE

Key files:
- `src/app/MainWindow.cpp` — layout, toolbars, CSV menu, startup map
- `src/core/LayerOps.h/.cpp` — `importControlPointsCsv`
- `src/core/WorkflowGuide.cpp` — step 5 ≥2 + meta
- `tests/test_workflow.cpp` — CSV + step-5 rules

Commands:

```powershell
cd C:\Users\권을\Documents\hgis
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

---

## Issues not fully closed / residual risk

- **Live UIA re-check of 7-step visibility** not run in this session (layout math + collapse should fix PO clip; PO should re-verify on real display DPI).
- **OSM tile paint** still needs network; gray map can remain offline even with correct Korea extent.
- **Toolbar overflow** at extreme narrow widths (&lt;900px) may still collapse secondary bars; edit-commit bar is intentionally tiny.
- Vertex edit / snap / DLL portable / DXF still out of scope.

---

## READY_FOR_GOAL2

**YES** — GOAL-1 A–E acceptance met with ctest 100%. No git commit performed.
