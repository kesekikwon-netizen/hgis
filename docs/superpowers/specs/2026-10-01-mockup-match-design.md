# 목업 맞추기 설계 (2026-10-01)

## 목표

홈과 지도 화면을 「Strata 개선 목업」 6쪽(홈)·7쪽(지도)처럼 보이게 한다. 사용자 말: 「목업은 지금과 다르다」, 「전혀 다르다 ui 아이콘이」.

다 됐다고 보는 기준은 세 가지다.

1. 1920×1080 창에서 홈과 지도를 찍어 목업과 나란히 놓았을 때, 차이표(`scratchpad\eval\visual-diff-home.md`, `visual-diff-map.md`)의 「다름」·「없음」 칸이 비거나 이 문서의 「그대로 둘 것」으로만 남는다.
2. 기존 시험과 새 시험이 모두 통과한다. 기준선에서 원래 실패하는 4개는 예외다.
3. `--smoke-quit`의 종료 코드가 0이다.

## 사용자 결정

2026-10-01 「추천으로」:

- 레이어 목록 줄은 목업처럼 크게 한다(약 32px). 체크박스를 두고, 글자 칸에는 쓰는 필드 이름(면적·이름·지번·글자)을 적는다. 2026-09-16의 「작은 줄(10px)」 결정은 이것으로 바뀐다.
- 내보내기 마지막 단추 이름은 「검수·제출」을 유지한다. 아이콘만 목업의 상자(box)로 바꾼다.
- 그리기 보조 도구 줄은 유지하고, 모양만 목업 문법(타일 아이콘·같은 글자 크기)으로 맞춘다.

## 그대로 둘 것

- 글꼴은 맑은 고딕이다(2026-09-30, IBM Plex가 흐려 되돌림). 목업이 Plex로 그려진 부분은 크기·굵기·행간만 맞춘다.
- 리본 한 줄, 시작은 홈만, 자동 복원 없음, 「조사 데이터 / 참조 지도」 구분.
- 지도 위 투명도·밝기 카드는 Phase 2 사용자 결정이므로 둔다. 모양만 목업의 흰 카드 문법으로 맞춘다.
- 그리는 동안 인스펙터는 지금처럼 「그리는 동안에는 기록을 고치지 않습니다」를 지킨다. 「그리는 동안 팝업 없음」 규칙과 같은 뜻이다. 다만 아래 배경 지도 카드에 가려 잘리는 것은 고친다.

## 아이콘 체계

### 모양 (목업 7쪽 리본에서 잰 값)

| 부분 | 값 (목업 1440px 화면 기준 화소, 1920 창에서도 같은 화소로 쓴다) |
|---|---|
| 타일 | 둥근 사각형 32×32px, 모서리 반경 8px, 채움 `#E4EAED`, 테두리 없음 |
| 글리프 | Lucide 24×24 선 아이콘을 18px로 그림, 선 굵기 2(24 기준 → 약 1.5px), 둥근 끝·둥근 이음, 색 `#2B4858` |
| 단추 칸 | 너비 = max(40px, 라벨 너비 + 8px), 같은 묶음 안 타일 중심 간격 40px(타일 사이 8px), 묶음 사이 세로 구분선 |
| 라벨 | 타일 아래 6px, 13px, 색 `#202830`. 묶음 이름은 11px, 색 `#5E6670` |
| 고른 도구 | 타일 채움 `#E0ECF8`, 테두리 2px `#1A68B0`, 글리프 `#105088`, 라벨 `#105088` 굵게. 단추 전체 배경은 칠하지 않는다 |
| 저장 안 됨(「저장」) | 타일 채움 `#206CB0`, 글리프 흰색, 라벨 굵게, 타일 오른쪽 위 지름 7px 주황 점 `#F2A33A`(흰 테두리 1.5px) |
| 비활성 | 타일 채움 `#EEF1F3`, 글리프·라벨 불투명도 45% |
| 마우스 올림 | 타일 채움 `#D9E2E7` |

### 그리는 방법

- 새 글리프 모양 `GlyphStyle::Mockup`을 기본으로 한다. 기존 `Outline`·`Tile`은 `KA_HGIS_ICON_STYLE=outline|tile`로 남긴다.
- 글리프는 `data/icons/lucide/<이름>.svg`(ISC, Lucide 1.49.0)와 `data/icons/ka/<이름>.svg`(직접 그린 3개, 같은 규칙)를 `QSvgRenderer`로 그린다. `currentColor`를 상태별 색으로 바꿔 그린다. Qt6::Svg는 이미 링크돼 있다.
- 타일과 점은 `QPainter`로 그린다. 고른 상태·비활성·마우스 올림은 QIcon 모드(Normal/Active/Selected/Disabled)와 On/Off 상태별 픽스맵으로 만든다.
- 고해상도 화면(devicePixelRatio)에 맞춰 그린다.

