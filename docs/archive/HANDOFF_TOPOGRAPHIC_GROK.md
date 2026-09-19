# Grok 4.6 인계 — 수치지형도 작업만

작성: 2026-09-10. 작업본: `D:\hgis`. 이 문서는 수치지형도 후속 수정의 인계 자료다. DEM·지질도·다른 기능으로 범위를 넓히지 않는다.

## 사용자가 해결을 요청한 현재 문제

1. 다운로드 위치는 **현재 지도 화면 중심 반경 10km**여야 한다. 이전 조사구역 위치에 고정되면 안 된다.
2. 새 조사구역에서 수치지형도를 받으면 완료까지 진행되지만, 잠시 후 심하게 느려지며 지도들이 화면에서 사라진다.
3. 사용자는 **완료 후 아무 조작 없이도 사라진다**고 확인했다. 사용자 조작 탓으로 판단하지 말 것.

**현재 상태(2026-09-10 위치도 분류):** 적재는 등고·도로·수계·경계·건물·표고만. 식생·시설·지명·미분류는 안 올림. `scaleChanged`로 분류를 갈아끼우지 않음. EXE `CB8D5E4C00E0F6FAB1AD479A488928DAC34EEC9B234D8C0E57105DD08A050265`.

**직전(2026-09-10 10km 적재):** 받기 10km는 유지한다. 조사 줌에서 이웃 도엽이 안 올라가던 원인은 적재 질의가 화면 상자만 본 것이다. 지금은 `coverageBounds`(화면 ∪ 중심 ±10km 정사각). 광역 19초·현장 무조작 소실은 **미해결**. EXE `8DD7AA6C0C7B6FEBCB5A5EEBE5B51FBC7DD1AB64A90A3D7C9A80F6353F77050D`.

## 유지할 사용자 요구

- 공식 로그인 → 여러 도엽을 한꺼번에 체크 → 신청·동의 → 다운로드 → 변환 → 지도 적재가 앱 자체 코드로 동작한다. 실행 중 GPT/Codex/MCP나 화면 좌표 클릭에 의존하지 않는다.
- 공식 웹사이트 화면은 숨기고 단계, 실제 전송량, 완료 개수, 실패·취소 이유를 표시한다.
- 반경은 **10km 고정**. 과거 20km 및 잘못 입력한 0.5/5는 현재 요구가 아니다.
- ID/비밀번호는 더보기의 VWorld API 키 아래 계정 설정에서 변경한다. 개인 계정을 소스·문서에 하드코딩하거나 로그에 출력하지 않는다. 계정 변경 시 이전 세션을 폐기한다.
- 저장 경로는 **현재 조사 폴더/지형도/{원본,SHP,index,receipts}**. 받은 DXF 원본을 보존하고 SHP로 준비한다.
- 지도는 회색 `#808080`, 선폭 `0.2mm`, 읽기 전용 **참조 지도**다. 조사 데이터와 분리한다.
- 작업 CRS 5186/5187, 제출 5179 규격을 유지한다. 좌표계가 불확실한 원본을 조용히 올리지 않는다. QGIS/PROJ/GDAL을 재구현하지 않는다.
- 시작할 때 조사·지도 자동 복원 금지. 원본 조사 파일을 수정하지 않는다.
- **커밋·푸시·포터블·publish·dist/L: 복사 금지**. 사용자가 별도로 요청할 때만 한다.
- 사용자가 실행 중인 앱을 종료·재시작·조작하지 않는다. 정상 진입점은 바탕 화면 **고고학 전용 HGIS** → `scripts/start-ka-hgis.vbs` → `launch.ps1` → `build/Release/ka-hgis.exe`다.
- 기존 미커밋 작업이 많다. 전체 reset/revert/clean으로 되돌리지 않는다. 현재 dirty인 dist 실행 파일은 이번 수정 이전부터 있던 것이다.

## 이전에 끝낸 다운로드 구현

공식 INNORIX HTML5 최종 다운로드를 연결했다. 도엽마다 별도 로그인·신청하지 않고 `downloadMapData`에 미보관 도엽 배열 전체를 전달한다. 각 파일 전송은 Qt 저장 완료 뒤 이어간다.

