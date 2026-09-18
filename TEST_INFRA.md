# Test Infrastructure: ka-hgis (Korean Field Archaeology Desktop HGIS)

## 1. Test Philosophy: Opaque-Box, Requirement-Driven Testing

ka-hgis is an archaeological desktop HGIS application built on Architecture B (standalone C++20/Qt6 linking OSGeo4W `qgis_core` and `qgis_gui`). Its primary users are Korean archaeological field researchers who require:
1. **Deterministic Accuracy**: Spatial coordinate transforms (EPSG:5186/5187 -> EPSG:5179), geometry topological validity, and drawing scales must be mathematically exact.
2. **Submission Integrity**: Packages uploaded to the National Cultural Heritage portal must strictly adhere to domain schemas, shapefile CRS definitions (EPSG:5179), SHA-256 manifests, and non-empty layout drawings (`조사도면.pdf`).
3. **Resilience & Fault Tolerance**: Corrupt geometries, boundary CRS conditions, or intermittent map service keys must be intercepted gracefully without data loss or application crashes.

### Core Testing Principles
- **Opaque-Box Verification**: Tests interact strictly through public interfaces, service contracts, and observable artifacts (GeoPackage databases, Shapefiles, PDFs, Manifest files). No white-box coupling to internal private members.
- **Specification-Derived Oracles**: Expected outputs are derived directly from domain requirements (`PROJECT.md`, `ORIGINAL_REQUEST.md`, National Heritage submission guidelines, and QGIS GIS specifications).
- **Zero-Facade Standard**: Every test executes genuine GIS/GDAL/GEOS logic. Mocking or dummy facade functions that bypass real engine execution are strictly prohibited.
- **Adversarial Edge Verification**: Verification explicitly subjects services to self-intersecting geometries, degenerate rings, boundary coordinates, and empty/uncomposed layouts.

---

## 2. Feature Inventory & Test Tier Mapping

Every feature from `PROJECT.md § Feature Inventory` is mapped to an authoritative test tier:

| Feature # | Feature Name | Target Scope | Test Tier | Primary Test Cases |
|---|---|---|---|---|
| F-01 | Layout Composition Validation | `LayoutService::isComposedStudioSheet` | Tier 1, Tier 2 | `TC_T1_LayoutComposition_ValidSheet`, `TC_T2_UncomposedLayout_BlankRejected` |
| F-02 | Checklist Rule Tightening | `ChecklistEngine`, `ProjectStateBuilder` | Tier 1, Tier 3 | `TC_T1_Checklist_RuleEvaluation`, `TC_T3_Checklist_StateBuilder_Roundtrip` |
| F-03 | Submission Package Layout Bundling | `ExportService::exportSubmissionPackage` | Tier 1, Tier 4 | `TC_T1_ExportPackage_BundlesPdfAndShp`, `TC_T4_ExcavationSite_FullPackage_Scenario` |
| F-04 | Streamed Package Hashing | `ExportService::writeSha256Manifest` | Tier 1, Tier 4 | `TC_T1_Manifest_Sha256_ChecksumIntegrity`, `TC_T4_ExcavationSite_FullPackage_Scenario` |
| F-05 | Geometry Auto-Repair Pipeline | Geometry sanitization & GEOS repair | Tier 1, Tier 2 | `TC_T1_Geometry_SanitizeAndRepair`, `TC_T2_DegenerateGeometry_EdgeCases` |
| F-06 | Digitizing Flow Intranet Guard | Intranet upload validation & repair | Tier 2, Tier 3 | `TC_T2_DegenerateGeometry_EdgeCases`, `TC_T3_DigitizeRepair_Save_Reopen_Export` |
| F-07 | VWorld Key Flexible Regex | `LayerOps::withVworldApiKey` | Tier 1, Tier 2 | `TC_T1_VworldKey_FlexibleRegex`, `TC_T2_VworldKey_EmptyAndMalformed` |
| F-08 | Cadastral XML Key Refresh | `LayerOps::refreshVworldApiKeyInLayers` | Tier 1, Tier 3 | `TC_T1_VworldKey_RefreshLayers`, `TC_T3_DigitizeRepair_Save_Reopen_Export` |
| F-09 | UI VWorld Key Sync | Key propagation into project layers | Tier 1, Tier 3 | `TC_T1_VworldKey_RefreshLayers`, `TC_T3_DigitizeRepair_Save_Reopen_Export` |
| F-10 | Hotspot Controller Extraction | `ProjectLifecycleController`, `DigitizingStateController` | Tier 3 | `TC_T3_DigitizeRepair_Save_Reopen_Export` |
| F-11 | Baseline Test Regression Fix | Canvas Mercator/5186 quad stability | Tier 1 | `workflow_engine` baseline regression suites |
| F-12 | Async Submission Export | Non-blocking export and package creation | Tier 1, Tier 4 | `TC_T1_ExportPackage_BundlesPdfAndShp`, `TC_T4_ExcavationSite_FullPackage_Scenario` |
| F-13 | High-DPI Scale Flutter Fix | Canvas DPI calculation & stability | Tier 1 | `TC_T1_CanvasDpi_StabilityCheck` |
| F-14 | Tile Heal Loop Elimination | Debounced tile heal / rate-limiting | Tier 1 | `TC_T1_TileHeal_DebounceCheck` |
| F-15 | In-Memory Tile Cache Tuning | Cache sizing and datasource standardizing | Tier 1 | `TC_T1_TileCache_SettingsVerification` |
| F-16 | 100% CTest & Smoke Pass | Complete test suite & `--smoke-quit` | Tier 1-4 | Complete CTest Suite Execution (15 targets) |
| F-17 | Forensic Integrity Audit | Anti-facade adversarial verification | Tier 1-4 | Verification of actual GDAL/GEOS/QtTest artifacts |

