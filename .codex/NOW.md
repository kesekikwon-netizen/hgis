<!-- Recent window only. Older entries: docs/archive/NOW-before-2026-09-25.md -->
## 2026-09-25 P4-2 오류 메시지 표준

- `KaUserError` what/why/how + 선택 해결 단추. 상위 20곳 적용. 전후표 `docs/user/error-message-standard-p4-2.md`.
- 시험 `user_error` 3개. 지적 본번/부번은 루트 유지. removeAllMapLayers·VWorld 키 하드코딩 없음.

## 2026-09-22 지도는 한 번만 그린다

- 조사선·유적은 덧그림에서 한 번. 지적 지번은 본 화면. 번호만 바뀌면 `ka_map_numbers`만 갱신. 범례 공간 검사는 범위·지질·토양이 바뀔 때만.
- CTest layer_state_regressions 7.52s, heritage_style 14.49s, above_labels 1.51s. smoke 0. clangd L2517 col 33 → LayerOps.h L104, diagnostic 0. Archify app-paint-memory 9/9.
- 커밋·푸시 없음. 앱은 다시 열어야 반영.

## 2026-09-22 조판 페이지가 비는 경우

- `ensureLayoutPage`가 없는 용지를 다시 만든다. 화면 밖이면 맞춘다. `layoutRegainsPageWhenTheSheetHasNone` 통과. smoke 0.

## 2026-09-22 다른 PC용 포터블

- 바탕화면 `HGIS-포터블-개인-20260922`. EXE D1B5366F8D8329DB508C82596A94FB76DB594F743D764BED77566F7C7C50D761. 약 1036 MB.
- 계정은 폴더 config의 password=. DPAPI 아님. verify 0. SDK PATH 없이 smoke 0. 미서명. 커밋·푸시 없음.

## 2026-09-22 시작 안내 5초·번호 간격

- 시작 왼쪽 그림은 창을 한 번 그리고 멈춘다. 5초. `ReadingDurationMs` 5000.
- 조판 번호 중심 간격 4.6mm, 여섯 링. 겹치면 유적 위로 되돌리지 않는다.
- CTest startup_splash 11.09초, heritage_style 11.98초. smoke 0. 커밋·푸시 없음.

## 2026-09-22 개발설정 점수 근거

- runner `ka-hgis-pc` online. `ENABLE_SELF_HOSTED_BUILD=true`.
- `heritage_style` 재빌드 후 Passed 13.64초. 아침 7개 실패는 옛 테스트 exe였다.
- 설정 파일은 `.cursor/`, `.vscode/settings.json`, tidy·worker 스크립트.

## 2026-09-22 My Machines worker ka-hgis-pc

- `scripts/start-cursor-worker.ps1`. 시스템 Node 22로 `A:\qgis`에 연결. https://cursor.com/docs/cloud-agent/self-hosted/my-machines
- 번들 Node 24는 better-sqlite3 ABI가 안 맞는다. 로그오프하면 worker도 끊긴다.

## 2026-09-22 클라우드 에이전트는 Ubuntu

- `.cursor/environment.json` + Dockerfile. https://cursor.com/docs/cloud-agent/setup
- OSGeo4W 빌드·CTest·smoke는 Windows. 클라우드에서 그 통과를 말하지 않는다.
- 훅은 `node .cursor/hooks/dev-loop.mjs`. 커밋·푸시 없음.

## 2026-09-22 Cursor 기본은 Grok 4.7

- 고정 모델 Grok 4.7. Auto·Fast 아님. https://cursor.com/docs/models
- 사용자 Cursor에서 Copilot·Claude Code·Gemini·Tabnine 설정, Orca `hooks.json`, `build/`용 Ninja CMake 설정을 지웠다.
- 워크스페이스는 `cmake.useCMakePresets: always`, `configureOnOpen: false`. 커밋·푸시 없음.

## 2026-09-21 복구사본 창 반복 제거

