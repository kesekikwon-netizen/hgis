# Strata (ka-hgis) 작업 지침

사용자는 개발자가 아닌 고고학 현장 전문가다. 요청은 짧은 한국어 문장, 증상("안 돼요"), 스크린샷으로 온다. 전역 `~/.claude/CLAUDE.md` 지침(결론과 다음 할 일을 첫 줄에 쉬운 한국어로, 증상은 수정 요청, 빌드·테스트 결과와 함께 보고, 요청한 것만, 화면에서 확인할 것 한 가지, 새 기능은 요청 기록부터)을 그대로 따른다.

## superpowers 사용 방식 (스킬 지침보다 우선한다)

1. 스킬은 해당 단계가 되면 적극 사용한다(사용자 지시 2026-10-03 「해당 시 적극 사용」. 2026-10-02 「빠르게 진행」의 '과제마다 작업자·검토자를 따로 돌리지 않는다'는 그대로다). 단계마다 Skill 도구로 실제로 부르고, 앞에서 읽었거나 대화 요약 전에 부른 것으로 대신하지 않는다. 증상·버그 신고·스크린샷·「예전으로 돌아갔다」는 superpowers:systematic-debugging으로 원인부터 찾고, 코드를 고칠 때는 superpowers:test-driven-development로 시험을 먼저 쓰고, 화면이 바뀌는 수정은 run-strata 스킬로 실제 화면을 확인하고, 끝났다고 말하기 전에는 superpowers:verification-before-completion을 쓴다. superpowers:brainstorming은 현장 쓰임새를 물어야 하는 새 기능에만 쓴다. 계획이 있는 큰 기능도 과제마다 작업자·검토자를 따로 돌리지 않는다(superpowers:subagent-driven-development를 쓰지 않는다). 이 대화에서 차례로 만들고(superpowers:executing-plans), 기능 하나가 끝났을 때 superpowers:requesting-code-review로 한 번 검토받는다. 어떤 단계인지 사용자에게 한국어로 밝힌다.
2. 사용자는 개발자가 아니다. "해줘/고쳐줘"라는 요청 자체가 진행 승인이다. 설계 승인, 접근법 고르기, 스펙·계획 검토, 실행 방식 선택, 브랜치 마무리 선택 같은 기술 결정은 묻지 말고 가장 안전한 쪽을 스스로 골라 끝까지 진행하고, 고른 것을 보고에 한 줄로 적는다. 멈추고 묻는 것은 앱이 현장에서 어떻게 보이고 동작해야 하는지(제품 질문)가 정말 불분명할 때만, 쉬운 한국어로 한 번에 묶어 묻는다. 단, 새 기능·동작 변경은 전역 지침의 요청 기록 확인에서 한 번 멈춘다(아래 brainstorming 항목).
3. docs/superpowers 스펙·계획 문서는 여러 단계짜리 큰 기능에서만 쓰고 한국어로 쓴다. 테스트를 붙일 수 없는 UI 코드는 지우지 말고 빌드와 가장 가까운 기존 테스트로 확인한다. ctest 기준선 실패(아래 Build, test, run 절의 목록)는 원래 실패하므로 묻지 말고 진행한다. 끝나면 현재 브랜치에 커밋까지만 하고 push·merge·PR은 사용자가 요청할 때만 한다.
4. 새 기능에서 현장 쓰임새가 두 갈래 이상으로 갈리면(예: 번호 매기는 규칙, 화면에 보이는 위치·모양, 지우거나 고칠 때 처리) 코드를 쓰기 전에 한 번만, 요청 기록 확인 메시지에 묶어 묻는다. 한 번에 4개 이하, 질문마다 추천 답을 붙인다. 코드가 지금 어떻게 동작하는지, 파일 위치, 좌표계 같은 사실은 직접 찾아보고 묻지 않는다. 사용자가 "추천대로"라고 하면 남은 질문도 추천 답으로 정하고 바로 만든다. 버그 수정과 작은 변경은 묻지 않는다.
5. 진행 중 안내와 마지막 보고는 모두 한국어로 쓴다. 스킬·단계 이름도 한국어로 풀어 쓰고('설계 논의', '원인 찾기', '시험 먼저', '끝나기 전 확인'), 영어는 사용자가 직접 입력하거나 누를 경로·명령에만 쓴다.