### 리본 아이콘 대응표 (목업 7쪽 순서)

| 묶음 | 단추 | 아이콘 |
|---|---|---|
| 조사 | 새 조사 · 열기 · 저장 · 다른 이름 | lucide `file-plus` · `folder` · `save` · `save-pen` |
| 기록 | 선택 · 측거 · 그리기 · 시굴격자 · 버퍼 | `mouse-pointer-2` · ka `ruler-horizontal` · `vector-polygon` · `grid-3x3` · `circle-dot-dashed` |
| 자료 받기 | 지적 · 수치지형 · 유산 | ka `cadastral` · `download` · `house` |
| 배경 지도 | 지형 · DEM · 등고선 · 토양 · 고지형 · 지질 · 수계 · 옛 지도 | `mountain` · `mountain-snow` · `target` · `layers` · `rotate-ccw-clock` · ka `geology` · `waves-horizontal` · `map` |
| 정합 | 정합 | `locate-fixed` |
| 내보내기 | 도면 · 인쇄 · 단면도 · GeoTIFF · 검수·제출 | `file-text` · `printer` · `chart-line` · `image` · `box` |
| 기타 | 더보기 | `ellipsis` |
| 앱 바 | 지역 · 주소 찾기 | `map-pin` · `search` |

그리기 보조 줄과 화면 부품에 쓰는 아이콘(탭 `house`·`map`, 되돌리기 `undo-2`·`redo-2`, 확대 `plus`·`minus`·`maximize`, 파일함 `folder`, 조사카드 `clipboard-list`, 배경 지도 `layers`, 경고 `triangle-alert`, 자석 `magnet` 등)은 계획 단계에서 부품별로 정한다.

## 화면별 바꿀 것

차이표의 「다름」·「없음」 줄을 아래 묶음으로 나눈다. 묶음마다 하나의 구현 단계가 된다.

1. **아이콘·리본**
   - 새 아이콘 체계를 적용한다.
   - 그룹 이름 글자, 버튼 크기·간격을 맞춘다.
   - 고른 도구는 타일에만 강조한다.
   - 저장 안 됨 표시를 넣는다.
   - 「지역」에 핀 아이콘, 주소 찾기에 돋보기 아이콘을 넣는다.
2. **탭 줄·조사 열림 칩·창 제목**
   - 탭 줄은 연회색 띠로, 고른 탭은 흰 카드 모양으로 한다.
   - 「조사 열림 · 이름」은 테두리 있는 칩으로 한다.
   - 창 제목은 「조사 이름 * - Strata」로 한다.
   - 앱 아이콘과 홈 로고는 남색 타일에 주황 나침반으로 한다.
3. **상태줄**
   - 높이는 36px로 한다.
   - 왼쪽 문장 앞에 상태 점을 둔다.
   - 홈에서도 「X — Y —」·축척·작업·제출·지도갱신 칩을 보인다.
   - 「저장 안 됨 n건」 노란 칩을 넣는다.
   - 지도갱신 앞에 초록 점을 둔다.
   - 좌표 숫자는 고정폭 숫자로 한다.
   - 상태줄의 도구 이름은 안내 띠와 겹치므로 뺀다.
4. **홈**
   - 히어로: 제목 크기, 단추 높이 48px, 동심 타원 등고선 배경, 로고 타일.
   - 이어서 작업 카드: 여백과 이름 크기.
   - 최근 조사 표:
     - 머리글 띠, 「조사」 왼쪽 정렬, 개수 칩
     - 행 높이 60px
     - 썸네일(격자 지도 + 주황 다각형)
     - 경로는 끝 두 단계만
     - 선택 행은 왼쪽 파란 막대와 연파랑 배경
   - 작업 순서: 완료 원은 연초록·진초록 체크, 단계 사이 세로 연결선, 설명 두 줄, 「제출 준비: 오류 n · 경고 n」 칩.
   - 연결 상태: 「연결됨」 문구, 카드 잘림 없음.
   - 오른쪽 열 폭 26%, 표 안쪽에만 스크롤.
   - 카드 모서리 12px, 옅은 그림자, 안쪽 여백 20px.