이전 검증에서는 제주 반경 10km의 3도엽/DXF+XML 6파일을 실제 수신했고, 한림·귀일을 SHP 33개/213,809피처로 변환·적재했다. 도두는 도곽만 있어 검토 대상으로 보관한다. 도곽과 공식 색인이 어긋난 귀일은 원본 도곽 근거로 CRS를 검증했으며 좌표를 임의 이동하지 않았다. 상세: `build/qa/topographic-complete/REPORT.md`.

이전 전체 CTest 37/37 및 Release 검증은 **아래 최신 안정화 변경 이전 결과**다. 현재 변경까지 통과한 것으로 인용하면 안 된다.

## 이번에 확인하고 수정한 내용

### 현재 화면 중심이 무시되는 원인 — 수정 및 회귀 검사 완료

`src/app/KaTopographicScopePanel.cpp`의 `refreshScope()`가 화면 중심을 구한 뒤 조사구역의 범위 중심으로 덮어썼다. 이를 제거하여 `m_canvas->extent().center()`를 사용한다. 표시 문구도 현재 화면 중심 반경 기준으로 고쳤다.

`tests/test_topographic_scope.cpp`의 `currentViewportWinsOverExistingSurvey`는 제주 조사구역을 남겨 둔 채 서울·강릉·제주로 화면을 이동시킨다. 5186/5187에서 수정 전 실패, 수정 후 통과를 확인했다.

### 적재 중 반복 재그리기 — 일괄 등록으로 수정

`src/app/KaTopographicImportDialog.cpp/.h`는 이전에 파일마다 QgsProject에 등록했다. 실제 안동 7도엽은 **122개 레이어**여서 레이어/범례 콜백과 글자 덧그림의 전체 재그리기가 반복됐다.

현재는 타이머 틱마다 레이어를 준비하여 `std::unique_ptr<QgsVectorLayer>`로 보관하고, 준비가 끝나면 `publishPrepared()`에서 `QgsProject::addMapLayers()`로 한 번에 등록한다. 준비 중 기존 지도를 유지한다. 참조 역할·스타일·읽기 전용 설정은 등록 전에 적용한다.

- `m_prepared`, `PreparedLayer`, `m_coverageGeneration`을 추가했다.
- 범위 변경, 프로젝트 clear, 캔버스 파괴, 자동 적재 취소 시 준비 중인 낡은 레이어를 폐기한다.
- 독립 리뷰에서 자동 적재 취소가 이미 준비된 수동 레이어까지 멈추게 하는 결함을 지적했다. 두 큐를 구분하여 수정하고 테스트를 추가했다.
- `addMapLayers`의 기본 범례 등록을 유지한다. `addToLegend=false`로 바꾸면 기존 동작을 깨뜨린다는 테스트 결과가 있었다.
- 진행 문구는 실제 등록 전이므로 “지도 레이어 …개 준비”라고 표시한다.

수정 파일은 위 두 구현 파일과 헤더, `tests/test_topographic_import.cpp`, `tests/test_topographic_scope.cpp`, `tests/test_save_open.cpp`다. 대다수 수치지형도 파일은 앞선 작업부터 untracked여서 일반 git diff만으로 전체 구현이 보이지 않는다.

## 확인한 검사와 남은 문제

근거 폴더: `D:/hgis/build/qa/topographic-stability/`.

| 검사 | 실제 결과 | 로그 |
|---|---|---|
| 현재 화면 중심 5186/5187 | 수정 전 실패 → 수정 후 통과 | `scope-red.txt`, `scope-green.txt` |
| 수치지형도 범위 전체 집중 검사 | 18 통과, 1 skip | `scope-all.txt` |
| 등록/취소/프로젝트 clear/범위 변경/수동·자동 혼합 | 18 통과 | `import-final.txt` |
| 실제 MainWindow, 안동 7도엽 122레이어 | 등록 1.538초, 1회 일괄 추가; 35초 무입력 동안 추가 재그리기 0회 | `window-after.txt` |
| 실제 조사 파일 복사본, 조사 위치 1:1219 | 초기 적재 2.777초; 15개 지형도+기존 4개; 35초 무입력 동안 ID·체크 유지, 늦은 렌더 1회 | `field-final.txt` |
| 위 복사본에서 7도엽 전체로 축소 | **80.862초 지연 — 미해결**, 지형도 122개 | `field-final.txt` |
| 조사 위치로 복귀 | 2.773초, 지형도 15개, 기존 조사 레이어 유지 | `field-final.txt` |

