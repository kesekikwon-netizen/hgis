# GOAL-1 — Field UX blockers after GOAL-0 (PO order)

**From:** PO terminal (verified GOAL-0 independently)
**To:** Implementer
**Harness now:** Grok Build (`AGENTS.md`, `HANDOFF.md`) — this brief was written before OpenCode was removed.
**Repo:** C:\Users\권을\Documents\hgis
**OSGeo:** C:\OSGeo4W | **CMake:** C:\CMake\bin
**Rules:** No git commit/push. No destructive git reset.

## PO independent verification of GOAL-0 (accepted with notes)
- ctest 2/2 PASS (re-run by PO)
- App launches: ka-hgis.exe window "고고학 전용 HGIS"
- Toolbar shows 편집저장 + 그리기종료 (UIA confirmed)
- saveEdits/stopEdits wired in MainWindow.cpp (code confirmed)
- m_packageCreated=true on export success (code confirmed)
- DEFAULT_VWORLD_KEY removed; requireVworldKey blocks empty key (code confirmed)
- Portable runner sibling-first (scripts/run-ka-hgis.ps1 confirmed)
- Startup OSM layer present when no VWorld key

## NEW bugs found by PO running the real app
1. **CRITICAL UX:** 7-step workflow list only shows steps 1-5. Steps 6 (도면 검수) and 7 (제출 패키지) are NOT visible in the dock without obvious scroll affordance. Beginners never see full path. Fix layout so ALL 7 steps are visible or clearly scrollable with visible scrollbar and minimum height.
2. Map canvas starts empty gray (OSM may be on but no tiles/extent). After new survey or first open, set a sensible Korea default extent (e.g. national or last extent) and ensure OSM is visible/checked.
3. Toolbar overcrowded (overflow ...). Group: File | Basemap | Digitize | Edit-commit. Keep 편집저장/그리기종료 always visible (not in overflow).
4. CSV GCP still fake / unwired (from prior audit) — implement for real.

## GOAL-1 scope (in order)

### A) Workflow rail shows all 7 steps
- List must expose steps 1..7 always (increase min height, reduce competing widgets, or put VWorld opacity in collapsible section below).
- UIA/tree must list all 7 items without End-key hunting.
- Click step still may run actions for now, but do NOT drop steps 6-7 off-screen by default.

### B) Real CSV GCP import + menu
- Implement importControlCsv to parse id,x,y,datum,ellipsoid,projection,accuracy_m,pdop,fix_type and WRITE Point features to control_points.
- Menu 도구 → CSV 기준점 가져오기.
- Unit test: 2 rows CSV → featureCount==2 with attributes.
- Workflow step 5 complete only if count>=2 AND meta fields present (align WorkflowGuide).

### C) Map first paint usable
- On startup with OSM: zoom to reasonable Korea extent if project empty.
- Status bar message if VWorld key missing: how to set key (메뉴 path).

### D) Toolbar always-visible edit actions
- 편집저장 and 그리기종료 must not sit only behind toolbar overflow at 1280px width.
- Prefer shorter labels already OK; reduce less-critical basemap buttons to menu-only if needed.

### E) Build + ctest green again
Same cmake/ctest commands as GOAL-0. 100% required.

## Out of scope
- DLL-vendored portable
- Full DXF
- Vertex edit/snap (note remaining)
- git commit

## Report: docs/PO_GOAL_1_REPORT.md
Include BUILD/CTEST/A_WORKFLOW7/B_CSV/C_EXTENT/D_TOOLBAR/EVIDENCE/READY_FOR_GOAL2
Also note any PO-found issues you could not fix.

START NOW.