How the superpowers skills fit this repo:
- Worktrees: the desktop app already runs each session in `A:\qgis\.claude\worktrees\<name>` on a `claude/<session>` branch; once the desktop reuses a folder the folder and branch names differ (pensive-ardinghelli-12cfa6 is on claude/current-dev-setup-0b45e7), so read `git branch --show-current` instead of assuming. Never create another worktree (no `EnterWorktree`, no `git worktree add`). In `A:\qgis` itself, work in place without asking.
- Baseline: do not run the whole suite as a baseline. Build, then run only the ctest groups for the area you will touch.
- brainstorming: for a new feature or behavior change, start with the global `capture-intent` skill: write the record in `docs/intent/`, show it with the rule-4 questions in one message, stop once for 「진행」, then commit the record alone on the session branch. From there the request is the approval (rule 2): say the path (spike/bounded/architectural), write the design in a few Korean sentences and go on in the same turn. The skill's HARD-GATE, one-question-per-message, approve-each-section and spec-review waits do not apply here; a spec file only under rule 3, and its first lines name the record. Do not offer the visual companion; describe screens in words.
- writing-plans → executing-plans: do not ask to review the plan or to choose the execution method; run executing-plans inline (rule 1) and say so in one line. Plan commit steps commit on the session branch only.
- systematic-debugging: its "ask for help" and "discuss with your human partner" steps mean: report in Korean what was tried and continue with the safest option (rule 2). After three failed fixes say so and stop changing code until the next request.
- finishing-a-development-branch: no option menu and no full-suite Step 1; the ctest groups for the touched area are the gate. Commit on the session branch (rule 3); merging into main and pushing follow the Git section.
- Code review: once per finished feature or change set (never per plan task), run superpowers:requesting-code-review (or the built-in /code-review) before the last commit of that feature; fix real findings inside the requested change and mention the rest in one line.
- TDD: put tests into the existing QtTest files and targets (see Tests). Do not add a new test framework. Where the skill says to run the project's whole suite, run the ctest groups for the touched area (never bare `ctest`, never the full suite). UI code with no testable seam needs no permission question: build plus the nearest existing test (rule 3), said in one line.
- Files in docs/superpowers dated 2026-09-13 or earlier are history, not current requirements.

## 코드 고치는 방식

2026-10-01 사용자가 붙여 준 일반 작업 규칙 가운데 지금 규칙과 겹치지 않는 것만 옮겼다(「모르면 멈추고 묻기」는 규칙 2와 부딪혀 넣지 않았다).

- 주변 코드의 이름·주석·모양에 맞춘다. 요청과 상관없는 옆 코드·주석·줄 맞춤은 고치지 않는다.
- 요청과 상관없는 죽은 코드를 보면 지우지 말고 보고에 한 줄로 알린다. 내 변경 때문에 안 쓰이게 된 것(가져오기·변수·함수)만 지운다.
- 요청하지 않은 설정 항목·확장성·추상화는 넣지 않고, 일어날 수 없는 경우를 위한 처리는 쓰지 않는다. 더 짧게 쓸 수 있으면 다시 쓴다.
- 바뀐 줄은 모두 요청에서 바로 이어져야 한다.

## security-guidance 결과 처리

- 턴이 끝날 때나 커밋할 때 보안 검토 결과가 돌아오면, 이 앱(Windows 데스크톱, 서버·웹 화면 없음)에 실제로 해당하는 것만 이번 변경 안에서 고치고, 해당 없는 것은 고치지 말고 보고에 한 줄로 적는다. 보안 판단을 사용자에게 묻지 않는다.
- The project review context is `.claude/claude-security-guidance.md` (read by the end-of-turn diff review only; the commit reviewer ignores it, and on this subscription login neither LLM review runs, so expect only the regex edit warnings). Run `git commit` in the Bash tool: the commit review only watches Bash, and the PowerShell tool turns git's CRLF warnings into errors.

