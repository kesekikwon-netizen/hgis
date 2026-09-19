# GOAL-2 Completion Report

**Date:** 2026-08-07  
**Repo:** `C:\Users\권을\Documents\hgis`  
**Rules:** no commit/push; OSGeo `C:\OSGeo4W`; CMake `C:\CMake\bin`

---

## BUILD

```
cmake --build build --config Release  → OK
  ka-hgis.exe, ka_workflow_tests.exe, ka_hgis_tests.exe
```

## CTEST

```
100% tests passed, 0 tests failed out of 2
  checklist_engine  Passed
  workflow_engine   Passed (includes osmBasemapValidWithExtent)
```

## C_SMOKE

```
scripts/po-smoke-field.ps1  → OK
  ctest 100%
  run-ka-hgis.ps1 --smoke-quit exit 0
```

---

## ROOT_CAUSE

Blank gray canvas with OSM present in the layer tree was multi-factor:

1. **Invalid XYZ/WMS layers still added** — `addXyzBasemap` fell through to GDAL and could keep an invalid raster without failing, so the tree showed a “layer” that never fetched tiles.
2. **Bad CRS token on OSM URI** — `crs=EPSG3857` (no colon) instead of `EPSG:3857`, hurting OTF 3857→5186.
3. **Canvas not forced to project layers/extent** — `addMapLayer(false)` + manual tree insert did not reliably drive `QgsMapCanvas::setLayers` + Korea extent after first show.
4. **No tile User-Agent** — OSM tile policy prefers an identifying UA; now set on `QgsNetworkAccessManager` request preprocessor.
5. **Plugin path not explicit** — `KaApplication` now sets `setPluginPath` + `setupDefaultProxyAndCache` so `provider_wms` loads for XYZ.

Secondary: `loadSurveyLayers` wiped all layers (including basemap) on 새 조사; caller already re-ran `applyStartupMap`, which now re-adds OSM/Carto and zooms Korea again.

---

## A_MAP

| Item | Status | Evidence |
|------|--------|----------|
| Valid-only XYZ add | DONE | invalid layer → error, not tree zombie |
| OSM URI `crs=EPSG:3857` + fallbacks | DONE | OSM → Carto Light if OSM URI fails |
| Canvas sync + Korea zoom | DONE | `syncCanvasToProject` + `zoomToKorea` + deferred refresh 100ms/1.5s |
| User-Agent | DONE | `ka-hgis/0.3 (QGIS-based archaeology field HGIS)` |
| Unit: layer valid + non-empty extent | DONE | `TestWorkflow::osmBasemapValidWithExtent` |
| Status Korean on fail/offline | DONE | status bar messages in `applyStartupMap` |

## B_SURVEY

| Item | Status | Evidence |
|------|--------|----------|
| After 새 조사: Korea extent | DONE | `loadSurveyLayers` + `applyStartupMap` zoom |
| Basemap under survey layers | DONE | basemap re-added after wipe; placed at legend bottom |
| Korean layer titles | DONE | 조사구역/유구면/유구선/단면선/GPS기준점 + `ka_hgis/layer_key` |
| Lookup still works by key | DONE | `layerByName` / ProjectStateBuilder / ExportService resolve custom property |
| Workflow step 1 after survey | DONE | `refreshWorkflowGuide` after load/startup |

## EVIDENCE

Key files:
- `src/core/LayerOps.cpp` — basemap pipeline, network UA, canvas sync, OSM/Carto fallback
- `src/app/KaApplication.cpp` — plugin path, NAM setup, UA
- `src/app/MainWindow.cpp` — startup map, survey Korean names, deferred refresh
- `src/core/ProjectStateBuilder.cpp`, `ExportService.cpp` — layer key resolve
- `tests/test_workflow.cpp` — `osmBasemapValidWithExtent`
- `scripts/po-smoke-field.ps1`

Commands:

```powershell
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\po-smoke-field.ps1
```

Network probe (this PC): OSM tile HTTP 200; Carto HTTP 200.

---

## Residual risk

- **Live pixel proof** of non-gray tiles not captured in this headless session; unit test proves valid XYZ layer + finite extent + Korea CRS extent. PO should confirm tiles paint within ~5s on the networked UI.
- Offline machine will show Korean status error (no silent gray success).
- VWorld WMS (지적/등고) correctly **rejects** invalid keys at construction (stricter than pre-GOAL-2).

## READY_FOR_GOAL3

**YES** — GOAL-2 A–D met; ctest 100%; smoke script green. No git commit.
