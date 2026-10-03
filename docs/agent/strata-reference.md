# Strata 작업 참고 (필요할 때만 읽는다)

CLAUDE.md에서 옮긴 내용이다(2026-10-03, docs/intent/2026-10-03-dev-setup-gaps.md). CLAUDE.md가 이 문서의 절을 가리킬 때 읽는다.

## 바탕화면 바로가기 (앱 빌드가 성공할 때마다, 사용자의 상시 요청)

Run the smoke, then create or repoint `C:\Users\kwonyoungin1\Desktop\Strata (개발).lnk` with WScript.Shell (`CreateShortcut` + `Save` makes it when missing):
- TargetPath `<worktree>\scripts\start-ka-hgis.vbs`, WorkingDirectory `<worktree>`, IconLocation `<worktree>\build\Release\ka-hgis.exe,0`, Description `Strata 개발 빌드 (워크트리 <name>)`.
- Then call `SHChangeNotify(0x08000000)` so the icon refreshes, and tell the user which worktree it now opens.
- 「고고학 전용 HGIS.lnk」 ran `A:\qgis\build` (exe from 2026-09-28): touch or recreate it only when asked.
- If the app is open, rename the running exe before rebuilding; close it only if its title has no " *".

## Code map

- `src/app` (UI): MainWindow is split over 16 files (`MainWindow.cpp` + `MainWindow{Align,Cadastral,ContextMenus,Downloads,Editing,Erase,Export,Files,Offline,Overlay,Ribbon,Session,SurveyContour,Topographic,Undo}.cpp`), `KaApplication` (startup, `--smoke-quit`, rendering settings), Ka* widgets and map tools, `KaDrawingStudio` (도면), `KaSectionDrawingStudio` (단면도).
- `src/core` (static lib `ka_core`): LayerOps, BasemapOps, SurveyStorage/SurveySession/SurveyProjectFactory, ExportService, LayoutService, SectionLayoutService, ChecklistEngine (+ data/rules/drawing_checklist.v1.json), ProjectStateBuilder, Heritage*, Topographic*, Cadastral*, PolygonErase, FeaturePick.

## Test baseline history

- Baseline at a6eb712 (main 9f3992c, 71 tests) was 67/71 with the 4 known failures listed in CLAUDE.md.
- The 10-01 merge line (`claude/strata-merge-20261001`, de0414f, 124 tests) was measured 117-118/120 at -j4 on 09-30 (closeSave flake, memory `strata-visual-redesign.md`). It was fast-forwarded into main on 2026-10-03. Replace this with a fresh measurement when one is taken.

## Worktrees

- The desktop app runs each session in `A:\qgis\.claude\worktrees\<name>` on a `claude/<session>` branch. Once the desktop reuses a folder, the folder and branch names differ, so read `git branch --show-current` instead of assuming.
- New desktop sessions start from origin/main.

## Stale docs

docs/HANDOFF.md, docs/README.md and TEST_INFRA.md still mention D:\hgis, D:\OSGeo4W, C:\CMake, AGENTS.md, .codex/NOW.md, publish-desktop after every build and a Codex section. Real paths: repo A:\qgis, OSGeo4W A:\OSGeo4W (QGIS 4.3 master, Qt 6.11), CMake C:\Program Files\CMake\bin, VS 2022 BuildTools. CLAUDE.md wins over those docs. docs/COMMIT_STATUS.md is stale; leave it.

## Why some rules exist

- 「코드 고치는 방식」 came from general rules the user pasted on 2026-10-01; 「모르면 멈추고 묻기」 was left out because it conflicts with rule 2.
- The security review context is `.claude/claude-security-guidance.md`. It is read by the end-of-turn diff review only; the commit reviewer ignores it, and on this subscription login neither LLM review runs, so expect only the regex edit warnings.
- `scripts/run-ka-hgis.ps1 --smoke-quit` is not a smoke test: ka-hgis.exe is a GUI exe, PowerShell does not wait for it, and the script returns 0 even if the app never ran.
- Files in docs/superpowers dated 2026-09-13 or earlier are history, not current requirements.
- Removed from CLAUDE.md on 2026-10-03 because it was out of date: "fast-forward main only on 「커밋」" and the note that `claude/strata-merge-20261001` (worktree happy-davinci-a9ee1e) was not on main. That line was merged into main and pushed on 2026-10-03, and since 2026-10-03 every verified commit is pushed.
