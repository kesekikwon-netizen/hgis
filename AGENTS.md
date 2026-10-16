# Strata (ka-hgis) 작업 지침

Claude Code·Antigravity 등 모든 개발 도구가 이 한 파일을 따른다(`CLAUDE.md`는 이 파일을 그대로 불러온다). 가끔 필요한 절차·경위·경로: `docs/agent/strata-reference.md`. 회귀: `docs/ERROR_REGRESSION.md`, 시험 구조: `docs/testing-map.md`, 데이터 흐름: `docs/architecture/data-flow.md`. 옛 `docs/HANDOFF.md`, `TEST_INFRA.md`, `docs/intent/`, `docs/superpowers/`는 기록일 뿐 현행 요구가 아니다.

## 0. 일하는 방식

- 사용자는 개발자가 아닌 고고학 현장 전문가다. 요청은 짧은 한국어, 증상("안 돼요"), 스크린샷으로 온다. 안내와 보고는 단계 이름까지 간결한 한국어로 쓰고, 영어는 사용자가 입력하거나 누를 경로·명령에만 쓴다.
- "해줘/고쳐줘"가 진행 승인이다. 기술 결정(설계, 접근법, 실행 방식)은 묻지 말고 가장 안전한 쪽으로 끝까지 하고 보고에 한 줄. 묻는 것은 앱이 현장에서 어떻게 보이고 동작할지가 정말 불분명할 때만, 한 번에 묶어서. 사실(코드 동작, 파일 위치, 좌표계)은 직접 찾는다.
- 단일 작업 흐름: 과제마다 작업자·검토자를 따로 돌리는 서브에이전트 분리 방식은 쓰지 않는다. 이 대화에서 차례로 만들고, 기능 하나가 끝나면 한 번 검토해 진짜 지적만 이번 변경 안에서 고치고 나머지는 보고에 한 줄.
- 요청하지 않은 계획서·의도 기록·보고서 마크다운(`docs/intent/`, `docs/superpowers/`, `HANDOFF.md` 등)은 만들지 않는다.
- superpowers 스킬이 있는 도구에서는 해당 단계에 쓴다: 증상·버그 → 원인 찾기(systematic-debugging), 코드 수정 → 시험 먼저(test-driven-development), 끝났다고 말하기 전 → 끝나기 전 확인(verification-before-completion). 없는 도구에서도 같은 순서(원인 → 실패하는 시험 → 수정 → 실제 실행 확인)를 따른다.
- Qt, QGIS, GDAL, PROJ, CMake API를 쓰거나 바꾸기 전에 그 판의 문서(context7) 또는 실제 헤더로 확인한다. 확인할 수단이 없으면 그렇다고 말하고 추측하지 않는다.
- C++ 정의·참조·오류는 grep보다 LSP(clangd)를 먼저 쓴다. `build/compile_commands.json`이 없으면 `cmake -S . -B build`.
- 고치기 세 번 실패하면 말하고, 다음 요청까지 코드를 바꾸지 않는다.
- **사람이 꼭 확인해야 하는 변경**(보고 맨 위에 따로 적는다): ① 좌표계·단위·수치 알고리즘(허용오차는 기존 시험 기준, 예: 5179 SHP 좌표 ≤ 1mm) ② 저장 형식(GPKG 스키마, 조사 파일)과 옛 조사 호환성 ③ 제출 꾸러미(5179 SHP·PDF·MANIFEST) ④ 공통 빌드 설정·의존성·CI, 키·비밀정보.
- **바꾼 시험**: 원래 있던 시험의 기댓값을 바꾸거나(검사문 안의 이름만 바꿔도) 지우거나 건너뛰게(`QSKIP`·`QEXPECT_FAIL`) 했으면 커밋 메시지에 `시험 변경: <이유>` 줄을 쓰고 보고 맨 위에 「바꾼 시험」으로 알린다. 시험을 통과시키려고 기댓값을 근거 없이 바꾸지 않는다.

