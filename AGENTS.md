# ka-hgis Cursor Agent Rules

This repository is **ka-hgis**: a standalone C++20/Qt6 desktop HGIS for Korean archaeology field drawings. It links against OSGeo4W `qgis-dev` through `qgis_core` and `qgis_gui`. It is not a QGIS fork.

Cursor reads this `AGENTS.md` as the workspace rule.

Reply in the user's language, usually Korean. Code, paths, and identifiers may remain English; user-visible UI strings should follow existing Korean `QStringLiteral` patterns.

## Cursor Runtime

- Use Cursor native Agent, subagents (Task), MCP, hooks, and skills when they are available and useful.
- The fixed model for this repository is Grok 4.7 in the Cursor model picker. Do not use Auto, Fast, or a different model unless the user switches it for that turn. https://cursor.com/docs/models
- Legacy `.codex/`, `.grok`, and `.agents` dispatch/history files may remain for history or compatibility; they are not the authoritative harness. Current project skills remain `.agents/skills/ka-hgis-gis/`, [ka-hgis-sheet](.agents/skills/ka-hgis-sheet/SKILL.md), [Archify](.agents/skills/archify/SKILL.md), and [문화재인트라넷](.agents/skills/heritage-intranet/SKILL.md).
- Do not introduce OpenCode, Sisyphus, hidden auto-push, or hardcoded personal credentials.
- Work directly for small and local changes. Use Cursor Task subagents only for bounded independent research, review, or verification when that improves correctness or throughput.
- Commit only when the user explicitly asks.

### Verified developer tools

- Product C++ edits under `src/` or `tests/` are not done until this session has run Graft (`hgis_graft`), `scripts/clangd-definition.py`, `scripts/archify.ps1`, and the matching `ctest`. `.cursor/hooks.json` `stop` sends that follow-up when the record is missing. Naming a tool is not evidence. Docs-only edits do not require the four.
- Cloud agents run on Ubuntu from `.cursor/environment.json`. https://cursor.com/docs/cloud-agent/setup They do not have OSGeo4W, Visual Studio, or `qgis-dev`. Do not configure, build, ctest, or smoke there, and do not report those as passed. The Windows machine remains the build gate.
- **Mandatory development workflow (user requirement, 2026-09-15; harness switched to Cursor 2026-09-18): `Cursor Agent + AGENTS.md + clangd + Graft + Archify + CMake/CTest`.** Use all six for product-code implementation and fixes, including small changes. Read the current repository instructions/state first; use Cursor Agent execution, Graft retrieval confirmed against source, clangd declaration/definition navigation and diagnostics, an Archify view of the affected behavior, and the required CMake/CTest checks. Record concrete inputs, artifacts and results in the task plan or QA report; naming a tool is not evidence of using it.
- Setup and evidence: [developer-tools.md](docs/developer-tools.md). Keep the existing source, compiler and tests authoritative.
- For C++ changes, use the real `build/compile_commands.json`; regenerate with `scripts/gen-compile-commands.ps1` after CMake/SDK changes. Use [clangd navigation](docs/clangd-navigation.md) for a concrete call location's declaration/definition and clangd checks for compiler diagnostics.
- For each product-code task, use project MCP `hgis_graft` to narrow candidate files or definitions; a focused file API query is sufficient for a small known-file edit. Graft is exposed to Cursor via the USER-level MCP config (`%USERPROFILE%\.cursor\mcp.json`, server `hgis_graft`), not a repo-level `.cursor/mcp.json`. Codex registration in `.codex/config.toml` remains for compatibility. Its four tools cover `src/` and `tests/` and refresh the local structural index before retrieval. Confirm parser spans/signatures in source; use `rg` for full text coverage and Qt signal/slot wiring.
- Do not enable Graft call tracing or claim complete impact coverage: HGIS C++ cross-file edges remain incomplete even with Windows LSP discovery repaired. Do not run upstream `graft init`, install its hooks, or use graph-first instructions over these rules. The pinned adapter omits updater, model-backed indexing and promotional output.
- For each product-code task, use the installed project [Archify skill](.agents/skills/archify/SKILL.md) via `scripts/archify.ps1` to inspect the affected structure or flow; see [setup](docs/archify-setup.md). Keep the diagram scoped to the change. Reuse an existing diagram only after checking its evidence against current source; update it when needed and run validate/deliver/visual-check. Cite current source for every relationship and distinguish planned components. Also use this workflow for architecture explanations and multi-module refactor reviews. A diagram's validation does not validate C++ semantics.
- If a required tool is unavailable, attempt the documented setup/recovery without changing pinned revisions. Source-based investigation may continue, but explicitly record the missing required check and report verification as incomplete; never silently omit the tool or invent its results. The existing documentation-only verification rule below still applies to changes that do not alter product code or C++ build behavior.

## SSOT

### Project-local GIS specialization

