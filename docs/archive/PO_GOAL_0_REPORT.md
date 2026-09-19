# GOAL-0 Completion Report

**Date:** 2026-08-07  
**Repo:** `C:\Users\권을\Documents\hgis`  
**Implementer:** historical (OpenCode / Sisyphus era; current harness is Grok Build)  
**Rules honored:** no git commit/push; no destructive git reset; OSGeo SSOT=`C:\OSGeo4W`; CMake=`C:\CMake\bin`

---

## BUILD

| Item | Result |
|------|--------|
| Configure | `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON` → OK |
| Build | `cmake --build build --config Release` → OK (`ka-hgis.exe`, `ka_hgis_tests.exe`, `ka_workflow_tests.exe`) |
| CMake `project()` VERSION | **0.3.0** (was 0.2.0); matches `VERSION` file |

## CTEST

```
100% tests passed, 0 tests failed out of 2
  checklist_engine  Passed
  workflow_engine   Passed
```

New/updated coverage in `tests/test_workflow.cpp`:
- empty VWorld key fails (no silent fallback)
- `VworldSettings` empty key path
- `editBufferCommitSurvivesReopen` — draw buffer → `commitChanges` → reopen layer → feature survives

## A_ENV

| Item | Status | Evidence |
|------|--------|----------|
| `scripts/dev-env.ps1` existence-based root | DONE | Prefer `OSGEO4W_ROOT` (if exists) → `C:\OSGeo4W` → `D:\OSGeo4W` |
| `scripts/run-ka-hgis.ps1` sibling exe first | DONE | `Join-Path $here ka-hgis.exe` is first candidate |
| `dist/ka-hgis-portable/run-ka-hgis.ps1` | DONE | Same sibling-first resolve |
| `scripts/build-all.ps1` CMake discover | DONE | `C:\CMake\bin` first in candidates |
| README / portable README C: SSOT | DONE | Documents `C:\OSGeo4W` preferred; not D:-only |
| CMake VERSION 0.3.0 | DONE | `CMakeLists.txt` + `VERSION` |

Verified portable resolve → `dist\ka-hgis-portable\ka-hgis.exe` first when present.

## C_SAVEEDITS

| Item | Status | Evidence |
|------|--------|----------|
| Toolbar → `saveEdits` | DONE | main toolbar action **「편집저장」** → `MainWindow::saveEdits` |
| Menu → `saveEdits` / `stopEdits` | DONE | 파일 menu: 편집 저장 (커밋), 그리기 종료 |
| Toolbar → `stopEdits` | DONE | **「그리기종료」** |
| Digitize status names real button | DONE | post-capture: `툴바 「편집저장」을 누르세요` |
| `stopEdits` status honest | DONE | points to 「편집저장」 not ambiguous 「저장」 |
| `saveProject` auto-commits buffers | DONE | `saveProject()` calls `saveEdits()` first |
| `refreshWorkflowGuide()` after commit | DONE | end of `saveEdits()` |
| Prove commit survives reopen | DONE | `editBufferCommitSurvivesReopen` ctest |

Note: toolbar **「프로젝트저장」** remains for QGS/QGZ project file write (distinct from edit commit).

## D_PACKAGE

| Item | Status | Evidence |
|------|--------|----------|
| `packageCreated=true` after success | DONE | `m_packageCreated = true` in `exportShpPackage` success path |
| Workflow step 7 uses flag | DONE | `WorkflowGuide::evaluate(..., m_packageCreated)` |
| DXF claims removed/qualified in UI | DONE | step 7 hint is `SHP/PDF 패키지(+MANIFEST)`; no `DXF` under `src/` |

## E_VWORLD

| Item | Status | Evidence |
|------|--------|----------|
| Remove `DEFAULT_VWORLD_KEY` | DONE | removed from `LayerOps.h`; grep `src/` = none |
| No silent empty→embedded key | DONE | `requireVworldKey()` returns false + Korean error |
| Basemap uses `VworldSettings` | DONE | `addKoreaBasemap` loads `VworldSettings::loadApiKey()`; UI via `vworldApiKeyOrPrompt()` |
| Empty key → Korean dialog | DONE | `QMessageBox` + status: 도움말 → VWorld API 키 설정 |
| Hardcoded key removed from MainWindow | DONE | no `m_vworldKey` default UUID |
| Startup without key | DONE | falls back to OSM; does not embed production key |

## GAPS_REMAINING (out of GOAL-0 / next goals)

- Full real CSV GCP import + map-click GCP
- True DLL-vendored portable
- Full CRS doc rewrite + workflow click-only semantics
- Vertex edit / snap / section_line
- LocationSearch still has a separate key store path (env/secrets.ini/QSettings); basemap path is VworldSettings-only. `configureVworldKey` writes both stores.
- UI-level e2e of toolbar click not automated (logic covered by unit/workflow tests)

## EVIDENCE

Commands run:

```powershell
cd C:\Users\권을\Documents\hgis
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Key files touched:
- `scripts/dev-env.ps1`, `scripts/run-ka-hgis.ps1`, `scripts/build-all.ps1`, `scripts/make-portable.ps1`
- `dist/ka-hgis-portable/run-ka-hgis.ps1`, `dev-env.ps1`, `README.txt`
- `README.md`, `CMakeLists.txt`
- `src/core/LayerOps.h`, `LayerOps.cpp`, `WorkflowGuide.cpp`
- `src/app/MainWindow.h`, `MainWindow.cpp`
- `tests/test_workflow.cpp`

## READY_FOR_GOAL1

**YES** — all GOAL-0 acceptance gates met:

1. ctest 100%  
2. saveEdits reachable from UI; status text names 「편집저장」  
3. packageCreated can become true after export success  
4. no DEFAULT_VWORLD_KEY silent fallback for basemap  
5. portable runner finds sibling exe  
6. this report file written  

No git commit performed (PO rule).