## 1. 앱 구조와 코드 고치는 방식

- 기술 스택: C++20 / Qt 6 Widgets(Qt 6.11, WebEngine 포함) / OSGeo4W `qgis-dev`(QGIS 4.3 master)의 `qgis_core`·`qgis_gui`, `gdal-dev`. 판은 `dev-env.lock.json`. CMake 3.21+, Visual Studio 2022(preset `vs`). QGIS를 포크하거나 PROJ·GDAL·렌더링·좌표변환을 재구현하지 않는다. GPLv2+ About 고지를 유지한다.
- `src/core` → 정적 라이브러리 `ka_core`(서비스, 창 코드 없음): LayerOps, BasemapOps, SurveyStorage/SurveySession/SurveyProjectFactory, ExportService, LayoutService, ChecklistEngine, ProjectStateBuilder, BufferAnalysis, NearbyHeritageExport, Heritage*, Topographic*, Cadastral*, PolygonErase, FeaturePick, KaSafeQgis, KaLogExcept.
- `src/app` → 실행 파일 `ka-hgis`: `MainWindow.cpp` + `MainWindow{Align,Cadastral,Chrome,ContextMenus,Downloads,Editing,Erase,Export,FeatureCard,Files,Offline,Overlay,Ribbon,Session,SurveyContour,Topographic,Undo}.cpp`, `KaApplication`(시작, `--smoke-quit`, 렌더링 설정), `KaDrawingStudio`(도면 조판), `KaTerrain3dStudio`(3D 지형), Ka* 위젯·지도 도구.
- 시험 실행 파일은 `ka_core`를 링크하고 필요한 `src/app` 파일을 자기 `add_executable`에 직접 적는다. 그래서 시험이 쓰는 새 app 파일은 그 목록에도 넣는다. CMake 소스 목록은 명시적이다(GLOB 없음).
- `data/`는 exe 옆으로 복사된다(규칙 `data/rules/drawing_checklist.v1.json`, 스키마 `data/schemas/ka_hgis_layers.yaml`). `data/`를 고치면 `ka-hgis`를 다시 빌드한다.
- 주변 코드의 이름·주석·모양에 맞춘다. 바뀐 줄은 모두 요청에서 바로 이어져야 하고, 상관없는 옆 코드·죽은 코드는 고치지 말고 보고에 한 줄(내 변경으로 안 쓰이게 된 것만 지운다). 요청하지 않은 설정·확장성·추상화, 일어날 수 없는 경우의 처리는 넣지 않는다.
- 줄 수를 맞추려고 클래스를 여러 파일로 쪼개지 않는다. 논리적 응집도대로 쓴다. 핫스폿(`KaDrawingStudio.cpp`, `MainWindow.cpp`, `LayerOps.cpp`, `BasemapOps.cpp`)은 고치는 범위를 좁게 한다. `scripts/scorecard.ps1 -LineLimitOnly`와 `docs/quality/line-limit-baseline.txt`는 참고용 숫자이며 커밋·CI를 막지 않는다(2026-10-16).
- 편집 도구가 파일 전체에 `clang-format`을 돌리면 내가 바꾼 줄 밖의 포맷 차이는 되돌린다.
- `src/` 아래 모든 `catch (...)`에는 `KA_LOG_EXCEPT();`(또는 `KaSessionLog::line`)를 넣는다(`catch_log` 시험). QGIS/GDAL 호출은 `KaSafeQgis.h` 패턴을 따른다.
- 보안 검토 결과는 이 앱(Windows 데스크톱, 서버·웹 없음)에 해당하는 것만 이번 변경 안에서 고치고 나머지는 보고에 한 줄.

## 2. 도메인 레이어·범례·좌표계·공간 연산 (깨지 않는다)