## Product rules (do not break)

- Standalone C++20/Qt 6 Widgets app linking OSGeo4W qgis-dev (`qgis_core`, `qgis_gui`). Never fork QGIS or reimplement PROJ, GDAL, rendering or CRS transforms. Keep the GPLv2+ About notices (docs/adr/0001-standalone-cpp-qgis-libs.md).
- Domain layers: keys `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, `artifact_point`. Identity is the `ka_hgis/layer_key` property; Korean titles are labels only. `LayerOps::ensureDomainLayer` is the only path that adds a domain layer. Never auto-add empty domain layers or add a layer just because a GPKG table exists. `loadSurveyLayers` must never call `removeAllMapLayers()`.
- Legend groups 조사 데이터 / 참조 지도. Basemaps, WMS/XYZ, soil, geology and aligned rasters are reference maps, never survey data. Cadastral layers (downloaded 5 km layer and the VWorld picture) stay at the tree root, outside 참조 지도. VWorld cadastral is a picture and cannot be snapped to.
- CRS: work CRS is EPSG:5187 (default) or 5186; never assume 5179. Submission = EPSG:5179 SHP + PDF + MANIFEST.sha256 through `ExportService`, written to files only, not added to the map. Checklist errors block submission. DXF is not a submit path.
- Keys: never hardcode a VWorld or personal key. `loadApiKey` order: `%LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini` → `HKCU\Software\ka-hgis` → env `VWORLD_API_KEY` → gitignored `config/secrets.ini`. Satellite: keyed api.vworld.kr WMTS first, xdworld only without a key. Cadastral WMS: `crs=EPSG:3857` + KEY, never send DOMAIN, CRS candidates 4326→3857→900913 only. An expired key returns HTTP 200 with an XML error: blank tiles, nothing logged.
- Crash rules (each caused 0xc0000005): keep `qgis/parallel_rendering=false` (KaApplication.cpp). Never stopRendering/clearCache or re-refresh while WMS/XYZ tiles load (skip when `isDrawing()`). No blocking GetMap or `canvas->refresh()` during a draw; use `LayerOps::refreshCanvasIfIdle`. Layout: no QGIS rubber band, no `setLayers({})`, no `refresh()`; call `m_toolSelect->setLayout`.
- Field behavior the user already decided: startup shows the home screen only (no auto-restore of survey, basemap or workspace); no autosave (save only on 저장/Ctrl+S or the close prompt, unsaved shows " *"); New/Save-As dialogs start in `preferredSurveyDir()`, never the Desktop; never modify the user's original survey data. Name Strata, one-row ribbon, Malgun Gothic UI font (the bundled IBM Plex in data/fonts stays opt-in: `KA_HGIS_BUNDLED_FONTS=1`): do not reopen.
- Regressions: for a familiar-sounding symptom read docs/ERROR_REGRESSION.md first; after fixing a new bug add a row there plus a regression test. For map, CRS, layer-order or layout bugs collect project/layer CRS, canvas layer order, scale and provider URI before editing.
- Jev (TypeSafe System One, https://docs.typesafe.ai), user decision 2026-10-02 evening (「jev는 앱적용이 아니라 개발에만」, docs/intent/2026-10-02-jev-dev-only.md): use it ONLY in Claude's development process, NEVER in the app (no app code, key setting or network call). Call it with `node scripts/jev/jev-ask.mjs <request.json>` to judge whether code-review findings are real (one noul per finding) and to rank debugging hypotheses; report one line 「Jev 판정 n건 · 진짜로 본 것 m건 · 비용」. Send only code, test output and review text, never survey data, coordinates, client drawings or keys (US servers). Jev never decides alone: build and tests decide, and when Jev and the Claude reviewers disagree, report both. The key is the user environment variable TYPESAFE_API_KEY, set by the user; never echo, store or commit it. Without a key, network or credits, use the Claude reviewers and say so in one line.
- QGIS edit buffer: unsaved features have negative ids; a multipolygon in a single-POLYGON GPKG table silently becomes an invalid zero-area ring (store pieces as separate features); undo stacks are per layer.

## Code map

- `src/app` (UI): MainWindow is split over 16 files (`MainWindow.cpp` + `MainWindow{Align,Cadastral,ContextMenus,Downloads,Editing,Erase,Export,Files,Offline,Overlay,Ribbon,Session,SurveyContour,Topographic,Undo}.cpp`), `KaApplication` (startup, `--smoke-quit`, rendering settings), Ka* widgets and map tools, `KaDrawingStudio` (도면), `KaSectionDrawingStudio` (단면도).
- `src/core` (static lib `ka_core`): LayerOps, BasemapOps, SurveyStorage/SurveySession/SurveyProjectFactory, ExportService, LayoutService, SectionLayoutService, ChecklistEngine (+ data/rules/drawing_checklist.v1.json), ProjectStateBuilder, Heritage*, Topographic*, Cadastral*, PolygonErase, FeaturePick.
- Hotspots, keep edits narrow: KaDrawingStudio.cpp, MainWindow.cpp, LayerOps.cpp, BasemapOps.cpp.
- CMake source lists are explicit (no GLOB): add new files to `add_library(ka_core …)` or `add_executable(ka-hgis …)`. data/ files are copied next to the exe at build time, so a data/ edit needs a ka-hgis rebuild.

## Build, test, run (Bash tool, from the worktree root)

- First build in a fresh worktree (no build/CMakeCache.txt yet; configures and builds everything, about 8 min): `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-now.ps1`
- Later builds, only the targets you need:
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; cmake --build build --config Release --parallel --target ka-hgis <test-target>; exit $LASTEXITCODE'`
  On out-of-memory errors (C1060, MSB4018, MSB4166, 0x800705AF) rerun it with `-- /m:1 /p:CL_MPCount=2` after the targets.
