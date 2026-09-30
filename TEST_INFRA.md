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
| F-10 | Hotspot Controller Extraction | 계획만 했고 만들지 않음(`PROJECT.md`). 컨트롤러 클래스는 없다 | — | — |
| F-11 | Baseline Test Regression Fix | Canvas Mercator/5186 quad stability | Tier 1 | `workflow_engine` baseline regression suites |
| F-12 | Async Submission Export | Non-blocking export and package creation | Tier 1, Tier 4 | `TC_T1_ExportPackage_BundlesPdfAndShp`, `TC_T4_ExcavationSite_FullPackage_Scenario` |
| F-13 | High-DPI Scale Flutter Fix | Canvas DPI calculation & stability | Tier 1 | `TC_T1_CanvasDpi_StabilityCheck` |
| F-14 | Tile Heal Loop Elimination | Debounced tile heal / rate-limiting | Tier 1 | `TC_T1_TileHeal_DebounceCheck` |
| F-15 | In-Memory Tile Cache Tuning | Cache sizing and datasource standardizing | Tier 1 | `TC_T1_TileCache_SettingsVerification` |
| F-16 | 100% CTest & Smoke Pass | Complete test suite & `--smoke-quit` | Tier 1-4 | Complete CTest suite (68 ctest entries from `ka_add_qtest`/`ka_add_qtest_filter`, 2026-09-29) |
| F-17 | Forensic Integrity Audit | Anti-facade adversarial verification | Tier 1-4 | Verification of actual GDAL/GEOS/QtTest artifacts |

---

## 3. Test Architecture & Structure

`tests/` 에는 QtTest 소스 96개가 있다(2026-09-29). 목록과 ctest 이름은 `CMakeLists.txt` 의 `ka_add_qtest(...)` 가 정본이다. 주요 것:

```
tests/
├── test_e2e_opaque.cpp       # Requirements-driven Opaque-Box E2E Suite (Tiers 1-4)
├── test_checklist.cpp        # Checklist evaluation and basic export tests
├── test_workflow.cpp         # Field workflow, layer, schema (data/schemas) tests
├── test_save_open.cpp        # Window lifecycle, GPKG persistence, old-version compat sample
├── test_perf.cpp             # perf_engine budgets (render, open, layout, startup, PDF, save, submit)
├── test_section_layout.cpp   # GeoTIFF section drawing layout tests
├── test_section_studio.cpp   # Section studio interactive tool tests
├── test_dem_trench.cpp       # DEM trench generation & analysis tests
├── test_georef.cpp           # Map georeferencing engine tests
├── test_storage_*.cpp        # Save generations, recovery, hygiene
└── ...                       # topographic_*, heritage_*, dem_*, survey_contour*, ribbon, shell ...
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
- CMake: 3.21+ (`C:\Program Files\CMake\bin`, `C:\CMake\bin` or PATH; versions in `dev-env.lock.json`)
- OSGeo4W: found by `scripts/dev-env.ps1` in the order `OSGEO4W_ROOT` → `C:\OSGeo4W` → `D:\OSGeo4W` → `A:\OSGeo4W`, containing `qgis-dev`, `Qt6`, `gdal-dev`

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
한 실행 파일이 스위트를 둘 돌리면(`ka_survey_contour_tests`: TestSurveyContour + Clip) 두 번째
`qExec` 가 같은 `-o` 파일을 잘라 첫 스위트 결과가 사라졌다. 두 번째 스위트는 `<로그>.clip.txt` 에
쓰고, `cmake/run_qtest.cmake` 가 실패 시 `<로그>.*.txt` 도 함께 찍는다.

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

### 실행 환경 기본값 (2026-09-29, F212)

`cmake/run_qtest.cmake` 래퍼가 모든 검사에 기본 환경을 준다. CTest `ENVIRONMENT` 나 셸이 이미 준 값이 우선한다.

- `PATH` 에 OSGeo4W `qgis-dev`·`Qt6`·`gdal-dev`·`pdal-dev`·`bin` 이 없으면 앞에 붙인다(루트는 빌드 폴더 `CMakeCache.txt` 의 `OSGEO4W_ROOT`). `GDAL_DATA`·`PROJ_LIB`·`QGIS_PREFIX_PATH` 도 비어 있을 때만 채운다. `ENVIRONMENT` 가 없던 `survey_contour`·`survey_scope_clip`·`tile_print`·`download_ui`·`startup_splash` 도 dev-env 없이 돈다.
- `TEMP`/`TMP`: CMake 가 준 `.../ka-hgis-tests-<id>/<검사>` 가 아니면 `build/test-tmp/<검사>` 로 바꾼다. qgis-dev 가 `TEMP` 안에 고정 이름 `qgis-project-*.zip` 을 쓰므로 병렬 ctest 에서 검사끼리 부딪히지 않게 한다.
- `workflow_engine` 의 시간 제한은 CMake 쪽 설정이다(통합 담당에게 요청).

`ka_workflow_tests` 는 `main()` 에서 `QStandardPaths::setTestModeEnabled(true)`, 임시 QGIS 프로필, 임시 `QSettings` IniFormat 경로를 쓴다. 사용자의 AppData·QGIS 프로필 설정을 건드리지 않는다. Windows 레지스트리(`ka-hgis/ka-hgis`)는 옮길 수 없어서, 그것을 쓰는 `vworldSettingsAndNoKeyTests` 가 시작 전 값을 적어 두고 끝날 때 되돌린다.

### 성능 예산 (perf_engine, workflow 라벨 분석)

예산은 기준 PC 값(5회 중앙값 × 1.3 등)이다. 다른 PC 등급에서는 코드를 고치지 않고 올린다.

| 방법 | 예 |
| --- | --- |
| 관문 하나 | `KA_PERF_BUDGET_STARTUP_HOME=120`, `KA_PERF_BUDGET_A3_PDF_EXPORT=40`, `KA_PERF_BUDGET_LABEL_ORDER=0.8` |
| 모든 관문 배수 | `KA_PERF_BUDGET_SCALE=1.5` |
| JSON 파일 | `KA_PERF_BUDGETS=C:\perf\laptop.json` → `{"scale": 1.3, "startup_home": 120, "calibration_ref_ms": 12}` |
| 상대 비교 | `calibration_ref_ms`(또는 `KA_PERF_CALIBRATION_REF_MS`)에 기준 PC 의 교정 중앙값을 적으면, 이 PC 교정 중앙값/기준 값(1 미만은 1)만큼 예산을 늘린다 |

관문 이름: `parcel_render`, `survey_open`, `layout_enter`, `startup_home`, `a3_pdf_export`, `survey_save`, `submission_export`(test_perf), `label_order`(test_workflow). 교정 작업(1000도형 격자 렌더 5회 중앙값)은 매 실행 `calibration_1000_polygons median_ms=` 로 로그에 남는다. 실패 메시지에는 모든 표본(`samples_ms=[...]`)과 교정 값이 들어가 일시적 흔들림과 회귀를 구분할 수 있다. `survey_save`(10 s)·`submission_export`(20 s)는 아직 기준 PC 에서 재지 않은 큰 상한이다. 기준 PC 중앙값 × 1.3 으로 바꾼다. `startup_home` 은 잰 뒤 이벤트 루프를 한 번 더 돌려도 레이어가 0개인지 본다(홈만 연다는 계약).

### 결정적 실패와 플레이크

`scripts/ctest-flake.ps1 -Repeat 5` 는 매 회 실패한 검사를 **결정적 실패**, 일부 회에만 실패한 검사를 **플레이크**로 나눠 `build/qa/ctest-flake-*/SUMMARY.md` 에 적는다. 검사별 TEMP 폴더는 CTest 속성(`--show-only=json-v1`)에서 읽어 만든다.

### 호환 표본

`save_open_portable::oldVersionSurveysStillOpen` 은 `tests/data/compat/<판>/` 를 연다. 이 폴더는 `.gitignore` 예외라 `.gpkg`·`.qgz` 도 커밋된다(합성 자료만). 2026-09-22 조사 파일은 잃어버려 시험이 그 판의 저장 모양으로 다시 만든다. 자세한 것은 `tests/data/compat/README.md`.

### 소스 줄 수 관문

`scripts/scorecard.ps1 -LineLimitOnly`(CI `Sanity checks`)는 `src/`·`tests/` 의 C++ 파일이 300줄을 넘으면 실패한다. 이미 넘은 파일은 `docs/quality/line-limit-baseline.txt` 에 그때 줄 수와 함께 적혀 있고, 줄어드는 것은 괜찮지만 늘어나면 실패한다. 파일을 나눠 줄였으면 `-LineLimitOnly -WriteBaseline` 으로 기준을 다시 쓴다.
