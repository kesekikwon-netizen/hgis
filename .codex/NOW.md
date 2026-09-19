## 2026-09-20 리본 칸 고정·창 너비 안 나눔

- 원인: 그룹이 Preferred라 `QHBoxLayout`이 창 남는 폭을 칸 사이에 나눔.
- 칸은 같은 크기(`ribbonChipWidth` 56, `ribbonHeight` 82, QSS min/max). 그룹 Maximum. `addStretch(1)`이 남는 폭을 오른쪽이 먹음. https://doc.qt.io/qt-6.8/qboxlayout.html#addStretch
- 넘침 버튼은 마지막 칸 옆. 1366에서 좌표 정합은 리본. CTest `theme_qss` Passed 0.75초.
- clangd `setFixedSize` L169 → Qt `qwidget.h` L526/L912. `diagnostic_error_count` 12는 기존 compile DB INCLUDE. Graft `applyTwoLine` L148.
- Archify workflow showcase 9/9 deliver visual-check pass → `build/qa/ribbon-pack-20260920/ribbon-pack.workflow.html`.
- EXE SHA256 49D726D6151A95DDD31C26424745BFEA98CEABA83FB717210F44F19CFCE92AC5. 커밋·푸시·포터블·사용자 앱 없음.

## 2026-09-20 리본 짧은 전문 용어

- 칸 글자: 신규·열기·도화·측거·수치·지적·대동여지·1919지형·정합·버퍼·유산·도면·5179. 전체 뜻은 툴팁.
- 폭은 `boundingRect` + `PM_ButtonMargin`/`PM_DefaultFrameWidth`. 한글이 `horizontalAdvance`보다 넓어 잘리던 것 보정.

## 2026-09-20 리본 간격 축소

- `ribbonChipGap=0`, `ribbonGroupPad=1`, `ribbonMinWidth=36`, `buttonPadding=1`. 조판용 `buttonSpacing=4`는 유지.
- overflow는 왼쪽 그룹을 남기고 오른쪽부터 접음. 좌표 정합이 내보내기보다 먼저 사라지지 않음.
- 목표: 현장 1366에서 「더 많은 작업」 없이 좌표 정합이 보임.
- CTest `theme_qss` Passed 0.75초. `ribbonOverflow_keepsAlignAtFieldWidth` 포함. Release `ka-hgis` 링크.

## 2026-09-20 리본 글자 한 줄

- `KaBeginnerRibbon::twoLine`이 4글자 초과·공백에서 두 줄로 쪼개던 동작을 없앰. 줄바꿈은 공백으로 붙이고 폭은 한 줄 전체.
- 긴 칸은 짧은 한 줄: `1919지형도`, `사진·CAD정합`, `주변범위`, `유적받기`, `GeoTIFF저장`, `5179내보내기`. 전체 이름은 툴팁.
- 근거: Qt 6.8 `QToolButton::toolButtonStyle` https://doc.qt.io/qt-6.8/qtoolbutton.html

## 2026-09-20 배경 지도 대동여지도·1919 조선지형도

- 리본 `배경 지도`에 **대동여지도**(키 없음), **1919 조선지형도 1:5만**(키 있을 때만 활성). 새 리본 없음. 더보기 **API 키 입력** 유지.
- 대동여지도: 국토정보플랫폼 역사지도 WMS `https://map.ngii.go.kr/spcemapserver/korea_old_map/ows` 레이어 `korea_oldmap_ddymap_kyu`, EPSG:5179, WMS 1.1.1. 2026-09-20 GetCapabilities/GetMap PNG 200, 키·Referer 없음. 뷰어 `https://map.ngii.go.kr/ms/map/NlipMap.do?tabGb=daedong`. 가짜 Google/Kakao URI 없음.
- 1919: 공식 `https://hgis.history.go.kr/api/intro.do` WMTS. VWorld 키로는 인증 안 됨. `HistoryGis/ApiKey`를 더보기 API 키 입력 둘째 칸에 저장. 예제 키 `840e2e2c-...` 소스 없음.
- 레이어는 `markReferenceLayer` + 이름 매칭으로 참조. `placeInLegendGroup`은 기존처럼 그룹명을 쓰지 않음(ORIG-3).
- CTest `cadastral` Passed 4.16초, `theme_qss` Passed 0.63초. Release `ka-hgis`·`ka_cadastral_tests` 빌드. EXE SHA256 03502542FFCB9024BA81518B694FE6A9B7BD00E4178C7492DFDA31BFEFE39DC0 (07:58:38).
- clangd `daedongyeojidoWmsUri` L1432→h:165, `addDaedongyeojidoMap` L1450→h:166, MainWindow L3252→h:166. `diagnostic_error_count` 4/21(기존 compile DB INCLUDE). Graft LayerOps/BasemapOps/MainWindow 조회.
- Archify workflow showcase 9/9 deliver visual-check pass → `build/qa/historical-maps-20260920/historical-maps.workflow.html`. Viewer UI 영어 fallback.
- 커밋·푸시·포터블·사용자 앱 없음.

## 2026-09-20 스플래시 만든이 권영인만

- 첫 화면 `KaSplashCredits::creators()`는 `권영인`만. 표제란 만든이와 footer `copyrightLine()`이 같은 값을 쓴다. 세 이름 문자열은 스플래시에 다시 넣지 않음.
- About `MainWindow::showAbout` L5394는 `권영인 · 조유량 · 박종환` 유지. `KaStartPage`는 이름 없음.
- CTest `startup_splash` Passed 17.91초. `ka_startup_splash_tests`·`ka_hgis_tests` Release 빌드. `ka-hgis.exe` 링크 성공(07:50:27). 고지도 심볼은 미수정.
- clangd `creators` L29→h:22, `copyrightLine` L33→h:23, `.arg(creators())` L35→L29. Graft L29/L33/L42. Archify sequence showcase 9/9 visual-check pass → `build/qa/splash-credit-20260920/`.
- 커밋·푸시·포터블·사용자 앱 없음.

## 2026-09-19 7-4 시험 지도

- `docs/testing-map.md` 축 A–J. G는 `theme_qss` `versionAndLaunchScripts_exist`. theme_qss Passed 0.80초.
- Archify showcase 9/9 → `build/qa/7-4-20260919/`. 커밋·푸시·포터블 없음. 다음: 8-3 또는 8-5 (8-1·8-2·8-4 결정 대기).

## 2026-09-19 7-3 save_open 분할

- 9개 CTest, TIMEOUT 60, RESOURCE_LOCK. 최장 31.84초. 240초 우산 제거.
- `openWhileDrawing` 폼 exec 정지 → `captureAndDismissForm`. window/topo/drawing/open Passed.
- edit/roundtrip/open_invalid/commit/saveas는 슬롯 실패(시간 아님). Archify 9/9. 다음: 7-4.

## 2026-09-19 7-2 CTest 5회

- 스크립트 `scripts/ctest-flake.ps1`. 5회(save_open 제외) 51/55 5/5 통과. 불안정 0.
- 고정 실패: `catch_log`(SurveySession 무로그 catch), import/scope/reference exe 미빌드. `save_open_window`는 240초 타임아웃(7-3).
- Archify showcase 9/9 → `build/qa/7-2-20260919/`. 커밋·푸시·포터블 없음. 다음: 7-3.

## 2026-09-19 6-5 낡은 문서 정리

- `PO_GOAL_*` 10개 + `HANDOFF_TOPOGRAPHIC_GROK.md` → `docs/archive/`. 현행 색인 `docs/README.md`.
- HANDOFF 두 파일 L477·L485, design chrome 문서 링크를 archive로. 현행 HANDOFF는 보관하지 않음.
- 문서만. Graft/clangd/CMake/CTest N/A. Archify showcase 9/9 visual-check pass → `build/qa/6-5-20260919/`.
- 커밋·푸시·포터블·사용자 앱 없음. 다음: 7-2 (7-1은 결정 5로 건너뜀).

## 2026-09-19 6-4 LayerOps 분할

- `LayerOps::` 본문을 `BasemapOps.cpp` / `LabelOps.cpp` / `ControlPointCsv.cpp`로 옮김. `LayerOps.h` API 유지. `LayerOps.cpp` 2930줄(≤3000).
- CTest: theme 0.81s, dem 2.16s, parallel 11.55s, storage 38.60s, workflow Passed. smoke-quit 종료 0.
- clangd addVworldSatelliteMap h:136 오류 0. Graft L1036. Archify showcase 9/9 visual-check pass → `build/qa/6-4-20260919/`.
- EXE SHA256 4C80BF5A0D54649A2C468836D37FCE0CD1026DD6139A60B5572DCFE6559C437B. 커밋·푸시·포터블·사용자 앱 없음. 다음: 6-5.

## 2026-09-19 6-3 SurveySession persistWork

- `SurveySession::persistWork`가 `persistWorkspace` + 동반 QGZ. UI는 `MainWindow::persistSurveyWork`. 열기/저장 본문 `MainWindowSession.cpp`. `MainWindow.cpp` 5477줄(≤5500).
- CTest: storage_safety 38.48s, recent 0.14s, workflow ~65s, theme 0.85s Passed. smoke-quit 종료 0. `save_open_window`는 7-3.
- clangd persistWork h:31, persistSurveyWork h:141, 오류 0. Graft L19/L782. Archify showcase 9/9 visual-check pass → `build/qa/6-3-20260919/`.
- EXE SHA256 23934E4108BA93D023139B9F1BDBFC447AACC761A7CD374948366399855854AF. 커밋·푸시·포터블·사용자 앱 없음. 다음: 6-4.

## 2026-09-19 6-1·6-2 MainWindow 제출/편집 분할

- 제출·조판은 `MainWindowExport.cpp`(637줄). 그리기·스냅·속성은 `MainWindowEditing.cpp`(1389줄). `MainWindow.cpp` 7127줄(6-1 7500 이하, 6-2 6500은 6-3).
- CTest: theme/export/storage/workflow/dem_trench/terrain Passed. `save_open_window` 240초 Timeout(7-3). smoke-quit 종료 0.
- clangd exportShpPackage h:163, beginEdit h:337, 오류 0. Graft L265/L853. Archify 6-1 showcase 9/9 visual-check pass. 6-2는 label 간격 1건으로 HTML 미전달.
- EXE SHA256 910224A0C77FE2904FE10D1109DA50BD07543554DDE66F5ED4EF05673891BFD6. 커밋·푸시·포터블·사용자 앱 없음. 다음: 6-3.

## 2026-09-19 5-3 건너뜀 · 5-4 GUI 기록 서식

- 5-3: 시작 게이지는 사용자 승인 항목. `KaStartupSplash` 미수정. 이유 `build/qa/5-3-20260919/REPORT.md`.
- 5-4: 서식 `docs/user/gui-scenario-checklist.md` (화면·조작·관측). 기록 1부 `docs/user/gui-scenario-records/2026-09-19.md`는 빈 칸·사용자 확인 대기. C++ 빌드 없음.
- 커밋·푸시·포터블·사용자 앱·원본 GPKG 없음. 6-1은 시작하지 않음. 다음: 6-1.

## 2026-09-19 5-2 리본 Tab/Enter로 새 조사→저장

- 핵심 8명령 단축키: Ctrl+N/O/S, Ctrl+Shift+S, Ctrl+D, Ctrl+1, Ctrl+L, Ctrl+E. `applyTabOrder` + Enter→`animateClick`.
- CTest `theme_qss` Passed 1.10초, `ribbon_tabEnterNewSurveyToSave` PASS. smoke-quit 종료 0.
- clangd applyTabOrder L197→h:25 오류 0. Graft L24/L25. Archify showcase 9/9 visual-check pass → `build/qa/5-2-20260919/`.
- EXE SHA256 F0BB7C592F85A8813CC3FCA0E513CF495747C5D8506F42BBB16B98FBB054E85F. 커밋·푸시·포터블·사용자 앱 없음. 다음: 5-3 건너뛰고 5-4.

## 2026-09-19 5-1 작은 창에서도 레이어 목록 5행

- 파일함 minHeight 200이 목록을 1px로 밀던 경로를 제거. `protectSidebarList`가 부족하면 파일함·표시 설정을 접고 `setSizes(목록, 0)`.
- CTest `layer_information` Passed 3.53초. 네 창 크기(1366×768/100, 1920×1080/100·150논리·200논리) 모두 PASS. smoke-quit 종료 0.
- clangd protectSidebarList L40→h:25 오류 0. Graft L24/L71. Archify showcase 9/9 visual-check pass → `build/qa/5-1-20260919/`.
- EXE SHA256 B11EED9B1A7AE1A724CF74E6142CBB05A29896944EE3E91D9F68F8A685865FB3. 커밋·푸시·포터블·사용자 앱 없음. 다음: 5-2. 5-3은 승인 대기.

## 2026-09-19 4-5 세션 로그 10MB 회전·덤프 경로 안내

