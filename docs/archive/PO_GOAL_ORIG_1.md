# GOAL-ORIG-1 — Restore original product (NOT 7-step UI)

**PO:** proxy user / senior GIS+C++ (other terminal = implementer)
**Repo:** C:\Users\권을\Documents\hgis
**OSGeo:** C:\OSGeo4W | **CMake:** C:\CMake\bin
**Rules:** No git commit/push. No destructive git reset of unrelated dirty files.

## CRITICAL CORRECTION (read first)
"7-step workflow rail / WorkflowGuide as product core" was an AGENT MISTAKE.
User asked for **context7** (analysis skill), NOT a 7-step UI.
Do NOT build or expand 7-step workflow panel. **Remove it from the primary UI.**

## Original product the human wanted
Korean field archaeology HGIS replacing ArcGIS:
1. Standalone C++/Qt6 exe linked to QGIS libs (Architecture B) — keep
2. Survey DATA in one GPKG — 5 domain layers (ids stable English):
   survey_area, feature_poly, feature_line, section_line, control_points
   UI labels Korean; logic uses layer_key only
3. Basemap = REFERENCE underlay user CHOOSES (file / OSM / VWorld if key valid)
   NOT auto-spam; NOT survey data; separate legend group "참조 지도"
4. User DRAWS and FILLS attributes (kind/period, GCP meta) — product is digitizing + save
5. Checklist (제26조-class rules) before export
6. Submit package: SHP (+ PDF layouts) in upload CRS EPSG:5179 + MANIFEST
7. Simple field UI: big draw/save/stop, no Mesh/3D/GRASS jungle
8. Work CRS 5186/5187 OK; document honestly (not fake "default 5179" on work layers)

## Current state (PO audit)
- v0.3.0 dirty tree; GOAL-0/1/2 partial fixes (saveEdits, portable path, CSV, map attempts)
- Handoff OVERSTATES completion; 7-step panel still productized wrongly
- Blank/gray map issues; loadSurveyLayers wipe; basemap/data confusion
- VWorld key may be invalid (API InvalidParameterValue) — handle gracefully, OSM fallback
- CTest 2/2 was green after GOAL-2; re-verify after changes

## GOAL-ORIG-1 scope (do all, in order)

### A) Remove false 7-step product UI
- Remove left-dock "7단계 발굴 워크플로우" list from MainWindow primary UI
- WorkflowGuide.cpp may remain as optional internal helper OR unused; must NOT drive main UX
- HANDOFF.md: rewrite section claiming 7-step as core — mark deprecated/misread
- Toolbar/menus carry real actions: New survey, Open, Basemap, Digitize, Save edits, Checklist, Export

### B) Layer model SSOT (DATA vs REFERENCE)
- Legend group **조사 데이터**: only GPKG domain 5 (or user-added survey vectors)
- Legend group **참조 지도**: basemaps only
- loadSurveyLayers: **NEVER removeAllMapLayers()** — replace domain layers only; keep basemaps
- layer identity = custom property ka_hgis/layer_key (English id); never branch on Korean title
- onGeometryCaptured / export / checklist all use layer_key

### C) Basemap = user choice
- No mandatory auto basemap that confuses empty project; on first run optional OSM if nothing visible OR clear status "배경지도 메뉴에서 선택"
- Menu 배경지도: OSM, VWorld base/sat/cadastral (key from VworldSettings only; invalid key = Korean error, no zombie layer)
- Canvas must show tiles when layer valid + network OK (fix gray map root cause if still broken)
- User-drawn survey geometries always render above basemap

### D) Digitize path field-usable
- 구역 / 유구면 / 선 / GPS tools work on correct domain layer
- 편집저장 = commitChanges; 그리기종료 = stop edit; saveProject auto-commits first
- After draw, status names real button 편집저장
- kind/period required for feature_poly via layer_key

### E) Checklist + submit still work
- 도면검수 runs ChecklistEngine
- 제출패키지 blocked if errors > 0
- Package sets success flag; SHP in 5179 when convert path used
- No DXF claims unless implemented

### F) Build + test
```
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
100% required. Fix tests that assumed 7-step UI as product requirement — adapt to DATA/REFERENCE model.

## Report file (required)
Write docs/PO_GOAL_ORIG_1_REPORT.md with:
BUILD / CTEST / A_NO_7STEP / B_LAYERS / C_BASEMAP / D_DIGITIZE / E_EXPORT / EVIDENCE / READY_FOR_ORIG_2
YES only if all gates pass.

## Out of scope
git commit/push; full offline portable DLL vendor; dark mode polish

START NOW. Only GOAL-ORIG-1 until report exists.