- 7개 도메인 레이어 `survey_area`(조사구역), `feature_poly`(유구 면), `feature_line`(유구 선), `section_line`(단면선), `control_points`(기준점), `artifact_point`(유물 점), `trial_trench`(트렌치). 정체성은 `ka_hgis/layer_key` 속성이다(한국어 이름은 표시용). 도메인 레이어를 추가하는 길은 `LayerOps::ensureDomainLayer` 하나다. 빈 레이어를 자동으로 넣거나 GPKG 테이블이 있다고 넣지 않는다. `loadSurveyLayers`는 `removeAllMapLayers()`를 부르지 않는다.
- 숨긴 도메인 레이어: 레이어 트리 체크가 꺼진 기존 레이어가 있을 수 있다. 이어그리기 대상(`KaSurveyAreaDialog::layerToContinue`)은 보이는 레이어만 고르고, 편집·도형 추가 때는 `setItemVisibilityChecked(true)`로 레이어와 상위 그룹을 켠다.
- 범례: 레이어 트리 그룹은 「조사 데이터」 / 「참조 지도」, 레이어 패널 표시 구역은 「조사 데이터 / 지적도 / 주변·참조 / 배경 지도」(`KaLayerSectionDelegate`). 배경지도, WMS/XYZ, 수치지형도, 토양도, 지질도, 주변유적, 정합 도면(DXF/DWG)·래스터는 참조 자료이며 조사 데이터에 넣지 않는다. 참조·지적 레이어는 나누기·묶기 같은 원본 변형 명령에서 막는다(`MainWindow::refuseReadOnlyLayer`).
- 지적 레이어(5km 다운로드, VWorld 지적 본번·부번)는 트리 최상위(「참조 지도」 밖)에 둔다. VWorld 지적 그림(WMS)에는 스냅이 붙지 않는다.
- 좌표계: 작업 좌표계는 EPSG:5187(기본) 또는 5186이고 5179로 가정하지 않는다. 제출은 `ExportService`가 EPSG:5179 SHP + `조사도면.pdf` + `MANIFEST.sha256` + `README_submit.txt`를 파일로만 만들고 지도에 넣지 않는다. 검수 오류는 막지 않는다(2026-10-10): 「제출 꾸러미 만들기」가 한 번 묻고 README_submit.txt에 적는다. 조판한 user_sheet가 없으면 조사도면.pdf가 없고, SHP도 시트도 없으면 꾸러미도 없다. DXF는 제출 경로가 아니다. 주변유적 현황 엑셀은 `NearbyHeritageExport::exportToXlsx`.
- 주변 거리 버퍼(500m/1000m): `BufferAnalysis::addDistanceRing`·`NearbyHeritageExport::collect`는 `JoinStyle::Miter`(모서리가 √2배로 튐)를 쓰지 않고 `Qgis::JoinStyle::Round`(36분할)와 유클리드 거리(`sitePoly.distance(g) <= radius`)를 쓴다.
- 정해진 것, 다시 열지 않는다: 시작하면 홈 화면만(자동 복원 없음). 자동 저장 없음(저장/Ctrl+S 또는 닫기 물음에서만, 저장 안 됨은 제목 " *"). 새 조사·다른 이름 저장은 `preferredSurveyDir()`에서 시작하고 바탕화면이 아니다. 다른 이름 저장(`SurveySaveAs::moveToCopy`)은 원본에 커밋하지 않는다. 이름 Strata, 한 줄 리본, 맑은 고딕(번들 IBM Plex는 `KA_HGIS_BUNDLED_FONTS=1`일 때만).

## 3. 크래시·GPKG·키 안전 규칙