---

## 3. Test Architecture & Structure

```
tests/
├── test_e2e_opaque.cpp       # [NEW] Requirements-driven Opaque-Box E2E Suite (Tiers 1-4)
├── test_checklist.cpp        # Checklist evaluation and basic export tests
├── test_workflow.cpp         # Comprehensive field workflow & layer tests
├── test_save_open.cpp        # Window lifecycle & GPKG persistence tests
├── test_section_layout.cpp   # GeoTIFF section drawing layout tests
├── test_section_studio.cpp   # Section studio interactive tool tests
├── test_dem_trench.cpp       # DEM trench generation & analysis tests
├── test_terrain_3d.cpp       # 3D terrain and contours tests
├── test_georef.cpp           # Map georeferencing engine tests
├── test_buffer.cpp           # Survey area buffer analysis tests
├── test_measure.cpp          # Tape measurement tool tests
├── test_presets.cpp          # Feature symbology preset tests
├── test_recent.cpp           # Recent survey project management tests
├── test_theme.cpp            # Dark/light styling tests
└── test_andong.cpp           # Andong regional pack basemap tests
```

### Test Tier Breakdown

### Tier 1: Feature Coverage (Isolated Happy-Path Checks)
- **Export Package Bundling**: Tests `ExportService::exportSubmissionPackage` on a project with valid survey area, feature lines, and composed `user_sheet`. Verifies output directory contains `survey_area.shp`, `feature_poly.shp`, `조사도면.pdf`, `README_submit.txt`, `encoding.txt`, and `MANIFEST.sha256`.
- **Streamed 64KB Package Hashing**: Tests `ExportService::writeSha256Manifest`. Verifies manifest syntax, SHA-256 formatting (`<hash>  <filename>`), and validates that hash matches direct `QCryptographicHash` re-read.
- **Layout Composition Validation**: Tests `LayoutService::isComposedStudioSheet` with valid map frame, positive scale, and active vector layers.
- **Checklist Engine Evaluation**: Tests `ChecklistEngine::evaluate` against complete valid archaeological project state, confirming zero errors.
- **VWorld Key Propagation**: Tests `LayerOps::refreshVworldApiKeyInLayers` and `LayerOps::withVworldApiKey` across WMTS and WMS layer URI patterns.
- **Geometry Repair Utility**: Tests repair of self-intersecting bow-tie polygons into valid standard polygons.

### Tier 2: Boundary & Corner Cases
- **Zero-Feature Layers**: Verifies export skips empty vector layers without failing the submission package.
- **Uncomposed & Dummy Layout Rejection**: Ensures empty layouts or maps referencing only placeholder memory layers (`layout_blank`) are rejected by `isComposedStudioSheet`.
- **Degenerate Geometries**: Tests handling of degenerate geometries (2-point polygons, collapsed slivers, duplicate coordinates).
- **Coordinate Boundary Extents**: Tests extents on Korean bounding limits (EPSG:5186 / EPSG:5179).
- **Checklist Block-on-Error**: Verifies `exportSubmissionPackage` blocks export when `blockOnError=true` and critical errors are present.