5. **레이어 패널** (띠 이름 「조사 데이터」·「주변·참조」·「배경 지도」는 목업과 같으므로 그대로, 「글자」 칸은 이미 필드 이름을 쓰므로 크기만 키운다)
   - 기본 폭 20%.
   - 「레이어」 머리글을 크게 하고 파일함에 아이콘을 단다.
   - 줄 32px, 진짜 체크박스, 글자 칸 필드 이름.
   - 선택 줄은 왼쪽 파란 띠, 연파랑 배경, 굵은 글자, 연필.
   - 묶음(그룹) 줄의 부분 체크 표시(–)를 목업처럼 보인다.
   - 이름 표기를 「유구 면」·「유구 선」으로 하고, 목업 순서로 놓는다.
6. **지도 위 요소**
   - 지도 회색 액자 테두리를 없앤다.
   - 양쪽 가장자리에 접기 손잡이 ‹ ›를 둔다.
   - 안내 띠 아이콘 타일, 되돌리기·다시 하기 흰 둥근 단추.
   - 확대 단추를 키운다.
   - 축척 막대는 선과 눈금 모양으로, 북쪽 화살표 「북」을 새로 넣는다.
   - 조사구역 기본은 옅은 채움, 유구 면 기본은 갈색.
   - 기본 라벨은 면적이 아니라 이름으로, 굵게, 흰 테두리.
   - 선택은 점선 상자와 핸들로 한다.
   - 그리는 중에는 점선과 네모 꼭짓점을 쓰고, 마지막 변 길이를 보여 준다.
7. **오른쪽 유구 패널(인스펙터)**
   - 기본 폭 21%.
   - 머리글 「1개 선택」은 글자로 쓰고, 접기는 가장자리 손잡이로 옮긴다.
   - 탭은 글자만, 고른 탭은 연파랑 알약 모양.
   - 제목 아래 종류·시대 칩, 시대 콤보 앞 해칭·색 견본.
   - 「변경됨 · 아직 저장 안 됨」 노란 칩을 둔다. 저장된 상태에서는 칩을 두지 않는다.
   - 배경 지도 카드 문구는 「배경은 자동으로 올리지 않습니다」, 각주는 「리본의 「배경 지도」와 같은 동작입니다.」로 한다.
   - 그리는 동안 카드가 겹쳐 잘리는 것을 고친다.
8. **공통 밀도**
   - 기본 글자를 1px 키운다.
   - 패널 안쪽 여백, 행 높이, 단추 높이를 4px 격자로 맞춘다.
   - 카드는 흰 바탕에 옅은 테두리로 한다.

## 지금 코드 위치 (de0414f, 조사 보고서 요약)

- 리본: `MainWindowRibbon.cpp`(addIcon 259-271), `KaBeginnerRibbon.cpp`(applyTwoLine 149-184, 칸 56×82, 아이콘 32), QSS 303-365(고른 상태가 단추 전체를 칠함).
- 아이콘: `KaIcons.cpp`(GlyphStyle 23-25·1023-1029, icon 1042-1112), `KaIconsOutlineEngine.cpp`(QIconEngine, 상태별 그림 91-138), 그림 함수 `KaIconsOutlineA/B/Ui.cpp`, 굵기 `KaIconMetrics.h`. 저장 안 됨 표시는 `MainWindowChrome.cpp:122-136`이 `strongIcon("save_unsaved")`로 바꾼다.
- 앱 바: `KaAppBar.cpp`(지역 단추 아이콘 없음, 검색칸 돋보기 없음).
- 탭·칩: `MainWindow.cpp:1035-1059`(viewTabs), QSS 719-743, `KaSurveyBadge.cpp`.
- 상태줄: `KaStatusBar.cpp`(홈에서는 왼쪽 문장만 보이게 숨김: 121-123·198-212, MainWindow.cpp:1068-1075·1107-1110), 도구 이름 `KaDrawSketchTools.cpp:148-164`.
- 홈: `KaStartPage.cpp`, `KaStartHero.cpp`, `KaSplashArt.cpp:169-216`(등고선), `KaHomeRecentCard.cpp`(행 48px), `KaHomeRowDelegate.cpp`, `KaHomeGuideCard.cpp`(연결선 없음), `KaHomeConnectionCard.cpp`(「설정됨」), 오른쪽 열 340/400px 고정.
- 레이어 패널: `KaShellFocus.cpp`(22%, 상한 260/348), `KaLayerInformationView.cpp:93-121`(10px 글꼴), `KaLayerSectionDelegate.cpp`(띠 16px), QSS 524-561.
- 지도 위: 액자 `MainWindow.cpp:487-492·934-942` + QSS 570-603·725-729, 안내 띠 `KaDrawGuideBand.cpp`, 확대·축척 `KaMapControls.cpp`(북쪽 화살표 없음), 투명도 카드 `KaLayerOpacityRail.cpp`, 접기 손잡이 없음(F9·F10만).
- 기본 기호·라벨: `LayerStyleDefaults.cpp:40-76`, `LayerOps.cpp:177-236`, 면적 라벨 `LabelOps.cpp:223-248`. 선택·그리기 표시: `KaFeatureSelectTool.cpp:213-216`, `KaVertexEditTool.cpp:158-197`, `KaCaptureMapTool.cpp:180-184`(변 길이 없음).
- 인스펙터: `KaInspectorPanel.cpp`(폭 272, 탭 아이콘), `KaFeatureCard*.cpp`(종류·시대 칩 없음), `KaBasemapQuickCard.cpp`(문구), 그리는 동안 `MainWindowFeatureCard.cpp:65-83`.
- 테마: `KaTheme.cpp:53-87`(색), `KaTheme.h:71-92`(크기), 본문 13px, QSS는 빌드할 때 박힌다(`ka-hgis.qss.inc`).