- 크래시(`0xc0000005`) 방지: `qgis/parallel_rendering=false` 유지. WMS/XYZ 타일을 그리는 중(`canvas->isDrawing()`)에는 `stopRendering`·`clearCache`·다시 새로고침·블로킹 `GetMap`·`canvas->refresh()`를 부르지 않고 `LayerOps::refreshCanvasIfIdle`을 쓴다.
- 도면 조판(`KaDrawingStudio`/`LayoutService`): QGIS 러버밴드 금지, `setLayers({})` 금지, `refresh()` 직접 호출 금지, `m_toolSelect->setLayout(ly)`를 부른다. 범례를 용지 밖에 놓아도(`ka_hgis/legend_kept_on_page`) 용지 안으로 되밀지 않는다.
- 편집 버퍼: 저장 전 피처는 음수 id다. 단일 POLYGON GPKG 테이블의 멀티폴리곤은 면적 0 링이 되므로 조각별 피처로 저장한다(`PolygonPieces`). undo 스택은 레이어마다 따로다. 도형 생성과 속성 입력은 `KaUndoGroup` 한 단계로 묶는다.
- OGR GPKG `featureCount()`는 실패하거나 낡은 값을 줄 수 있으므로 도형 유무·개수는 `LayerFeatures::any`·`LayerFeatures::count`로 본다. 지운 조사 레이어는 `SurveyHandles::release`로 핸들을 놓는다(저장 때 `MoveFileEx` 거부 방지).
- 키: VWorld 키·개인 키를 코드에 박지 않고 로그·오류에 드러내지 않는다(`SecretMask`). `loadApiKey` 순서: `%LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini` → `HKCU\Software\ka-hgis` → 환경변수 `VWORLD_API_KEY` → gitignore된 `config/secrets.ini`. 위성은 키가 있으면 api.vworld.kr WMTS 먼저, 없을 때만 xdworld. 지적 WMS는 `crs=EPSG:3857` + KEY, DOMAIN은 보내지 않고 CRS 후보는 4326→3857→900913만. 만료된 키는 HTTP 200 + XML 오류라 빈 타일만 나오고 로그가 없다.
- 사용자의 원본 조사 데이터(`*.gpkg`, `*.qgz`)는 읽기 전용으로만 열고 고치지 않는다. 시험은 `QTemporaryDir`와 `QStandardPaths::setTestModeEnabled(true)`만 쓰고 앱의 조직·앱 이름을 쓰지 않는다.
- 회귀: 익숙한 증상이면 `docs/ERROR_REGRESSION.md`를 먼저 읽는다. 새 버그를 고치면 그 표에 한 줄과 회귀 시험을 더한다. 지도·좌표계·레이어 순서·조판 버그는 고치기 전에 프로젝트/레이어 CRS, 캔버스 레이어 순서, 축척, provider URI를 모은다.

## 4. 빌드·시험·실행 (Windows, 작업 폴더 최상위)