- Tests: `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; ctest --test-dir build -C Release -j4 --output-on-failure -R "^(name1|name2)$"; exit $LASTEXITCODE'`
  Run the groups for the area you changed (docs/testing-map.md); results are in build/test-logs/<name>.txt. Full suite (same command without -R) only when asked or before a release. Never plain `ctest`: dev-env sets 16-way parallelism, and perf_engine, heritage_download_retry, topographic_browser, survey_contour and startup_splash then flake; rerun a failure alone before calling it a regression.
- Baseline at a6eb712 (main 9f3992c, 71 tests) is 67/71. Known failures: workflow_engine `shapeEditing_livesInsideSelectTool`, cadastral `referenceLayerHasOutlineAndOptionalJibunLabels`, storage_safety `persistWorkspace_writeExceptionKeepsPreviousGeneration`, save_open_portable `oldVersionSurveysStillOpen` (its sample gpkg is missing). The 10-01 merge line (`claude/strata-merge-20261001`, de0414f, 124 tests) has no measured baseline yet: run the touched groups, compare with memory `strata-visual-redesign.md` (117-118/120 at -j4 on 09-30, closeSave flake), and replace this line once measured.
- New tests: register with `ka_add_qtest(<name> <target>)`. A new function in tests/test_save_open.cpp runs only if its name is added to a `ka_add_qtest_filter(save_open_* …)` list in CMakeLists.txt. Tests write only to QTemporaryDir, use QStandardPaths test mode, and never use the app's org/app name.
- Smoke (app starts and quits, expect exit 0):
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $p = Start-Process build/Release/ka-hgis.exe -ArgumentList "--smoke-quit" -WorkingDirectory build/Release -Wait -PassThru; exit $p.ExitCode'`
  Do not rely on `scripts/run-ka-hgis.ps1 --smoke-quit`: ka-hgis.exe is a GUI exe, PowerShell does not wait for it, and the script returns 0 even if the app never ran.
- Never start build/Release/ka-hgis.exe raw: the OSGeo DLLs are not on PATH and the agent job object kills it.

## After every successful app build (standing request)