- 스크린샷 `저장 실패` + `D:\회사작업-QGIS 작업\저장경로\복구사본\…` 은 예전 「사용 중」 저장이다. `…-이동` 포터블에는 수정 없음.
- 2분 백업은 폴더에만 남긴다. 상태줄·시작 「복구 사본이 있습니다」 창은 끈다. pending 은 시작 때 지운다.
- Graft `offerRecoverySnapshot`. clangd L1105 → clearRecoveryOffer L1047. storage_safety 0. Archify 9/9 visual-check pass.
- 바탕화면 `HGIS-포터블-개인-20260921-복구` EXE 852434B7D5E2C39E1368D0F7D7B4FCE033B0B7963D8501E3744EA896C8EC68EC. verify 0. smoke 0.
- 커밋·푸시·실행 중 앱 없음.

## 2026-09-21 다른 PC 저장·복구사본 창

- `copySurvey`가 대상 `-wal`만 보고 「사용 중」실패 → 복구사본 대화상자. 다른 PC의 `D:\회사작업-QGIS 작업\저장경로\복구사본\…` 이 그 창이다.
- 저널을 지우고 교체한다. 잠기면 `-저장.gpkg` 또는 fallback. `persistWorkspace_staleWalDoesNotBlockSave` 0. 잠긴 원본 테스트는 옆 파일 성공으로 바꿈.
- Graft `copySurvey` L418. clangd L418 → SurveyStorage.h L34. diagnostic_error_count 16(QGIS 헤더).
- Archify survey-save-open-lock 9/9, visual-check 1440–2048 pass.
- 바탕화면 `HGIS-포터블-개인-20260921-저장`. EXE SHA256 C83523E0EF2BF206124A1BE8F3CFEE5A215A9966868DBCB06501432A47350C56. verify-portable-pack 0. 계정 password= (DPAPI 없음). 기존 `…-이동`은 이 수정 없음.
- 커밋·푸시·실행 중 앱 없음.

## 2026-09-21 개인 포터블(다른 PC)

- 바탕화면 `HGIS-포터블-개인-20260921-이동`. EXE SHA256 CEDCB7BDC05324E137DDDD9B7CC8B7D12AD6F0797B5FDFC87B44E984E1744195. 기존 `HGIS-포터블-개인-20260921`·`…-5km` 유지.
- VWorld·1919 HistoryGis ini hash 원본과 일치. 지형도·유산·지적은 password= 로 풀어 실음. DPAPI 없음. 값 미기록.
- verify-portable-pack 0. OSGeo4W PATH 없이 smoke 0. SDK A:\OSGeo4W. 약 1013 MB. 커밋·푸시·사용자 앱 없음.

## 2026-09-21 포터블을 다른 PC에서도 같게

- 비밀번호는 설치본만 DPAPI. 포터블은 `password_portable`이라 폴더 복사로 로그인된다. https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
- 원래 `D:\…` 조사 파일이 없으면 쓸 수 있는 폴더에 다시 저장한다. 원본은 덮지 않는다.
- `make-portable -IncludeLocalCredentials`가 이 PC DPAPI를 풀어 config에 넣는다. 값은 로그에 안 남긴다.
- CTest: topographic_settings 2슬롯 0, heritage_flow 계정 0, storage_safety 경로 2슬롯 0. smoke 0.
- Graft `writableSurveyPath` L243. clangd writePassword L115 → KaSecretStore.h L17. Archify portable-any-pc 9/9, visual-check 1440–2048 pass.
- EXE SHA256 CEDCB7BDC05324E137DDDD9B7CC8B7D12AD6F0797B5FDFC87B44E984E1744195. 커밋·푸시·포터블·사용자 앱 없음.

## 2026-09-21 맵·조판 휠 한 칸 1.2배