캡처:
- `build/qa/topographic-stability/field-final/actual-window-idle.png`
- `build/qa/topographic-stability/field-final/actual-window-navigation-0.png`
- `build/qa/topographic-stability/field-final/actual-window-navigation-1.png`
- `build/qa/topographic-stability/andong-after/actual-window-idle.png`

화소 검사는 자체 QA 창에서 수행했다. 테스트 배경지도는 통신을 분리한 흰색 fixture이므로 실제 위성영상과의 정합 검증으로 표현하지 않는다. 원본 조사 GPKG 바이트 불변은 검사했다.

주의: `window-baseline.txt`는 초기 테스트의 화면 범위/판정 결함 때문에 유효한 기준 비교가 아니다. `field-after.txt`도 잘못된 전체 범위에서 중단한 검사다. 최종 `field-final.txt`를 사용한다. 수정 전 실제 7도엽 적재는 CPU 약 429초 동안 끝나지 않아 자체 QA 프로세스를 중단했다. 완료된 전후 시간 비율을 만들면 안 된다.

## 2026-09-10 받은 3장 vs 범례 1장

증상: 반경 10km로 3장을 받았다고 나오는데 레이어에는 한 장만 보인다.

원인: `KaTopographicScopePanel` 상태의 N장은 `prepared.available`(보관·준비 도엽). `updateCoverage`는 예전엔 `m_canvas->extent()`만 `query`해서 조사창(~1:1200, 수백 m)과 겹치는 한 도엽만 올렸다. 축척 필터는 분류만 줄이고 도엽을 지우지 않는다.

수정:
- `TopographicCatalog::coverageBounds` — 유한한 화면과 중심 ±10km 정사각의 union. 빈 화면은 그대로(0,0 오탐 방지).
- `KaTopographicImportDialog::updateCoverage`가 이 상자로 `query`.
- 상태 문구: 「도엽 N장 보관. 화면 중심 반경 10km에 겹치는 SHP를 올립니다」.

검증(이번 턴, 전체 37/37 아님):
- `coverageBoundsGrowsSurveyViewToTenKilometers` 0
- `tenKilometerCoverageLoadsNeighborSheetNotFarSheet` 0 (8km mid 적재, 20km far 제외, 100km 팬 언로드)
- ctest `topographic_catalog` 0 (4.41s), `topographic_import` 0 (18.18s / 재검증 16.85s), `topographic_scope` 0 (8.29s)
- unlazy `.unlazy/topo-three-sheets` 4/4 met
- `ka-hgis.exe` SHA256 `8DD7AA6C0C7B6FEBCB5A5EEBE5B51FBC7DD1AB64A90A3D7C9A80F6353F77050D`

제주 도두(도곽만)는 `retainForReview`라 자동 적재되지 않는다. 3장 수신이면 한림·귀일만 올라갈 수 있다.

## 2026-09-10 축척 필터 후속

`TopographicCatalog::visibleAtScale` / `query(..., scale)`: 축척 분모가 25000보다 크면 Contour·Road·Water·Boundary만 맞춘다. `updateCoverage`는 `m_canvas->scale()`을 넘긴다. 그리는 중이고 캔버스에 있는 레이어를 지울 때만 `stopRendering`한다. 화면 밖 언로드/재적재와 일괄 `addMapLayers`는 유지한다.

안동 재측정 `build/qa/topographic-stability/field-scale.txt`, 캡처 `next-run/`:

| 단계 | 결과 |
|---|---|
| 조사 1:1219 | 1.124초, 지형도 15, 기존 4 보존, 추가 0(채택) |
| 35초 무입력 | 레이어·체크 유지, 늦은 렌더 1회 |
| 7도엽 축소 1:227884 | **19.044초, 55레이어**, 회색 화소 190800 |
| 조사 복귀 | 1.583초, 15레이어, 기존 조사 레이어 유지 |
| 원본 GPKG | 바이트 불변 |

