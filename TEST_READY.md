# TEST READY — ka-hgis Opaque-Box E2E Test Suite

## Executive Summary
The requirement-driven, opaque-box E2E test suite for ka-hgis has been implemented, compiled under MSVC 2022 /W4 with 0 warnings, and verified through CTest.

- **Test Target**: `ka_e2e_tests` (`tests/test_e2e_opaque.cpp`)
- **CTest Suite**: `e2e_opaque_suite` (Test #15)
- **Framework**: QtTest (Qt 6.11.1) linked against `ka_core`, `qgis_core`, `qgis_gui`, `gdal`, `geos`
- **Result**: **18 passed, 0 failed (100% PASS)** in 9.47s
- **Status**: **VERIFIED & READY**

---

## 1. Test Coverage Summary by Tier

### Tier 1: Feature Coverage (Isolated Happy-Path Checks)
- `testT1_ExportPackage_BundlesPdfAndShp`: Validates export package bundling with EPSG:5186 project, survey area and feature polygons, composed `user_sheet` layout, generating `survey_area.shp`, `feature_poly.shp`, `조사도면.pdf` (>500B with `%PDF-` header), `README_submit.txt`, `encoding.txt`, and `MANIFEST.sha256`.
- `testT1_Manifest_Sha256ChecksumIntegrity`: Verifies SHA-256 manifest generation, ensuring 64KB chunked stream processing and exact hash correspondence for text and binary data.
- `testT1_LayoutComposition_Validation`: Verifies `LayoutService::isComposedStudioSheet` against null projects, projects without `user_sheet`, sheets without `ka_map`, zero-scale maps, and fully-composed sheets.
- `testT1_ChecklistEngine_RuleEvaluation`: Verifies `ChecklistEngine` loads 12+ rules and evaluates zero errors on a complete archaeological project state.
- `testT1_VworldKey_PropagationAndRegex`: Verifies `LayerOps::withVworldApiKey` regex substitution across WMTS, WMS, percent-encoded WMS URLs, and non-VWorld preservation.
- `testT1_Geometry_SanitizeAndRepair`: Tests GEOS validity detection and `makeValid` repair on self-intersecting bow-tie polygons and duplicate vertex removal.
- `testT1_SurveyProjectFactory_StorageInit`: Verifies creation of standard GeoPackage workspaces with valid `survey_area`, `feature_poly`, `feature_line`, `control_points`, and `section_line` tables.

### Tier 2: Boundary & Corner Cases
- `testT2_ZeroFeatureLayers_ExportGraceful`: Verifies graceful handling of empty vector layers (0 features), avoiding 0-byte corrupt shapefile generation while still producing package metadata.
- `testT2_DegenerateGeometry_EdgeCases`: Verifies robustness against 2-point polygon rings, 3-point collinear zero-area slivers, and NaN coordinates without crashing.
- `testT2_CoordinateBounds_Extents`: Verifies Korean Central Belt (EPSG:5186) bounding coordinate transforms to EPSG:5179, and handling of extreme out-of-domain coordinates.
- `testT2_UncomposedLayout_BlankRejection`: Verifies rejection of layout sheets where map layers are empty or only reference placeholder `layout_blank` memory layers.
- `testT2_Checklist_BlockOnError`: Verifies export blocking when `blockOnError=true` and critical checklist errors remain, returning descriptive error messages.
- `testT2_InvalidDestination_DirectoryHandling`: Verifies graceful failure and error reporting when export paths are uncreatable or invalid.

### Tier 3: Cross-Feature Combinations
- `testT3_DigitizeRepair_Save_Reopen_Export`: Full end-to-end lifecycle:
  1. Create GPKG survey database via `SurveyProjectFactory`.
  2. Digitize self-intersecting bow-tie polygon into `feature_poly`.
  3. Run geometry auto-repair pipeline (`makeValid` + dominant single-part extraction).
  4. Commit changes with attributes (`feature_no="1호 주거지"`, `kind="주거지"`, `period="청동기시대"`).
  5. Close and reopen project from disk, verifying attribute and geometry persistence.
  6. Compose drawing layout sheet.
  7. Export submission package to EPSG:5179.
  8. Forensically inspect exported `feature_poly.shp`, validating EPSG:5179 CRS, feature count, and manifest entry.
- `testT3_Checklist_StateBuilder_DynamicSync`: Verifies that live modifications to domain layers (`survey_area`, `control_points`) dynamically reflect in `ProjectStateBuilder::fromProject` JSON and cause checklist rule failure counts to strictly decrease.

### Tier 4: Real-World Archaeological Field Scenarios
- `testT4_MultiPeriodExcavationSite_FullPackage`: Complete simulation of Korean excavation site ("안동 임하리 유적 발굴조사"):
  1. Survey Area: 50m x 40m boundary polygon.
  2. Trial Trench: 20m x 2m trench in `trial_trench`.
  3. Multi-Period Features:
     - Bronze Age semi-subterranean dwelling (`feature_poly`, kind="주거지", period="청동기시대").
     - Three Kingdoms stone-lined tomb (`feature_poly`, kind="석곽묘", period="삼국시대").
     - Historic drainage ditch (`feature_line`, kind="구", period="조선시대").
  4. Stratigraphic Section Line: Balk section line A-A' in `section_line`.
  5. Geodetic Control Points: CP1 and CP2 in `control_points` with elevations.
  6. Composed Layout: A3 Landscape Drawing Studio sheet at scale 1:200 with map frame and all layers.
  7. Submission Package Export: Complete generation of EPSG:5179 SHP suite (`survey_area`, `trial_trench`, `feature_poly`, `feature_line`, `section_line`, `control_points`), `조사도면.pdf` (>1KB), `README_submit.txt`, `encoding.txt`, and `MANIFEST.sha256`.
  8. Forensic verification of all generated files, shapefile CRS, geometry validity, and SHA-256 hashes.

---

## 2. Test Execution & Reproduction

```powershell
# Environment Setup
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1

# Build ka_e2e_tests target
cmake --build build --config Release --target ka_e2e_tests

# Run through CTest
ctest --test-dir build -C Release -R e2e_opaque_suite --output-on-failure

# Direct execution with detailed report
.\build\Release\ka_e2e_tests.exe
```

---

## 3. Discovered Implementation Defects (Escalated to Orchestrator / Developer)

> 상태(2026-09-29): 아래 두 항목은 이 문서를 쓸 때의 기록이다. Defect 1 은 **고쳐졌다** — `ExportService` 가 `encoding.txt` 를 `QSaveFile` 로 쓰고 `commit()` 한 뒤 MANIFEST 를 만든다. Defect 2 의 검사(`zoomToKorea_5186StaysInsideMercatorSatelliteQuad`)는 `workflow_engine` 에 그대로 있으며 현재 결과는 ctest 로그가 정본이다. 스위트 번호("Test #15")와 "18 passed" 는 그때의 숫자다.

### Defect 1 (Resolved — was Critical): Unclosed `encoding.txt` File Handle Causes Corrupt Hash in `MANIFEST.sha256`
- **Location**: `src/core/ExportService.cpp:123-127`
- **Observation**:
  ```cpp
  QFile encf(dir.filePath(QStringLiteral("encoding.txt")));
  if (encf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    encf.write(enc.toUtf8());
    encf.write("\n");
  }
  // encf is NOT flushed or closed before writeSha256Manifest(outDir, &merr) is called at line 137!
  ```
- **Impact**: When `writeSha256Manifest` runs, `encoding.txt` is 0 bytes on disk. The manifest records the SHA-256 of empty content (`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`). When `exportSubmissionPackage` exits, `encf` destructor flushes "UTF-8\n" (7 bytes) to disk. Forensic checksum verification against the package fails because the actual file hash does not match `MANIFEST.sha256`.
- **Recommended Fix**: Add `encf.close();` immediately after writing, matching `f.close();` at line 121 for `README_submit.txt`.

### Defect 2 (Pre-existing Baseline Regression / Milestone M2 Item 11):
- **Location**: `tests/test_workflow.cpp:2914` (`TestWorkflow::zoomToKorea_5186StaysInsideMercatorSatelliteQuad`)
- **Observation**: `vis.xMinimum() + 50.0 >= fill.xMinimum()` fails due to aspect ratio clipping in Mercator satellite fill quad bounds. Assigned to Milestone M2 in `PROJECT.md § Feature Inventory (Item 11)`.
