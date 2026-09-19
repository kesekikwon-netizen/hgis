# HGIS task map

Paths are relative to the repository root. Read the relevant row only. Test names are current CTest registrations in `CMakeLists.txt`; recheck them if the build definition changes.

| Task | Primary code | Evidence / focused tests |
|---|---|---|
| Startup, create/open, CRS UI | `src/app/KaApplication.cpp`, `src/app/MainWindow.cpp`, `src/core/SurveyProjectFactory.h` | Home-only startup; project/canvas/layer CRS on 5186 and 5187 creation/reopen; `save_open_persist`, `workflow_engine` |
| Layers, basemaps, editing | `src/core/LayerOps.cpp`, `src/app/KaCaptureMapTool.cpp`, `src/app/KaVertexEditTool.cpp` | Layer keys, separate groups, lazy creation, edit/commit failure, reference visibility; `workflow_engine`, `save_open_edit` |
| Save, Save As, workspace | `src/core/SurveyStorage.cpp`, `src/app/MainWindow.cpp` | Reopened counts/geometry/attributes/CRS, table identity, committed WAL data, styles/visibility, external rasters; `save_open_roundtrip`, `save_open_persist`, `recent_surveys` |
| Raster alignment | `src/core/GeorefService.cpp`, `src/app/KaAlignMapTool.cpp` | Pixels versus map coordinates, pair residuals, worldfile, raster appearance and retained references; `georef_engine` |
| Export/checklist | `src/core/ExportService.cpp`, `src/core/ChecklistEngine.cpp`, `src/core/ProjectStateBuilder.cpp` | 5179 output coordinates/CRS, schema/geometry, actual package contents and manifest; `checklist_engine`, `workflow_engine`, `e2e_opaque_suite` |
| Composed drawing/PDF | `src/core/LayoutService.cpp`, `src/app/KaDrawingStudio.cpp` | Page size, scale, CRS label, visibility, complete rasters, composed output; `workflow_engine` and applicable portable UI/output check |
| Section drawing | `src/core/SectionLayoutService.cpp`, `src/app/KaSectionDrawingStudio.cpp` | Separate distance/elevation coordinates and GeoTIFF workflow; `section_layout_engine`, `section_studio_engine` |
| Measure/buffer/DEM/trench | `src/core/MeasureOps.cpp`, `src/core/BufferAnalysis.cpp`, `src/core/DemAnalyzer.cpp`, `src/core/TrenchGridGenerator.cpp` | Units, invalid geometry, selected boundary, operation-specific tolerances; `measure_tape`, `buffer_ring`, `dem_trench_engine` |
| Remote/reference maps | `src/core/VworldSettings.cpp`, `src/core/LayerOps.cpp`, relevant map service | Sanitized URI/response, supported CRS, canvas order, extent/scale; relevant service tests and bounded rendering check |

Example focused diagnostic after the existing build:

```powershell
. .\scripts\dev-env.ps1
ctest --test-dir build -C Release -R '^(save_open_|workflow_engine$)' --output-on-failure
```

Root `AGENTS.md` defines required completion checks. Do not run field-data writes, network mutations or publish scripts solely because a related area is listed here. Use the requested operation and applicable root rules to determine verification.