Run the smoke, then create or repoint `C:\Users\kwonyoungin1\Desktop\Strata (개발).lnk` with WScript.Shell (`CreateShortcut` + `Save` makes it when missing; on 2026-10-01 17:30 no Strata shortcut existed on the Desktop): TargetPath `<worktree>\scripts\start-ka-hgis.vbs`, WorkingDirectory `<worktree>`, IconLocation `<worktree>\build\Release\ka-hgis.exe,0`, Description `Strata 개발 빌드 (워크트리 <name>)`. Then call `SHChangeNotify(0x08000000)` so the icon refreshes, and tell the user which worktree it now opens. 「고고학 전용 HGIS.lnk」 (also absent on 2026-10-01) ran `A:\qgis\build`, whose exe dates from 2026-09-28: touch or recreate it only when asked. If the app is open, rename the running exe before rebuilding; close it only if its title has no " *".

## Git (single branch: main = GitHub kesekikwon-netizen/hgis)

- Commit message: `type(scope): 한국어 요약`, a Korean body with one line of test results, and the Co-Authored-By trailer.
- Work that exists only in a worktree is unsaved: deleting a desktop session wipes it. Commit verified work on the session branch (rule 3) and say it is saved there.
- "커밋" from the user: commit, then fast-forward A:\qgis main to it (`git -C A:/qgis merge --ff-only <sha>`); if main has moved, rebase the session branch onto main and rerun the tests first. "푸시" or "커밋 푸시": also `git push origin main`. Since 2026-10-03 (user: 「푸시하는것만 하게하라」, 「매단계 푸시를 안하나?」) every verified commit is fast-forwarded into main and pushed right away. GitHub CI keeps running on push (「자동테스트는 하게해야지」): never cancel or disable it, and do not wait on its 1–2 h run; check the last run once at the next task.
- A PreToolUse hook (`.claude/hooks/commit-gate.mjs`) denies a commit that touches src/, tests/, cmake/, data/ or CMakeLists.txt until build/Release is newer than the changed files and a later ctest ran at least one test with no failures except the baseline ones. Follow the commands in its deny reason; there is no bypass.
- Never force-push. Never run scripts/auto-git-push.ps1 or scripts/commit.ps1. Never commit secrets, *.gpkg/*.qgz field data or a portable's config folder. docs/COMMIT_STATUS.md is stale; leave it.
- After a lost-work report, check `git -C A:/qgis for-each-ref refs/snapshots` first (a global hook snapshots uncommitted worktree state).
- New desktop sessions start from origin/main. When memory names a newer line the user calls the current app (lost-work-20260930.md: `claude/strata-merge-20261001`, worktree happy-davinci-a9ee1e, not on main), say so in the first report and fast-forward main only on 「커밋」.

## Portable (only when asked)

「포터블」 or 「키 전부 포함 포터블」: follow the project skill `portable-release` (`.claude/skills/portable-release/SKILL.md`; project skills are read from `A:\qgis`, so it reaches sessions only after main is fast-forwarded). Say first that a portable is about 1 GB and C: has little room.

## Windows pitfalls

- PowerShell tool: native stderr (git CRLF warnings, qWarning) becomes a terminating error. Run repo scripts as `powershell.exe -NoProfile -ExecutionPolicy Bypass …` from the Bash tool, and set `$ErrorActionPreference="Continue"` after dot-sourcing dev-env.ps1 (it sets Stop).
- A .ps1 that contains Korean needs a UTF-8 BOM; PowerShell 5.1 reads BOM-less files as cp949.
- moc cannot parse `R"(...)"` raw strings in Q_OBJECT sources or headers they include (silent AutoMoc or link failures); use concatenated "..." literals.
- In Git Bash `>nul` creates a stray file named nul; use /dev/null.
- Stale docs: docs/HANDOFF.md, docs/README.md and TEST_INFRA.md still mention D:\hgis, D:\OSGeo4W, C:\CMake, AGENTS.md, .codex/NOW.md, publish-desktop after every build and a Codex section. Real paths: repo A:\qgis, OSGeo4W A:\OSGeo4W (QGIS 4.3 master, Qt 6.11), CMake C:\Program Files\CMake\bin, VS 2022 BuildTools. This file wins over those docs.