이전 field-final 축소 80.862초/122레이어와 비교한다. 19초는 남은 지연이다. 다음 후보는 overview에서 등고 라벨을 끄는 것이다. 병렬 렌더는 켜지 않는다.

무조작 소실: 이 QA에서는 지도가 비지 않았다. importer·field 통과를 현장 소실 해결로 말하지 말 것.

Release EXE SHA256 `1A36F5D488D8344E3A31EA2386519A6C10C147EF6E48D1764CDBE7A9788754E2`. CTest 37/37(220.87초), smoke-quit 0. 커밋·포터블·publish 없음.

## 이어서 할 일

1. 광역 19초가 현장에서 여전히 느리면 overview 등고 라벨만 끈다. 언로드를 없애거나 레이어를 더 줄이기 전에 재측정한다.
2. 사용자 앱에서 완료 후 무조작 소실이 남으면 다운로드 직후 화면 축척·`stopRendering`·범위 감시 로그를 QA 이벤트와 구분해서 본다.
3. 커밋·포터블·publish는 사용자가 요청할 때만.

## 실행 파일과 환경

Windows PowerShell, C++20/MSVC, Qt 6.11.1, 설치 QGIS-dev 4.3 Master, OSGeo4W `D:\OSGeo4W`. AGENTS와 로컬 QGIS 3.44 매뉴얼 및 설치 SDK 근거를 따른다.

10km 커버리지 적재는 `build/Release/ka-hgis.exe`에 반영됐다. SHA256:
`8DD7AA6C0C7B6FEBCB5A5EEBE5B51FBC7DD1AB64A90A3D7C9A80F6353F77050D`

`build/release-verified.json`은 이번 CTest 37/37·smoke 기록이다. 사용자 앱은 자동 종료하지 않는다.

```powershell
Set-Location D:\hgis
. .\scripts\dev-env.ps1
cmake --build build --config Release --target ka_topographic_import_tests ka_topographic_scope_tests ka_save_open_tests
$env:QT_QPA_PLATFORM='offscreen'
.\build\Release\ka_topographic_import_tests.exe
.\build\Release\ka_topographic_scope_tests.exe
# 소스 확정 후 전체 구성·빌드·CTest·시작 검사 및 현재 해시 기록
.\scripts\verify-release.ps1
```

## 실제 자료를 쓰는 재현 검사

원본 조사: `C:/Users/kyi25/OneDrive/바탕 화면/안동시/새 폴더/안동시.gpkg`

SHP 캐시: `C:/Users/kyi25/OneDrive/바탕 화면/안동시/새 폴더/지형도/SHP`

도엽: 368121, 368081, 368072, 368083, 368074, 368073, 368112. 총 122개 레이어.

`tests/test_save_open.cpp`의 `topographicActualWindowRemainsStableAfterCompletion`은 아래 환경변수가 없으면 skip된다. 원본 GPKG를 임시 폴더로 복사한 뒤 연다. 원본을 직접 열어 저장하지 않는다.

```powershell
$env:KA_HGIS_QA_TOPOGRAPHIC_SHP='C:/Users/kyi25/OneDrive/바탕 화면/안동시/새 폴더/지형도/SHP'
$env:KA_HGIS_QA_TOPOGRAPHIC_SURVEY='C:/Users/kyi25/OneDrive/바탕 화면/안동시/새 폴더/안동시.gpkg'
$env:KA_HGIS_QA_OUTPUT_DIR='D:/hgis/build/qa/topographic-stability/next-run'
.\build\Release\ka_save_open_tests.exe topographicActualWindowRemainsStableAfterCompletion
```

이 검사는 실제 MainWindow와 활성 범위 감시, 35초 무입력 대기, 넓은 범위 이동 후 복귀, 원본·체크 보존과 지도 화소를 검사한다. 공식 사이트 재다운로드 검사는 아니다.

현장 로그 `%LOCALAPPDATA%/ka-hgis/logs/session.log`에는 자체 QA 로그도 섞일 수 있다. 사용자 앱 증상과 QA 이벤트를 구분한다. 계정 설정이나 로그를 무분별하게 출력하지 않는다.