### Tier 3: Cross-Feature Combinations
- **Digitize -> Repair -> Save GPKG -> Reopen -> Refresh Key -> Export**:
  Full lifecycle roundtrip:
  1. Create new survey GPKG via `SurveyProjectFactory`.
  2. Digitize self-intersecting bow-tie polygon into `feature_poly` layer.
  3. Execute geometry repair pipeline (`makeValid`) ensuring validity before commit.
  4. Save project into GPKG workspace.
  5. Reopen GPKG workspace and verify feature attributes and geometry intact.
  6. Refresh VWorld key across project layers.
  7. Create and compose `user_sheet` layout.
  8. Run `ExportService::exportSubmissionPackage` to export EPSG:5179 SHP + PDF + SHA256 manifest.
  9. Inspect output shapefiles and verify reprojected coordinates in EPSG:5179.

### Tier 4: Real-World Archaeological Field Scenarios
- **Multi-Period Excavation Trench with Features & Section Lines**:
  Simulates a real excavation site:
  1. Survey Area: 50m x 50m boundary in EPSG:5186 (Middle Origin).
  2. Trial Trench: Grid trench 20m x 2m.
  3. Multi-Period Features:
     - Bronze Age Pit Dwelling (`feature_poly`, kind="주거지", period="청동기시대").
     - Three Kingdoms Stone-lined Tomb (`feature_poly`, kind="석곽묘", period="삼국시대").
     - Drainage Ditch (`feature_line`, kind="구", period="조선시대").
  4. Section Line: Archaeological balk section line (`section_line`, name="A-A'").
  5. Control Points: 2 reference datum points (`control_points`, name="CP1", "CP2").
  6. Composed Layout: A3 Landscape Drawing Studio sheet with standard scale 1:200, frame grid, north arrow, and title.
  7. Checklist Verification: Project state builder extracts state and verifies all 12+ archaeological rules pass.
  8. Final Delivery: Export submission package and perform forensic verification on all generated files (SHP geometry types, coordinate bounds, PDF non-zero size, manifest hash match).

---

## 4. Runner Instructions & Environment Setup

### Prerequisites
- Compiler: Microsoft Visual Studio 2022 (MSVC 14.44+, C++20, /W4)
- CMake: 3.21+ (Installed in `C:\CMake\bin`)
- OSGeo4W: `D:\OSGeo4W` (or `C:\OSGeo4W` / `A:\OSGeo4W`) containing `qgis-dev`, `Qt6`, `gdal-dev`

### Running Tests

To run the complete test suite including the new opaque-box E2E suite:
```powershell
# Set environment
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1

# Run all CTest targets
ctest --test-dir build -C Release --output-on-failure

# Run only the Opaque-Box E2E suite
ctest --test-dir build -C Release -R e2e_opaque_suite --output-on-failure
```

### Direct Executable Execution
```powershell
# QtTest 결과는 화면에 나오지 않는다. -o 로 파일을 받아서 본다.
.\build\Release\ka_e2e_tests.exe -o result.txt,txt
```

### QtTest 결과 로그 (2026-09-18)

이 Qt 빌드(OSGeo4W Qt 6.11)의 QtTest 는 결과를 표준출력으로 내보내지 않는다.
콘솔·파이프·`cmd` 리다이렉트 모두 빈 출력이고 `-o <파일>,txt` 로만 나온다.
그래서 예전에는 ctest 로그에 `<end of output>` 만 남아 실패 원인을 알 수 없었다.

지금은 모든 검사를 `cmake/run_qtest.cmake` 래퍼로 실행한다. 래퍼가 결과를
`build/test-logs/<검사이름>.txt` 로 받고, **실패했을 때만** 그 내용을 출력하므로
`ctest` 출력에 실패 지점이 그대로 남는다. 검사를 추가할 때는 `add_test` 대신
`ka_add_qtest(<검사이름> <타깃>)` 을 쓴다.

### 검사 격리 규칙

- 실제 앱과 같은 조직/앱 이름(`ka-hgis`)을 쓰지 않는다. 쓰면 WebEngine 프로필과
  설정이 사용자의 실제 폴더에 쌓인다. 검사 전용 이름과
  `QStandardPaths::setTestModeEnabled(true)` 를 쓴다.
- 결과물은 저장소 안에 쓰지 않는다. `QTemporaryDir` 이나 `QDir::temp()` 를 쓴다.
- 고정 이름 폴더를 쓸 때는 시작할 때 지운다. 제출 내보내기는 비어 있지 않은 폴더를
  거부하므로, 남겨 두면 다음 실행부터 계속 실패한다.