## 바뀌는 기준값과 그에 맞춰 고치는 시험

이 설계는 사용자가 정한 화면을 바꾸므로, 그 화면을 숫자로 고정한 시험의 기대값을 새 값으로 바꾼다. 확인하는 내용은 줄이지 않는다.

| 시험 | 지금 기대값 | 새 기대값 |
|---|---|---|
| test_theme `ribbonButtons_renderAtIntendedSize` 등 | 아이콘 32, 칸 56×82, 라벨 12px | 타일 32·글리프 18, 칸 너비 ≥40(라벨에 맞춤), 라벨 13px |
| test_theme `toolbarCheckedHasDistinctTreatment` | 단추 전체 배경이 다름 | 타일 테두리·채움이 다름(대비 ≥ 4.5 유지) |
| test_icons_outline / test_icons | 기본 Outline, 타일 없음 | 기본 Mockup, 타일 `#E4EAED`, 고른 상태 테두리 `#1A68B0`, 저장 안 됨 점. Outline·Tile은 환경 변수로 계속 시험 |
| test_theme_options, test_layer_list_chrome | 목록 글꼴 10px 고정, 기본 행 ≤ 28 | 목록 13px, 행 32px. 띠 16px는 유지 |
| test_home_cards / test_home | 최근 행 48, 오른쪽 열 340/400, 연결 상태 「설정됨」 | 행 60, 열 26%(최소 340), 「연결됨」 |
| test_inspector_panel | 탭에 아이콘, 폭 272 | 탭 글자만, 폭 21%(최소 260) |
| test_shell_focus / test_shell `focus_*` | 왼쪽 22%(상한 260/348) | 왼쪽 20%(상한 300/400) |
| test_shell_chrome | 상태줄 칩이 홈에서 숨음 | 홈에서도 보임(작업·제출은 조사가 열려 있을 때만) |
| test_layer_styles | feature_poly 초록 채움 | 목업 갈색 계열 |
| test_ribbon_overflow | 「제출 변환」 라벨 크기 | 「검수·제출」, 1920에서 접힘 없음 유지 |

## 시험

- 아이콘:
  - `KaIcons`의 Mockup 스타일이 리본 id마다 아이콘을 만드는지(없는 SVG 0개) 본다.
  - 상태별 픽스맵 색을 본다. 타일 가운데는 `#E4EAED`, 고른 상태 테두리는 `#1A68B0`, 저장 안 됨 점은 주황이어야 한다.
  - `KA_HGIS_ICON_STYLE=outline`이면 옛 모양이 나오는지 본다.
- 화면 부품: 기존 시험 실행 파일(test_shell, test_shell_chrome, test_home, test_home_cards, test_layer_panel, test_inspector_panel 등)에 줄 높이, 칩 문구, 상태줄 칩 보임, 손잡이·북쪽 화살표 존재 같은 동작 시험을 더한다.
- 소스 문자열만 보는 시험은 새로 만들지 않는다.
- 끝에 1920×1080에서 홈·지도를 찍어 목업과 나란히 놓고 차이표를 다시 만든다.

## 라이선스

- Lucide(ISC)의 쓰는 SVG만 `data/icons/lucide/`에 넣는다. `LICENSE-lucide.txt`를 함께 두고 `THIRD_PARTY_NOTICES.md`에 한 줄 적는다.
- 직접 그린 SVG 3개(`ka-cadastral`, `ka-geology`, `ka-ruler-horizontal`)는 저장소 라이선스를 따른다.