- 빌드·ctest·스모크는 같은 셸에서 `. ./scripts/dev-env.ps1`을 먼저 적용한다. 없이 `build/Release/ka-hgis.exe`를 띄우면 「Qt6Xml.dll이 없어…」 창이 떠서 멈춘다. Bash 도구에서는 `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; …'` 형태로 부른다.
- 첫 빌드(`build/CMakeCache.txt` 없음, 약 8분): `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-now.ps1`
- 이후 빌드: `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; cmake --build build --config Release --parallel --target ka-hgis <test-target>; exit $LASTEXITCODE'` (C1060, MSB4018, MSB4166, 0x800705AF가 나면 `-- /m:1 /p:CL_MPCount=2`).
- 시험은 건드린 묶음만(`docs/testing-map.md`, 로그 `build/test-logs/`): `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; ctest --test-dir build -C Release -j4 --output-on-failure -R "^(name1|name2)$"; exit $LASTEXITCODE'`. 그냥 `ctest`(전체)는 요청할 때만. perf_engine, heritage_download_retry, topographic_browser, survey_contour, startup_splash는 부하에서 흔들리니 먼저 혼자 다시 돌린다. 알려진 실패는 없다(2026-10-03부터): 실패는 모두 진짜다.
- 시험은 기존 QtTest 파일에 넣는다(`ka_add_qtest(<name> <target>)`; test_save_open.cpp의 새 함수는 `ka_add_qtest_filter(save_open_* …)`도). 시험할 틈이 없는 UI는 코드를 두고 빌드와 가장 가까운 시험으로 확인한다.
- 스모크(종료 코드 0): `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $p = Start-Process build/Release/ka-hgis.exe -ArgumentList "--smoke-quit" -WorkingDirectory build/Release -Wait -PassThru; exit $p.ExitCode'`. 사용자의 앱이 이 exe로 떠 있으면 다시 빌드하기 전에 exe 이름을 바꾼다. 제목에 " *"가 없을 때만 닫는다.
- 앱 빌드가 성공할 때마다: 스모크 후 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-and-shortcut.ps1 -ShortcutOnly`로 바탕화면 「고고학 전용 HGIS」 바로가기를 갱신한다(바탕화면은 OneDrive로 옮겨졌을 수 있다). `scripts/update-app-icon.ps1`은 옛 파란 아이콘으로 되돌리므로 돌리지 않는다.
- 바탕화면에 개인용 포터블 폴더(`Strata-포터블-개인용-*`)가 있으면 앱 변경 뒤 최신 `build/Release/ka-hgis.exe`를 그 폴더에도 복사하고 그 폴더 기준 `--smoke-quit`으로 확인한다. 새 「포터블」은 요청할 때만(스킬 `portable-release`, 약 1GB이고 C:가 거의 찼다고 먼저 말한다).
- 클라우드(리눅스) 세션에서는 Windows 빌드·시험을 할 수 없다. 통과했다고 하지 말고 「Windows 빌드·시험 미실행」으로 적는다.

## 5. Git (main = GitHub kesekikwon-netizen/hgis)

- `git remote -v`의 origin이 `kesekikwon-netizen/hgis`인지 먼저 본다. `kwonyoungin11/hgis`는 2026-10-01에 멈춘 옛 사본이다. 작업 전 `git status`로 사용자의 미커밋 변경을 확인하고 덮어쓰거나 지우지 않는다.
- `<main>` = 메인 체크아웃(`git worktree list` 첫 줄; PC별 `D:\hgis` + `D:\OSGeo4W`, `C:\Users\권을\Documents\qgis` + `C:\OSGeo4W`, 옛 `A:\qgis` + `A:\OSGeo4W`). 세션 작업 폴더에서 일하고 새 워크트리를 더 만들지 않는다.
- 커밋 메시지 `type(scope): 한국어 요약`, 본문에 시험 결과 한 줄. Bash 도구에서 커밋한다.
- 검증된 커밋: main이 움직였으면 rebase하고 건드린 시험을 다시 돌린 뒤 `git -C <main> merge --ff-only <sha>`, `git -C <main> push origin main`. GitHub CI는 취소·비활성화·대기하지 않고 다음 작업 때 마지막 실행을 한 번 본다. PR은 요청할 때만(클라우드 세션은 브랜치 + 초안 PR).
- `.claude/hooks/commit-gate.mjs`는 `src/`, `tests/`, `cmake/`, `data/`, `CMakeLists.txt`를 건드린 커밋을 빌드와 그 뒤 ctest가 증명할 때까지, 시험을 약하게 바꾼 커밋을 `시험 변경:` 줄이 있을 때까지 막는다. 이유를 따르고 우회하지 않는다.
- 작업 폴더에만 있는 작업은 데스크톱 세션을 지우면 사라진다. 잃어버린 작업 신고는 `git -C <main> for-each-ref refs/snapshots`부터 본다.
- 금지: force-push, `scripts/auto-git-push.ps1` 실행, 비밀정보(`config/secrets.ini`)·`*.gpkg`·`*.qgz`·포터블 설정 커밋.

## 6. Windows 함정

- 저장소의 다른 .ps1도 Bash에서 같은 `powershell.exe -NoProfile -ExecutionPolicy Bypass` 형태로 부른다. 한국어가 든 .ps1은 UTF-8 BOM이 있어야 한다.
- moc는 Q_OBJECT 파일의 `R"(...)"`를 못 읽는다(조용한 링크 실패).
- Git Bash에서는 `>nul`이 아니라 `/dev/null`.
