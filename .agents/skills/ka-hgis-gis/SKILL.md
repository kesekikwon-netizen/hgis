---
name: ka-hgis-gis
description: Implement, diagnose or review GIS behavior in the ka-hgis C++20/Qt6 application, including QGIS layers, CRS, editing, GeoPackage save/open, georeferencing and map/PDF export. Project-local; use for HGIS GIS work, not generic C++ or unrelated projects.
---

# ka-hgis GIS development

## Scope and authority

Work from the repository root containing `AGENTS.md` and `src/core/SurveyProjectFactory.h`. This skill supplements global C++ rules only for ka-hgis. Read root `AGENTS.md`, `.codex/NOW.md` and current handoffs before non-trivial GIS work; use [the task map](references/task-map.md) to locate relevant code and checks. Map paths are relative to the repository root. For completed 조판·정합·주변유적 번호 contracts, read [ka-hgis-sheet](../ka-hgis-sheet/SKILL.md) before changing those paths.

Follow explicit user intent. Explanation/review remains read-only; authorized implementation includes investigation, change and verification. Do not turn this skill into a repeated approval gate. Ask for missing input only when it cannot be established from available evidence and materially affects the task.

Keep Architecture B: reuse the installed QGIS/Qt/GDAL/PROJ stack. Inherit global GPT-6 and C++ tools. Do not import legacy dispatch instructions, add GIS rules globally or substitute the standalone C++ template for this build.

## Evidence that changes GIS decisions

Gather only evidence relevant to the reported behavior:

- **Coordinates:** Distinguish project/canvas CRS, datasource CRS and export CRS. Inspect actual transform context, units and representative extent. Assigning CRS metadata is not reprojection. Preserve selected work CRS (5186/5187) and use the existing export transform for 5179. Never fix an unknown CRS by relabeling original data.
- **Layers/editing:** Inspect `ka_hgis/layer_key`, provider validity, legend groups, visibility and canvas order. A GPKG table does not imply a visible layer. Use `LayerOps::ensureDomainLayer` for domain creation and preserve reference maps according to the requested flow. Trace edit buffer, commit failure, rollback and save separately; a layer commit is not proof the workspace was saved.
- **Persistence:** Reproduce with synthetic fixtures or a copy of survey data. Follow explicit-save behavior, embedded workspace restoration, actual table names and external raster references. Never run destructive tests against original field data. Verify state after save and explicit reopen, including failed saves when relevant.
- **Network maps:** Inspect provider request/response type, supported service CRS, extent/scale and layer order. HTTP success can contain a service error. Record key source and sanitized request shape; redact credentials from URLs/logs/reports. A raster WMS picture does not provide vector snapping or editable features.
- **Georeferencing/layout:** Distinguish pixels, map coordinates, page millimeters and DPI. Check transform direction, control-pair semantics and residuals when relevant. Alignment pairs are not survey GNSS control_points. Preserve the separate section workflow; inspect composed output, not only the canvas.
- **Qt/QGIS lifetime:** Follow QObject ownership, layer removal, map-tool callbacks, task cancellation and GUI-thread affinity. Check installed headers for C++ ownership/signatures; Python cookbook examples explain behavior but do not establish C++ ownership.

Use `docs/vendor/qgis-manual-3.44/README.md` and available local cookbooks for lifecycle behavior. Manual version is not proof of the installed qgis-dev version. For missing or version-specific API evidence, check installed headers/version and matching official QGIS/Qt documentation. Do not make the user diagnose provider or CRS internals.

## Change and verify

Keep root `AGENTS.md` product invariants; update both handoffs when product truth changes. Do not adopt historical C++17/LTR/5179 defaults over current code and selected settings. Reuse the existing behavior owner or a small service instead of expanding unrelated MainWindow responsibilities.

Use `scripts/dev-env.ps1`, current presets and root verification commands. The task map suggests focused tests during investigation; it does not waive root Release/CTest and portable validation requirements for applicable C++/UI changes. A passing unit test alone does not establish map rendering or exported GIS correctness.

Report observed behavior, relevant evidence, change and checks actually run. Stop when requested behavior and required checks are satisfied. Skill/documentation-only changes need loading/link/diff checks, not a product rebuild. No automatic commit, push or merge.