- Cursor user rules and global settings supply general C++ guidance. GIS rules belong only to this repository; do not copy them to global user rules or global skills.
- Use [ka-hgis-gis](.agents/skills/ka-hgis-gis/SKILL.md) for HGIS implementation, diagnosis and review. It routes to relevant evidence and tests without requiring a GIS investigation for an unrelated edit.
- Use [문화재인트라넷](.agents/skills/heritage-intranet/SKILL.md) for nearby-heritage intranet downloads, related legends and recovery; it records the confirmed success contract and the stale-object-file restore failure.
- Use [ka-hgis-sheet](.agents/skills/ka-hgis-sheet/SKILL.md) when a completed sheet/snap/heritage-number behavior regresses, or when a newly finished field contract must be recorded. Design map: [ka-hgis-sheet-contracts.workflow.json](docs/architecture/ka-hgis-sheet-contracts.workflow.json).
- Resolve historical documentation against current user requirements, `.codex/NOW.md`, current handoffs and actual code. The original ADR's C++17/LTR baseline and the schema's legacy 5179 default do not override the current C++20/Qt6/qgis-dev build or selected survey CRS.
- Use the repo PowerShell environment and installed OSGeo4W SDK. Do not replace this application's Qt/QGIS build, runtime plugins or package layout with the global standalone C++ template.

Read these before non-trivial product or GIS work:

1. `.codex/NOW.md` - current session state / most recent field constraints (historically Codex-maintained; still the live state file).
2. `docs/HANDOFF.md` - product truth (canonical). Root `HANDOFF.md` is a short pointer only.
3. `docs/adr/0001-standalone-cpp-qgis-libs.md` - Architecture B, no QGIS fork.
4. `docs/domain/data-model.md` - GPKG layers and fields.
5. `docs/architecture/data-flow.md` - critical path.
6. `docs/vendor/qgis-manual-3.44/` - local QGIS manual/cookbook evidence for map, layer, edit-buffer, and layout behavior.

Use official online documentation only when local repo evidence is missing or version-specific behavior matters.

## Current Field Behavior

- Startup opens the home screen only.
- Do not auto-restore the last survey, recent project, drawing, basemap, WMS, XYZ, or workspace on launch.
- The user must explicitly choose **조사 열기**, a recent survey item, or **새 조사** to open/create a project.
- Do not reintroduce automatic LayersOnly restore, automatic basemap loading, or automatic project restore.
- Do not modify the user's original survey data unless the requested operation is explicitly a save/export/edit operation.

## Product Invariants