- 기본 상한 `KaSessionLog::kDefaultMaxBytes` = 10MiB. 초과 시 `session.old.log`로 회전(기존 파일은 remove 후 rename, https://doc.qt.io/qt-6/qfile.html). 시험은 `KA_HGIS_LOG_MAX_BYTES=2048`.
- `KaCrashGuard::dumpHint()` / 정보 창 / 부트 로그에 session.log·crash-*.log·crash-*.dmp 경로.
- CTest `catch_log` Passed 0.30초, `gdal_error_log` Passed 0.29초. smoke-quit 종료 0. clangd maxBytes L19→h:16, dumpHint L241→h:22, 오류 0. Graft L16/L17. Archify showcase 9/9 visual-check pass → `build/qa/4-5-20260919/`.
- EXE SHA256 196063651FA27133F300062CB8866F583182E35D92ED31341D61B314F17D2744. 커밋·푸시·포터블·사용자 앱 없음. 다음: 5-1.

## 2026-09-19 4-4 지적 준비 교차 중복제거를 30% 이상 줄임

- 합성 9600필지: 예전 WKB SHA256 76ms → cheap key 26ms (같은 자료 66% 단축). 준비 245ms = hash 15 / clip 173 / index 23. 지번 첫 렌더 736ms.
- 지번 글꼴은 `QgsTextFormat::setFont(Malgun Gothic)`. 구미·칠곡 원본 ZIP은 쓰지 않음. 현장 46.5s는 재측정 없음.
- CTest `cadastral` Passed 4.61초. clangd cheapParcelKey L51·setFont → qgstextformat.h:200 오류 0. Graft L51. Archify showcase 9/9 visual-check pass → `build/qa/4-4-20260919/`.
- EXE SHA256 C72D8585FB6F3A7513072902BF7BAEE2B7F45B188A79E22249E71987925CF933. 커밋·푸시·포터블·사용자 앱 없음. 다음: 4-5.

## 2026-09-19 4-3 catch(...)는 세션 로그에 남김

- 코어는 `KaSessionLog::line`, 앱은 `KaCrashGuard::logLine` 래퍼. `src/`의 `catch (...)` 본문은 모두 로그(종료 핸들러는 `appendUtf8`).
- CTest `catch_log` Passed 6.05초, `gdal_error_log` Passed 5.09초. 첫 `ka-hgis` 병렬 빌드는 C1060, `/m:1`로 성공.
- clangd KaSessionLog.cpp:19 → KaSessionLog.h:9, LayerOps.cpp:1324 → L9, 오류 0. Graft L9/L10. Archify showcase 9/9 visual-check pass → `build/qa/4-3-20260919/`.
- EXE SHA256 806A40897081DAE66709D4F04D642DD30DCAA6BCFCA3A2EB1B805667A7561EA8. 커밋·푸시·포터블·사용자 앱 없음. 다음: 4-4.

## 2026-09-19 4-2 병렬 렌더는 재현 안 되어 꺼 둠

- 자식 `QProcess --parallel-wms-child`가 로컬 XYZ를 붙잡고 `setParallelRenderingEnabled(true)` + `stopRendering`. `crashed=false` `exitCode=0`.
- 제품은 `KaApplication.cpp:764` `qgis/parallel_rendering=false`, 캔버스는 `setParallelRenderingEnabled(false)` 유지. `true`로 바꾸지 않음.
- CTest `parallel_render` Passed 10.97초. clangd KaApplication.cpp:764 → qgssettings.h:218 오류 0. Graft L764. Archify showcase 9/9 visual-check pass → `build/qa/4-2-20260919/`.
- 제품 EXE는 3-5 빌드 유지. `gen-compile-commands.ps1` 미재생성. 커밋·푸시·포터블·사용자 앱 없음. 다음: 4-3.

## 2026-09-19 4-1 15만 합성은 상한을 넘으면 실패

- `ka_perf_tests`: 필지 SequentialJob 8000ms, 조사 열기(`addNonEmptyDomainLayers`) 1000ms, 조판 `renderPageToImage` 10000ms. 관측 약 3.6s / 0.18s / 4.1–5.1s.
- CTest `perf_engine` Passed 16.10초. 합성 GPKG만.
- clangd test_perf.cpp:160 → LayerOps.h:342. diagnostic_error_count=1(시험 파일). Graft L342. Archify showcase 9/9 visual-check pass → `build/qa/4-1-20260919/`.
- 제품 EXE는 3-5 빌드 유지. 커밋·푸시·포터블·사용자 앱 없음. 다음: 4-2.

## 2026-09-19 3-5 자기교차·빈 도형·0면적은 제출 차단

- `ProjectStateBuilder`가 도메인 키 도형을 `isGeosValid`/`isEmpty`/`area`로 본다. 규칙은 `GEOMETRY_VALID`·`GEOMETRY_NOT_EMPTY`·`GEOMETRY_NONZERO_AREA`. 빈 도형은 자기교차로 치지 않는다.
- CTest `export_survey_areas` Passed 2.85초, `checklist_engine` Passed 10.55초. 나비 자기교차·빈 도형·0면적 슬롯 포함.
- clangd ProjectStateBuilder.cpp:86 → qgsgeometry.h:618, MainWindow.cpp:6011 → ProjectStateBuilder.h:6 오류 0. Graft L44 / L81 / L52. Archify showcase 9/9 visual-check pass → `build/qa/3-5-20260919/`.
- EXE SHA256 9307FEC7DB2F6FD50D6FB53437A79C06D2DA699BC4A0D9D3B415481AE5DCC23F. `gen-compile-commands.ps1` 미재생성. 커밋·푸시·포터블·사용자 앱 없음. 다음: 4-1.

## 2026-09-19 3-4 그리기 뒤 이름·번호는 선택

- `KaFeatureFormDialog`는 `addFeature` 뒤에만 뜬다. 건너뛰기(취소)해도 도형은 남는다. 저장은 `applyFeatureFormValues`가 `changeAttributeValue`로 이름·번호를 쓴다. 파일 커밋은 조사 저장.
- CTest `workflow_engine` Passed 62.56초. `featureForm_cancelKeepsGeometryAndOkWritesNameNumber` 포함.
- clangd MainWindow.cpp:5344 → LayerOps.h:76 오류 0. Graft L76 / L375. Archify showcase 9/9 visual-check pass → `build/qa/3-4-20260919/`.
- smoke-quit exit 0. EXE SHA256 40EF83DA82CFF271F9244A6EA9E0E3EC47B91D9B84F49C9E313CCF42857EB317.
- 커밋·푸시·포터블·사용자 앱 없음. 다음: 3-5.

## 2026-09-19 3-3 공유 경계는 위상 편집

- 그리기 막대 「공유 경계」가 `QgsProject::setTopologicalEditing`과 `ka_hgis/topological`을 쓴다. 켜면 `applyVertexMove`가 같은 레이어 1mm 안 꼭짓점을 같이 옮긴다. 끄면 끈 도형만.
- CTest `workflow_engine` Passed 63.27초. `topologicalVertexMove_movesSharedVertexOnBothFeatures` 포함.
- clangd KaVertexEditTool.cpp:267 → LayerOps.h:378 오류 0. Graft L378–379. Archify showcase 9/9 visual-check pass → `build/qa/3-3-20260919/`.
- smoke-quit exit 0. 바로가기 현재 Release. EXE SHA256 B60819103E829C899BEEDC4529D7B8023C4D0CB55E06808E25B2B376E58B4F23.
- 커밋·푸시·포터블·사용자 앱 없음. 다음: 3-4.

## 2026-09-19 3-2 자석 설정은 그리기 막대 한 곳

- `KaSnapSettingsWidget`: 켬/끔·픽셀 허용치·현재 레이어/모든 조사 레이어. `applySnapSettings`가 `QgsSnappingConfig`+`ka_hgis/snap_target`. 참조 지도는 AdvancedConfiguration에서 뺌. 레이어 추가 때 다시 적용.
- CTest `workflow_engine` Passed 62.65초. `snapSettings_surviveProjectWriteAndReopen` 포함.
- clangd MainWindow.cpp:1170 → LayerOps.h:372 오류 0. Graft L372–373. Archify showcase 9/9 visual-check pass → `build/qa/3-2-20260919/`.
- smoke-quit exit 0. 바로가기 `고고학 전용 HGIS.lnk` → `start-ka-hgis.vbs` → `launch.ps1` → 현재 Release.
- EXE SHA256 0D5D610EFE282F1CEBAEFFE98D69E98D3972738A0761CC5849284ABD08179E75. `gen-compile-commands.ps1`는 vcvars 줄 길이로 실패, 기존 compile DB 사용. 커밋·푸시·포터블·사용자 앱 없음. 다음: 3-3.

## 2026-09-19 3-1 지도 Undo/Redo는 레이어 undoStack

- 그리기·정점·삭제는 `runEditCommand`로 버퍼에만 남긴다. 리본·Ctrl+Z/Y가 `undoLayerEdits`/`redoLayerEdits`로 `undoStack()`을 호출한다. 파일 쓰기는 조사 저장.
- CTest `workflow_engine` Passed 61.52초. `mapEditUndoRedo_drawMoveDeleteRestoresFeature` 포함.
- clangd MainWindowUndo.cpp:194 → LayerOps.h:355 오류 0. Graft L355–356. Archify showcase 9/9 visual-check pass → `build/qa/3-1-20260919/`.
- smoke-quit exit 0. 바로가기 `고고학 전용 HGIS.lnk` → `start-ka-hgis.vbs` → `launch.ps1` → 현재 Release.
- EXE SHA256 A3C00996FF8857A48451C1DF3ADC04CC5824757F2FF1F786D7D5CD6F15175FAB. 커밋·푸시·포터블·사용자 앱 없음. 다음: 3-2.

## 2026-09-19 2-6 기준점 축 안내

- GPS 기준점 수동 입력과 CSV가 같은 문구를 쓴다. QGIS X=동쪽·Y=북쪽. 측량 X=북·Y=동이면 `X·Y 교환`. 조사구역이 있으면 거리로 교환을 제안한다.
- CTest `workflow_engine` Passed 61.36초. `suggestControlPointAxisSwap_matchesCsvRule` 포함.
- clangd MainWindow.cpp:5935 → LayerOps.h:285 오류 0. Graft L283–285. Archify showcase 9/9 visual-check pass → `build/qa/2-6-20260919/`.
- EXE SHA256 F5964AA2AEEFE7368437A6C1EFBAF08B5F8CB7BDACD735DEF83E9B5299DC54AF. smoke-quit·바로가기 확인은 안 함. 커밋·푸시·포터블·사용자 앱 없음. 다음: 3-1.

## 2026-09-19 2-2 제출 SHP는 PROJ 5179와 1mm 안

- `exportShp_matchesProjDirectWithinOneMillimetre`: 5186·5187 점을 `ExportService` SHP와 GDAL OSR(PROJ)로 비교, hypot ≤ 0.001m. CTest `export_survey_areas` Passed 2.42초.
- 이 SDK에 cs2cs/proj.h 없음. 공식 GDAL OSR: https://gdal.org/en/stable/tutorials/osr_api_tut.html
- 2-1 미확정(사용자 저장본 없음). 2-3·2-4·2-5는 결정 1·8 없어 건너뜀.
- clangd ExportService.cpp:295 → qgscoordinatetransform.h:61 오류 0. Archify showcase 9/9 visual-check pass → `build/qa/2-2-20260919/`.
- 커밋·푸시·포터블·사용자 앱 없음. 다음: 2-6.

## 2026-09-19 1-4 참조 벡터를 조사 파일 밖으로

- `더 많은 작업` → `참조 벡터를 조사 파일 밖으로…`. 확인 기본값 No. `extractEmbeddedReferenceVectors`가 sidecar `참조지도/`로 옮기고 세대 파일에서 테이블 삭제·VACUUM·교체. 도메인 키는 제외.
- CTest `storage_safety` Passed 24.29초. `extractEmbeddedReferenceVectors_shrinksSurveyAndKeepsDomain` 포함. 합성 GPKG만.
- clangd MainWindow.cpp:7510 → SurveyStorage.h:94 오류 0. Graft L748. Archify showcase 9/9 visual-check pass → `build/qa/1-4-20260919/`.
- 첫 Release 빌드는 없는 타깃 `ka_hgis_core`로 MSB1009. `ka_core`로 재빌드 성공. 전체 CTest 첫 병렬 48/53은 전용 TEMP 폴더 5개 없음. 복구 후 해당 5개 Passed. `recent_surveys`는 `persistWorkspace`를 본다.
- EXE SHA256 85866298E7ABD62AC79D8809B59F35AC64395151B85203FF36A80007E82EC78D. 커밋·푸시·포터블·사용자 앱 없음. 다음: 단계 2.

## 2026-09-19 1-3 세대 파일에 쓴 뒤 원본을 교체

- `publishSurveyGeneration` = 검증 + `copySurvey`. `persistWorkspace`는 `.ka-survey-gen`에 편집·흡수·내장 쓰기를 하고 성공할 때만 원본을 바꾼다. `writeProject` 예외 시 원본 해시·피처 수 유지.
- CTest `storage_safety` Passed 21.81초. `persistWorkspace_writeExceptionKeepsPreviousGeneration` 포함. 합성 GPKG만.
- clangd persistWorkspace L533 → 헤더 76 오류 0. Graft L533. Archify showcase 9/9 visual-check pass → `build/qa/1-3-20260919/`.
- EXE SHA256 248E8F914042322C629CA66BB683E7D674DCADB8F14D420E3F0805627218C786. 커밋·푸시·포터블·사용자 앱 없음. 다음: 1-4.

## 2026-09-19 1-2 부분 실패는 경고·복구 사본

- `SurveyStorage::persistWorkspace`: 커밋→흡수→내장 쓰기. 한 단계 실패면 `saved=false`, 유효 벡터만 복구 사본. `persistSurveyWork`는 `Notice::Warning`만. Success 없음.
- 시험 3개(`test_storage_safety.cpp`): 둘째 레이어 `setAllowCommit(false)`, GPKG `ReadLock`, 표시 없는 외부 SHP 삭제. CTest `storage_safety` Passed 20.71초. 합성 GPKG만.
- clangd MainWindow.cpp:7429 → SurveyStorage.h:71 오류 0. Graft `persistWorkspace` L459. Archify showcase 9/9 visual-check pass → `build/qa/1-2-20260919/`.
- EXE SHA256 C6647BC2ADBBB02EFC97B4C93D32B509F6FBC1130FD24800061DC3AE7193BF46. 커밋·푸시·포터블·사용자 앱 없음. 다음: 1-3 세대별 저장.

## 2026-09-19 1-1 저장→재열기→제출 5186·5187

- `tests/test_save_open.cpp` `saveReopenSubmit_preservesWorkAndPackage`. 합성 GPKG만. 원본 제주 파일 안 씀.
- 재열기 후 피처 수·속성·작업 CRS·역할·가시성·외부 SHP 경로 유지. 제출 SHP는 5179, 하우스도르프 ≤1mm. PDF 전후 존재·크기 근접.
- 단독 QTest 5186/5187 PASS 8.07초. CTest `save_open_window` Passed 143.52초/240.
- clangd L2562 → ExportService.h:9. Graft 시험 L2493. Archify showcase 9/9 visual-check pass → `build/qa/1-1-20260919/`.
- 커밋·푸시·포터블·사용자 앱 없음. 다음: 1-2 부분 실패 시험.

## 2026-09-19 0-3 VERSION이 CMake·앱을 만든다

- `VERSION` 2.0.0 → CMake `file(STRINGS)` → `project(VERSION)` → `KA_HGIS_VERSION="2.0.0"`. 정보 창·`setApplicationVersion`이 같은 매크로.
- PIN: `QGIS 4.3.0-Master … A:\OSGeo4W`. EXE 3371DCDB… smoke 0, startup_splash PASS.
- Graft 3파일, clangd setApplicationVersion → qcoreapplication.h:88 오류 0. Archify visual-check pass. `build/qa/0-3-20260919/REPORT.md`.
- 0-4는 사용자 화면 대기로 건너뜀. §2 여덟 항은 결정 없어 건너뜀(`build/qa/section2-skip-20260919/`).
- 다음: 1-1 저장→재열기→제출 연속 시험. 커밋 없음.

## 2026-09-19 0-2 MainWindow 경고 4종 0

- C4996 `messageReceived` → L1563 `messageReceivedWithFormat` (qgsmessagelog.h:93). C4456 `treeRoot` → `visibilityRoot`/`orderRoot`. C4505 `projectLayerNames` 심볼 없음.
- MainWindow.cpp 강제 재컴파일, 해당 파일 경고 0. clangd L1563 → 헤더 93, 오류 0. Graft `messageReceivedWithFormat` 2곳.
- Archify showcase 9/9 visual-check pass → `build/qa/0-2-20260919/`. 근거 REPORT.md. 커밋 없음.
- 다음: 0-3 VERSION SSOT. 0-4는 사용자 화면.

## 2026-09-19 0-1 기준선 + 메모리 참조는 흡수

- 계획 0-1: Release CTest **53/53 Passed**, 실패·비활성·skip 0. 벽시계 169.14초. `save_open_window` 169.13초/제한 240. 표와 여섯 도구 근거: `build/qa/baseline-20260919/REPORT.md`.
- 파일 참조 벡터(`isReferenceLayer`이고 provider ≠ memory)는 조사 GPKG에 넣지 않는다. **메모리 레이어는 참조 표시가 있어도 흡수**한다. persistSurveyWork가 커밋을 먼저 하므로 isModified만 보면 닫기 저장에서 빠진다. SurveyStorage.cpp:402.
- 시험: `storage_safety` PASS. `closeSave_preservesMemoryReferenceVectorOnReopen` PASS. smoke-quit 종료 0. 원본 `제주 광령리.gpkg` 안 씀.
- clangd: SurveyStorage.cpp:402 col 19 → LayerOps.h:299 오류 0. MainWindow.cpp:7428 col 43 → SurveyStorage.h:58 오류 0. Python 3.13. Graft `absorbExternalVectors` L380–445, freshness Refreshed:false.
- Archify sequence showcase 9/9 → deliver spec c5276d30… artifact b64107f3… → visual-check pass. `build/qa/baseline-20260919/absorb-memory-ref.sequence.html`. Viewer UI 영어 fallback.
- EXE SHA256 DE2AA95C760608329210838769ACEEE7749D52262A729975913E37A69DC83750 (2026-09-19 10:44:29). 커밋·푸시·포터블·사용자 앱 없음.
- 다음: 0-2 MainWindow 경고 4종. 0-3은 파일상 이미 2.0.0/`A:\OSGeo4W`이나 항목에서 재확인. 0-4는 사용자 화면.

## 2026-09-19 저장 때 파일 참조 벡터는 조사 파일에 넣지 않음

- 표시 없는 외부 벡터는 그대로 조사 GPKG로 들어간다. `ka_hgis/layer_role=reference` 이거나 위성·지적 같은 참조 이름인 **파일** 벡터는 `skippedReference`에만 남기고 복사하지 않는다. 메모리 참조는 위 0-1 항목. 이미 조사 파일 안에 있는 수치지형도는 이번 저장에서 빼지 않는다. 원본 `제주 광령리.gpkg`는 쓰지 않았다.
- 시험: `absorbLeavesReferenceVectorsOutsideTheSurvey` PASS. `surveyFileTravelsAloneWithAbsorbedShp` PASS (표시 없는 주변유적 SHP는 계속 흡수). clangd SurveyStorage.cpp:402 col 20 → LayerOps.h:299, diagnostic_error_count 0. Graft `absorbExternalVectors` L380. Archify sequence showcase 9/9, visual-check pass → build/tooling/tmp/reference-skip.sequence.html (spec 70860e78, artifact da171a33).
- SHP 별칭도 이 실행 파일에 들어 있다. `survey_name`→`surv_name`, `artifact_no`→`artif_no`. GPKG 필드명은 그대로. 기관 필드 사전이 아니라 이 프로그램의 DBF 10바이트 이름이다. `ka_export_survey_areas_tests` 8 PASS (직전 빌드와 같은 ExportService).
- EXE SHA256 E7AC4972827F9C448B5981E3CEC40C8E29256A2DC4373915A5054550158FAEE1 (2026-09-19 08:03:36). 바탕화면 바로가기로 다시 실행해야 보인다. 커밋·푸시·포터블 없음.
- 여기서 멈춘 것: 제출 인코딩 기본값은 UTF-8 (`MainWindow.cpp` 6056, `docs/user/job-cards/07-제출.md`는 UTF-8 또는 EUC-KR). 받는 쪽 확인 전이라 바꾸지 않음. `--no-sandbox`는 2026-09-11 이 PC A/B에서 필요했음 (`KaPortableRuntime.cpp` 240). 병렬 렌더는 `ParallelJob` 크래시 때문에 끔 (`KaApplication.cpp` 761). GPL 문구와 `© 2026 동국문화재연구원`은 정보 창·스플래시에 이미 있음. 소스 제공 URL은 만들지 않음. LTR 전환·CI 러너(`vars.ENABLE_SELF_HOSTED_BUILD`)·다른 PC 시험·실제 제출 접수는 이 자리에서 못 함. 이 브랜치 PR 없음.

## 2026-09-19 더 많은 작업의 찾기 창이 잘림

- 창이 좁으면 찾기 그룹이 「더 많은 작업」 메뉴로 들어간다. 스크롤 영역이 그룹을 줄인 뒤 스크롤 막대가 시·도 칩 마지막 줄을 가렸다. 시·도를 누르면 주소 창이 그 메뉴의 자식이 되어 같이 이상해졌다.
- 수정: 메뉴 안 그룹은 줄어들기 전 크기를 유지하고, 화면보다 클 때만 스크롤을 켠다. 주소 창은 본창에 붙이고 메뉴는 닫는다.
- 시험: 넘친 찾기에서 부산 칩이 메뉴 안에 있고, 주소 창 부모가 메뉴가 아님. `ka_region_locator_tests` 32 PASS, `ribbonOverflow_preservesControlsAndKeyboard` PASS. 수정 전 메뉴 그림은 셋째 줄이 회색 막대에 잘렸고, 수정 후 17개 칩이 다 보였다. clangd KaRegionLocator.cpp:112 오류 0. Graft `updateOverflow` L206. smoke-quit SMOKE_EXIT=0. EXE SHA256 5176F0EC1FCC9ECCFE99D0B2681296DDF45F779FC9B32C256F9D799DE3517F5C (2026-09-19 07:56:41). 사용자 창은 누르지 않음. 커밋 없음.

## 2026-09-19 계정 비밀번호는 DPAPI로만 저장

- `vworld-account.ini`, `ngii-account.ini`, `heritage-account.ini`의 비밀번호는 현재 Windows 사용자 DPAPI(`CryptProtectData`, `CRYPTPROTECT_UI_FORBIDDEN`)로 `password_dpapi`에 넣는다. 평문 `password` 키는 지운다. 개인 파일을 읽을 때 옛 평문이 있으면 그때 옮긴다. `config/` 폴백 ini는 다시 쓰지 않는다. 빈 비밀번호는 두 키를 지워 이 PC 로그인을 끈다.
- 공식 API: https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
- 시험: `ka_topographic_settings_tests` 3개 PASS (특수문자 왕복, 저장 파일에 `ngii/password` 없음, 번들 ini는 평문 유지). `credentialsStoreIsSeparateFromTheTopographicOne` PASS (`password=` 줄 없음). `logDescriptionNeverCarriesThePassword` PASS.
- 근거: Graft `KaSecretStore.cpp` writePassword L63. clangd KaSecretStore.cpp:63 col 22 → KaSecretStore.h:12, diagnostic_error_count 0. `scripts/gen-compile-commands.ps1`은 PowerShell이 `command` 속성을 못 고쳐 실패했고, `build-clangd/compile_commands.json`에 기존 SDK include를 붙여 `build/compile_commands.json`을 다시 썼다. Archify sequence showcase 9/9 → build/tooling/tmp/dpapi-account.sequence.html (spec ead63a97, artifact a84a801f). smoke-quit 종료 코드 0. EXE SHA256 3CB84E25BEC44070B46CE209DA29034FDE7863D9C71CB01BBA932EBFB9F46CA6 (2026-09-19 07:39:08). 사용자 계정 ini와 원본 GPKG는 쓰지 않음. 커밋/푸시 없음.

## 2026-09-19 전체 화면에서 지도가 비는 문제

- 작은 창(1:2,920,505, 지적 본번·부번·위성)에서는 보이고, 전체 화면으로 키우면 지도가 비었다. 리사이즈 중에 `outputSize`·화면 배율을 바로 바꿔서, 끝난 그림이 새 격자와 어긋났다. QGIS `imageRect`는 그 격자로 그림을 놓는다.
- 수정: 리사이즈 필터는 격자를 당장 바꾸지 않는다. QGIS가 500ms 뒤에 스스로 다시 그린 뒤, 700ms 시점에 아직 크기가 다르면 그때만 맞춘다. 그리는 중이면 최대 약 6초까지 기다린다.
- 근거: 설치본이 아닌 master `qgsmapcanvas.cpp` resizeEvent(500ms refresh)·imageRect·refreshMap의 stopPreviewJobs. clangd MainWindow.cpp:3093 오류 0. Graft `scheduleMapDisplayRefresh` L3093. Archify sequence showcase 9/9 → build/tooling/tmp/maximize-map.sequence.html. smoke-quit SMOKE_EXIT=0. EXE SHA256 6730EE734936F2FC8CA6BD27D355AB8DD2E739BFE84FA31C3DB4C0060ABADCAD (2026-09-19 07:25:53). 전체 화면 클릭은 사용자 창에서 하지 않음. 커밋 없음.

## 2026-09-19 미저장 작업은 복구사본에만 남김

- 2분마다, 저장하지 않은 조사 도형만 `복구사본/조사복구_*/복구조사.gpkg`에 복사한다. 참조 지도는 빼되, 그 레이어에 커밋 안 된 편집이 있으면 포함한다. 원본 조사 파일에는 쓰지 않는다. 최신 3개만 남긴다.
- 성공하면 `복구사본/pending.txt`를 쓴다. 저장에 성공하면 그 표시를 지운다. 다음 실행에서 표시가 있으면 「복구 사본 열기 / 나중에」를 묻는다. 자동으로 열지 않는다. 스모크와 테스트 프로세스에서는 묻지 않는다.
- 시험: `recoverySnapshot_layerFilterSkipsUnlistedVectors` PASS, `recoveryPending_notesNewestAndPrunesOldCopies` PASS, `recoverySnapshot_preservesPendingEditsAndSource` central/east PASS. clangd SurveyStorage.cpp:200·MainWindow.cpp:7583 오류 0. Graft `noteRecoveryPending` L200. Archify sequence showcase 9/9 → build/tooling/tmp/recovery-snapshot.sequence.html. smoke-quit SMOKE_EXIT=0. EXE SHA256 80A7C8B258BBB231D27A60C30A678D4DC7411912865F52F05BE2BE802220630D (2026-09-19 07:09:06). 원본 GPKG는 쓰지 않음. 커밋/푸시 없음.

## 2026-09-19 기준점 CSV 미리보기·축 교환·경위도·CP949

- P0-1: 가져오기 전에 미리보기를 띄운다. 조사구역에서 1km보다 멀고 교환한 쪽이 1km 이상 더 가까우면 "X·Y가 바뀐 것 같습니다"를 제안한다. 자동 교환은 하지 않는다. 경위도 열은 EPSG:4326에서 작업 좌표계로 변환한다. CSV는 UTF-8을 먼저 읽고 실패하면 CP949, 그다음 EUC-KR이다.
- 시험: `importControlCsv_keepsKoreanAxesAndEncoding` PASS, `importControlCsvWritesFeatures` PASS (csv-result2.txt, TEST_EXIT=0). 한국 관례 X=98120 Y=148100은 교환 제안 후 (148100, 98120). CP949 `기준1` 유지. lon/lat 126.48, 33.45는 4326 변환과 1m 이내.
- 근거: Graft `previewControlPointsCsv` LayerOps.cpp L5651; clangd LayerOps.cpp:5651 col 40 → LayerOps.h:281 오류 0; Archify sequence showcase 9/9 errors 0 → build/tooling/tmp/control-csv.sequence.html. Release EXE SHA256 8F90727BD7A3971D8D214A70E46BF69F896FCDCF3C7C83EC77CCAC6AD5E82200 (2026-09-19 06:51:05). smoke-quit SMOKE_EXIT=0. 사용자 앱은 꺼져 있었고 원본 GPKG는 쓰지 않음. 커밋/푸시/포터블 없음.
- 코드로 안 한 것: 제출 필드명(규격 문서 필요), 제출 인코딩 기본값(받는 쪽 미확인, 현재 UTF-8), 복구 스냅샷, 참조자료 분리, DPAPI. 비밀번호는 레지스트리가 아니라 `vworld-account.ini`·`ngii-account.ini`·`heritage-account.ini` 평문이다.

## 2026-09-19 저장 후 조사구역 도형이 지도에서 사라짐

- 증상: 저장을 누르면 조사구역 도형이 지도에서 사라진다. 원본 `제주 광령리.gpkg`는 읽기만 했고, 디스크 `survey_area`는 Polygon 1개로 남아 있었다. 세션 로그 06:03:34 저장 직후 `GetNextRawFeature(): sqlite3_step() : unable to open database file`이 반복됐다. `featureCount` 캐시는 1인데 이터레이터가 실패한다.
- 수정: `SurveyStorage::writeEmbedded`가 프로젝트·스타일을 쓴 뒤 `LayerOps::reloadSurveyGpkgReaders`로 같은 GPKG의 OGR 레이어를 다시 연다. 커밋되지 않은 편집 버퍼는 건너뛴다. `persistSurveyWork`는 쓰기 전에 캔버스 렌더를 멈춘다.
- 재현: 원본을 `build/tooling/tmp/jeju-repro`로 복사(파일명 유지)한 뒤 `save_fieldPackageKeepsSurveyAreaReadable`. 수정 전 저장 후 이터레이터 실패(TEST_EXIT=1). 수정 후 같은 검사 TEST_EXIT=0, GDAL unable-to-open 없음. `save_keepsDrawnSurveyArea` SMALL_EXIT=0.
- 근거: Graft `reloadSurveyGpkgReaders` LayerOps.cpp L2371; clangd SurveyStorage.cpp:404 선언 LayerOps.h:328 오류 0, LayerOps.cpp:2380 `reloadData`는 qgsdataprovider.h:461 오류 0; Archify sequence showcase 9/9 errors 0 → build/tooling/tmp/save-survey-area.sequence.html. Release EXE SHA256 B7756D2FCAED5B8FBE5DA5C60AC42B0FE95A0D9B9D7EC291EF17EDDDF40715F9 (2026-09-19 06:28:57). 사용자 앱은 꺼져 있었고 원본 GPKG는 쓰지 않음. 커밋/포터블 없음.

## 2026-09-19 GDAL 세션 로그 핸들러 안전성 보강 (리뷰 반영, Cursor 6도구 루프)

- 커밋 f49fddc/f6f1613의 KaGdalErrorLog 리뷰(request-changes) 반영: CE_Fatal은 snprintf/fputs CRT 전용(Qt·mutex 금지), 핸들러 thread_local 재진입 가드+try/catch, `logLine` thread_local 가드+QRecursiveMutex, `uninstall()` 추가 후 `exitQgis()` 직전 호출, 메시지 개행은 `simplified()`로 한 줄화.
- 테스트 3개 추가(uninstall 복원·재설치, spy previous 중첩 CPLError 재귀 없음, 개행 정규화). 첫 실행에서 `\r\n`→공백 2개 실패를 잡아 수정.
- 근거: Graft `graft_file_api src/app/KaGdalErrorLog.cpp`·`graft_find_all` install L690/uninstall L847/exitQgis L848; clangd KaGdalErrorLog.cpp·KaCrashGuard.cpp 오류 0(test .cpp는 moc 미생성 1건); Archify sequence showcase 9/9 errors 0 → build/tooling/tmp/gdal-error-log.sequence.html; CMake VS2022 Release 빌드 성공; CTest gdal_error_log·checklist_engine·startup_splash·save_open_window 4/4 통과; `run-ka-hgis.ps1 --smoke-quit` exit 0.
- 바탕화면 `고고학 전용 HGIS.lnk` → scripts/start-ka-hgis.vbs → launch.ps1 → build/Release/ka-hgis.exe 확인. EXE SHA256 73C9043A2E00BCF4F286122DF872FBA100EFF7C1F6A5366E5ADC389E151A64C8. 사용자 앱 조작/포터블/커밋 없음.

## 2026-09-18 Cursor 하네스 전환

- 개발 하네스를 OpenAI Codex에서 Cursor로 전환. Cursor가 저장소 문서의 권위 있는 Agent 하네스다.
- USER Cursor MCP(`%USERPROFILE%\.cursor\mcp.json`)에 `hgis_graft` 등록·검증(도구 4개). 저장소 `.cursor/mcp.json` 없음. `.codex/config.toml`은 호환용.
- `AGENTS.md`, `docs/developer-tools.md`를 Cursor Agent 기준으로 갱신. 제품 C++/CMake/스크립트/테스트/커밋 없음.

## 2026-09-16 사용자 요청 v2 포터블 제작

- 배포 ZIP: A:/qgis/dist/ka-hgis-v2-portable-20260916.zip (375,871,305 bytes, 약376MB). 최신 레이어목록·다운로드UI·아이콘 포함. 실행은 압축전체해제 후 ka-hgis.exe.
- scripts/make-portable.ps1·verify-portable-pack 통과. SDK경로제거한환경에서패키지offscreen시작검사0,36provider로드. ZIP전체CRC·내부EXE/QSS/ICO/PNG원본hash일치 확인. 개인계정/API키와QA설정/cache는ZIP에서제외.
- ZIP SHA256 741ebef6a06f7aa84b96add2dfca8fcabc9af0c637524d1e1c5988a2d5ce4d1a. EXE는기존D7C860DF...4CF3. 원본생성폴더dist/ka-hgis-portable에는QA기본설정이남으므로배포용ZIP사용.
- 다른PC실행미검증·코드서명없음. 기존회귀2실패/200%작은전체창배치한계는버전안내에명시. 제품코드수정/사용자앱조작/커밋없음. 근거build/qa/portable-v2-20260916/REPORT.md.

## 2026-09-16 레이어 목록 글자·행간 축소와 상태 구별

- 지도/조판 KaLayerInformation 공통목록만13→10px(약20%), 실제행34→22px. 그룹진한파랑굵게/일반진한회색/숨김옅은회색기울임, 번갈아옅은행배경. 지도지명·번호·심볼은변경없음.
- Release·smoke·대상CTest3/3통과. 전체최초47/50에서theme검사의정규식블록범위오탐을고쳐재검사통과, 기존heritage_flow/e2e_opaque_suite2실패·topographic_browser비활성. 전체save_open182.21초통과.
- 네이티브목록100/150/200%행높이·체크검증pass, 실제지도/조판100/150%통합pass. 200%작은전체창에서는표시설정/파일함이목록viewport를1px로압축하여통합검사fail; 별도배치보완필요. 선행원인/회귀여부단정하지않음.
- Graft소스대조·clangd0오류/SDK선언탐색·Archify9/9및4화면완료. 계획plans/2026-09-16-layer-list-density.md, 근거build/qa/layer-list-density-20260916/REPORT.md, 사용법docs/user/layer-information.md.
- Desktop→현재Release확인. EXE D7C860DF058529FDCBB81D0CC988FEB88A1D777987E6E657F93A3B12778C4CF3. 사용자앱재시작/포터블/커밋없음.

## 2026-09-16 수치지형도 다운로드 창 정합성 보완

- 공통 색상만 적용되고 남은 수치지형도 전용 기본카드/680×360/Qt::Tool 차이를 정리. 일반 비모달 Dialog·680×330, 현재 단계/안내/막대와 상세 보기/취소 구성. 긴 단계·도엽/파일 정보는 상세에 배치하고 펼친 중에도 상태 카드는 유지한다. 상세 영역의 OS 어두운 팔레트 대비도 보완.
- 실제 리본 클릭→MainWindowTopographic→KaTopographicBrowser 경로 QA 추가. 다운로드/인증/범위/좌표 처리 변경 없음. 전체 Release·smoke, Windows100/150/200% UI 각6pass, 최종 실제 리본 검사 및 대상CTest2/2 통과. 전체CTest48/50, 기존 heritage_flow/e2e_opaque_suite 실패·topographic_browser 비활성 유지.
- Graft 소스 대조, clangd 두 제품파일 오류0·공통 configure 선언탐색0, Archify9/9·4화면 검토. 근거 build/qa/topographic-download-ui-20260916/REPORT.md, 계획 plans/2026-09-16-topographic-download-ui.md.
- 바탕화면 바로가기→현재 Release 연결 확인. EXE SHA256 4120EC33FDD463C4DD7D3BD67165D802A12BE422702859816B9604BA2F329097. 사용자 앱 재시작/새 포터블/커밋 없음.

## 2026-09-16 둥근 글로시 UI·바탕화면 흙손 아이콘

- 사용자참조대로 KaIcons 공통타일을 둥근색상/곡선반사광/밝은그림으로 변경. 기능별색·체크·선택테두리·회색비활성 보존,64/128px캐시. 명시적단색아이콘은별도flatglyph캐시로유지.
- 금색흙손 투명PNG를 built-in이미지도구로만들고 Qt내장/WindowsRC/바탕화면ICO로연결.1차가짜격자배경거부,2차진짜RGBA채택.마스터 data/theme/ka-hgis-app.png; scripts/update-app-icon.ps1로8크기ICO생성·네이티브Windows검증. 앱외부ICO우선경로제거로같은내장그림사용.
- 바탕화면 %USERPROFILE%/Desktop/고고학 전용 HGIS.lnk 생성완료. scripts/start-ka-hgis.vbs→launch.ps1→현재Release, 새data/theme/ka-hgis.ico연결확인. 새포터블/커밋/사용자앱·탐색기재시작없음.
- 전체Release·smoke통과. Windows100/150/200%테마각23pass, 실제Qt미리보기및EXE추출아이콘확인. stale내장이미지를검사에서발견하여QRC갱신/원본hash일치검사로해결. 전체CTest48/50통과, 기존heritage_flow/e2e_opaque_suite2실패·topographic_browser비활성,save_open181.38초.
- 실제compileDB282항목·clangd아이콘/boot오류0·Qt선언탐색0, Graft소스대조, Archify9/9·4화면·명암검토. 근거 build/qa/glossy-icons-20260916/REPORT.md, 계획 plans/2026-09-16-glossy-icons.md, 사용법 docs/user/glossy-icons.md.
- EXE SHA256 35E063FAC72CE6D0F9818A9177E897121740A1852ED14E87512B2BDC8D506C34. ICO E3C9E1D2235C87116F4BEB9DE2856A971E6556672BB797DBB8A07A0287272ECA. 다른PC미검증.

## 2026-09-16 찾기 지번 정확 일치 검색

- 오른쪽 위 찾기에서 번지가 있으면 VWorld ADDRESS/PARCEL 검색으로 분리한다. 기존 place 검색과 주소 결과의 title 부재가 지번 검색을 방해하던 경로를 수정했다. 법정동·리/산/본번/부번 및 19자리 PNU의 지번 부분을 확인하고, 불일치·오류·잘린 결과·서로 다른 PNU 후보에는 이동하지 않는다. 번지 없는 지역 검색은 유지.
- 입력창은 법정동·리와 산번지 예시를 안내한다. 세종시 중복 명칭·시도 개칭·세종로·숫자 법정동·생략된 일반구를 처리한다. 일치 PNU의 열린 지적도는 경계 선택과 실제 필지 범위 이동(5186/5187)을 제공하며 원본 도형/속성을 수정하지 않는다.
- Release 전체 빌드·시작 smoke 통과. 대상3개 통과, 최종 네트워크37pass/실시간항목기본skip; 공개 주소 서울 종로구 세종로1-68의 실제 VWorld 결과/PNU 별도 확인 통과. Windows100/150/200% 입력창31pass씩과 이미지 검토. 전체CTest48/50통과, 이전 heritage_flow/e2e_opaque_suite 실패2개·topographic_browser 비활성. save_open183.07초.
- 필수 도구: Graft 후보/소스 대조, 실제 compile DB279항목·clangd 제품3파일오류0 및 MainWindow/SDK선언탐색오류0, Archify9/9·4viewport·명암이미지 검토. 근거 build/qa/parcel-search-20260916/REPORT.md, 계획 plans/2026-09-16-parcel-search.md, 사용법 docs/user/parcel-search.md.
- EXE SHA256 75C82B536F8389AD2D1BA410CD0BCCA9F60E0ACCAF59C420DC96DB4E57FEBB7B. 시작스크립트→현재Release 연결 확인. 이번Desktop에.lnk없음. 새포터블/커밋/사용자앱재시작없음. 실제문제지번미제공·모든지번/다른PC미검증; 기관의 최신 지번 미반영 가능성은 남는다.

## 2026-09-16 v2 시작 안내·다운로드 UI 통일

- 요청대로 파란 글로시/둥근 외곽 KaStartupSplash, v2·만든이 권영인·실제 기술/자료 고지. 초기 준비 후 10,000ms 안내 게이지가 끝난 뒤에만 메인창 표시. 클릭 조기숨김 없음, 이벤트루프 타이머 사용. 홈만 여는 기본 시작·명시적 smoke/QA 빠른 경로 유지. 앱버전2/CMake2.0.0/READMEv2, About공통고지.
- KaDownloadUi 공통 제목/흰 상태 카드·여백·진행률·버튼을 수치지형도/지적도/주변유적에 적용. 지적도는 비모달 전용창에서 기존 QgsTask 진행·취소 연결. 유적 공식자동화/시군범위/원본자료 변경 없음. 수치지형도 간단화면에서도 공식화면 펼치기 지원. 실제 수신율을 알 수 없으면 미확정 표시를 유지한다.
- 실제 Windows100/150/200% 시작화면 각4pass(실제10초/응답/준비gate)와 세 다운로드창 취소·상세전환·진행률 검사 통과. 최종 캡처 직접 검토. 전체 Release·smoke통과, CTest48/50통과, 기존heritage_flow/e2e_opaque_suite2개실패·topographic_browser비활성. save_open_window184.42초. Graft소스대조·clangd3제품파일진단0/선언탐색·Archify9/9 및4viewport명암브라우저/이미지검토완료.
- 근거 build/qa/startup-download-ui-20260916/REPORT.md, 계획 plans/2026-09-16-startup-download-ui.md, 사용법 docs/user/startup-download-ui.md. EXE SHA256 C210B22F9F3BD2A08B2FA930FBFC1CFCC12C8A0E988A337561B0BFC5DECBCDCB. Desktop에GIS.lnk없어실제바로가기재확인은미완료; 시작스크립트→현재Release연결확인.
- 실서버 다운로드/사용자원본/실행앱조작·포터블·커밋·푸시없음. 고지갱신은 전체종속성라이선스감사나전체포터블배포고지검증이아니다. 기존GIS미해결사항유지.

## 2026-09-16 레이어 열 경계 드래그

- 사용자 요청으로 도면 · 레이어 / 글자 사이 헤더 경계선을 좌우로 드래그할 수 있게 했다. 공통 KaLayerInformation 뷰가 첫 열 Interactive/둘째 Stretch를 사용하며, QGIS 기본 resize 이후 선택 폭을 복원한다. 창을 줄이거나 경계를 끝까지 끌 때 글자 체크·문구 폭(최소88px)을 남긴다. 앱 재시작 간 폭 저장은 추가하지 않았다.
- 제품 변경은 KaLayerInformation.cpp/.h, 실제 지도/조판 모드 마우스 드래그·창 확장/복귀·상태 갱신 유지 검사를 test_layer_information.cpp에 추가. 너비 조작의 labelsEdited0회·원본 라벨/도형 유지 확인. 초기 좁은 목록 통합 실패는 최소 글자 폭 보완으로 해결했고 기존 검사 조건을 낮추지 않았다.
- 최종 Release 빌드·smoke·Windows100/125/150/200% 조작 및 실제 MainWindow/Studio 통합 검사 통과. 전체CTest46/48, 기존heritage_flow/e2e_opaque_suite2개실패·topographic_browser비활성. save_open_window190.20초통과. Graft API/소스 대조, clangd오류0/Qt선언 탐색, Archify9/9·4viewport명암브라우저·이미지 검토 완료.
- 근거 build/qa/layer-column-resize-20260916/REPORT.md, 계획 plans/2026-09-16-layer-column-resize.md. EXE SHA25612059BEEB6794C7037DA5465034B182BFEF11F2BBA5750655820FB986D7CAE3B. 시작 스크립트→현재Release 연결 확인. 이번 Desktop에는.lnk가없어실제바로가기재확인은미완료. 포터블·사용자앱종료·커밋·푸시없음.

## 2026-09-16 레이어 도면·글자 체크 UI

- 1차 프롬프트 검토 뒤 사용자 ‘진행’ 승인으로 구현. 지도/레이아웃 왼쪽 목록에 도면 · 레이어 / 글자 두 열, 목록 아래 선택 레이어 표시 설정을 제공한다. 지번·높이·유적명·면적·번호를 의미에 맞춰 표시하며 그룹 부분 체크와 일괄 변경, 부모 숨김/축척 제한 안내를 지원한다.
- LayerLabelControls는 기존 표현식·서체·색·위치·축척을 보존하고 임의 첫 필드를 자동 선택하지 않는다. 내용/면적 편집도 설정을 복제하고 관련 표현식만 바꾼다. 전용 지번/높이/기호에는 일반 필드 편집을 숨긴다. 영상에 합성된 글자는 별도 조작 불가다.
- KaLayerInformation 공통 모델/뷰/패널을 지도와 조판 및 기존 우클릭에 연결했다. QGIS 기본 단일 열 폭·Space 동작을 작은 뷰 보정으로 해결했다. 기본 왼쪽 폭348px·최소300px·들여쓰기12px. 상태 변경 알림120ms 병합, 200레이어 3회 변경71ms·알림1회 관측(지도 렌더 시간 아님).
- 조판 유적의 ka_hgis/layout_numbers_visible는 원본 이름 설정과 독립. 끄면 복제 스타일의 글자와 번호 범례를 제거하고 다시 켜면 페이지 표시 대상만 분류별 연번으로 복원한다. 숨김 상태 삭제·혼합 범례·PDF·QGZ 저장/재열기 검사 통과.
- 최종 전체 Release 빌드·시작 smoke 통과. CTest46/48 통과, 기존 heritage_flow/e2e_opaque_suite 2개 실패·topographic_browser 비활성. save_open_window174.63초 통과. Windows100/125/150/200% DPI 입력 및 실제 MainWindow/Studio 통합 조작 통과, 정상 폭 캡처 검토. Graft 소스 대조·실제 compile DB clangd3제품파일 오류0/SDK선언 탐색·Archify9/9 및4viewport 명/암 브라우저·이미지 검토 완료.
- 근거 build/qa/layer-information-20260916/REPORT.md, 실행계획 plans/2026-09-16-layer-information-ui-implementation.md, 사용법 docs/user/layer-information.md. 바탕화면 고고학 전용 GIS 바로가기→현재 Release 연결 확인. EXE SHA256 F25D75FCCD135E58256344655F4B257928F20D8594299C2FB77CE29F44B2FF4B.
- 포터블/커밋/푸시 없음. 실행 앱·원본 조사·이전 배포 보존. 다른 실물PC/장시간 현장 자료 미검증. 기존 지질도 오프셋의 사용자 조사 상태 확인은 별도 미해결이다.

## 2026-09-16 조사 주변 5km 지적도 자동 다운로드

- 사용자 Document.pdf 3쪽을 먼저 개발 프롬프트(plans/2026-09-16-cadastral-development-prompt.md)로 가공한 뒤 구현했다. 후속 답변 5km는 조사구역 전체 경계의 5000m 버퍼다.
- CadastralPortal이 공식 VWorld 행정구역 API·로그인·목록·ZIP 요청을 앱 작업자에서 실행한다. 경계에 걸친 여러 시군구를 받고 원본을 보존한다. 개인 계정은 로컬 설정으로 저장하며 소스/문서/로그에 값을 남기지 않는다.
- CadastralImport가 QGIS CRS 변환·정확한 교차로 필지를 추려 공간 인덱스 GPKG(PNU/JIBUN)를 만든다. 전체 필지 형상 유지, 동일 PNU의 다른 도형 보존, 정확한 중복만 제거. 기본 검정 0.2mm·채움 없음, 지번은 1:10000보다 확대할 때 표시. 색/지번 설정·참조 그룹·QGZ 재열기 검증.
- 배경 지도 → 지적도, 옆 메뉴 → VWorld 계정 설정 / 선 색·지번 표시. 비모달 진행/취소, 조사 전환 결과 폐기, 갱신일/원본 내용/범위/CRS 캐시. 조사 폴더 지적도/원본과 지적도/표시에 저장한다.
- 실제 C++ 경로로 구미·칠곡 2개 ZIP을 받아 원본 453,579필지→49,017필지, 표시 파일 17,301,504bytes. 최초 준비 46.451초, Windows 폰트 검사 캐시 11.445초(공식 목록 확인 포함), 전체 렌더 1.904초, 확대 렌더 0.181초·지번 207개. 해당 PC/범위의 관측값이며 무지연 보장 아님.
- Release 전체 빌드·시작 smoke 통과. 전체 CTest 44/46 통과, 기존 heritage_flow/e2e_opaque_suite 2개 실패, topographic_browser 비활성. clangd 3파일 오류0·SDK 선언 탐색, Graft 소스 대조, Archify9/9 및4화면 확인. offscreen 폰트 사각형은 Windows 플랫폼으로 재검사해 실제 한글·숫자 정상 확인.
- 근거 build/qa/cadastral-20260916/REPORT.md와 live-windows-fonts.xml, ctest-final.log. 사용법 docs/user/cadastral-download.md. EXE SHA256 6687F3886D88E21A1FA77C475763667E2382A77F24721D09B92A53A6435E4343.
- 새 포터블/커밋/푸시 없음. 사용자 실행 앱·원본 조사·기존 배포 보존. 다른 실물 PC·모든 지역 실시간 다운로드·장시간 수동 UI 검증은 미실행. 이전 지질도 오프셋의 사용자 조사 상태 확인은 별도 미해결이다.

## 2026-09-15 페이지 표시 유적의 분류별 연번

- 최신 사용자 요청으로 이전 ‘재번호화하지 않는다’ 규칙을 대체한다. 실제 페이지에 배치된 유적만 자료 분류마다 1..N으로 다시 매기고 지도·범례의 번호/색/유적명을 일치시킨다. 같은 유적의 여러 피처는 같은 번호와 범례 1행이다.
- HeritageLayoutNumbers가 전체 후보/원래 override를 별도 보관한다. QGIS PAL의 layer ID/FID/번호 및 종이 교차로 실제 표시 피처만 골라 연번화한다. 이미 배치된 라벨 하단 변 중점을 원본 CRS로 변환해 Center/Bottom으로 고정하므로 번호 폭 변화로 충돌 배치가 뒤바뀌는 것을 피한다. 새 번호에 맞춰 원 크기를 줄이며 충돌 금지·기존 여백을 유지한다. 숨겨진 같은 유적의 형제 FID는 다시 표시하지 않는다.
- 미리보기는 연번 렌더 완료 뒤 범례를 갱신한다. 범위/축척/지도 위치/페이지/DPI/원본 변경에는 전체 후보를 복원한다. 드래그 중 복원과 결과 적용은 release까지 보류한다. PDF는 인쇄 DPI의 실제 배치부터 다시 확인하고 최대 3회 내 번호/범례 일치를 검증한 뒤 QSaveFile로 확정한다.
- 최종 Release 빌드·시작 smoke·clangd 진단0과 실제 SDK 선언 탐색·Graft 소스 대조·Archify9/9 및4화면 통과. 서비스35개와 실제 Studio 표시/숨김·200개 갱신 병합·지도50%종이밖이동/복귀 검사 통과. 두자리11/12→1/2,5186→5187,30도회전,숨겨진형제,PDF일치,원본스타일보존 확인. 전체 CTest43/45통과, 기존heritage_flow/e2e_opaque_suite2개실패·topographic_browser비활성. save_open_window176.17초통과. build/qa/consecutive-numbers-20260915/ctest-final.log 참조.
- 승인된 개인 포터블 후속본: 바탕화면 HGIS-포터블-페이지연번-20260915. EXE SHA256 1B6ECEF15C573A26460054ADA260164BBAB98C209FC5FED1F234937BEB3DA00C. 개발SDK경로없는 Windows 시작244모듈·번들CRS 통과. ZIP/CRC/소스snapshot: build/qa/portable-consecutive-numbers-20260915/delivery-result.json. 미서명·실제다른PC미검증. 실행앱·원본조사·이전배포보존. 커밋/푸시없음.
- 사용법 docs/user/layout-heritage-numbers.md, 근거 build/qa/consecutive-numbers-20260915/REPORT.md. 이전 지질도 오프셋의 현재 조사 상태 확인은 별도 미해결이다.

## 2026-09-15 페이지에 실제 표시된 번호만 범례에 연결

- 원인: 도면과 겹치는 유적 후보를 범례로 만들면서 QGIS 충돌 배치에서 숨긴 번호까지 포함했다. 실제 PAL 결과의 레이어ID·피처ID·번호와 페이지/지도 교차를 확인하여 표시 번호만 남긴다. 같은 레이어/번호는1행이며 분류별 번호와 색·유적명 연결을 유지한다. 제외된 번호를 메우려고 재번호화하지 않는다.
- 미리보기의 시작 상태·revision을 추적해 진행 중 변경/교체된 결과는 버리고 새 렌더를 확인한다. 완료 후 범례만 갱신한다. 지도/종이 이동은120ms 병합, held 입력은최종release까지보류, 범례 이동만으로지도재렌더없음.
- Studio 및 LayoutService 제출/저장조판 PDF는 실제 인쇄 PAL 결과를확인하고필요할때만2차출력해같은집합인지검증한뒤QSaveFile확정한다. 출력제외페이지를제외하고DPI를복원한다. 일반PDF도현재Studio서비스를재사용하며QObject종료때지도owner참조를해제한다.
- 원본자료·스타일보존. 실제밀집12후보중10개표시와범례정확일치,중복/피처ID교체/페이지밖/30도회전/일반PDF/liveowner수명 검사통과. Poppler PDF직접확인. 최종Release전체빌드·smoke·clangd3파일진단0·실제SDK선언탐색·Graft대조·Archify9/9및4화면통과. 전체CTest43/45통과, 기존heritage_flow/e2e_opaque_suite실패2개·topographic_browser비활성, save_open_window171.86초통과. 근거: build/qa/visible-number-legend-20260915/REPORT.md.
- 기존개인포터블요청의후속본: 바탕화면 HGIS-포터블-표시번호범례-20260915. EXE SHA256 1E33D4BDDE98A168C87749495F216D24487ECB306E5EA58DB34F8F155ACBBAD8. 개발SDK경로없는Windows시작·번들CRS통과. ZIP/CRC/소스snapshot결과: build/qa/portable-visible-numbers-20260915/delivery-result.json. 미서명·실제다른PC미검증. 실행앱·원본조사·이전배포보존. 커밋/푸시없음.
- 이전지질도오프셋의현재조사본경로답변대기는별도미해결이다. 이번번호범례수정으로지질도화면문제까지해결됐다고주장하지않는다. 사용법: docs/user/layout-heritage-numbers.md.


## 2026-09-15 참조 그룹 숨김·DEM/혼합 범례·지질 응답 CRS

- 상위 참조 그룹 숨김을 무시하던 LayerOps 표시목록/덧그림 캐시/표시 질의를 isVisible로 수정했다. 명시적인 배경지도 켜기는 부모도 켠다. Studio 레이어 트리와 전체 체크는 조판의 m_project에 연결한다. 부모 숨김→도형/번호/범례 제거→다시켜기200개 번호복원 검사 통과.
- DEM 범례1×1mm초기화를 제거하고 이전1mm손상은55mm로복구한다. 번호+지질 혼합에서는QGIS map filter를 유지하고 최종override후명시갱신한다. 비동기hit test완료와 동기행변경에 맞춰높이재배치한다. PDF직전범례만scratch paint→hit test대기→높이확정하므로 첫PDF도 긴빈테두리가남지않는다.
- Geology prepare에서 응답CRS를무시하던별도결함을수정했다. 명시GeoJSON CRS를fromOgcWmsCrs로검증하고 실제5186변환을수행한다. 정상5186/5187/4326/미선언4행,미상/잘못된위경도2행,5187조사적재위치·범위·원본보존 검사통과. 소스에 임의좌표계 지정으로 맞추지 않는다.
- **사용자화면 지질도오프셋은 아직미확정.** 최신로컬지질GPKG5186/1583개는 저장조사5187중심을변환한위치와실제겹친다(feature406). 개발/포터블QGIS+PROJ결과동일. 저장QGZ와내장workspace에는아직지질layer가없고transformContext비어있다. 현재어긋난상태를다른이름으로저장한경로를사용자에게요청했고답변대기. 이번응답CRS수정이해당화면문제까지해결했다고말하지않는다.
- Release전체빌드/시작smoke/실제DB clangd진단0 및 isVisible선언탐색/Graft대조/Archify9검사·4화면 확인. DEM4행,혼합전환·범위변경·첫PDF,그룹숨김,응답CRS6행검사통과. 전체CTest43/45통과, 기존heritage_flow/e2e_opaque_suite2개실패·topographic_browser비활성, save_open_window235.07초통과. 근거: build/qa/reference-dem-geology-20260915/REPORT.md 및 ctest-final.log. 사용법: docs/user/reference-map-checks.md.
- 앞선개인포터블요청후속본: 바탕화면 HGIS-포터블-참조지도범례-20260915. EXE SHA256 CA24D08301B65667AA95B649B95B5604067D3D94FBE48DD8BF88F539CE654004. 개발SDK없는Windows시작247모듈·번들CRS통과. ZIP/CRC/현재소스snapshot결과: build/qa/portable-reference-dem-20260915/delivery-result.json. 미서명·다른실물PC미검증. 사용자실행앱·조사원본·이전배포보존. 커밋/푸시없음.
## 2026-09-15 조판 조작 최적화·축척 일치·분류색·번호 겹침 방지

- 사용자 요청: 범례 생성/이동/크기조절과 축척변경 지연, 도면 정보와 하단 축척 불일치, 비슷한 분류색, 도면 번호 겹침을 수정한다.
- 조판의 번호/범례/폭 계산은 마우스를 잡은 동안 보류하고 놓은 뒤 최종 상태를 반영한다. release 상태는 즉시 해제하며 대기 콜백은 새 press가 있으면 종료한다. 원본 revision별 복제 레이어를 재사용하고 실제 도면 범위의 category만 전달하며 native 범례 중복 재생성을 제거했다. 제목/글꼴 변경은 번호 트리를 유지하고 범례 이동은 QGIS 캐시를 사용한다.
- 활성 레이아웃의 실제 map scale을 도면 정보·종이 축척·하단 축척에 양방향 연결했다. 일반 지도 범위/축척과 기존 조판 탭 상태를 보존한다. 지표 금황#D4AA00·발굴 진분홍#C2187D으로 구별하며 옛 기본색은 조판 복제본에서만 바꾼다. 임의색·원본 XML·자료는 유지한다.
- 번호는 point_on_surface 주변 후보에 배치하고 색 원 지름을 포함한 여백·충돌 금지·가는 연결선을 적용했다. 덧지도는 도형만 그려 본 지도의 번호 배치와 중복되지 않는다. 밀집12항목 PDF에서10개 표시, 모든 원 쌍 최소여유2.79649mm 검증. 공간이 부족한 번호는 숨기며 범례에는 남는다. 모든 번호가 항상 표시된다는 보장은 없다.
- CMake Release·시작smoke·clangd 실제 compile DB 진단0·Graft 소스 대조·Archify9/9 및4화면 검증. 최종 대상7개 통과(studio-release-settle.xml). 전체 CTest45개 중43개 통과, 기존heritage_flow/e2e_opaque_suite2개 실패·topographic_browser비활성. save_open_window164.79초 통과. 근거: build/qa/layout-interaction-20260915/ctest-delivery.log. 실제 색상/밀집 PDF를 Poppler로 확인했다. 성능 수치는 합성자료 조건이며 다른 PC의 완전무지연을 보장하지 않는다. 상세: build/qa/layout-interaction-20260915/REPORT.md.
- 이전 포터블 요청의 후속 수정본: 바탕화면 HGIS-포터블-조판최적화-20260915. 최종 EXE SHA256 1F3C6B48319017B526F9F4EEA604BBD38FD94EF3D656D7F623CFD0C3B272DA82. SDK 경로 없는 Windows 시작245모듈·번들CRS 통과. ZIP/CRC/소스 스냅샷 결과: build/qa/portable-interaction-20260915/delivery-result.json. 승인된 개인config는 값미기록 hash일치. 미서명·다른 실물PC미검증. 사용자 실행앱·원본조사·기존배포보존. 커밋/푸시없음.
## 2026-09-15 범례 폭에 따른 자동 여러 열 배치

- 사용자 요청: 범례 상자를 가로로 넓히면 긴 한 열을 여러 열로 자동 배치한다. LayoutService::flowSheetLegend가 범례 소유120ms 타이머로 폭 변경만 반영하고, QGIS 열 분할·자동 줄바꿈·내용 높이 측정을 사용한다. 폭·위치·색 원·자료 분류별 번호·유적명을 유지한다. 원본 레이어/범례 노드/번호 캐시 재생성 없음.
- 합성43항목에서 폭70→190→70mm에 따라 열1→3→1, 높이372.149→137.185→372.149mm. 실제 크기변경 타이머, 이동만 할 때 미실행, 대기 중 삭제 안전성, 원본/번호/노드 보존 검사 통과. PDF의 좁고 넓은 범례를 Poppler로 직접 확인했다.
- core24개 통과, 전체Release빌드·시작smoke·clangd두파일 오류0·실제API선언탐색 통과. Graft/실제소스 대조와 Archify9검사·4화면 통과. 전체 CTest43/45, 기존heritage_flow/e2e_opaque_suite실패2개·topographic_browser비활성. save_open_window161.87초 통과. 근거: build/qa/legend-auto-columns-20260915/REPORT.md.
- 앞서 승인된 포터블 후속 수정본: 바탕화면 HGIS-포터블-범례자동열-20260915. 개발SDK경로 없는 Windows독립시작244모듈·번들CRS통과. 승인된개인config원본일치(값미기록). EXE SHA2562365E2714B6F6E23CC8985F0656A7F0D610898E2154C665045B17B1C8583AC9F. ZIP/CRC/소스일치결과: build/qa/portable-legend-columns-20260915/delivery-result.json. 기존배포·실행앱·원본자료보존, 미서명·실제다른PC미검증, 커밋/푸시없음.
## 2026-09-15 레이아웃 진입 지연 수정

- 사용자 보고한 레이아웃 지연을 합성1500분류/도면100항목으로 재현했다. 변경 없는 번호 확인 평균240.793ms, 범례 적용974.782ms가 강제·중복 실행되는 구조였다.
- HeritageLayoutNumbers는 source style/data/repaint/CRS 및 편집·rollback 신호를 lifetime-bound revision으로 추적한다. 변경 없는 확인에서 전체 스타일 XML 직렬화와 feature count 조회를 없앴고, 범례 수동노드에 revision을 기록해 같은 결과를 재생성하지 않는다. 새 트리·실제 편집·범위 변경은 재계산한다.
- Studio는 갱신 요청을80ms timer로 합치고 숨긴동안 보류한다. 같은 결과에서는 범례/덧지도/지도 refresh를 생략한다. 동일 중심·축척 재설정도 생략하여 QGIS의 범위 범례 재생성을 막는다. PDF 직전은 pending layer 반영 후 강제계산1회를 유지한다. 일반 지도/원본 자료는 변경하지 않는다.
- 같은 합성1500분류에서 cache update0.095ms, legend0.006ms로 감소. 실제 Studio200분류 검사: 번호스타일 최초준비455ms, 재표시14ms, 변경없는10요청 계산0회·범례노드유지, 실제색변경1회·숨김변경후표시1회 반영. 이 수치는 사용자 원본 속도나 모든 PC 보장이 아니다.
- core22개 통과/선택PDF1개 생략 후 별도PDF3개 통과, Studio 대상5개 통과. PDF를 Poppler로 렌더해 색원/번호/한글 범례를 확인했다. 전체 CTest43/45, 기존heritage_flow/e2e_opaque_suite실패2개·topographic_browser비활성. save_open_window227.30초 통과. Release전체빌드/시작smoke/clangd core·Studio오류0/Archify9검사·4화면 통과. Graft·실제소스와리뷰근거: build/qa/layout-entry-performance-20260915/.
- 기존 포터블 요청의 후속 수정으로 바탕화면 HGIS-포터블-레이아웃속도개선-20260915에 최신Release를 포함했다. 독립Windows시작250모듈·번들CRS통과, 승인된개인용config원본일치(값미기록). EXE SHA2561704DADA39EFABEE11AD21295D4DD510475C06133FAF22DB01CD07DF52663164. ZIP/CRC/소스스냅샷결과는 build/qa/portable-layout-performance-20260915/delivery-result.json. 기존배포/사용자실행앱/원본조사를조작하지 않았다. 미서명상태·실제다른PC미검증, 커밋/푸시없음.
## 2026-09-15 조판 유적 번호·색 원 범례와 최종 포터블

- 사용자 확정: 지정유산 등 여섯 자료 분류별로 1번부터 시작한다. 조판의 긴 유적명을 레이어 색의 원 안 번호로 바꾸고, 범례에는 같은 색 원·번호와 유적명을 표시한다. 같은 레이어의 같은 category는 같은 번호, 다른 레이어는 별도 번호다.
- HeritageLayoutNumbers가 실제 도면의 회전 범위를 원본 CRS로 변환해 표시 유적만 조회한다. 숨긴 category/범위 밖은 제외하며 single-symbol 자료도 지원한다. 복제 스타일을 조판 map overrides에 적용하므로 일반 지도 라벨·원본 데이터/스타일은 유지한다. 본 지도/덧지도·범위 변경·PDF 직전 갱신을 연결했다. 다운로드·로그인 흐름은 변경하지 않았다.
- Codex/AGENTS/프로젝트 GIS·인트라넷 스킬, Graft 실제 소스 대조, clangd 선언 탐색·core/Studio 검사 오류0, CMake Release 전체 빌드·시작 smoke, Archify 9/9·브라우저4크기 검증 완료. 컴파일 DB 250항목. clangd의 unrelated refactoring action 자체검사를 제외한 ExpandAutoType 제한 검사이며 실제 compiler diagnostics 오류0이다.
- 합성 번호·범례·원본 보존·CRS/회전·단일 심볼·PDF 포함 Qt 검사19개 통과. 실제 PDF를 Poppler로 렌더해 색원/번호/한글 범례 대응과 잘림 없음을 확인했다. 전체 CTest 45개 중43개 통과, 기존 heritage_flow/e2e_opaque_suite 2개 실패, topographic_browser 별도 비활성. save_open_window 215.98초 통과. 상세: build/qa/heritage-numbers-20260915/.
- 바탕화면 새 개인용 폴더 HGIS-포터블-유적번호-20260915에 최신 실행본과 런타임을 포함했다. 사용자 승인한 계정 파일은 config에만 복사했고 원본과 일치한다(값 미기록). 개발 SDK 없는 Windows 별도 실행과 250개 모듈·번들 CRS 검사 통과. 실행본 SHA256 2FD76374192FCF2305275C50987B16B4004DA8E6EAF340050461C5FF8BF11147. ZIP·CRC·소스 스냅샷 결과: build/qa/portable-heritage-numbers-20260915/delivery-result.json.
- 실행본은 미서명이라 SmartScreen 경고가 남을 수 있다. 사용자가 확인한 자세한 정보 → 실행 안내와 정식 서명 도구/문서를 포함한다. 실제 다른 PC 실행은 미검증이다. 이전 배포와 사용자 실행 중 앱/원본 조사를 변경하지 않았다. 커밋·푸시 없음. 사용법: docs/user/layout-heritage-numbers.md.
## 2026-09-15 개발 도구 조합 필수 사용

- 사용자 최신 지시: 개발에 `Codex + AGENTS.md + clangd + Graft + Archify + CMake/CTest`를 꼭 사용한다. AGENTS.md와 docs/developer-tools.md에 제품 코드의 작은 수정까지 모두 사용하는 절차와 실제 근거 기록을 명시했다.
- 기존 Graft 소규모 수정 생략/Archify 구조 설명 한정 규칙을 교체했다. 도구 장애 시 복구를 시도하고 필수 검증의 미완료를 밝힌다. 문서 전용 검증·포터블 별도 요청·커밋 명시 요청 규칙은 유지한다. 이번 변경은 지침만 수정했고 제품 재빌드는 하지 않는다.

## 2026-09-15 조판 장식 이동·축소 및 포터블 SmartScreen 안내

- 네 장식(축척자·축척 글자·좌표계·방위표)을 잠그던 코드와 축척 동기화 때 기본 위치로 돌리던 동작을 수정했다. 항목 이동→선택/끌기, 도면 정보→선택 항목 크기 25~200%를 제공한다. 글자/그림과 축척자 구간 거리가 함께 조절되며 지도 자체 축척은 바꾸지 않는다. 지도 갱신/모양 변경 뒤에도 위치를 보존하고 크기 undo를 지원한다.
- 수정 전 ka_scalebar is locked 실패 재현. 수정 후 네 항목의 실제 마우스 이동(자동 Qt 이벤트), 50%/25% 축소, 거리·글꼴, undo, 지도축척/새로고침/스타일 전환, PDF 생성 통과. 조판 관련 5개 검사와 init/cleanup 합계 7개 통과. QGIS 미리보기·실제 PDF 재렌더 확인. 원본 조사 대신 EPSG5187 합성 빈 지도 사용.
- 전체 Release 빌드·시작 smoke 통과. 전체 CTest 45개 실행 중 43개 통과, 기존 heritage_flow/e2e_opaque_suite 2개 실패, topographic_browser 1개 비활성. 이후 나침반 스타일 위치 보정은 앱/검사 재빌드와 대상 검사/시작 smoke로 재검증했다. 전체-suite 재실행은 하지 않았다.
- 분석 DB 249항목·autogen 갱신. clangd resizeToMinimumWidth 실제 SDK 선언 조회 진단0. 전체 clangd 구문 검사0(--tweaks=ExpandAutoType); 기본 refactoring self-check는 기존 ExtractFunction break/continue 검사 36건 실패로 분리 기록. source compiler 오류와 구분한다.
- 사용자 답변으로 다른 PC의 자세한 정보→실행 버튼을 확인해 SmartScreen 평판 경고로 안내했다. 미서명 EXE이며 인증서 부재로 실제 서명은 미완료. scripts/sign-release.ps1 기본 읽기 전용 검사/명시 서명 경로와 docs/portable-desktop.md 안내를 준비했다. 실제 타 PC 경고 제거를 확인한 것은 아니다.
- 개발 바로가기→scripts/start-ka-hgis.vbs→launch.ps1→현재 Release 연결 확인. 사용자 실행 앱/원본 조사/계정/기존 포터블/ZIP을 조작하지 않았고 새 포터블 생성·커밋·푸시 없음. 사용법 docs/user/layout-decorations.md, 계획 plans/2026-09-15-layout-decoration-editing.md, 근거 build/qa/decoration-20260915/REPORT.md 및 build/qa/decoration-*.

## 2026-09-15 GeoTIFF 포함 개인용 포터블 전달

- 사용자 요청으로 바탕화면 `HGIS-포터블-GeoTIFF-20260915`와 ZIP을 새로 만들었다. 이전 포터블은 보존했다. 현재 GeoTIFF 저장 버튼과 기존 화면/UTF-8 개선을 포함한다.
- 앞선 명시 요청대로 현재 API 키·NGII·국가유산 계정을 config에 포함하고 원본 설정과 일치 확인했다. 값은 출력하지 않았다.
- Release 빌드, 번들 파일, SDK 없는 PATH의 Windows 시작·좌표계 검사 통과(253개 모듈). EXE는 현재 Release DD6A852E…와 일치. ZIP 381,672,599바이트/6,699파일 CRC 통과, SHA256 3635ae9b1f168887098825d78565915b9e619b4516743b28a855cbccca5a9957.
- 소스 ZIP·사용 안내 포함. 실제 다른 PC/SMARTTOPO C 로드는 미검증. 커밋/푸시 없음. 근거: build/qa/portable-geotiff-20260915/REPORT.md 및 delivery-result.json.
## 2026-09-15 현재 지도 GeoTIFF 저장 버튼

- 사용자는 SMARTTOPO C 배경 영상용으로 지도 화면에 보이는 것을 저장하는 버튼만 요청했다. 추가 좌표계·해상도 설정 없이 지도 탭의 내보내기 → GeoTIFF 저장 → 파일 위치 선택으로 끝난다.
- MapGeoTiffExport는 현재 mapSettings의 표시 레이어/스타일/순서/투명도/선택 표시/범위/회전/CRS를 QGIS로 렌더링하여 RGB8 일반 GeoTIFF를 쓴다. 실제 mapToPixel 역변환으로 GeoTIFF 픽셀 모서리의 위치를 기록한다. 임의 좌표계 변경이나 단순 스크린샷에 EPSG만 붙이는 방식이 아니다.
- 새 GeoTIFF 검사 12개, 지도 탭 버튼 검사, 전체 Release 빌드·시작 smoke 통과. 전체 CTest는 45개 중 43개 통과, 기존 heritage_flow/e2e_opaque_suite 실패 2개, 별도 1개 비활성이다. 좌표/픽셀 검사는 EPSG5187/5179/5186/4326, 회전, DPI, 투명도, 선택 표시, 취소/기존 파일/원본 보존을 포함한다.
- 컴파일 DB 249항목 갱신. 신규 core clangd 검사 종료0, 실제 MainWindow 호출→MapGeoTiffExport::write 선언 탐색 및 진단 오류0. MainWindow 전체 토큰 검사는 6분 초과로 해당 QA 프로세스만 종료했으며, MSVC 빌드와 별도 LSP 진단/탐색은 통과했다.
- 개발 실행본 SHA256 DD6A852EC2DB9AE6241F7E5BD9FCFB71904057326D6B75584731DFB9A5BBF887. 바탕화면 고고학 전용 GIS → start-ka-hgis.vbs → launch.ps1 → 현재 Release 연결 확인. 기존 포터블/ZIP은 이번 작업에서 갱신하지 않았다. 실제 SMARTTOPO C 로드 미검증, 커밋/푸시 없음. 근거: build/qa/map-geotiff-20260915/, 사용법: docs/user/map-geotiff.md.

## 2026-09-15 개인용 바탕화면 포터블·화면 크기·UTF-8

- 사용자 요청으로 바탕화면 `HGIS-포터블-20260915`를 생성했다. 현재 앱의 VWorld 키와 수치지형도·국가유산 인트라넷 계정을 명시적으로 포함하도록 요청받았다. 값은 로그/소스에 넣지 않고 개인용 config에만 복사했다. 개발용 바로가기는 현재 Release 연결을 유지한다.
- 메인 창 표시/모니터 이동과 주변유적 창 확장을 작업영역에 맞췄다. 단면도 1100px 고정 최소폭을 제거하고 속성 폼의 줄바꿈·스크롤바 공간을 확보했다. 5개 화면(1024/1366/1920, 150%/200% 상당 논리공간), 메인/다중 모니터 9개 검사, 작은 화면 캡처 검토 통과.
- 새 주변유적 적재는 원본 SHP의 CP949/UTF-8을 해석하여 UTF-8 GPKG 작업 사본을 사용한다. 긴 한글 값·필드명을 보존하며 원본 SHP/CPG는 변경하지 않는다. 기존 열린/저장된 SHP의 자동 마이그레이션은 하지 않는다. importer 14개 통과/3개 표본 선택 검사 생략, clangd 오류 0.
- 전체 Release 빌드 통과. 바탕화면 번들의 native Windows 시작에서 SDK 경로 없이 245개 로드 모듈을 관찰했고 QGIS/Qt/GDAL/PROJ/WMS는 번들에서 로드했다. 번들 5186/5187/3857 좌표계 및 WebEngine 한글 DOM 검사 통과. 실제 다른 PC와 모든 물리 모니터 검증을 뜻하지 않는다.
- 전체 CTest는 44개 중 41개 통과/3개 실패, 별도 1개 비활성이다. topographic_scope는 단독 실행 18개 및 CTest 재검사 통과했다. 남은 heritage_flow의 JS 기대값 8개와 e2e_opaque_suite의 WMTS 예시 키 검사 1개는 이전과 같은 실패다. 전체 통과로 표현하지 않는다. 최종 ZIP·해시 기록: `build/qa/desktop-portable-20260915/delivery-result.json`; 작업 계획/로그는 같은 폴더, UTF-8 근거는 `build/qa/heritage-utf8-20260915/`. 이번 변경은 아직 커밋/푸시하지 않았다.

## 2026-09-15 문화재인트라넷 SHP 한글 깨짐 조사 중

- 사용자가 자동으로 받은 SHP에서 한글이 다른 문자로 깨진다고 명확히 확인했다. UI 글꼴 크기/두께 문제는 아니다.
- 현재 로컬 캐시 24개 SHP의 DBF 문자 필드와 CP949 .cpg를 읽기 전용 검사했다. 설치 SDK의 QGIS에서 CP949로 읽은 속성·uniqueValues·필드 이름은 정상, 대체 문자 0. UTF-8로 강제 읽으면 대체 문자 51,959개로 오독을 재현한다. 이 강제 설정이 실제 사용자 화면의 원인이라는 증거는 아직 없다.
- C++ HeritageImport 경로에 CP949 합성 SHP 회귀 검사 cp949NamesSurviveAutomaticImport를 추가하여 원본→자동 적재→범례 이름 정상 확인. 테스트 빌드/단독 검사 통과. 제품 코드/원본 데이터/폰트는 아직 변경하지 않았다.
- 현재 GUI PID28828은 대구 조사에 미저장 표시(*)와 응답 없음 상태다. 읽기 전용 화면에는 유산 레이어 제목이 정상이며 지도 라벨은 표시되지 않아 깨진 글자 자체를 확인하지 못했다. 사용자 앱을 종료하거나 미저장 작업을 폐기하지 않았다.
- 깨진 유적명 한 개와 가능한 원래 표기를 사용자에게 비동기로 요청했다. 증상이 재현되지 않은 상태에서 추정 인코딩/글꼴 변경을 하지 않는다. 근거/현재 계획: build/qa/heritage-text-20260915/.

## 2026-09-15 자동 지역 판정·지적도 복구, 글꼴 추가 조사 중

- 현재 PC의 이전 VWorld 키는 공식 주소 API와 WMS 양쪽에서 INVALID_KEY였다. 사용자가 새 키를 앱에 입력했으며 새 개인 키로 두 API가 정상이다. 키 값은 출력하거나 저장소에 넣지 않았다.
- LayerOps는 저장된 GDAL XML 경로에서 키를 갱신하지 못했다. VWorld 지적 XML만 읽어 새 키와 0.5m 정방형 픽셀의 inline XML로 provider를 다시 연결한다. 원본 XML/조사 파일을 덮어쓰지 않는다. 신규 GDAL 지적도도 같은 해상도를 사용한다. 기존 79.3457m/pixel에서는 유효 키로도 빈 영상이었고 0.5m에서는 필지·지번이 나왔다.
- HeritageRegionResolver의 기존 조사구역 내부 대표점 → 원본 레이어 CRS → 4326 → 시도/시군 경로를 유지한다. INVALID_KEY/INCORRECT_KEY를 일반 판정 실패로 숨기지 않고 정확한 키 설정 메뉴를 안내한다.
- 안동시_복사본_복사본.gpkg를 읽기 전용으로 확인: survey_area EPSG:5187, 2개 도형, 마지막 도형 주소 응답 경상북도 안동시. 보정한 GDAL 설정을 같은 SDK/QGIS로 5187에 재투영한 지적도 렌더 오류 0, 지번/경계 이미지 확인. 실행 중 미저장 도형 상태를 검사했다는 뜻은 아니다.
- Release 빌드·heritage_region/heritage_setup_dialogs·키갱신/원본보존/표시상태/해상도/QGS 저장재개 회귀·시작 smoke 통과. 기존 실행본은 종료하지 않고 build/Release/ka-hgis.before-region-cadastral.exe로 이름을 보존해 빌드했고, 기존 앱이 사라진 뒤 정상 바로가기 경로로 새 Release 홈 화면을 확인했다. 현재 EXE SHA256 E338A9DDAEF0C82D1A787F084552A40BDABB6F6ED9C8813C8F9AC3A5CAF469E9.
- 근거: build/qa/region-cadastral-20260915/. Graft 사용처, clangd 정의/진단, 독립 소스 검토와 수정 전 Archify 진단 9/9/브라우저 확인. 실사이트 주변유적 로그인·여섯 자료 다운로드 성공은 아직 별도 미검증이다.
- 사용자가 이어서 '폰트가 이상하게 나온다. 수정해야한다' 요청. 새 앱 홈 화면 캡처 확인, UI 글꼴/지도 글자 중 어디인지 질문 대기. 현재 기본 UI는 Malgun Gothic 13px이며 아직 글꼴 관련 코드는 변경하지 않았다.

## 2026-09-15 주변유적 받기 시작 단계 복구

- 사용자 증상은 로그인·시군 찾기에서 멈춤. 이 PC에 국가유산 인트라넷 계정 파일이 없고, 기존 안내가 가리키는 계정 메뉴도 없었다. 실행 로그에 VWorld INVALID_KEY가 있으며 자동 지역 판정 실패 슬롯은 안내만 보여 수동 선택으로 진행할 수 없었다.
- MainWindow에서 판정 실패 시 남은 요청을 취소하고 KaHeritageRegionDialog의 시도/시군 선택으로 이어진다. 기존 KoreaRegionCatalog 목록을 사용하고 전국·미선택은 허용하지 않는다. 정상 판정도 같은 창에서 확인·수정한다.
- KaHeritageAccountDialog와 더보기 메뉴를 추가했다. 계정이 없으면 지역 선택 후 입력받고, 기존 HeritageIntranetSettings::saveCredentials가 성공하여 완전한 계정이 있을 때만 기존 Browser 시작으로 이어진다. 취소·저장 실패·빈 계정은 시작하지 않는다. 사용자 조사 원본과 실제 계정 값은 수정하지 않았다.
- Graft 실제 MCP로 사용처/테스트/최신성을 확인했고, clangd에서 새 UI 진단 오류 0과 기존 계정 저장 함수 연결을 확인했다. 컴파일 DB 246항목으로 갱신. Archify 검토자료와 로그는 build/qa/heritage-startup-20260915/에 보존한다.
- Release 앱 빌드, 관련 CTest 6개(새 dialog/region/form/retry/import/style), 최신 Browser 재컴파일 후 retry 재검사, 시작 smoke 통과. 새 앱 PID30360에서 홈 시작과 실제 계정 메뉴/입력창을 확인하고 사용자 입력을 위해 열어 두었다. 실사이트 로그인·여섯 자료 다운로드 성공은 계정 부재로 미검증이며 기존 전체-suite 실패 해결로 간주하지 않는다.

## 2026-09-14 제작자 표시

- 사용자의 요청으로 시작 스플래시와 더보기 → 정보에 `만든이: youngin kwon`을 추가했다. 기관명과 라이선스 고지는 유지했다. 변경은 `KaApplication.cpp`, `MainWindow.cpp`의 표시 문자열뿐이다.
- 현재 Release 앱 빌드와 `--smoke-quit` 통과. 로그는 `build/qa/author-credit/`. 일반 실행도 창 표시까지 확인했으나 정보 창 UI 검증 중 앱 창/프로세스가 사라져 이름의 실제 화면 캡처는 완료하지 못했다. 종료 원인은 확인하지 않았으며 재실행·데이터 조작하지 않았다.

## 2026-09-14 검증된 개발 도구 연결

- 사용자의 추가 설정 요청에 따라 프로젝트 로컬 도구를 실제 설치·연결했다. 현재 사용법과 고정 버전은 `docs/developer-tools.md`, 역할 분담은 `AGENTS.md`의 Verified developer tools를 따른다.
- `.codex/config.toml`의 `hgis_graft` 서버는 `scripts/graft-mcp.mjs`로 검색·파일 구조·최신성 네 가지 도구만 제공한다. Codex app-server의 실제 config/read에서 프로젝트 설정 병합을 확인했고, 같은 실행 명령의 stdio 초기화·목록·네 가지 실제 조회·제외된 호출 추적 거부를 확인했다. 다음 턴에서 도구가 보이지 않으면 앱을 다시 열어 설정을 로드한다.
- Graft는 `build/tooling/graft-source`의 고정 f9e65396e638e517aecae0d731017f53084d70ed 소스를 사용한다. `src/`, `tests/`의 구조 인덱스는 `build/tooling/graft-index`에 쓰며 내용 해시로 최신성을 검사한다. 같은 크기·수정 시간을 보존한 변경도 회귀 검사에 포함했다. Windows LSP 탐지 보정 실험은 표본 6/10 호출만 찾아 호출 추적은 제공하지 않는다.
- 직접 clangd 정의 조회 표본은 10/10 정확했다. `scripts/clangd-definition.py`로 현재 코드 위치의 선언/정의를 조회한다. helper의 실제 조회 3건 및 실패·시간제한 7건을 확인했다. 전체 영향 범위 조회라는 의미는 아니다.
- Archify 고정 a07fa1d5b2a10cbea110c5a2be2817397a301cdc를 `.agents/skills/archify`에 로컬 설치했다. `scripts/archify.ps1`에서 doctor, 구조도 validate/deliver 9/9, Chrome visual-check를 확인했다. 고정 영어 Viewer UI 및 Windows preview 종료 한계는 `docs/archify-setup.md`에 남겼다.
- 이번 설정의 상세 검증은 `build/qa/dev-tools-setup-20260914/REPORT.md`. 제품 C++와 테스트는 변경하지 않았다. 아래 전체 CTest의 기존 실패는 해결된 것으로 간주하지 않는다.

## 2026-09-14 현재 PC 개발 경로 정렬 및 도구 검증

- 현재 작업 저장소는 `A:/qgis`, GIS SDK는 `A:/OSGeo4W`다. 아래 과거 `D:/hgis` 복구·전달 경로는 이 PC의 개발 경로가 아니다.
- 앱 빌드는 VS2022 x64 `build/Release/ka-hgis.exe`, 분석용 Ninja 구성은 `build-clangd`다. `scripts/dev-env.ps1`과 CMake preset은 현재 SDK를 사용한다. `scripts/gen-compile-commands.ps1`이 VS/Windows SDK include를 포함한 실제 컴파일 DB를 생성하며 `.clangd`는 `build/compile_commands.json`을 읽는다.
- Release 전체 빌드, 일반 PowerShell의 ExportService clangd 구문 분석, 시작 스모크가 통과했다. 바탕화면 `고고학 전용 GIS` 바로가기는 현재 저장소 `scripts/start-ka-hgis.vbs` → `launch.ps1` → Release로 연결했고 이전 바로가기는 QA 폴더에 백업했다. 자동 포터블 생성은 제거했다.
- 전체 CTest는 43개 실행 중 39개 통과/4개 실패, 별도 1개 비활성이다. 실패 4개 중 `heritage_download_retry`는 단독 7/7 통과했고 `save_open_window`는 순차 재검사 239.87초에 통과했다(제한 240초에 근접). 반복 재현 실패는 `heritage_flow`의 JS 기대값 8개와 `e2e_opaque_suite`의 WMTS 예시 키 교체 검사다. 상세 결과는 `build/qa/tooling-validation-20260914/path-alignment.md`를 따른다. 전체 테스트 통과로 표현하지 않는다.
- 최초 검증 당시 Graft 0.18.0은 표본 직접 호출 10개 중 1개만 찾았고 Windows clangd 탐색도 실패했다. Archify는 표본 구조도 검증/브라우저 표시를 통과했으나 Windows preview 종료 테스트 실패가 남았다. 이후 사용자의 추가 설정 요청으로 위의 제한된 프로젝트 로컬 연결을 적용했다.
- 상세 검증/재현 로그/구조도: `build/qa/tooling-validation-20260914/REPORT.md`. 제품 C++/테스트 소스, 사용자 조사 원본은 변경하지 않았고 커밋·포터블 배포하지 않았다.

## 2026-09-13 주변유적 파일 응답·재시도 복구

- 이번 변경은 KaHeritageBrowser.cpp/h와 로컬 회귀 테스트 소스에 한정한다. 지형도·공용 압축·번호·범례 코드는 이번 작업에서 수정하지 않았다. 이전 작업의 미커밋 변경은 보존했다.
- 손상/빈 파일 또는 파일 대신 웹페이지가 돌아온 경우, 진행 중 전송이 끝난 뒤 기존 재시도 간격으로 공식 화면을 다시 연다. 현재 자료 번호와 지역을 유지해 기존 로그인·서약·자료 선택·지역 검색·전체다운로드 경로를 다시 수행한다. 별도 HTTP 다운로드는 추가하지 않는다.
- 웹 화면 오류가 진행 중인 파일 수신을 취소하지 않게 했다. 실제 파일 완료 후에는 웹 화면 준비 상태와 관계없이 기존 ZIP/SHP 검사·동기 적재를 진행하고, 성공해야 다음 자료로 넘어간다. 이전 시도의 늦은 콜백과 닫힌 페이지 응답은 새 작업에 반영하지 않는다.
- 실제 파일이 늦게 도착하면 웹페이지 응답만으로 예약했던 재시도는 해제한다. 파일 검사 실패에 따른 재시도와 구분한다.
- 변경 소스/빌드는 D:/hgis/build/recovery/github-main-publish-20260913/. 전달 경로는 D:/hgis/build/Release/ka-hgis.exe. 빌드 로그는 D:/hgis/build/qa/heritage-response-recovery-build.txt, 전달 명세는 heritage-response-recovery-delivery.json.
- 사용자 지시대로 테스트·실서버 다운로드·사용자 조사 조작은 실행하지 않는다. 로컬 테스트 소스는 외부 요청을 차단하도록 갱신했으며 이번 버전의 실제 다운로드 성공은 아직 사용자 확인 전이다. 성공 기준 PID15408은 별도 보존한다.
## 2026-09-13 ZIP 이상 시 같은 자료 자동 다시 받기

- 사용자 요청: 받은 ZIP이 정상이 아니면 같은 자료를 다시 받는다. 빈 파일, 수신/저장 크기 불일치, ZIP 열기·압축 해제 실패와 ZIP 안 SHP 구성 누락은 다음 자료로 넘어가지 않고 공식 전체다운로드를 다시 실행한다. 기존 ZIP 검사/동기 지도 적재가 성공해야만 다음 자료로 진행한다.
- 정상 파일의 기존 약 4초 수신 정리 대기는 유지한다. 실패 시만 5/10/20/30초(최대 30초) 뒤 재시도하며 진행 중 전송이 끝나기 전에 중복 요청하지 않는다. 사용자는 중지할 수 있다. 저장장치·권한·지도 provider 오류는 무한 다운로드로 숨기지 않는다. 원본은 보존한다.
- 변경 소스와 빌드: D:/hgis/build/recovery/github-main-publish-20260913/. 작업 도중 원래 D:/hgis가 과거 main(72692e1)으로 변경되어 해당 체크아웃을 덮어쓰지 않고 17:57 소스(a478965) 보존 작업트리에서 수정했다. 기본 실행 경로 build/Release에는 이 수정 빌드를 전달한다. 원래 체크아웃으로 재빌드하면 옛 앱이 되므로 현재 소스 경로를 확인한다.
- Release 빌드 성공. 중지 지시 전 로컬 WebEngine 손상/빈 ZIP→정상 ZIP 재다운로드와 실제 18:12 불완전 ZIP 거부 검사는 통과했다. 압축 경로 테스트 중 GDAL이 경로를 숨기는 4개 항목의 분류 기대를 보정했다. 사용자가 '테스트는 내가한다'고 지시하여 추가 테스트/실서버 다운로드/사용자 조사 조작은 하지 않는다. 새 실사용 성공으로 기록하지 않는다.
- 번호·범례·검색 Flow는 변경하지 않았다. 사용자 확정 PID15408 백업은 보존한다. 이번 자동 재시도는 최신 명시 요청이며 아래 과거 복원 당시의 '재시도 제거' 기록보다 우선한다.

## 2026-09-13 최종 복원 기준: 사용자 지정 17:57 시점

- 사용자가 제주시 정상/불완전 ZIP 시간 비교를 보고한 오후 5:57 시점을 최종 복원 대상으로 지정했다. 분석 기록만 추가한 시점이며, 현재 Browser cpp/h와 Flow/Import cpp가 당시 백업과 모두 일치한다.
- Release 재빌드 결과를 정상 실행 스크립트로 열었다(PID23148). 30초 추가 대기는 제거된 상태다. 실사용 다운로드 확인은 사용자가 한다.
- GitHub main 등록 대상과 소스 해시는 docs/recovery/2026-09-13-1757.md를 따른다. 기존 조사 데이터·다운로드 원본·설정·dist 포터블은 등록 대상이 아니다.

## 2026-09-13 18:07 사용자 요청: 방금 수정 전으로 복원

- Browser cpp/h를 build/recovery/before-heritage-long-wait-20260913-175939/로 복원했다. 30초 대기·팝업 닫기 지연·전체 건수 표시 변경을 함께 되돌렸다. 5초로 줄이는 제안은 구현하지 않았다. 두 파일의 SHA256은 PID35732 소스 기준과 같으며 수정 시간을 갱신했다.
- 복원 전 파일과 exe는 build/recovery/before-revert-long-wait-20260913-180703/에 보존했다. Flow/Import/범례 및 다른 미커밋 변경은 건드리지 않았다. 실제 다운로드 완료·파일 확인·기존 검사·동기 지도 적재 후 다음 자료 진행 계약은 유지한다.
- Browser/MainWindow/MOC 재컴파일 및 Release 빌드 성공: build/qa/heritage-revert-long-wait-build.txt. 정상 실행 스크립트로 새 앱 PID36196을 열었다. 실사용 다운로드 확인은 사용자가 한다.

## 2026-09-13 18:01 사용자 요청: 다운로드 종료 뒤 30초 대기 및 전체 건수 표시

- PID35732 복구 상태에서 사용자가 “시간을 길게 잡아라, 중간에 잘리게 하지 말고”라고 요청했다. Browser cpp/h만 바꿔 마지막 DownloadCompleted 뒤 실제 경과 시간으로 30초 기다린다(기존 약 4초). 추가 전송이 시작되면 타이머를 무효화하고 마지막 완료 뒤 다시 센다. 전송 중에는 기존처럼 시간 제한으로 중단하지 않으며 중지 버튼은 유지한다.
- 파일 존재/0바이트 판단도 30초 뒤에 한다. 파일 수신 중 또는 확인 대기 중에는 사이트의 windowCloseRequested를 보류하고, 다운로드 팝업은 기존 ZIP/SHP 검사와 동기 지도 적재 성공 후 다음 자료로 갈 때 닫는다. 불완전 ZIP을 성공 처리하지 않는다. 종료된 불완전 응답이 기다림만으로 복구된다고 주장하지 않는다.
- 화면의 24줄·5쪽은 document 전체 행/보이는 페이지 버튼을 센 표시였다. 전체다운로드는 그 값으로 제한되지 않는다. 진행 문구를 사이트 검색 결과의 실제 전체 건수로 변경했다. 공식 전체다운로드 1회, Flow/Import/범례/번호는 변경하지 않았다.
- 복구 전 백업: build/recovery/before-heritage-long-wait-20260913-175939/. Release 빌드 성공: build/qa/heritage-long-wait-build.txt. 타이머/취소/팝업 수명 독립 읽기 검토에서 확정 결함 없음. 실서버 자동 시험은 하지 않았다. 사용자 앱은 강제 종료하지 않고 정상 실행 스크립트로 새 빌드를 열었다.
- 제주시 17:52 원본 조사 결과는 build/qa/heritage-jeju-1752-download-check.md에 기록했다. 정상 파일의 처음 16항목과 내용이 같지만 보호구역 SHP 2종의 8항목 및 ZIP 중앙 디렉터리/종료 레코드가 빠진 파일이다. 파일 불완전은 확인했으나 응답 조기 종료 원인은 아직 확정하지 않았다.

## 2026-09-13 17:50 사용자 지정 PID35732 다운로드 소스 복구

- 사용자가 PID35732의 공식 전체다운로드 버튼 변경 안내문을 지정하고 복구를 요청했다. build/recovery/heritage-user-confirmed-all-download-20260913/의 KaHeritageBrowser.cpp/h, HeritageIntranetFlow.cpp, HeritageImport.cpp로 복구했으며 네 파일의 SHA256 일치를 확인했다. 당시 exe 자체가 아닌 보존 소스 재빌드다.
- 이후 추가한 자동 재시도, 캐시 방지 헤더, ZIP 사전 검사와 진행 중이던 폼 재생성/0건 재검색 변경을 제거했다. 해당 새 검색 결과 테스트만 함께 제거했다. 공식 버튼 1회, 실제 DownloadCompleted, 저장 파일 확인, 기존 ZIP/SHP 검사와 동기 지도 적재 뒤 다음 종류 진행은 당시 코드대로 유지한다. 기존 검색 0건 처리도 그대로다.
- 복구 전 소스·실행 파일: build/recovery/before-pid35732-restore-20260913-174958/. 확정 성공 백업은 덮어쓰지 않았다. 다른 미커밋 변경, 사용자 조사·계정·다운로드 원본은 보존했다.
- 소스/헤더 수정 시간을 갱신하고 Release 재컴파일 성공. 로그: build/qa/heritage-pid35732-restore-build.txt. 정상 scripts/run-ka-hgis.ps1로 새 앱 PID25736을 열고 창 핸들을 확인했다. 실제 다운로드 시험은 사용자에게 맡기며 이번 복구의 수신 성공을 아직 주장하지 않는다. 문화재인트라넷 스킬에도 현 복구 기준과 제거한 변경을 기록했다.

## 2026-09-13 주변유적 불완전 다운로드 자동 다시 받기

- 사용자 최신 정정: 대상은 수치지형도가 아니라 주변유적. 빈 파일/불완전 응답에서 오류로 끝내지 말고 다시 받도록 요청했다.
- 구미 17:00 성공 ZIP은 55,059바이트/24항목, 17:12 실패 ZIP은 18,388바이트/앞 16항목만 있고 중앙 디렉터리가 없다. 경산 17:13 파일은 0바이트. 구미 성공/실패 요청은 CSRF 외 14개 매개변수와 인코딩이 동일했다. 완료 신호 이후 실제 ZIP 검사에서 중단된 것이며 조기 다음 단계 진행으로 단정하지 않는다.
- KaHeritageBrowser.cpp/h만 수정: DownloadCompleted 뒤 수신/저장 크기 및 GDAL ZIP 디렉터리 확인. 최초 ZIP 검사부터 CP949를 적용하고 스레드 로컬 설정을 원복해 한국어 파일명 캐시 손상을 방지한다. 빈 파일/불완전 ZIP/일시적 전송 중단은 현재 종류에서 5/10/20/40/60초 간격(이후 60초)으로 공식 전체다운로드 버튼을 다시 실행한다. 진행 중 요청이 끝나야 재시도하며 중지 가능하다. 과거 불완전 원본은 보존하고 재시도는 새 UUID 폴더에 받는다. 다운로드 엔드포인트에만 캐시 재사용 방지 요청 헤더를 넣는다. 계정/권한/저장 실패와 지도 적재 실패를 성공 처리하지 않는다.
- 성공 파일은 기존 ZIP/SHP 검사·동기 지도 적재 성공 후 다음 자료로 진행한다. Flow/Import/MainWindow/범례/수치지형도 소스는 PID15408 기준과 동일하다. 성공 백업은 덮어쓰지 않았다. 변경 전 백업: build/recovery/before-heritage-retry-20260913-172410/.
- Release 빌드 통과: build/qa/heritage-incomplete-retry-build.txt. 실서버 다운로드와 사용자 조사 조작은 실행하지 않았다. 실제 재시도 성공 여부는 사용자 확인 전이다.

## 2026-09-13 최신 사용자 확인: PID 15408 빠른 다운로드 성공

- 사용자가 PID 15408 실행 뒤 “지금 파일이 가장깔끔하게 빠르게 받아지는 파일이다”라고 확인했다. 이 빌드를 최신 성공·복원 기준으로 삼는다. 사용자 실사용 확인이며 별도 자동 다운로드나 시간 측정은 수행하지 않았다.
- 확정 복구본: D:/hgis/build/recovery/heritage-user-confirmed-pid15408-20260913/. 실제 exe, 소스·빌드 관련 183개 파일, 빌드 로그 및 SHA256 manifest.json을 보존했다. exe 해시 DECB7FE6A7393667AE51475AA2C4EE5E4F3628E63015F014A8E995EC794B0175. 계정·프로필·조사·다운로드 SHP·전체 OSGeo4W 런타임은 포함하지 않았다.
- 문화재인트라넷 스킬의 SKILL.md와 references/success-state.md를 갱신했다. PID 34708은 관련 소스 복원의 참고 시점이며, 실제 보존·사용자 확인한 실행은 PID 15408이다. 과거 네 파일 백업이나 실패 기록을 최신 확정본보다 우선하지 않는다.
- 공식 전체다운로드 1회 → DownloadCompleted → 저장 파일/ZIP/SHP 확인 → 동기 지도 적재 성공 → 다음 종류 계약을 유지한다. 번호/자동 열/그룹 라벨 및 후속 다운로드 진단 변경은 제거 상태다. 실사용 확인은 사용자에게 맡긴다.
- 이번 작업은 스킬·기록 갱신과 복구본 보존만 수행했다. 앱 코드/실행 파일 변경, 빌드, 앱 재시작, 자동 실서버 확인, 커밋·푸시·포터블은 하지 않았다. 아래 기록은 과거 이력이다.
## 2026-09-13 16:56 사용자 지정 PID 34708 관련 소스 복원

- 사용자가 PID 34708 기준 복원을 요청했다. 당시 exe 자체가 없어 동일 바이너리 복원은 아니며, 남아 있는 관련 소스 백업으로 재빌드했다.
- KaHeritageBrowser.cpp는 heritage-user-confirmed-all-download-20260913, MainWindow.cpp는 before-heritage-layout-deferral-20260913, MainWindow.h는 before-heritage-import-error-detail-20260913 백업으로 복원했다. 제거한 차이는 16:35 수신/저장량 대조와 오류 상세 전달이다. 기존 DownloadCompleted/파일 존재·크기/ZIP·SHP 검사/동기 지도 적재 후 다음 종류 계약은 유지한다.
- Browser.h/Flow/Import/DrawingStudio/HeritageSiteLegend/LayoutService는 번호·범례 작업 전 관련 백업과 일치하여 다시 덮어쓰지 않았다. 기타 미커밋 작업, 계정, 원본 조사, 내려받은 자료는 보존했다.
- 변경 전 소스 3개와 exe: build/recovery/before-pid34708-source-restore-20260913-165548/. manifest.json에 원본·복원 해시와 경로가 있다. 복원 파일은 현재 시각으로 기록했다.
- Release 빌드 통과: build/qa/heritage-pid34708-source-restore-build.txt. 정상 run-ka-hgis.ps1로 앱을 실행했다. 사용자 최신 지시로 자동 다운로드·실서버 비교·실사용 테스트는 실행하지 않는다. 다운로드 복구 성공은 사용자 확인 전이다.
## 2026-09-13 16:35 수신/저장 대조 증거와 검사 보강

- 사용자 확인 성공 실행의 원본 여섯 ZIP을 다시 읽어 CRC 정상 및 총 13 SHP(6/1/1/1/2/2)를 확인했다. 자료는 AppData 원위치에 유지했다.
- 원래 성공 코드의 별도 진단 실행: 임시 브라우저 프로필에서 강릉시 지정유산 공식 버튼 1회 실행. 요청 초기 total=71289, 완료 state=2/finished=true/reason=0/received=71289/total=71289/saved=71289. 1초 뒤 동일. 실제 ZIP은 Python zipfile.is_zipfile=false. 기록: AppData ka-hgis/ka-hgis/주변유적-QA/transfer-probe-20260913-163309/transfer.jsonl. 기존 사용자 프로필/계정을 수정하지 않았다.
- 이 시도는 다운로드 파일 본문 자체가 잘못됐다는 증거다. 임시 프로필에서도 재현됐으므로 기존 프로필만의 문제라고 확정할 수 없다. 앱이 다음 자료를 먼저 실행해서 잘린 것으로 판단할 근거도 없다. 서버 내부 원인/중간 전송 경로 원인은 미확정이다.
- KaHeritageBrowser.cpp에서 완료 시 예상/수신/저장 바이트를 기록하고 크기 불일치를 차단한다. 수신 0바이트와 수신 후 저장 실패를 구분해 표시한다. MainWindow.cpp/h는 실제 압축/적재 오류를 진행 창과 기록에 전달한다. 성공 조건/공식 버튼 1회/다음 자료 순서는 유지한다. 재시도/번호/범례 기능은 추가하지 않았다.
- 변경 전 백업 build/recovery/before-transfer-byte-validation-20260913-163532/. 빌드 기록 build/qa/heritage-transfer-byte-validation-build.txt. 별도 진단 코드는 build/qa/heritage-transfer-probe/에만 있다. 앱 의존성/루트 CMake 설정 변경 없음. 변경의 범위는 검사/진단 보강이며 잘못된 ZIP 응답의 복구 완료라고 보고하지 않는다.

## 2026-09-13 완료 조건 설명 정정

- 파일 완료/검사/적재 후 다음 종류로 진행하는 조건은 원래 성공 코드와 문화재인트라넷 스킬에 있었다. 이를 새 요구나 미구현 조건처럼 설명한 것은 잘못이다.
- 임시로 추가한 confirmDatasetImported 확인 신호는 0바이트 응답의 원인을 해결하지 못하므로 즉시 제거했다. 수정 전 세 파일 백업 before-explicit-import-confirmation-20260913-162708과 내용 일치, 원래 완료 검사 유지. 재시도 로직은 추가하지 않았다.
- 0바이트 응답 원인은 아직 미확정이며, 완료 확인을 건너뛰어서 발생한 것으로 단정하지 않는다. 원래 슬롯은 동기 importHeritageDataset 실패 시 rejectDataset으로 중단한다.

## 2026-09-13 16:21 성공 기준 소스 전체 재컴파일

- 전체 재빌드 통과. 정상 실행 스크립트로 앱을 열었다. 자동 실서버 다운로드/테스트는 실행하지 않았으며 실사용 성공은 사용자 확인 전이다.

- 사용자 기준은 여섯 종류를 모두 받아 레이어에 올린 실제 확인 상태다. 번호/범례 변경은 제거한 상태를 유지하며 새 기능·재시도 로직을 추가하지 않는다.
- 다운로드 네 파일 및 번호 작업 전 DrawingStudio/HeritageSiteLegend/LayoutService 백업과 현재 내용 일치를 재확인했다. MainWindow/헤더/메뉴/LayerOps/스타일 비교에서도 번호·범례 후속 변경 제거를 확인했다. 배경 레이어를 아래로 두는 기존 성공 작업은 유지한다.
- 이전 증분 객체 혼용 가능성을 배제하기 위해 cmake --build --preset release --target ka-hgis --clean-first로 전체 재컴파일한다. 기존 빌드와 소스 백업은 build/recovery/before-full-recompile-20260913-162112/ 및 manifest.json에 있다. 이것은 복구 작업 전 보관본이며 사용자 성공 확인 빌드로 부르지 않는다.
- 빌드 기록 build/qa/heritage-success-full-recompile.txt. 소스 일치·재빌드와 실제 다운로드 성공은 별개다. 계정/브라우저 프로필/사용자 원본 자료는 변경하지 않는다.

## 2026-09-13 16:06 재실패: 완전 복원으로 보고하지 말 것

- 사용자가 16:02 소스 복원 빌드에서도 실패를 확인했다. 강릉시 지정유산_260913160642260.zip은 실제 0바이트이며, 앞선 160045680.zip도 0바이트다. 복원이 다운로드 성공을 회복했다고 보고하면 안 된다.
- 현재 Browser cpp/h, Flow, Import는 사용자 여섯 종류 성공 백업과 일치한다. 번호/범례 자동 열/갱신 보류/오류 상세 표시 후속 변경은 제거 상태다. 빌드 로그에서 복원한 구현·호출자·MOC 재컴파일을 확인했다.
- 확인한 build/recovery 및 hgis-backups에는 여섯 종류 성공 당시 전체 실행 파일/프로젝트 스냅샷이 없다. recovery에 남은 단일 앱 exe는 12:05 빌드로, 13:42~13:45 여섯 종류 성공 빌드가 아니다. 이를 성공 실행 파일로 대체하지 않는다.
- 앞선 '성공 당시 상태로 복원' 표현은 소스 복원 범위를 넘어선 보고였다. 소스 일치와 실제 동작 복구를 구분한다. 이번 조사에서 앱 코드·계정·자료를 추가 변경하지 않았다. 빈 응답의 근본 원인은 미확정이다.

## 2026-09-13 16:02 사용자 요청: 성공 확인된 상태로 재복원

- 사용자 요청으로 범례 자동 열, 도면/범례 갱신 보류, 직전 오류 상세 전달 변경을 제거했다. 번호 기능/그룹 라벨 메뉴는 제거 상태를 유지한다. 이후 다시 추가하지 않는다.
- Browser cpp/h, Flow, Import는 heritage-user-confirmed-all-download-20260913 백업과 일치한다. DrawingStudio.cpp는 heritage-before-extent-filter, 헤더는 before-layout-number-columns 백업으로 복원했다. MainWindow.cpp는 before-heritage-layout-deferral, 헤더는 before-heritage-import-error-detail에서 복원했다. 비교한 차이는 해당 후속 변경뿐이다. HeritageSiteLegend.cpp/h, LayoutService.cpp도 범례 작업 전 백업과 일치한다.
- 모든 복원 파일을 현재 시각으로 기록해 오래된 객체 재사용을 막았다. 직전 소스 백업 build/recovery/restore-success-20260913-160216/. 계정·원본 조사·내려받은 자료·관련 없는 미커밋 변경은 유지한다.
- 빌드 기록 build/qa/heritage-restore-success-1602-build.txt. 아래 기록은 과거 상태이며 오류 상세 전달/갱신 보류/범례 자동 열은 현재 제거됐다. 실제 다운로드 정상 여부는 사용자 확인으로 구분한다.

## 2026-09-13 강릉시 불완전 ZIP 확인 · 실제 실패 이유 전달

- 15:54:42 받은 지정유산_260913155429243.zip은 34,275바이트, ZIP 로컬 헤더 4개만 있고 중앙 디렉터리와 종료 레코드가 없다. Python zipfile도 정상 ZIP으로 열지 못했다. 앞서 성공한 강릉시 지정유산 ZIP 3개는 각각 90,844바이트, 항목 24개와 정상 종료 레코드를 가진다. 실제 원본 파일은 AppData에 그대로 보존했다.
- 이번 오류는 압축 준비 단계로, 번호·범례 적재 이전이다. 불완전 응답 발생 원인(서버/전송/브라우저)은 확정하지 않았다. 단순 시간 연장이나 손상 파일 적재로 처리하지 않는다.
- MainWindow.cpp/h만 변경: importHeritageDataset의 실제 오류를 rejectDataset에 전달해 진행 창과 진단 기록에 보존한다. 별도 경고 후 일반 실패 문구로 덮던 중복 표시를 제거했다. 열린 조사가 없을 때도 이유를 반환한다.
- Browser cpp/h, Flow, Import는 성공 기준 해시와 일치한다. 완료 확인 → ZIP/SHP 검사 → 지도 적재 → 다음 종류 계약을 유지한다. 변경 전 백업 build/recovery/before-heritage-import-error-detail-20260913/.
- Release 빌드 및 이번 변경 diff 검사 통과. 빌드 기록 build/qa/heritage-import-error-detail-build.txt. 자동 실서버 다운로드/자동 테스트는 실행하지 않았다. 다운로드 장애 자체가 해결됐다고 주장하지 않는다.

## 2026-09-13 다운로드 세션 중 도면·범례 갱신 보류

- 사용자 요청으로 다운로드 세션과 도면 갱신을 분리했다. 번호 기능은 제거 상태를 유지한다. 각 종류 파일의 검증·지도 적재 성공 후 다음 종류로 진행하는 계약은 그대로다.
- MainWindow가 기존 stageChanged/failed/allFinished를 통해 KaDrawingStudio::setProjectRefreshSuspended를 제어한다. Login~Download 동안 보류하고 Done/Idle/Failed에서 해제한다. 다운로드 중 새 도면 생성에도 초기 보류 상태를 전달한다. Browser/Flow/Import 파일은 수정하지 않았다.
- 도면 레이어 동기화·장식/범례 재구성·축척 예약 작업은 보류 시 pending만 남긴다. 전체 종료 후 이벤트 처리가 끝나면 보이는 도면에서 최종 레이어를 한 번 반영한다. 숨긴 도면은 다음 표시 때 반영한다. 도면 view repaint도 보류한다. 주 지도에서 각 종류 적재 결과 확인은 유지한다.
- 다운로드 중 범례 폭 자동 열 계산도 보류한다. 제목·글꼴·새 범례 위치 입력은 pending으로 보관하여 재개 시 반영한다. 보류 중 PDF 저장은 완료 후 저장하도록 안내해 미완료 도면 출력을 막는다.
- 변경 파일 MainWindow.cpp, KaDrawingStudio.cpp/h. 변경 전 백업 build/recovery/before-heritage-layout-deferral-20260913/. Release 빌드/변경 파일 diff 검사 통과, 독립 코드 검토 완료. 정상 스크립트로 PID 18372/창 핸들 6032020 확인. 빌드 기록 build/qa/heritage-layout-deferral-build.txt.
- 다운로드 성공 네 파일 SHA256 일치. 자동 실서버 다운로드/자동 테스트는 사용자 확인 방식에 따라 실행하지 않았다. 실제 다운로드 속도 개선은 사용자 확인 전이며 원인 확정이나 속도 개선을 측정했다고 주장하지 않는다. 커밋·푸시·포터블 없음.

## 2026-09-13 사용자 요청: 번호 기능 제거, 범례 자동 열만 유지

- 사용자가 번호 기능을 취소했다. 도면 번호 생성/번호·유적명 범례/공간 조회/레이어 복제/번호용 스타일 override/캐시/타이머와 관련 숨김 동기화 변경을 모두 제거했다.
- HeritageSiteLegend.cpp/h 및 KaDrawingStudio.h는 번호 추가 전 로컬 백업 내용으로 복구했다. 현재 시각으로 기록해 구현·호출자·MOC가 재컴파일되도록 했다. 다른 미커밋 작업은 보존했다.
- KaDrawingStudio.cpp에 범례 폭/글자 크기에 따른 여러 열 배치만 남겼다. 폭을 넓히면 열이 늘고 좁히면 줄며 긴 이름은 줄바꿈한다. 이름·분류와 기존 범위 필터를 유지한다. 번호 생성이나 도형 조회를 하지 않는다.
- 다운로드 성공 네 파일은 성공 백업과 SHA256 일치. 직전 번호 포함 상태 백업 build/recovery/before-columns-only-20260913/. 빌드 기록 build/qa/heritage-columns-only-build.txt.
- Release 빌드 및 변경 파일 diff 검사 통과. 정상 스크립트로 앱 실행 PID 18704/창 핸들 6817234 확인.
- 자동 실서버 다운로드/원본 조사 조작/자동 테스트는 수행하지 않는다. 실사용 다운로드 속도 개선은 아직 측정하지 않았으며 서버 또는 번호 기능이 지연 원인이라고 확정하지 않는다. 커밋·푸시·포터블 없음.

## 2026-09-13 도면 번호 · 번호/유적명 범례 · 폭에 따른 여러 열

- 사용자 최신 요청: 도면 안에는 번호만, 범례에는 같은 번호와 유적명. 종류별 1번부터. 범례 폭을 넓히면 여러 열로 배치한다. 분류별 항목은 유지하며 같은 종류의 같은 명칭은 같은 번호를 쓴다.
- 변경은 HeritageSiteLegend.cpp/h와 KaDrawingStudio.cpp/h에 한정했다. 현재 도면과 교차하는 유산 카테고리를 번호로 매기고 도면용 스타일 override에만 적용한다. 번호/범례 글자색은 해당 자료 색으로 일치시킨다. 원본 SHP와 주 지도 라벨은 수정하지 않는다.
- 숨긴 studio는 지도 동기화를 pending으로 두고 표시할 때 반영한다. 번호 계산은 표시된 도면에서 범위/종류/원본 스타일이 달라진 경우에만 수행한다. 모델 재생성 시 캐시의 layerId별 행을 새 노드에 재적용한다. 폭 변경은 fitColumns만 수행하며, PDF 직전에 번호를 갱신한다. 덧지도에도 같은 override를 복사한다.
- 다운로드 성공 네 파일은 사용자 확인 백업과 SHA256 일치. 다운로드/로그인/완료 상태는 변경하지 않았다. 작업 전 백업 build/recovery/before-layout-number-columns-20260913/.
- Release 빌드 통과, 변경 파일 diff 검사 통과, 독립 코드 검토에서 차단 결함 없음. 정상 실행 스크립트로 PID 20244, 창 핸들 7210324 확인. 빌드 기록 build/qa/heritage-layout-number-columns-build.txt.
- 사용자의 현재 확인 방식에 따라 자동 실서버 다운로드/자동 테스트/원본 조사 조작은 하지 않았다. 실제 도면/PDF의 배치와 속도는 사용자 확인 전이다. 이름 필드가 없거나 2,000개 초과로 단일 심볼이 적용된 기존 유산 레이어에는 번호 처리를 적용하지 못하는 제한이 있다.
- 커밋·푸시·포터블 없음. 아래 항목은 과거 상태다.

## 2026-09-13 문화재인트라넷 스킬 작성 · 도면 범례 조사

- 사용자 요청으로 프로젝트 로컬 .agents/skills/heritage-intranet/SKILL.md를 작성했다. 표시명은 문화재인트라넷, 호출 키는 heritage-intranet이다. AGENTS.md에 연결했다. 성공 해시/6종 완료 계약/보안·저장 경로/복원 시 과거 mtime으로 인한 재컴파일 누락을 기록했다.
- quick_validate.py 통과, openai.yaml 표시명·설명 길이·참조 파일 확인 완료. 새 스킬 참조 success-state.md와 legend-state.md는 실제 확인과 미검증 부분을 구분한다.
- 현재 범례는 설치 QGIS 4.3 commit 5073f6f11ec의 linkedMap 범위 필터에 이미 연결돼 있고 자체 extent 변경 갱신을 사용한다. 명백한 필터 누락을 발견하지 못했다. 긴 범례가 시군 전체라고 확정하지 않으며, 사용자 실제 도형 출력 검사는 하지 않았다.
- 사용자의 다음 선택을 요청했다: 자료 종류 안 동일 유적명을 한 줄로 합칠지, 분류를 유지할지. 선택 전 중복 제거·여러 열·번호 생성은 구현하지 않았다. C++ 변경 없음. 다운로드 성공 파일은 유지했다.
- 범례 작업 전 소스 백업 build/recovery/heritage-before-extent-filter-20260913/. 커밋·푸시·포터블 없음.
## 2026-09-13 복원 빌드의 오래된 객체 파일 재사용 수정

- 복원 앱이 주변유적 받기 시 종료됐다. WER 0xc0000374(힙 손상), 덤프의 주 스레드는 MainWindow::openHeritageBrowserFor의 시군 입력창, 충돌 스레드는 QtWebEngineCore 내부였다. Qt 내부 함수명은 PDB 없는 근접 심볼이라 실제 함수명으로 단정하지 않는다.
- 빌드 결함 확인: Copy-Item으로 복원한 KaHeritageBrowser.cpp/h 시간이 13:41로 돌아갔다. KaHeritageBrowser.obj는 확장된 클래스 멤버가 있던 14:18 빌드 그대로 남았고 MainWindow.obj는 복원 헤더로 14:51 재컴파일됐다. 당시 rollback 빌드 로그에 KaHeritageBrowser.cpp 재컴파일이 없다. 서로 다른 클래스 레이아웃의 객체 파일이 링크되는 오류다.
- 소스 동작은 바꾸지 않고 복원한 cpp/h의 수정 시간을 갱신해 Browser/MainWindow/MOC를 함께 재컴파일한다. 이후 백업 복원은 원래 수정 시간을 유지한 채 증분 빌드하지 말 것. 내용 해시 일치만으로 실행 파일 복원이 완료됐다고 판단하지 말 것.
- Release 빌드 성공. 로그에서 mocs_compilation_Release.cpp, KaHeritageBrowser.cpp, MainWindow.cpp 재컴파일 모두 확인. 정상 실행 스크립트로 PID 34708/창 핸들 확인. 주변유적 실행 후 안정성은 사용자 확인 전이다. 빌드 기록 build/qa/heritage-restore-recompile-build.txt. 읽기 전용 덤프 분석 기록 build/qa/app-crash-16892-stack.txt 및 app-crash-16892-main.txt. 기존 LLVM 디버거 실행에 필요한 Python 3.11 임베디드 런타임만 build/qa/lldb-python311에 내려받았다. 앱 의존성·전역 환경·계정·캐시·조사 자료는 변경하지 않았다.
- 자동 실서버 다운로드나 사용자 조사 데이터 조작은 하지 않는다. 그룹 메뉴·번호 범례는 제거된 성공 소스로 유지한다.
## 2026-09-13 사용자 요청: 그룹 메뉴도 제거하고 빠른 복원 상태 유지

- 0바이트 ZIP 실패 화면 이후 사용자가 다시 성공 상태로 복원하라고 요청했다. MainWindowContextMenus.cpp의 그룹 이름·번호 보이기/숨기기 추가만 제거했다. 번호 범례와 후속 갱신 변경도 제거된 상태다.
- 다운로드 네 파일(KaHeritageBrowser.cpp/h, HeritageIntranetFlow.cpp, HeritageImport.cpp)은 여섯 자료 성공 백업과 SHA256 일치한다. 자료·계정·기타 작업은 수정하지 않았다. 이번 복원 전 메뉴 소스는 build/recovery/before-group-menu-rollback-20260913/에 보관했다.
- 실패 기록의 실제 ZIP은 0바이트였으며 빈 파일 실패 조건은 유지한다. 메뉴 수정과 빈 응답의 인과관계는 확정하지 않는다. 다운로드 재실행/자동 테스트 없이 빌드 후 앱을 열어 사용자가 확인한다.
- 빌드 기록 build/qa/heritage-group-menu-rollback-build.txt. 커밋·푸시·포터블 없음. 아래 그룹 메뉴 추가 항목은 과거 기록이다.
## 2026-09-13 복원 후 빠른 다운로드 사용자 확인 · 그룹 라벨 메뉴만 추가

- 사용자가 복원 버전에서 다시 빨리 받아진다고 확인했다. 이후 변경이 문제라는 사용자 관찰을 우선 보존한다. 다운로드 네 파일은 여섯 자료 성공 백업 그대로 유지한다.
- 이번 범위는 MainWindowContextMenus.cpp의 그룹 우클릭 메뉴뿐이다. 참조 지도/지정유산/현상변경허용기준 등 그룹에 이름·번호 보이기와 숨기기를 추가한다. 하위 벡터 레이어에 클릭 시 한 번 적용하며, 유산의 최초 라벨은 분류 렌더러의 실제 명칭 필드를 사용한다. 기존 사용자 라벨 설정은 유지한다.
- 도면 번호 생성·번호 범례·자동 열·갱신 타이머는 재도입하지 않는다. 다운로드·적재·완료 조건은 변경하지 않는다. 원본 SHP는 변경하지 않는다.
- 빌드 기록 build/qa/heritage-group-label-menu-build.txt. 사용자 요청대로 자동 실서버/자동 테스트는 실행하지 않는다. 커밋·푸시·포터블 없음.
## 2026-09-13 사용자 요청: 번호·범례 추가 전 성공 상태 복원

- 사용자가 범례에 번호를 요청하기 전, 여섯 자료가 빠르게 모두 받아졌던 상태로 복원하라고 명시했다. 최신 활성 기준은 이 항목이며 아래 번호 범례/숨은 갱신/팝업 응답 수정 항목들은 과거 기록이다.
- KaHeritageBrowser.cpp/h를 build/recovery/heritage-user-confirmed-all-download-20260913/에서 복원했다. HeritageIntranetFlow.cpp, HeritageImport.cpp까지 네 파일 모두 당시 백업과 SHA256 일치를 확인했다. 공식 버튼 1회 → DownloadCompleted → 검증 → 지도 적재 → 다음 종류의 성공 경로를 유지한다.
- 번호·색상 범례, 자동 열, 그룹 이름 일괄 메뉴, 후속 숨은 도면 갱신 조정을 제거했다. KaDrawingStudio.cpp/h, MainWindowContextMenus.cpp, HeritageSiteLegend.cpp/h, LayoutService.cpp의 차이가 해당 기능/후속 변경에만 해당하는지 읽고 복원했다. HeritageStyle.cpp는 해당 기능이 추가한 metadata 한 줄만 제거했고 이전 shadeIndex 호환 변경을 보존했다.
- 복원 전 소스는 build/recovery/before-numbered-legend-rollback-20260913/에 보관했다. 조사 자료, 내려받은 파일, 계정, 기타 미커밋 작업은 건드리지 않았다. 커밋·푸시·포터블 없음.
- Release 빌드 성공, run-ka-hgis.ps1로 복원 앱 실행(PID 19384, 창 핸들 확인). 빌드 기록: build/qa/heritage-success-rollback-build.txt. 사용자 요청에 따라 별도 자동 다운로드/자동 테스트는 실행하지 않는다. 복원 후 실제 다운로드 속도는 사용자 실행에서 확인할 사항이다.
## 2026-09-13 범례·번호 수정 이후 숨은 도면 갱신 차단

- 사용자 기준: 여섯 자료를 빠르게 모두 받은 뒤 범례·번호 변경 이후 느려졌다. 성공을 다시 입증하기 위한 실서버 다운로드는 실행하지 않았다.
- 확인된 불필요 작업: KaDrawingStudio의 모델 변경 신호는 도면이 숨겨져도 syncMapFromLayers → tuneSheetLegend → 유산 레이어 clone/도형 조회/번호 계산을 실행했다. 숨은 탭에서는 변경을 pending으로 모으고 재표시할 때 반영하도록 수정했다. 범례·축척 예약 작업도 숨은 도면에서 실행하지 않는다. PDF의 직접 갱신은 유지한다.
- HeritageSiteLegend: 유산 레이어도 이전 번호 스타일도 없는 범례에서는 지도/덧지도 렌더 캐시를 무조건 무효화하던 작업을 제거했다.
- 다운로드/완료 판정 코드는 변경하지 않았다. 131초 파일 응답 대기를 이 경로 때문이라고 확정하지 않는다. 실사용 다운로드 속도 개선은 미측정이다. 자동 테스트/실서버 재실행은 사용자 요청에 따라 하지 않았다.
- Release 빌드 성공. 정상 run-ka-hgis.ps1로 PID 35868/창 핸들 확인. 변경 파일 diff 검사 통과. 전체 diff 검사의 tests/test_heritage_flow.cpp:188 공백 오류는 기존 별도 변경이며 수정하지 않았다. 변경 전 백업 build/recovery/heritage-hidden-legend-20260913/. 빌드 기록 build/qa/heritage-hidden-legend-build.txt. 커밋·푸시·포터블 없음.
## 2026-09-13 다운로드 창 응답 누락 수정

- 최신 성공 기준은 제주시 13:42:53~13:45:56의 여섯 자료 전체 다운로드·검증·적재와 사용자 확인이다. 각 파일 수신 시작은 요청 후 2~9초였다. 성공 소스 build/recovery/heritage-user-confirmed-all-download-20260913/를 유지한다.
- 후속 실행은 공식 버튼/실제 요청 전송까지만 기록되어 외부 응답 원인은 아직 확정되지 않았다. 저장된 공식 버튼 함수는 openWindow(..., hwpDown, ...)를 사용하지만 Browser의 loadingChanged는 주 화면 이외의 신호를 전부 버리는 결함을 확인했다.
- KaHeritageBrowser.cpp/h: 다운로드 창도 HTTP/연결 오류·중단·파일 전환·로그인/404 문서 응답을 확인한다. 로그에는 상태/오류영역/코드만 남긴다. Qt 6.11 설치 헤더 및 https://doc.qt.io/qt-6/qwebengineloadinginfo.html 근거.
- 일시적인 연결 실패/HTTP 408·5xx/파일 전 창 종료가 확인된 경우만 실패 창을 정리하고 5초 뒤 같은 자료를 1회 재시도한다. 시군·전국 가드를 다시 거친다. 시간 경과만으로 실패·완료 처리하거나 진행 중 요청을 다시 보내지 않는다. 중지 시 예약 재시도와 페이지 연결을 중단한다.
- 실제 DownloadCompleted → 파일 검증 → 지도 적재 → 다음 자료 조건과 여섯 자료 범위는 유지했다. Flow/Import/범례 코드는 이번에 수정하지 않았다.
- Release 빌드 성공: build/qa/heritage-popup-response-build.txt. 정상 실행 스크립트로 새 앱 PID 26284/창 핸들을 확인했다. 사용자 요청에 따라 자동 실서버 재실행·자동 테스트는 하지 않았다. 이 수정 후 여섯 자료 실수신은 미확인이다.
- 변경 전 백업 build/recovery/heritage-popup-response-20260913/. 커밋·푸시·포터블 없음.

## 2026-09-13 주변유적 그룹 라벨 · 도면 번호 범례

- 사용자 선택: 자료 종류별로 각각 1번부터. 도면 범위와 겹치는 유적을 범례 순서로 번호 매기며, 같은 종류/이름이 여러 하위 레이어에 있으면 같은 번호를 사용하고 범례 중복을 줄인다.
- 그룹 우클릭에 이름 한꺼번에 보이기/숨기기를 추가했다. 주변유적은 분류 렌더러의 실제 유적명 필드를 사용한다.
- HeritageSiteLegend가 도면 전용 QGIS 스타일 사본으로 번호 라벨을 만들고 범례에 번호+명칭을 표시한다. 라벨과 범례 텍스트는 심볼 색을 공유한다. 원본 SHP, 지도 화면 라벨, 조사 데이터는 수정하지 않는다.
- QGIS 3.44 영문 사용자 매뉴얼 PDF 801쪽: Columns Count/Equal column widths/Split layers 확인. 실제 SDK QgsLayoutItemMap::setLayerStyleOverrides, QgsSymbolLegendNode, QgsFeatureRequest::setDestinationCrs를 사용한다.
- 도면 범위 변경 시 번호 재계산, 범례 너비 변경 시 자동 여러 열과 줄바꿈. PDF 저장 직전에도 목록을 동기화한다. 도면 덧지도에도 같은 스타일을 적용한다.
- 사용자 요청대로 별도 자동 실사용 검증은 하지 않고 빌드 후 앱을 연다. 새 번호/색상/여러 열의 화면·PDF 결과는 아직 사용자 확인 전이다. 빌드 로그 build/qa/heritage-numbered-legend-build.txt.
- 공식 다운로드 성공 코드는 유지했다. 성공 소스 백업 build/recovery/heritage-user-confirmed-all-download-20260913/. 커밋·푸시·포터블 없음.

## 2026-09-13 사용자 확인: 주변유적 전체 다운로드 성공

- 사용자가 "현재 지금까지 완벽하게 받아졌다"고 확인했다. 공식 전체다운로드 버튼을 한 번 실행하는 현재 경로가 사용자 환경에서 성공했다. 이 성공은 이전의 단일 지정유산/빌드 성공과 구분한다.
- 유지할 조건: 시군 범위 → 공식 버튼 1회 → 실제 요청/DownloadCompleted → 파일 검증 → 지도 적재 성공 → 다음 자료. 작은 진행 창, 모두 완료 시 닫기, 참조 자료를 위성/지적 위에 배치. 다운로드 코드를 후속 범례 작업에서 바꾸지 않는다.
- 다음 사용자 요청: 자료 그룹 우클릭 일괄 이름 표시/숨기기, 도면 번호와 범례 번호·명칭·동일 색상, 범례 너비에 맞는 여러 열 배치.

## 2026-09-13 공식 전체다운로드 버튼 실행으로 변경

- Release 빌드 성공. 사용자 확인용 앱 PID 35732 실행. 실서버 다운로드 성공은 아직 미확인.

- 제주시 13:35 실행은 검색 174건 이후 본 화면 URL 이동까지 기록됐으나 파일 수신은 없었다. 성공 기록과 지역/자료 파라미터 형식이 같아 URL 값 오류나 서버 지연으로 단정하지 않는다.
- 브라우저 Download 단계의 별도 URL 조립/이동을 제거하고, 현재 검색 결과 tabContentDiv 안의 보이는 전체다운로드 버튼 한 개를 1회 클릭한다. 시도·시군 선택과 전국 가드는 유지한다. 같은 이름의 지도 함수를 직접 호출하지 않는다.
- 버튼 실행 중 동기 사이트 오류를 구분하고, 요청 인터셉터에서 실제 다운로드 경로 전송을 확인한 뒤에만 요청 전송 확인으로 표시한다. 요청 기록에서는 질의/사용자정보/프래그먼트를 제외한다.
- 성공 조건은 변경 없음: DownloadCompleted → 저장 파일 존재/검증 → 지도 적재 성공 → 다음 자료. 실제 여섯 자료 전체 성공은 아직 미확인. 자동 재클릭·별도 HTTP 재요청 없음.
- 변경 전 백업 build/recovery/heritage-official-download-click-20260913/. 빌드 로그 build/qa/heritage-official-download-click-build.txt. 사용자 요청대로 실서버 동작 확인은 사용자가 진행하며 빌드 후 앱을 연다.

## 2026-09-13 파일 응답 대기 · 본 화면 다운로드 연결

- 제주시 174건 검색 후 새 탭 요청에서 파일 수신 시작 기록이 없었다. 09:07 지정유산 성공 기록의 본 화면 다운로드 방식으로 연결을 변경했다. URL 인코딩 방식은 기존 성공 사례가 있어 유지했다.
- m_downloadNavigation으로 파일 요청의 loadStarted가 화면 준비 상태를 해제하지 않게 한다. 파일 대신 웹페이지가 로드되면 오류로 표시한다. 수신 시작 전에는 서버 준비 완료 여부를 추정하지 않고 파일 응답 대기로 표시한다.
- 성공 조건 유지: 실제 DownloadCompleted + 파일 존재/검증 + 지도 적재 성공 후 다음 자료. 여섯 자료 전체 실제 성공은 아직 미확인이다.
- Release 빌드 성공: build/qa/heritage-download-main-build.txt. 사용자 확인용 앱 PID 32264 실행. 테스트/실서버 다운로드 재실행은 하지 않았다.
- 변경 전 백업: build/recovery/heritage-download-main-20260913/. 커밋·포터블 없음.

## 2026-09-13 다운로드 화면 열기 판정 수정

- 제주시 13:23 실패: not-yet/opened 반복. openDownloadPageScript가 tabContentAjax 함수 존재만으로 opened를 반환하여 showAgreePopup을 실행하지 않는 경로 확인.
- 함수 존재를 화면 완료로 취급하는 분기 제거. 실제 다운로드 폼/서약서만 열린 상태이며 showAgreePopup은 한 번 요청 후 화면 도착을 확인한다.
- 서약 확인 버튼은 서약서 범위에서 찾는다. 제출 시 실제 원문을 기록하고 다운로드 폼 도착 후 다음 단계로 진행한다. 프레임 src를 새 탭으로 승격시키며 m_pageReady를 낮추던 잔여 우회 삭제.
- 다운로드 수신 완료 대기, 순차 적재, 여섯 자료 범위 및 작은 진행 창 유지. 실서버 전체 성공은 아직 미확인. 빌드 로그 build/qa/heritage-open-confirmation-build.txt, 변경 전 백업 build/recovery/heritage-open-confirmation-20260913/.

## 2026-09-13 로그인 후 화면 대기 보완

- 제주시 사용자 실행 13:18:13 로그인 제출 → 13:19:06 튜토리얼 단계 시간 초과. 그 사이 단계 스크립트 결과 없음. 로그인 후 main.do 요청만 확인되며 서버 오류 자체는 기존 로그로 확정하지 못함.
- Login/DismissTutorial은 전체 페이지 loadFinished 대신 실제 로그인 폼/로그아웃 DOM을 확인한다. 빈 문서의 로그인 성공 판정 제거. 인증된 화면 확인 후에만 튜토리얼 처리로 진행.
- 실제 주 화면 로딩 실패는 Qt loadingChanged 오류 코드/경로를 표시. 리다이렉트 취소(-3)와 다운로드 전환은 실패로 오인하지 않음.
- 다운로드 완료 대기·여섯 자료 순서·작은 창·레이어 순서 변경은 유지. 사용자 요청대로 빌드 후 앱을 열며, 원격 여섯 자료 실제 성공은 아직 미확인.
- 백업 build/recovery/heritage-login-readiness-20260913/, 빌드 build/qa/heritage-login-readiness-build.txt.

## 2026-09-13 여섯 자료 순차 처리 · 작은 진행 창

- 사용자 확인: 지정유산은 대기 수정 후 삼척 및 다른 지역에서도 다운로드·적재 성공. 구미 한 지역에 한정했던 성공 범위가 사용자 실사용으로 확대됨. 여섯 자료 전체 성공은 아직 별도 확인 전.
- 최신 요구: 앱 도킹 패널 제거, 작은 비모달 진행 창, 전부 완료하면 자동 숨김. 실패는 창/이유를 남긴다. 웹사이트와 진단은 자세히에서만 표시.
- 여섯 자료(S/P/B/U/R/E) 순차 처리. 탭 요청은 한 번 보내고 새 폼/해당 mode 도착 확인. 검색은 결과 DOM 교체를 확인하며 전국 가드 유지. 지역별 0건은 명시 기록 후 다음 자료. 수신은 실제 DownloadCompleted/저장 파일 확인 후 검사 및 동기 지도 적재 성공 때만 다음 자료.
- 기존 성공 전송(한 번 인코딩·새 탭·완료 신호)은 유지. 앞선 기록에서 확인된 codedetaCd/codeCdSg와 bjdCd/bjdCd1 폼 차이 처리를 필요한 함수에만 재적용.
- ZIP 오류·필수 SHP 부속파일 누락·벡터 열기 오류는 일부 성공으로 숨기지 않는다. LayerOps의 기존 배경 정렬을 확장해 배경 타일이 참조 벡터보다 아래, 위성은 맨 아래. 조사 데이터/기존 참조 데이터 보존.
- 변경 전 소스: build/recovery/heritage-six-sequential-20260913/. 빌드 기록: build/qa/heritage-six-sequential-build.txt. 사용자 요구대로 빌드 후 앱을 바로 열고 실사용 확인을 받는다. 커밋·푸시·포터블 없음.

## 2026-09-13 삼척시 다운로드 대기 및 앱 내 진행 패널

- 사용자 구미시 12:26 실행: 지정유산 ZIP 55,059바이트 CRC 정상, SHP6개/도형88개, 화면 적재 확인. 다른 지역 성공으로 확대하지 않는다.
- 삼척시 12:31 실행: 검색43건 정상, 요청 후 약4초의 고정6틱 대기로 파일 수신 전에 실패. 이 대기 조건을 제거했다.
- 파일 준비는 중지 가능 상태로 실제 다운로드 신호까지 기다린다. 전송 중 실제 수신량/전체량을 표시하며 DownloadCompleted와 저장 파일 확인 후에만 파일 검사·지도 적재로 진행한다. 적재 실패는 다음 자료/전체 완료로 넘어가지 않는다.
- 별도 웹 창 대신 MainWindow 하단 도킹 진행 패널. 웹/진단은 기본 숨김, 자세히로만 표시. 지정유산1종 범위·요청 URL 구성·새 탭 수신·원본/참조지도 처리 유지.
- Release 빌드 통과: build/qa/heritage-completion-build.txt. 사용자 지시대로 자동 실서버 검사/전체CTest를 앞세우지 않고 정상 실행 스크립트로 앱을 열었다. 수정 후 삼척시 실수신은 아직 미확인.
- 변경 전 백업: build/recovery/heritage-samcheok-wait-20260913/. 커밋·푸시·포터블 없음.

## 2026-09-13 PID 22940 복원 — 최신 작업

사용자가 오전 08:53의 「새 빌드 (PID 22940)」로 복원을 명시 요청했다.
Claude 대화의 2026-09-12T23:52:38Z 실행과 23:53:12Z 보고를 식별하고,
해당 시점의 파일 이력으로 주변유적 Browser/Flow/Import 및 당시 검사를 복원했다.
MainWindow는 당시 AppData 저장 경로와 지정유산 한 종류 대상을 재구성했다.
현재본과 실행파일 백업: `build/recovery/pid22940-before-restore-20260913-121846/before/`.
복원 원본·SHA256·파일별 근거: 같은 폴더의 `manifest.json`과 `target/`.
이후 추가한 브라우저/단일버튼 검사 코드는 백업에 보존하고, 빌드는 당시 API의 검사 구성으로 복원했다.
다른 기능 및 사유 자료의 제출 제외·계정 로그 비노출 변경은 보존했다.
이번 작업은 과거 코드 복원이며, 원격 다운로드 성공을 새로 확인했다는 뜻이 아니다.
복원 Release 빌드 완료. 사용자 지시로 진행 중이던 자동 검사를 중단했으며 전체 통과로 보고하지 않는다. 2026-09-13 12:22에 정상 실행 스크립트로 앱을 열었고 PID 37328의 실제 창을 확인했다. 이후 수정 시 자동 검사를 사용자보다 먼저 강행하지 말고, 빌드 후 앱을 바로 열어 사용자가 확인하도록 한다.
커밋·푸시·포터블·dist/L 복사 및 실제 조사/유산 자료 변경 없음.

# NOW — Codex Resume

## 2026-09-13 주변유적 여섯 자료 실수신·지도·조판 검증

- 후속 단일 버튼 통합 검사는 11:22에 첫 다운로드 시작 대기 제한으로 실패했다(받은 자료 0종). 로그인·동의·실제 포항 판정/확인·53건 검색 및 GET 전송은 통과했다. 분리 수신/적재 검사 통과와 전체 완료를 구분한다. 세션값 외 성공 요청과 파라미터가 같아 탭 응답 지연 진단 중이다.
- 현재 성공 조건과 증거: `docs/superpowers/plans/2026-09-13-heritage-verification.md`. 기준은 사용자 제공 2026-09-11 설계 9장이다. 이전 지정유산 ZIP 한 개/일부 단위 검사 결과를 전체 완료로 취급하지 않는다.
- 최신 실패 지역인 포항시에서 실제 로그인·동의·시군 검색·S/P/B/U/R/E 여섯 ZIP 수신까지 194.246초에 완료했다. 받은 13개 SHP/3709도형을 실제 MainWindow 연결로 적재하고 지도 화소·여섯 그룹·고정 색을 검사했다. 사용자 실행 창을 조작한 검사는 아니다.
- 원본 ZIP 불변, 독립 압축 해제본과 SHP/DBF 26개 SHA256 일치. 원본 EPSG:5179 → 검증 프로젝트 EPSG:5187, 참조 지도/읽기 전용, 기존 조사 데이터 보존. 실제 유적명·범위 밖 이름 제외·지도와 범례 색을 여섯 종류의 조판 PNG/PDF로 확인했다.
- 원인: 숨겨진 폼과 동의 체크상자, 팝업 밖 확인 버튼, 잘못된 지정유산 탭 코드, 자료별 지역 필드 차이, 중복 AJAX 요청. 실제 보이는 폼/라벨/팝업 확인 버튼을 사용하고 지정유산 S를 확인했다. B/U/R/E의 bjdCd/bjdCd1을 실제 검색 폼 안에서 사용한다. 시/군 정확 일치와 한 번 요청 후 새 폼 대기를 적용했다.
- iframe 이동 우회 제거와 실제 404/주 화면 로드 실패의 1회 홈 복구를 유지한다. 퍼센트 인코딩 한 번+새 탭+파일 완료 신호라는 검증된 수신 조합은 유지한다. 과다 접속 차단·영속 프로필 오염은 확인된 원인이 아니다.
- 적재 실패가 완료로 덮이지 않도록 연결했다. 묶음 안 깨진 ZIP/SHP, SHP 없는 ZIP, DBF 누락은 일부만 등록하지 않고 실패한다. 동의 영수증 저장 실패·제출 SHP 제외의 추가 회귀 및 최신 Release 검사는 검증 문서의 최종 기록을 따른다.
- 사유 원본·캡처·PDF는 AppData의 `주변유적-QA/경상북도 포항시` 아래에만 있다. 저장소/포터블/제출 SHP에 복사하지 않는다.
- 바탕 `고고학 전용 HGIS.lnk`→`scripts/start-ka-hgis.vbs`→`launch.ps1`→`build/Release/ka-hgis.exe` 연결 확인. 커밋·푸시·포터블·dist/L 복사 및 사용자 앱 종료/조작 없음. 기존 미커밋 변경 보존.


## 2026-09-10 수치지형도 축척 필터

- 광역 80.862초(122레이어)의 1차 원인은 extent-only coverage가 7도엽 전체를 연 것이다. `visibleAtScale`(>1:25000이면 등고·도로·수계·경계만)과 `stopRendering` 가드를 넣었다.
- 안동 재측정(`field-scale.txt` / `next-run/`): 조사 1:1219 1.124초 15레이어(기존 채택), 35초 무입력 유지, 축소 **19.044초/55레이어**, 복귀 1.583초/15레이어. 80초는 줄었으나 19초는 남음. QA에서 무조작 소실은 재현되지 않음(회색 화소 유지). 현장 소실 원인은 미확정.
- Release `ka-hgis.exe` SHA256 `C0FB744CB277C30CCD7EE53FD7D8A676D87127405D41C8DB39C3B6F9651B95D7` (`scaleChanged`도 coverage 타이머에 연결한 뒤 재링크). 직전 CTest 37/37·smoke 영수증의 EXE 해시(`1A36F5…`)는 이 한 줄 연결 이전이다. import 집중 검사 20/20. 커밋·포터블·publish 없음.

## 수치지형도 자동 받기 — 2026-09-09 최종 전송·지도 적재

- 최신 요구는 **여러 도엽을 한꺼번에 체크해 한 신청서로 받기**다. 공식 `downloadMapData`에 확인된 미보관 도엽 배열 전체를 전달한다. 실제 제주 반경10km의 도두·한림·귀일 3도엽/DXF+XML 6파일 수신을 확인했다(40.514초). 파일끼리의 전송만 Qt 저장 완료 뒤 순서대로 시작하며 도엽마다 로그인/신청하지 않는다.
- 최종 공식 INNORIX 페이지의 HTML5 다운로드 컨트롤 생성/주소/파일 ID/메타데이터를 연결했다. 외부 Agent 설치나 GPT·Codex·MCP 개입 없이 앱 코드로 로그인·신청·동의·파일 수신을 진행한다. 공식 웹페이지는 숨기고 단계·실제 파일 수신량·전체 완료 수·실패/취소 사유만 표시한다.
- 실제 받은 한림·귀일은 SHP33개/213,809피처로 자동 준비·지도 적재됐고 1:25000 Qt/QGIS 화소를 확인했다. 회색 #808080/0.2mm, 읽기 전용 참조 역할, 프로젝트/레이어 EPSG:5186 및 원본 SHA256 불변을 검사했다. 위성/지적과의 독립 측량 정합 검증과는 구분한다.
- 귀일 원본 도곽과 공식 색인은 위도0.025°(중심 약2770m)가 다르다. 기존100m 여유를 늘리거나 좌표를 이동하지 않았다. 원본 H0017334의 닫힘·모든 변·모서리·기하 유효성과 후보별 왕복 잔차를 검증해 유일한 EPSG:5186(최대4.757mm)을 확인했다. 차이는 사용자에게 안내한다. 단일/유일 후보가 아니거나 손상·미폐합·365m 이동·도곽 밖 지형은 자동 적재하지 않는다.
- 도두 원본은 주요 지형 없이 도곽만 있다. 수신 실패나 정상 지도 적재로 혼동하지 않고 검토 대상으로 보관한다. 검토 영수증은 공식 메타데이터·상대경로·원본 SHA256·solverVersion을 확인한다. 이전 판별기 결과는 받은 원본으로 재평가하고 변환/저장 실패에도 재다운로드하지 않는다. 원본과 기존 기록은 보존한다.
- F0027132 표고점수치의 `Text`를 읽어 표시한다. 글자 삽입점 Z=0 때문에 모두0.0으로 표시되던 원인을 수정했다. 실제0m와 다른 표고점의 Z를 보존한다. 확대 중 레이어 제거로 렌더가 취소되면 갱신을 다시 예약한다. 백그라운드 도곽 읽기는 GUI 프로젝트에 접근하지 않는 읽기 전용 QGIS OGR provider를 사용한다.
- 최신 범위는 **10km 고정**. 저장은 현재 조사 폴더/지형도 아래 원본·SHP·index·receipts. 더보기의 VWorld API 키 아래 계정 설정에서 ID/비밀번호를 바꾸며, 변경 시 이전 세션을 폐기한다. 개인정보/비밀번호를 코드나 보고서에 하드코딩하지 않는다.
- 검증: 실제 공식 전송, 실제 파일 변환·지도 화소, 도곽/표고/저장실패/취소 회귀를 확인했다. **Release 구성·빌드, 전체 CTest 37/37(216.35초), 시작 검사 통과**. build/release-verified.json 발급. EXE SHA256 E1EBC6FC4F9371FBC5A0604612D44C8C9E8BE9D5CED875BF5FAE97F17546CEB2. 상세 근거와 캡처는 `build/qa/topographic-complete/REPORT.md`.
- 실행은 바탕 화면 **고고학 전용 HGIS** → `scripts/start-ka-hgis.vbs` → `launch.ps1` → `build/Release/ka-hgis.exe`. 사용자 실행 창을 종료/재시작/조작하지 않는다. 포터블/publish/dist/L: 복사는 새로 명시적 요청받을 때만. 이번 작업 커밋·푸시·포터블 없음.

## DEM 연속 표현 — 2026-09-09

- 최신 사용자 지시에 따라 DEM은 전국 고정 0–2000m 연속 보간, 저지대에 조밀한 16개 표고 기준점, 공통 세로 색띠(표고 m)로 변경했다. 전국/저지대/화면맞춤 프리셋을 제공하며 화면맞춤은 이동 후 눈금도 갱신한다.
- DEM 음영은 기본 ON으로 최신 요구를 적용했다. 미터 좌표계 VRT, 다방향·Multiply·기본 z=1/강도30%, 표고 샘플의 provider bilinear 보간을 사용한다. 사용자 OFF 및 저장/재열기 설정을 보존한다. 이전 'DEM 음영 자동 추가 제거' 보류 기록보다 이번 명시적 요구가 우선한다.
- 당시 수치지형도 공급원 0단계 조사를 마쳤다. 이후 사용자의 자동 다운로드 구현 요청으로 진행 상태가 바뀌었으며 위 최신 항목을 따른다. 지형맵(OpenTopoMap PNG XYZ)과 수치지형도 벡터는 별개다.
- 이후 사용자 지시로 포터블/publish는 새로 명시적으로 요청한 경우에만 실행한다. 검증 대상은 바탕 화면 아이콘이 여는 현재 Release다.
- 검증 상세와 전후 화소: build/qa/dem-continuous/REPORT.md 및 gallery.html. 완료 여부는 해당 보고서의 최종 실행 결과로 판단한다.


## Current State (2026-09-08)

- 사용자 최신 지시: 새로 명시적으로 요청하기 전에는 포터블 제작·publish·dist/L: 복사를 실행하지 않는다. 이전 포터블 요청과 AGENTS의 일반 publish 절차보다 우선한다. 사용자 실행 앱도 종료·재시작·자동 조작하지 않는다.
- 사용자 고정 실행 경로는 바탕 화면 **고고학 전용 HGIS.lnk** → `scripts/start-ka-hgis.vbs` → `launch.ps1` → `build/Release/ka-hgis.exe`다. 실제 바로가기 대상을 확인했고 그대로 유지했다. Release가 없을 때 다른 빌드/포터블을 실행하던 fallback을 제거했다. AGENTS Verification도 이 경로와 요청 시만 포터블 제작으로 갱신했다.
- 최신 수정 범위는 주변 거리 경계의 중복 표시, 시도 선택 이동, 요청하지 않은 「현장 지도」 버튼 제거, 좁고 긴 레이어의 전체 범위 이동이다. 자동 검증 결과와 화소는 `build/qa/navigation-repair/REPORT.md`에 기록한다. 기존 전체 Phase가 모두 완료된 것은 아니다.
- 위 이동/중복 수정은 Release·전체 CTest 24/24(192.44초)·headless 시작 검사 통과로 마무리했다. 후속 요청은 **면적 기본 ON 및 글자 크기 변경 시 표시 보존 → 시굴격자 면적10%/길이≤20m/폭≤2m/구역 내부 → 도면 정보 아래 공백을 활용한 행간과 작은 화면 스크롤** 순으로 처리 중이다. 각 묶음 검증 후 다음으로 진행한다. 면적 수정 보고는 `build/qa/label-area-repair/`에 기록한다.
- 면적 표시 수정 완료: 새 조사 폴리곤 면적 기본 ON, 글자 크기 변경 및 후속 편집에서 표시식·체크·숨김 보존. 독립 리뷰의 크기 식 재정의 문제도 보완했다. Release·전체 CTest 24/24(190.34초)·headless 시작 검사와 실제 Qt/QGIS 전후 화소가 통과했다. 보고: `build/qa/label-area-repair/REPORT.md`. 다음은 시굴격자 규격이며, 도면 정보 패널은 그 다음 묶음이다.
- 시굴격자 수정 완료: 자동 생성 10%/2%, 폭≤2m·길이≤20m·구역 내부·비중복, 재생성 트랜잭션과 미저장 편집 보존. 25×115m 검사에서 길이 24m/면적 288㎡ → 최대 길이 19.6918m/면적 287.50㎡. 기하 49/49·통합 2조건·전체 CTest 24/24(196.72초)·headless 시작 검사 통과. 소스별 실제 QGIS 전후 화소: `build/qa/trench-limits/REPORT.md`.
- 도면 정보 패널 수정 완료: 아래 공백을 행간으로 분배하고 최소 8px 간격, 카드 내용 높이 보존, 작은 창 세로 스크롤과 최소 폭을 적용했다. 1800×1150 검사에서 마지막 행 아래 공백 358→17px. 일반/큰/작은 창 및 150% 배율 집중 검사 각 6/6, 전체 Release·CTest 24/24(196.79초)·headless 시작 검사 통과. 자동 Qt 전후 화소와 로그: `build/qa/drawing-panel-spacing/REPORT.md`. 면적 표시·시굴격자·패널의 후속 세 묶음을 각각 검증하고 마무리했다. 사용자 앱을 조작하거나 포터블을 제작하지 않았다.


### 회귀 안정화 — 2026-09-08 (이전 자동 검증 20/20, 사용자 실측 확인은 별도)

- 사용자가 “하나를 고치면 다른 기능이 고장 난다”는 회귀를 우선 해결하도록 요구했다. 하천명 표시·DEM의 지형 음영 자동 추가 제거·새 지질도 공급원 연결은 보류하고 이번 안정화 결과를 보고한 뒤 멈춘다.
- 실제 16:13 지형맵/16:01 지적 추가 종료 스택은 위성 정렬의 `QgsLayerTreeGroup::reorderGroupLayers`에 모인다. 설치 SDK의 삭제 노드 재참조 경로를 피하도록 앱의 3호출을 `LayerOps::moveLegendLayer`로 대체했고, 중복 정리 전부터 재진입을 막는다. 복제 삽입 후 기존 노드만 제거하여 원본 데이터와 레이어 수명을 보존한다.
- 주제도 100001 축척 제한을 해제했다. 기존 앱 생성 자료의 같은 제한만 열기 후 메모리에서 갱신하고, 외부 자료의 사용자 제한은 보존한다. 새 다운로드 범위/지질 80 km·수계 160 km 제한은 유지한다.
- 주소 팝업을 클릭한 모니터 작업영역 안에 배치하고 작은 화면에서는 줄바꿈한다. 일반·150% DPI 각각 QtTest 9/9 및 자동 화소 확인을 마쳤다.
- 별도 WMS 종료 스택의 부분 렌더 중첩을 완화하도록 `RenderPartialOutput`을 해제했다. 실제 로컬 XYZ의 타일 수신 중 이동·취소·최종 화소 검사(5186/5187)는 통과했지만 이전 부분 출력 설정도 같은 합성 검사에서 통과했다. 따라서 이 별도 현장 종료를 완전히 재현·해결했다고 표시하지 않는다.
- 앞선 공통 우클릭 복구, Delete 레이어 제거와 Ctrl+Z 복원, 꼭짓점·일괄 삭제 Undo, DEM/오프라인 다운로드 진행·취소, 큰 축척 분모에서 팬 수정도 현재 코드에 보존되어 전체 검사 대상이다. 병합·격자 재생성 등 모든 Undo 경로와 과거 전체 Phase가 완료된 것은 아니다.
- `scripts/verify-release.ps1`이 구성·Release 빌드·전체 CTest·headless 시작 검사 후 소스/테마/테스트/EXE 해시 기록을 남긴다. `publish-desktop.ps1`은 이 기록과 현재 입력이 일치해야 복사하며, 실행 중인 사용자 앱은 강제 종료하지 않고 배포를 거부한다. Codex 전역 hooks 설정은 변경하지 않았다.
- 최종 전체 CTest **20/20, 201.25초**, Release 시작 검사, D: portable의 시스템 PATH만 남긴 직접 EXE 시작 검사 exit 0. EXE SHA256 **4C09CC66E760304576CB5704E3680DDCF38BEA3A3F4D2F86FDA158E8D61FBA8B**. 원본 조사 파일 수정 및 커밋 없음.
- 실제 사용자 앱의 화면 자동 조작은 중단 요청을 유지하여 실행하지 않았다. 자동 Qt/QGIS 합성 캡처를 실제 현장 portable 화면 검증으로 대체하지 않는다. 보고/화소/로그: `build/qa/regression-repair/REPORT.md`. L: 이동식 포터블 배포 결과도 이 보고서에 기록한다.

### 앱 전체 테마·광택·축척 추가 — 2026-09-08 (자동 검증·배포 완료, 실제 화면 확인 미완료)

- 사용자의 후속 요청에 따라 버튼 비례에 이어 **앱 전체 테마·아이콘을 실제로 변경**했고, 가장 최근 요청인 **10000·25000 축척 추가, UI 색 20% 완화, 광택 강화**까지 구현했다. 앞선 단계 순서만을 이유로 명시적인 후속 요청을 보류하지 않는다.
- 크림 바탕을 쿨그레이/차콜/청록빛 파랑의 공통 팔레트로 통일했다. 수계는 파랑, 토양은 갈색·황토층과 검은 윤곽이며 기능별 아이콘 색과 상태가 구분된다. 리본·홈·레이어/파일함·메뉴·입력·상태표시줄·지도/조판/사진 정합의 UI 바탕에 같은 테마를 적용했다. 지도 심벌과 PDF 용지 내용 색은 보존한다.
- 최신 완화 정책: 기능 채움색의 HSL 채도는 직전의 80%, 밝은 표면은 기존 RGB 80%+흰색 20%. 글자·윤곽·어두운 홈 레일은 가독성을 위해 보존한다. 상단 반사광 띠와 중간톤/하단 음영을 강화했다. 팔레트 계산 최저 대비 4.821:1, 실제 자동 위젯의 검사한 상태 중 최저 5.251:1.
- 도면 정보는 기존 3열 정렬에 10000/25000을 한 행 추가했다. `syncScaleChips()`가 실제 QToolButton을 찾게 고쳐 선택 표시가 하나만 남는다. 합성 EPSG:5187 스튜디오에서 버튼 클릭→지도 축척·입력값·선택 상태를 검사한다.
- 전체 Release 빌드, 테마 21개 검사, build/portable 시작 스모크와 배포가 통과했다. 첫 전체 CTest의 workflow QSS 검사는 고정 220자 제한 때문에 실패해 해당 규칙의 닫는 중괄호까지 확인하도록 고쳤다. 캡처 fixture의 한글 글꼴·실제 테마 적용도 보완했다. 수정 후 최종 전체 CTest **17/17(179.27초)**가 통과했다. 테마 21개와 실제 축척 버튼 단독 검사도 통과했다.
- 배포 실행 파일 SHA256은 `D1B11AD5FE89580F62B4EF76925996937EB415D0DEA62333FE5A34FC8DA9592B`이며 Release와 portable가 동일하다. 소스/portable QSS도 동일하다. `publish-desktop.ps1` 실행 직전 HGIS 프로세스가 없음을 확인했고 사용자 앱을 강제 종료하지 않았다.
- **실제 portable 변경 후 화면·PDF 검증은 미완료**다. native 창 목록 조회에서 사용자 물리 Escape 중단이 반환되어 이번 턴에 추가 Computer Use를 하지 않는다. 자동 Qt 렌더와 기존 사용자 캡처를 실제 portable 변경 후 검증으로 대신하지 않는다. 다음 단계 자동 진행과 전체 요구 완료 판정은 하지 않는다.
- 최신 상세 보고·팔레트·전후 자동 화소·검증 로그: `build/qa/pro-ui-soft20/REPORT.md`. 이전 전체 테마/아이콘 근거는 `build/qa/pro-ui-step2/`, 버튼 비례 기준은 `build/qa/pro-ui-step1/`. 사용자 원본 조사 파일을 수정하지 않았고 커밋하지 않았다.

### 고고학 전용화 1단계 — 검증 미완료 (2026-09-08)

- 이전 사용자 요청은 1 우클릭 메뉴 → 2 UI/동선/Undo → 3 격자 → 4 조판 → 5 전체 GIS 정리였다. 1단계 구현을 보존했고 2~5단계는 완료하지 않았다. 현재 우선순위는 위의 최신 전문가용 UI 요청이다.
- 새 `MainWindowContextMenus.cpp`에서 layer_key/명시적 역할/공급자로 메뉴를 나눈다. 불필요 항목은 제외, 일시적으로 불가능한 명령은 비활성+한국어 툴팁, 삭제는 마지막 그룹. 우클릭 행과 작업 대상을 일치시키고 목록 제거와 원본 도형 삭제를 분리했다.
- 선택 도구 메뉴도 조사 layer_key에만 편집 항목을 제공한다. 이전/다음 지도 범위는 QGIS 기존 기능을 사용한다. 가져온 래스터/CAD에는 imported_reference 표식을 보존하며 정합 결과 객체에도 승계한다.
- 작업 전 소스 사본/합성 9종 fixture/변경 전 캡처 8개/단계 전용 패치: `build/qa/archaeology-step1/`. 상세 상태는 같은 폴더 `REPORT.md`.
- **사용자가 물리 Esc로 Computer Use를 중단했다. 이 턴에서는 추가 UI 조작을 하지 않는다.** 지적 WMS 메뉴는 관찰했지만 파일 저장 전에 중단됐고 지도 캔버스 및 변경 후 9종 portable 캡처는 남았다.
- 앞서 사용자 조사 앱이 Release 실행 파일을 잠가 LNK1104가 발생했으나 사용자가 앱을 닫아 해소됐다. 우클릭 후 창 종료 시 지도 도구가 캔버스보다 늦게 파괴되는 문제를 소멸자 순서로 수정했고, 전체 CTest에서 save_open_window와 workflow가 통과했다. 최종 빌드·검사 로그는 전문가용 UI 단계에 함께 기록한다. 메뉴 9종 변경 후 화면 검증은 여전히 미완료다.
- Ctrl+Z/삭제 키 전체, UI 글자·크기·색·여백, 격자 알고리즘과 조판 흐름은 이번 단계 변경에 포함하지 않았다. 저장되지 않은 도형 편집의 Undo/레이어 제거 키 연결은 2단계에서 우선 확인해야 한다. 기존 원본 데이터와 다른 미커밋 변경을 보존하고 커밋하지 않았다.

### 현장 완성도 — Phase 0 / Phase 1 (2026-09-08)

- 사용자는 Phase 0 측정 후 Phase 1 안정화까지 진행하고, 각 Phase 끝에서 멈춰 보고하도록 요청했다. Phase 2~6을 이어서 자동 진행하지 않는다.
- Phase 0은 제품 수정 없이 약 13분 동안 측정했다. 실제 portable 캡처와 24개 참조 레이어/24,000 피처 반복 측정, 작업 전 소스 사본은 `build/qa/production-phase0/REPORT.md`와 같은 폴더에 있다.
- Phase 1: 새 조사 동명 파일 보호, QGZ 원자적 교체·실패 복원, Save As 실패 시 원본 소스/메모리 피처 복원, 닫기 저장의 메모리 벡터 보관, 조사 전환 전 미저장 확인, 저장 중 닫기 재진입 차단을 구현했다. 다른 이름으로 저장은 기존 대상 GPKG/QGZ를 덮어쓰지 않고 새 이름을 요구한다.
- 위치/행정경계 요청은 전체 20초, 참조지도 요청은 각 15초 제한과 취소를 적용했다. 토양·지질·수계 및 고지형의 토양 준비는 QgsTask에서 수행하고 GUI에는 값과 파일만 전달한다. 종료·조사 전환 뒤 늦은 결과는 적용하지 않는다.
- 설치 QGIS 4.3 `QgsArchive::zip`의 공통 `qgis-project-XXXXXX.zip` 경합을 확인했다. 앱은 Qt/QGIS 생성 전 프로세스별 TEMP/TMP를 분리하며, 생성된 래스터의 저장 후 참조를 보존하려고 자동 삭제하지 않는다. CTest도 각 테스트별 임시 폴더를 사용한다.
- 꼭짓점 이동의 저장 실패는 편집 버퍼를 유지하고 한국어로 알린다. 화면 CRS와 레이어 CRS가 다를 때 좌표 변환도 보완했다.
- 최종 Release 빌드, CTest **17/17 (152.32초)**, 시작 스모크, portable 배포가 통과했다. Release/portable SHA256은 `B4FC088F23430D42B248C04060FCC80EFF6321FDB8326F205E6587D0ECF24FBC`로 일치한다. 실제 portable 화면 확인과 재측정 결과는 Phase 1 보고서에 마무리한다.
- Phase 1 산출물: `build/qa/production-phase1/`. 기존 미커밋 변경과 원본 조사 자료를 보존했다. 커밋하지 않았다.

### New survey work CRS verification (2026-09-08)

- Fixed the stale work-CRS status chip on new survey creation and saved-project reopen. The chip now follows the canvas destination CRS change signal; a selected CRS is committed to the session only after survey creation succeeds.
- Verified both EPSG:5186 and EPSG:5187 through the real portable application's new-survey dialog, save, and explicit reopen. Screenshots: `build/qa/crs-selection/{created,reopened}-{5186,5187}.jpg`.
- Regression tests assert project CRS, canvas CRS, GPKG layer CRS, manual status-chip switching, reopen, and the invariant submission chip 5179.
- Release build and isolated smoke passed (exit 0). Final CTest: 14/14 passed, exit 0, 167.16 seconds. Corrected two existing test fixtures (hidden canvas output-size initialization; actual polygon required for composed layout) without relaxing their checks or changing product GIS/layout behavior.
- Published portable executable matches the Release SHA256. Original field data and the geology 80 km / river 160 km ranges were not changed. No commit.

The current requested startup behavior is **home screen only**. On launch, ka-hgis opens the application shell and waits for the user to choose **조사 열기**, a recent survey item, or **새 조사**.

Automatic startup restore is intentionally disabled. Do not reintroduce automatic LayersOnly restore, last-project restore, drawing restore, basemap restore, WMS/XYZ restore, or background project loading on app startup.

The previous automatic restore path loaded 31 layers, including a 390k-lot cadastral layer, and missed the aerial imagery. The measured startup/log delay included survey opening work that should happen only after the user explicitly opens a survey.

Current implementation direction:

- Constructor-time map/background initialization and scheduled last-survey restore were removed or disabled by default.
- Explicit open paths remain valid: **조사 열기**, recent survey click, and **새 조사**.
- Do not modify original survey data during startup.
- Do not commit unless the user explicitly asks.

Validation expected for future C++ startup/UI work:

- configure/build Release with the repo PowerShell commands;
- run `ctest --test-dir build -C Release --output-on-failure`;
- run `.\scripts\run-ka-hgis.ps1 --smoke-quit`;
- run `.\scripts\publish-desktop.ps1` for field-facing UI changes.

Docs/settings-only work does not require rebuilding the C++ application.

## Recent Product Notes To Preserve

- 안동 뷰어 work is separate from the main ka-hgis product. Do not fold it into `MainWindow` unless explicitly requested.
- VWorld keys remain local only. Do not hardcode a production key.
- `removeAllMapLayers()` remains forbidden in survey-load flows.
- Work CRS may be EPSG:5186 or EPSG:5187; upload/export output stays EPSG:5179 SHP + PDF + MANIFEST.
- Domain layers appear in the legend only after draw/import/open of actual user data.
- Reference maps stay separate from survey data.

## Harness Migration

This project now treats `AGENTS.md`, `.codex/NOW.md`, `HANDOFF.md`, and `docs/HANDOFF.md` as the active Codex-native guidance surface.

Legacy `.grok`, `.cursor`, `.agents` dispatch/history files, and Orca files may remain for history or compatibility, but they are not the active preset source for new Codex work. The project-local `.agents/skills/ka-hgis-gis/SKILL.md` is the maintained Codex GIS specialization. Do not add new project-level Grok, Antigravity, Cursor-only, or old fixed model routing requirements unless the user explicitly asks for that compatibility layer.

## Project-local GIS setup (2026-09-08)

General C++/GPT-6 settings remain global. HGIS GIS instructions and the `ka-hgis-gis` skill live only in this repository. The skill links GIS evidence, source areas and relevant regression tests. Historical C++17/LTR and 5179 schema notes are distinguished from the current toolchain and survey CRS selection. This is a documentation/skill configuration change, not a product behavior change or new application test run.
