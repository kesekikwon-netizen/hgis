# GOAL-0 — ka-hgis P0 ship-blockers (PO order)

**From:** Product Owner (historical session)
**To:** Implementer
**Harness now:** Grok Build (`AGENTS.md`, `HANDOFF.md`) — this brief was written before OpenCode was removed.
**Repo:** C:\Users\권을\Documents\hgis
**OSGeo SSOT this PC:** C:\OSGeo4W (no D: drive). CMake: C:\CMake\bin.

## Role
Implementer only. No git commit / push unless PO says so.
Do not git reset / checkout that drops unrelated dirty work.

## Verdict already known (do not re-litigate)
Lab prototype, not field-ready. Handoff overstates completion.

### P0 bugs confirmed by code audit
1. Data loss UX: Digitize says press save but toolbar save = saveProject, not commitChanges. saveEdits/stopEdits slots exist but unwired.
2. CSV GCP fake: importControlCsv counts lines only; does not write features; not on menu.
3. packageCreated always false - workflow step 7 never completes.
4. Hardcoded VWorld key in LayerOps::DEFAULT_VWORLD_KEY and MainWindow; three key stores.
5. Portable launcher broken: dist/ka-hgis-portable/run-ka-hgis.ps1 never looks at sibling ka-hgis.exe.
6. Path docs wrong: README/CI prefer D:\OSGeo4W - this PC is C:\OSGeo4W only.
7. CMake project VERSION 0.2.0 vs VERSION file 0.3.0.

## GOAL-0 scope (do ALL, in order)

### A) Env + launch SSOT
1. dev-env.ps1: existence-based root, prefer env then C:\OSGeo4W then D:\OSGeo4W.
2. run-ka-hgis.ps1: when run from portable dir, resolve Join-Path $here ka-hgis.exe FIRST.
3. build-all.ps1: discover cmake at C:\CMake\bin\cmake.exe.
4. Minimal README/portable README: document C: SSOT; stop requiring D: only.
5. Bump CMakeLists.txt project VERSION to 0.3.0.

### B) Build + test green
```
cd C:\Users\권을\Documents\hgis
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
Must be 100% pass.

### C) Data integrity
1. Wire toolbar/menu edit-save -> saveEdits (commitChanges all editable layers).
2. Wire stop-drawing -> stopEdits.
3. Digitize status text must name the real button (not ambiguous save).
4. saveProject must auto-commit edit buffers first.
5. After commit, refreshWorkflowGuide().
6. Prove: draw -> edit-save -> reopen -> feature survives (test or scripted AC).

### D) packageCreated + honest workflow step 7
1. After successful submission package (shp+manifest+pdf), set packageCreated=true.
2. Workflow step 7 shows complete when package validates.
3. Remove or qualify DXF claims in UI strings until DXF exists.

### E) Kill hardcoded VWorld key path (minimal)
1. Basemap add must use VworldSettings only - no default production key in binary.
2. Empty key -> Korean dialog: enter key in settings; do not silently use embedded key.

## Out of scope for GOAL-0 (next goals)
- Full real CSV GCP + map-click GCP
- True DLL-vendored portable
- Full CRS doc rewrite + workflow click-only semantics
- Vertex edit / snap / section_line

## Completion report (also write docs/PO_GOAL_0_REPORT.md)
BUILD / CTEST / A_ENV / C_SAVEEDITS / D_PACKAGE / E_VWORLD / GAPS_REMAINING / EVIDENCE / READY_FOR_GOAL1

## Acceptance gates
1. ctest 100%
2. saveEdits reachable from UI; status text not lying
3. packageCreated can become true after export success
4. no DEFAULT_VWORLD_KEY silent fallback for basemap
5. portable runner finds sibling exe
6. Report file written

START NOW. Only this GOAL-0 until report file exists.