- Architecture B only: link `qgis_core` / `qgis_gui`; do not fork QGIS; do not reimplement PROJ, GDAL, QGIS rendering, or CRS transformation.
- Domain layer logic keys are `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, and existing `artifact_point` where already supported.
- Store logical domain identity in `ka_hgis/layer_key`; Korean titles are UI labels only.
- Keep legend groups separated as **조사 데이터** and **참조 지도**. Downloaded cadastral (**지적도 · 조사 주변 5km**) stays at the layer-tree root, outside **참조 지도**, not merged into a **지적도** group and not merged with the VWorld picture. VWorld cadastral picture layers (**지적 본번/부번**, VWorld 지적) stay at the layer-tree root, outside the **참조 지도** group. Basemaps, WMS, XYZ, soil, geology, masks, and aligned rasters are reference maps, not survey data. VWorld cadastral WMS is a picture and cannot snap.
- Work CRS may be EPSG:5186 or EPSG:5187. Upload/export output is EPSG:5179 SHP + PDF + MANIFEST.
- `loadSurveyLayers` must not call `removeAllMapLayers()`. Drop domain layers only and keep basemaps/reference layers when the workflow requires it.
- `loadSurveyLayers` must not auto-add empty domain layers. GPKG schema can exist on disk; legend entries appear only after an explicit user draw/import/open action.
- `LayerOps::ensureDomainLayer` is the only path that adds domain layers to the project/legend.
- No hardcoded VWorld production API key. Use `VworldSettings`, local settings, environment, or gitignored `config/secrets.ini` only.
- DXF is not a submit path. Submission/export remains SHP/PDF package through `ExportService`.
- Keep GPLv2+ compliance and About notices intact.

## QGIS Behavior Rules

Use the local QGIS manual/cookbook as the design reference for QGIS lifecycle behavior:

- Project starts without user layers until they are added or created.
- A layer is a datasource; a feature is digitized geometry.
- Basemap/reference data is separate from survey edit data.
- GUI edit flow is `startEditing` -> modify/add features -> `commitChanges` or `rollBack`.
- Feature creation should use `QgsFeature(layer.fields())`, `setGeometry`, and `addFeature`; do not add attribute dialogs during draw unless the user asks for that workflow.
- Do not add a layer to the project only because a GPKG table exists.
- Demo geometry belongs only behind explicit QA/demo flags.

## Module Map

| Area | Path | Notes |
| --- | --- | --- |
| UI shell | `src/app/MainWindow.*` | digitize, menus, project open/save |
| App boot | `src/app/KaApplication.*` | `QgsApplication`, prefix, PATH |
| Map tools/icons | `src/app/KaCaptureMapTool.*`, `src/app/KaAttributeMapTool.*`, `src/app/KaVertexEditTool.*`, `src/app/KaIcons.*` | capture, select, edit |
| Layers/basemap | `src/core/LayerOps.*` | groups, `layer_key`, reference maps |
| Survey GPKG | `src/core/SurveyProjectFactory.*`, `src/core/SurveyStorage.*` | survey creation/save/open |
| Checklist | `src/core/ChecklistEngine.*`, `data/rules/drawing_checklist.v1.json` | submit blockers |
| State | `src/core/ProjectStateBuilder.*` | live state for rules |
| Export | `src/core/ExportService.*` | SHP 5179 + MANIFEST |
| Layout/PDF | `src/core/LayoutService.*`, `src/app/KaDrawingStudio.*` | drawing studio, composed sheets |
| Section studio | `src/app/KaSectionDrawingStudio.*`, `src/core/SectionLayoutService.*` | GeoTIFF section drawings |
| Location/search | `src/core/LocationSearch.*`, `src/app/KaRegionLocator.*` | region and field map flows |
| VWorld | `src/core/VworldSettings.*` | local key handling |
| Tests | `tests/*` | `ka_hgis_tests`, workflow/save/open/theme tests |
| Scripts | `scripts/*.ps1` | build, run, smoke, publish |

Hotspots: `src/app/MainWindow.cpp` and `src/core/LayerOps.cpp`. Keep changes narrow; prefer small services over growing these files further when new behavior spans multiple concepts.

## Development Flow

- For explanations, investigations, and reviews, inspect and report without edits unless the user asks to change something.
- For explicit verbs such as 적용, 설정, 구현, 수정, fix, add, change, create, or wire, make the smallest correct change and verify it.
- Before non-trivial implementation, write a short working plan in the response or notes: symptom/goal, likely code or GIS cause, files to touch, and user-visible done check.
- Preserve unrelated dirty work. Never revert changes you did not make.
- Prefer existing project patterns and existing helper APIs.
- No drive-by refactors.
- No new dependencies unless the user explicitly asks or the existing codebase already requires them for the requested change.

## C++/Build Defaults

- C++20, Qt6, CMake, MSVC on Windows.
- Prefer target-scoped CMake options over global flag pollution.
- MSVC compile expectations: `/std:c++20`, `/Zc:__cplusplus`, `/permissive-`, `/EHsc`, and appropriate warnings where the existing target supports them.
- Keep `.clangd`, `.clang-format`, `.clang-tidy`, and `CMakePresets.json` aligned with the actual build when editing them.
- Do not use `-march=native` or CPU-specific release flags for portable field builds unless the user asks for machine-local performance binaries.
- Treat `/fp:fast`, fast-math, sanitizer changes, PGO, and LTO as explicit build-policy changes that require evidence and compatibility checks with Qt/QGIS/OSGeo4W.

## Verification

Use PowerShell on this machine:

```powershell
$env:PATH = "C:\Program Files\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DOSGEO4W_ROOT=$env:OSGEO4W_ROOT" -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For startup, UI, menu, map, or project-open changes, also run:

```powershell
.\scripts\run-ka-hgis.ps1 --smoke-quit
```

The user's normal entry point is the desktop **고고학 전용 HGIS** shortcut: `scripts/start-ka-hgis.vbs` -> `launch.ps1` -> `build/Release/ka-hgis.exe`. Keep that icon connected to the current Release build. Do not silently fall back to an older executable. Verify this launch chain for UI/map changes; do not close, restart, or operate the user's running app without a request.

Portable creation, `scripts/publish-desktop.ps1`, and copying to `dist/ka-hgis-portable` or L: are separate delivery actions. Run them **only when the user explicitly requests portable output**, never automatically as a verification step. This is the user's latest workflow requirement and supersedes historical portable-check instructions.

Docs/settings/hooks/rules-only changes do not require CMake, ctest, smoke, or publish unless they modify C++ build behavior. Validate those changes with file reads, diff review, and targeted text checks.

## GIS Verify Gate

For map, CRS, WMS/WMTS, digitize, georeference, layout, export, or layer-order bugs, gather concrete evidence before editing:

- project CRS, layer CRS, on-the-fly transform state, canvas layer order, scale, and provider URI where relevant;
- local QGIS manual/cookbook behavior for unfamiliar `Qgs*` APIs;
- VWorld request shape and key source when VWorld layers are involved.

Do not ask the user to diagnose EPSG, WMS, QGIS provider, or CRS behavior.

## Git

- Do not commit unless explicitly requested.
- Do not force-push.
- Do not commit secrets.
- `docs/COMMIT_STATUS.md` is hook/script-maintained; do not hand-edit it casually.

## Anti-Patterns

- Treating basemap/reference data as survey domain data.
- Calling `removeAllMapLayers()` during survey load.
- Assuming work CRS is always 5179.
- Reintroducing automatic startup restore.
- Hardcoding VWorld keys.
- Adding DXF as primary submit output.
- Reintroducing Codex, Antigravity, OpenCode, Sisyphus, or a second harness. Cursor with Grok 4.7 is the harness.
- Forcing large agent graphs for local one-file fixes.
