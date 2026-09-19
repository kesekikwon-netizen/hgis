# GOAL-ORIG-1 Completion Report

**Date:** 2026-08-07  
**Repo:** `C:\Users\권을\Documents\hgis`  
**Rules:** no git commit/push  

---

## BUILD

```
cmake --build build --config Release → OK
  ka-hgis.exe, ka_core.lib, ka_workflow_tests.exe, ka_hgis_tests.exe
```

## CTEST

```
100% tests passed, 0 tests failed out of 2
  checklist_engine  Passed
  workflow_engine   Passed (14 QtTest cases, 0 failed)
```

---

## A_NO_7STEP

| Item | Status | Evidence |
|------|--------|----------|
| Remove left-dock 7-step list | DONE | No `m_workflowList` / `workflowGuideList` / `onStepClicked` / `refreshWorkflowGuide` in MainWindow |
| WorkflowGuide not primary UX | DONE | Not included/called from MainWindow; remains for unit tests only |
| Handoff rewritten | DONE | `HANDOFF.md` marks 7-step as **DEPRECATED (context7 misread)** |
| README product axis | DONE | Replaced “7단계” section with GPKG digitize + basemap + checklist + 5179 |
| Toolbar real actions | DONE | 파일 / 편집커밋 / 디지타이즈 / 검수·제출; 배경지도 menu |

## B_LAYERS

| Item | Status | Evidence |
|------|--------|----------|
| Legend **조사 데이터** | DONE | `LayerOps::kGroupSurveyData`; domain layers placed there |
| Legend **참조 지도** | DONE | `LayerOps::kGroupReference`; basemaps placed there |
| `loadSurveyLayers` no wipe | DONE | `removeSurveyDomainLayers` only — basemaps kept |
| Identity = `ka_hgis/layer_key` | DONE | `markSurveyLayer` / `findByLayerKey` / digitize/export/checklist resolve by key |
| Korean titles cosmetic only | DONE | UI names 조사구역/유구면/…; logic uses English keys |

## C_BASEMAP

| Item | Status | Evidence |
|------|--------|----------|
| Not mandatory auto-spam | DONE | `applyStartupMap` does **not** force OSM; status: choose from 배경지도 menu |
| Menu OSM/VWorld | DONE | unchanged menu path; invalid key → Korean error, no zombie layer |
| Basemap under survey data | DONE | reference group + place at bottom of group |
| Tiles path from GOAL-2 kept | DONE | UA, valid-only XYZ, Carto fallback still in LayerOps |

## D_DIGITIZE

| Item | Status | Evidence |
|------|--------|----------|
| Tools on domain layers by key | DONE | `layerByKey("survey_area"|feature_poly|…)` |
| 편집저장 / 그리기종료 | DONE | toolbar `editCommitToolbar` |
| saveProject auto-commit | DONE | `saveProject` → `saveEdits` first |
| Status names 편집저장 | DONE | post-draw message |
| kind/period via layer_key | DONE | `onGeometryCaptured` uses `LayerOps::layerKeyOf` |

## E_EXPORT

| Item | Status | Evidence |
|------|--------|----------|
| 도면검수 ChecklistEngine | DONE | toolbar/menu |
| 제출 blocked on errors | DONE | `exportShpPackage` + ExportService `blockOnError` |
| packageCreated flag | DONE | `m_packageCreated` on success |
| No DXF claims in about/UI | DONE | About string SHP/PDF only |

---

## EVIDENCE

Key files:
- `src/app/MainWindow.h/.cpp` — 7-step UI removed; layer_key digitize path
- `src/core/LayerOps.h/.cpp` — legend groups, mark/find/remove domain layers
- `src/core/ProjectStateBuilder.cpp`, `ExportService.cpp` — findByLayerKey
- `HANDOFF.md`, `README.md` — product SSOT corrected

Commands:

```powershell
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

---

## READY_FOR_ORIG_2

**YES** — all ORIG-1 gates A–F pass; ctest 100%; no git commit.