- 맵 QGIS 기본 2.0, 조판 1.35를 `LayerOps::kWheelZoomFactor` 1.2로 맞춤. 부팅 `qgis/zoom_factor`, 메인·정합·지형 미리보기 `setWheelFactor`, 조판 휠 같은 칸.
- 공식: https://qgis.org/pyqgis/master/gui/QgsMapCanvas.html (`setWheelFactor` > 1)
- CTest: `wheelZoomFactorIsFinerThanQgisDefault` 0, `layoutWheelZoom_keepsPointUnderCursor` 0. 전체 layer_state/workflow는 기존 flake(임시폴더·유산받기·perf 0.555ms). smoke 0.
- Graft `applyWheelZoomFactor` L2451. clangd L2454 col 13 → qgsmapcanvas.h L643; L2451 col 16 → LayerOps.h L41. diagnostic_error_count 14(QGIS 헤더).
- Archify wheel-zoom-step validate/deliver 9/9, visual-check 1440–2048 pass.
- EXE SHA256 B12AE2AC0ABD266D606D5ED5D19444C18CF142745A8C15144B2E6B58683683B3. 커밋·푸시·포터블·사용자 앱 없음.

## 2026-09-21 조판 주변유적 전체·덧지도 동기

- 주변유적은 아래 글자가 없어도 덧그림에 모은다. 본지도에도 남겨 여섯 자료 도형·범례가 빠지지 않는다. 번호는 이름당 하나, 형제 도형은 그대로.
- `applyLayersToMap`이 같은 호출에서 `syncAboveLabelsMap`. 번호 지도는 본지도+덧지도 레이어.
- CTest: layer_state 6.40s, heritage_style 15.19s, save_open_drawing 22.17s. smoke 0.
- Graft `numberedSourceLayers` L268–L284. clangd `raiseAboveGeometries` L1021. Archify sheet-overlay-stable validate/deliver 9/9, visual-check 1440–2048 pass.
- EXE SHA256 6F49FF9023F2E0292C0A9A76BBB4F8686D391A305ED65E056803EEEEF75182E2. 커밋·푸시·포터블·사용자 앱 없음.

## 2026-09-21 지적도 Delete·우클릭 삭제

- 지적도 레이어·묶음은 Delete와 우클릭 삭제로 목록에서 뺀다. 지운 뒤 자동 VWorld 지적을 다시 올리지 않는다.
- layer_state 6.01s PASS. 지적 삭제 3테스트 PASS. save_open_edit 전체는 글자크기 메뉴 flake.
- Graft `rememberUserRemovedCadastral` L1420. clangd L1430 col 33 → h L352.
- Archify cadastral-delete validate/deliver 9/9, visual-check 1440–2048 pass.
- EXE 510B713765B3B9CD0222CDAF93FAC26B9C1ACA2A818BCB3570F3B44D8B09C490. smoke 0. 커밋·푸시 없음.

## 2026-09-21 개인 포터블(5km 빌드)

- `C:\Users\kwonyoungin1\Desktop\HGIS-포터블-개인-20260921-5km`. EXE 863847A3B96FD775AC58C8CDF36ECD61D0777307E7E2267CC6D7FFA570AC1B5C.
- 기존 `HGIS-포터블-개인-20260921` 유지. VWorld·1919 HistoryGis·지형도·유산·지적 계정 hash 일치. 값 미기록. SDK A:\OSGeo4W. verify 0, SDK 없이 smoke 0. 커밋·푸시 없음.

## 2026-09-21 조사구역 5km 클립·명칭 축척·조판 한 번

- 받은 뒤 조사구역 5km만 올린다(지적은 기존). 맵 명칭 1:10000 제한. 조판 본지도에서 덧그림 도형 제외.
- CTest 7종 통과. smoke 0. clangd L75→h L23. Archify survey-5km-clip 9/9.
- EXE 863847A3B96FD775AC58C8CDF36ECD61D0777307E7E2267CC6D7FFA570AC1B5C. 커밋·푸시 없음.

## 2026-09-21 개인 포터블(키 포함)

- `C:\Users\kwonyoungin1\Desktop\HGIS-포터블-개인-20260921`. EXE D80E554E0463AF3A93844AB179497A44C8CA404804517D923432C7E8D7C8D483.
- VWorld·1919 HistoryGis·지형도·유산·지적 계정 파일 hash 일치. 값 미기록. verify 0, SDK 없이 smoke 0. 커밋·푸시 없음.

## 2026-09-20 바탕화면 포터블

