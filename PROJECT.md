# Project: ka-hgis (Korean Field Archaeology Desktop HGIS)

## Architecture
- Architecture B: Standalone C++20/Qt6 desktop application linked to OSGeo4W `qgis_core` and `qgis_gui`. No QGIS fork; do not reimplement PROJ/GDAL/renderer.
- Domain Layers: `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, `artifact_point`, `trial_trench`. Logic property `ka_hgis/layer_key`.
- Legend Groups: `조사 데이터` (Domain data) vs `참조 지도` (Basemap & references).
- CRS Policy: Working CRS EPSG:5186 / EPSG:5187; Submission package export CRS strictly EPSG:5179.
- Sub-controllers: `ProjectLifecycleController`, `DigitizingStateController`, `LayerStateController` — 계획했으나 하지 않음. `src/app/controllers/`는 없다. 조사 열기·디지타이즈·레이어 동작은 `MainWindow*.cpp`와 `LayerOps`에 있다.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | Layout Composition Validation | Prevent empty/dummy `layout_blank` memory layers from passing `isComposedStudioSheet` | M1 | Survey R1 |
| 2 | Checklist Rule Tightening | Tighten `drawing_checklist.v1.json` & `ProjectStateBuilder` so uncomposed templates fail | M1 | Survey R1 |
| 3 | Submission Package Layout Bundling | Seamlessly export `user_sheet` -> `조사도면.pdf`, check errors, support `section_sheet` | M1 | Survey R1 |
| 4 | Streamed Package Hashing | Chunked 64KB hashing for `MANIFEST.sha256` avoiding memory spikes | M1 | Survey R1 / R3 |
| 5 | Geometry Auto-Repair Pipeline | `sanitizeAndRepairGeometry` — 계획했으나 하지 않음. `src`에 그 심볼이 없다. | M2 | Survey R2 |
| 6 | Digitizing Flow Intranet Guard | Enforce geometry repair in `onGeometryCaptured` and vertex edits to prevent upload rejection | M2 | Survey R2 |
| 7 | VWorld Key Flexible Regex | Update `withVworldApiKey` regex to handle non-hyphenated, empty, or custom keys | M2 | Survey R2 |
| 8 | Cadastral XML Key Refresh | Update local `vworld-cadastral.xml` and reload raster data provider in `refreshVworldApiKeyInLayers` | M2 | Survey R2 |
| 9 | UI VWorld Key Sync | Wire `MainWindow::configureVworldKey` to refresh active project layer sources and tile caches | M2 | Survey R2 |
| 10 | Hotspot Controller Extraction | 계획했으나 하지 않음. 컨트롤러 클래스 대신 `MainWindow`를 cpp 파일로만 나눈다. | M2 | Survey R2 |
| 11 | Baseline Test Regression Fix | Fix aspect margin in `TestWorkflow::zoomToKorea_5186StaysInsideMercatorSatelliteQuad` | M2 | Survey R2 / R3 |
| 12 | Async Submission Export | GUI 스레드 밖 제출 내보내기 — 계획했으나 하지 않음. `ExportService.cpp`에 `QThread`/`QtConcurrent`가 없다. | M3 | Survey R3 |
| 13 | High-DPI Scale Flutter Fix | `extentsChanged`에서 `applyCanvasScreenDpi`를 떼기 — 계획했으나 하지 않음. `MainWindow.cpp`의 `extentsChanged` 슬롯이 아직 `applyCanvasScreenDpi`를 부른다. | M3 | Survey R3 |
| 14 | Tile Heal Loop Elimination | `extentsChanged`마다 `m_tileHealCount`를 비우지 않기 — 계획했으나 하지 않음. 그 슬롯이 아직 `m_tileHealCount.clear()`를 한다. | M3 | Survey R3 |
| 15 | In-Memory Tile Cache Tuning | Tune `QgsSettings` tile cache size to 256 and standardize tile datasource URIs | M3 | Survey R3 |
| 16 | 100% CTest & Smoke Pass | Verify all 14 test suites pass, 0 compiler warnings under /W4, clean smoke-quit | M4 | Survey Baseline |
| 17 | Forensic Integrity Audit | Pass adversarial forensic audit against cheating/dummy facades | M4 | Survey Baseline |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | R1: Submission Package & Layout Integration | `isComposedStudioSheet`, checklist, `user_sheet` PDF, streamed SHA256 are in `LayoutService` / `ExportService` / `ProjectStateBuilder` | none | 코드 있음. 이 표의 예전 상태 칸(IN_PROGRESS)은 현재 완료 기록이 아니다. |
| M2 | R2: Hotspot De-risking, VWorld Lifecycle & Geometry Repair | VWorld 키 갱신(`withVworldApiKey`, `refreshVworldApiKeyInLayers`, `configureVworldKey`)은 코드에 있다. 기하 자동수리와 서브컨트롤러 추출은 계획했으나 하지 않음. | M1 | 일부만 코드에 있음 |
| M3 | R3: Async Operations & High-DPI Tile Stability | 비동기 제출, `extentsChanged`와 DPI/타일힐 분리, 타일 캐시 256 고정은 계획했으나 하지 않음. | M2 | 계획했으나 하지 않음 |
| M4 | Final E2E Test Suite Pass & Adversarial Verification | "CTest 14개 · /W4 경고 0 · forensic audit"은 계획했으나 하지 않음. 현재 스위트 수는 14가 아니다. | M3 | 계획했으나 하지 않음 |

## Interface Contracts
### `LayoutService` ↔ `ProjectStateBuilder` ↔ `ExportService`
- `bool LayoutService::isComposedStudioSheet(QgsProject* project, const QString& layoutName = QStringLiteral("user_sheet"))`: Returns true only if map frame exists, has positive scale, valid finite extent, and non-empty vector/raster layers (excluding `layout_blank` and reference layers).
- `bool ExportService::exportSubmissionPackage(QgsProject* project, const QString& outDir, const QString& encoding, QString* errorOut)`: Validates composed sheet, calls `LayoutService::exportLayoutPdf`, writes `조사도면.pdf`, writes 5179 SHPs, and computes streamed `MANIFEST.sha256`.

### `LayerOps` ↔ `MainWindow`
- `sanitizeAndRepairGeometry` / `DigitizingStateController`: 계획했으나 하지 않음.
- `int LayerOps::refreshVworldApiKeyInLayers(QgsProject* project, const QString& currentKey, QStringList* changed)`: Updates WMTS/WMS URLs and cadastral GDAL XML files, reloading providers. 선언은 `LayerOps.h`, 구현은 `BasemapOps.cpp`.

### `ExportService` Async Pipeline
- 계획했으나 하지 않음. 제출 패키지는 `ExportService::exportSubmissionPackage`가 호출 스레드에서 처리한다.

## Code Layout
- `src/core/ExportService.h / .cpp`: Export submission package, shapefile reprojection, SHA256 manifest.
- `src/core/LayoutService.h / .cpp`: Layout templates, studio sheet composition validation, PDF export.
- `src/core/ChecklistEngine.h / .cpp`: Rule evaluation against project state.
- `src/core/ProjectStateBuilder.h / .cpp`: Gathers project state for checklist evaluation.
- `src/core/LayerOps.h / .cpp`, `BasemapOps.cpp`: 레이어 키, 배경지도, VWorld 키 갱신. 기하 자동수리(`sanitizeAndRepairGeometry`)는 계획했으나 하지 않음.
- `src/app/controllers/`: 계획했으나 하지 않음. 디렉터리가 없다.
- `src/app/MainWindow.h`와 `MainWindow.cpp`, 그리고 같은 클래스의 구현 파일: `MainWindowRibbon.cpp`, `MainWindowOffline.cpp`, `MainWindowAlign.cpp`, `MainWindowOverlay.cpp`, `MainWindowFiles.cpp`, `MainWindowCadastral.cpp`, `MainWindowDownloads.cpp`, `MainWindowEditing.cpp`, `MainWindowExport.cpp`, `MainWindowSession.cpp`, `MainWindowTopographic.cpp`, `MainWindowUndo.cpp`, `MainWindowContextMenus.cpp`.
- `src/app/KaCaptureMapTool.h / .cpp`: Map digitizing tool.
- `tests/test_checklist.cpp`, `tests/test_workflow.cpp`: Test cases for validation, lifecycle, and export.
