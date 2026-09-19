# GOAL-2 — First paint must show a real map (PO order)

**From:** PO (verified GOAL-1 on real app)
**To:** Implementer
**Repo:** C:\Users\권을\Documents\hgis
**Rules:** No commit/push. OSGeo C:\OSGeo4W. CMake C:\CMake\bin.

## PO GOAL-1 verification (PASS)
- ctest 2/2 re-run PASS
- UIA shows ALL 7 workflow steps (1..7) — clip fixed
- Toolbars: 파일 | 편집커밋(편집저장,그리기종료) | 디지타이즈 | 검수·제출
- VWorld panel collapsed
- Status mentions VWorld key path

## PO GOAL-1 FAIL still open (ship blocker for first impression)
**Map canvas is solid gray.** OSM layer exists in layer tree but tiles do not paint.
Field user opens app → empty gray → product looks broken.

## GOAL-2 scope

### A) Diagnose and fix blank basemap (root cause)
Likely causes to check in order:
1. Project CRS EPSG:5186 vs XYZ EPSG:3857 transform / destination CRS on canvas
2. QgsRasterLayer XYZ URI encoding / wms provider registration
3. zoomToKorea extent wrong for current CRS (too zoomed / inverted / null)
4. canvas freeze/refresh not called after layer add
5. Network/user-agent (OSM blocks empty UA) — set proper tile User-Agent if QGIS supports
6. Fallback: if OSM fails to get valid extent after 2s, try Carto light or another XYZ; show status error with reason

Acceptance:
- Fresh launch screenshot-equivalent: map shows recognizable Korea basemap tiles (not solid gray) within 5s on this networked PC.
- Layer tree OSM (or fallback) checked visible.
- Status bar: either silent success or clear Korean error if offline.

### B) New survey first experience
- After 새 조사 creates GPKG: canvas zooms to work CRS Korea extent; basemap still visible under survey layers.
- Survey layers appear in layer tree with Korean names.
- Workflow step 1 becomes complete after project CRS set / survey created.

### C) Smoke script (headless where possible)
Add `scripts/po-smoke-field.ps1` that:
1. builds if needed / assumes Release
2. runs ctest
3. launches `--smoke-quit` or equivalent exit 0
4. optionally runs a Qt test that creates OSM layer + asserts `layer->isValid()` and non-empty extent

### D) ctest 100% still

## Report docs/PO_GOAL_2_REPORT.md
BUILD/CTEST/A_MAP/B_SURVEY/C_SMOKE/ROOT_CAUSE/EVIDENCE/READY_FOR_GOAL3

START NOW.