- `C:\Users\kwonyoungin1\Desktop\HGIS-포터블-20260920`. EXE SHA256 D80E554E0463AF3A93844AB179497A44C8CA404804517D923432C7E8D7C8D483.
- verify-portable-pack 0. SDK PATH 없이 smoke 0. 계정·키 미포함. 기존 바로가기 유지. 커밋·푸시 없음.

## 2026-09-20 전체 끄기 후 개별 체크가 지도를 켠다

- 전체 끄기는 그룹까지 끈다. 자식만 체크하면 체크만 되고 `isVisible()`은 거짓이다. 체크된 노드의 조상을 연다.
- RED `checkingLayerAfterAllOffShowsOnMap` 실패 → GREEN. CTest layer_state 3.69초. smoke 0.
- Graft `revealCheckedLegendNode` L2217. clangd L2217 col 16 → h L237.
- Archify legend-check-shows-map validate/deliver 9/9, visual-check pass.
- EXE SHA256 D80E554E0463AF3A93844AB179497A44C8CA404804517D923432C7E8D7C8D483. 커밋·푸시·포터블 없음.

## 2026-09-20 끈 지적도는 다른 지도를 받아도 꺼진 채

- 자석 갱신이 꺼 둔 지적을 다시 켜지 않음. CTest workflow 66.10초. smoke 0.
- Graft `placeCadastralLayer` L1266. clangd → h L329.
- EXE SHA256 6722A79AE3B44E1027D1CEAEFF4068C730813B6AF4B0820CDB230F1B29E0E8A7. 커밋·푸시·포터블 없음.

## 2026-09-20 주변유적 깜빡임·줄자 겹침

- 유적 도형은 덧그림에 다시 올림. 줄자는 투명도 막대 오른쪽. smoke 0.
- CTest layer_state 6.25초, heritage_import 7.50초, workflow 69.76초.
- EXE SHA256 CB17CA041C2B9AA28CD3E0F4617D76F33966B133D019866D699A1501B03A2FC7. 커밋·푸시·포터블 없음.

## 2026-09-20 번호 선은 같은 점만 짧게

- 근처 유적은 각자 위. 같은 점만 2.4mm 한두 칸. CTest heritage_style 12.93초.
- Graft `stackedPins` L155. clangd L166 col 12. smoke 0.
- EXE SHA256 08FED8B73186F20FBF49AEB77A8E89A4C0D060F448DD182FF5FA7A33165DE114. 커밋·푸시·포터블 없음.

## 2026-09-20 조판 렉·A4 세로 기본

- 기본 용지 A4 세로. 덧지도 재페인트 생략. 번호 범례는 다음 틱.
- CTest save_open_drawing 20.70초. smoke 0.
- Graft `applyHeritageNumberChrome`. clangd L3303 → h L150.
- EXE SHA256 1991346D7F96B3ED81DCA6658550A4BD828099DA9A33A5F02656B7121CFB5816. 커밋·푸시·포터블 없음.

## 2026-09-20 조판 여백 1cm

- 위·좌·우 10mm. 전면 칸은 조판을 열 때 맞춘다. 아래 축척 띠는 그대로.
- CTest save_open_drawing 23.04초. smoke 0.
- Graft `defaultMapRect`. clangd L1707 → h L69.
- EXE SHA256 A0D6468406344A3F74BFE168E95595135C6386697A8748375450973E9A4A2843. 커밋·푸시·포터블 없음.

## 2026-09-20 현장 조판 317×220

- A4 가로 297×210을 좌·우·위 1cm 키워 317×220. 저장된 시트는 조판을 열 때 키운다.
- CTest save_open_drawing 20.78초. smoke 0.
- Graft `applyFieldPageGrow` L917. clangd L917 → h L70.
- EXE SHA256 7500A6C541F12D3A62A2BD9932DF22047573DB13419AF1400B75AA87F7C63001. 커밋·푸시·포터블 없음.

## 2026-09-20 범례 번호가 지도와 같게

- 단색 원본 노드에 뱃지를 먼저 붙이면 모든 줄이 1이 된다. override 노드를 만든 뒤 다시 찍는다. 번호 갱신마다 범례 트리를 갈아엎지 않는다.
- CTest heritage_style 12.93초, save_open_drawing 20.31초. smoke 0.
- Graft `applyLegend` L1036. clangd L1036 → HeritageLayoutNumbers.h L37.
- EXE SHA256 A194D1F3543323A4494FDD1A3893ACE1A6F2B56954794E3DD1F61C1FDD103F7C. 커밋·푸시·포터블 없음.

## 2026-09-20 용지 이동은 클론 재사용

- 발자국만 바뀌면 `update()`가 레이어 clone·전체 심볼 순회를 하지 않는다. 원본 분류표에서 페이지 범주만 그린다. 원본에 `startRender` 없음.
- CTest heritage_style 11.79초, save_open_drawing 19.61초. smoke 0.
- Graft `update` L636. clangd L636 → HeritageLayoutNumbers.h L34. Archify validate/deliver 9/9. visual-check 1440 높이 넘침 미완.
- EXE SHA256 84B02ECA3E3A8850B9B18055C91E69792E2BAB737E89DB121DEC33E04C422B64. 커밋·푸시·포터블 없음.

## 2026-09-20 덧그림은 조사·지적만

- 2차 패스에서 `isReferenceLayer` 제외. 조사 선은 지번 위. CTest layer_state 4.97초, workflow 67.28초. smoke 0.
- EXE SHA256 BCB46FF3ECB6F861307BE819503C9CE7BFE63749B50C9DBD84933022676E7C65. 커밋·푸시·포터블 없음.

## 2026-09-20 조판 번호 PAL 한 번

- 본 지도·덧지도는 `labelsEnabled=0`. PAL은 `ka_map_numbers`만. 번호는 도형 위. 숨긴 레이어는 번호 지도에서 뺀다.
- CTest heritage_style 12.68초, save_open_drawing 20.42초. smoke 0.
- EXE SHA256 540A3F20D8BF851BC722F6E9DC7B8AA87F5120C75343ED8B5E4B0CF363971FC9. 커밋·푸시·포터블 없음.

## 2026-09-20 용지에서 밀면 발자국으로 번호 다시 잡음

- `sizePositionChanged` → `update()`. 테스트는 남은 유적 수. CTest drawing 19.96초, heritage_style 11.81초.
- smoke 0. EXE SHA256 BE79823D68804E4F60C938D31B94A80DB53B95A3278515E9F60DB9FFF54BC694.
- 커밋·푸시·포터블 없음.

## 2026-09-20 번호 30%·검정 테두리·도형 위

- 2.99mm / 5.2pt, 검정 0.15mm, `ka_map_numbers` 최상. CTest 12.63초. smoke 0.
- EXE SHA256 D3A2CF8F896D0FF960C4583AEB0AE007FDC47CA0ED0A8441692951F8CDC871FD.

## 2026-09-20 조판 번호 절반·선은 겹칠 때만

- 원 2.3mm / 4pt. 겹칠 때만 짧은 선. CTest 11.05초. smoke 0.
- EXE SHA256 A728A45BE3D17928E39A7A2D0E6C095C8FE64F4F96A3B5B11321E268C1C4E06F.

## 2026-09-20 조판 번호 전부 비키고 선

- 모든 번호 7mm 이탈 + 콜아웃. CTest heritage_style 11.53초. smoke 0.
- EXE SHA256 D6F2314BF7F6AA2A8C8EC9F99B8A0601D1657EB70B5873AA43C62BE285C06BE7.

## 2026-09-20 완료 계약 skill·설계도

- `.agents/skills/ka-hgis-sheet/` + `docs/architecture/ka-hgis-sheet-contracts.workflow.json`.
- 커밋·푸시·포터블 없음.

## 2026-09-20 페이지 안 주변유적 전부 표시

- 페이지에 걸린 점은 번호를 유지. 겹쳐도 남김. 면은 중심 점.
- CTest heritage_style 10.75초. smoke 0.
- EXE SHA256 011CC0586578A06011857486A1BF9ACDAB3E4297F5BB95F8D028C8671D6DF80C.
