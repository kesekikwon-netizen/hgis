# 목업 맞추기 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 홈·지도 화면을 개선 목업 6·7쪽처럼 보이게 한다. 리본 아이콘은 연회색 둥근 타일 위에 남색 Lucide 선 아이콘이다.

**Architecture:**
- 아이콘은 새 `GlyphStyle::Mockup`이 맡는다. 그리는 일은 `KaIconsMockupEngine`(QIconEngine)이 한다. 리소스에 넣은 SVG(Lucide 1.49.0과 직접 그린 3개)를 `QSvgRenderer`로 그리고, 타일·점·상태 색을 덧그린다.
- 나머지 화면은 지금 위젯·QSS·`KaTheme` 크기값을 고친다. 없는 것만 새 위젯으로 더한다(북쪽 화살표, 가장자리 접기 손잡이, 그리는 중 변 길이).

**Tech Stack:** C++20, Qt 6.11 Widgets(Qt6::Svg는 이미 링크됨), QGIS 4.3, QtTest + CTest, CMake AUTORCC.

**Spec:** `docs/superpowers/specs/2026-10-01-mockup-match-design.md`

## Global Constraints

- 사용자 결정:
  - 레이어 줄은 목업처럼 크게(32px, 13px 글자).
  - 「검수·제출」 이름을 유지하고 아이콘은 `box`.
  - 그리기 보조 줄은 유지하고 모양만 맞춘다.
  - 글꼴은 맑은 고딕 유지. 리본 한 줄. 시작은 홈만. 자동 복원 없음.
- 색과 크기는 spec 「아이콘 체계」 표의 값을 그대로 쓴다.
  - 타일: `#E4EAED` 32px, 반경 8.
  - 글리프: `#2B4858` 18px.
  - 고른 도구: 채움 `#E0ECF8`, 테두리 2px `#1A68B0`, 글리프·라벨 `#105088`.
  - 저장 안 됨: `#206CB0` 타일, 흰 글리프, 주황 점 `#F2A33A` 7px.
  - 비활성: `#EEF1F3`, 45%. 마우스 올림: `#D9E2E7`.
  - 라벨 13px `#202830`, 묶음 이름 11px `#5E6670`.
- QSS에는 `#rrggbb`를 직접 쓰지 않는다(`test_theme_options sheetHygiene`). 새 색은 `KaTheme` 토큰으로 더하고 QSS는 `@token@`으로 쓴다. `font-weight: 600`은 지금처럼 한 곳만 둔다.
- 파일은 300줄 이하. 기준선에 있는 큰 파일(MainWindow.cpp, LayerOps.cpp, KaDrawingStudio.cpp, BasemapOps.cpp, MainWindowEditing.cpp, MainWindowRibbon.cpp)은 늘리지 않는다. 새 코드는 새 파일이나 작은 MainWindow*.cpp에 둔다.
- Q_OBJECT 소스에 `R"(...)"`를 쓰지 않는다.
- 빌드:

  ```
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '$env:_MSPDBSRV_ENDPOINT_="ka-merge-1001"; $env:MSBUILDDISABLENODEREUSE="1"; . ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; cmake --build build --config Release --target ka-hgis <시험대상> -- /m:3 /p:CL_MPCount=4 /nodeReuse:false; exit $LASTEXITCODE'
  ```

  QSS를 바꾸면 ka-hgis와 시험 대상을 함께 다시 빌드한다(`ka-hgis.qss.inc`가 박힌다).
- 시험:

  ```
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; ctest --test-dir build -C Release -j3 --output-on-failure -R "^(이름)$"; exit $LASTEXITCODE'
  ```

- 커밋 메시지: 한국어 `type(scope): 요약`. 본문에 시험 결과 한 줄을 적고, 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`를 단다. 커밋은 Bash에서 한다.
- 화면 모양을 숫자로 고정한 기존 시험은 spec 「바뀌는 기준값」 표대로 새 값으로 바꾼다. 확인하는 내용을 지우지 않는다.

## Review Focus

1. 고해상도 화면(배율 125%·150%)에서 타일과 글리프가 흐려지거나 어긋나는 경우. 기대: devicePixelRatio에 맞춰 선명하게 그린다 → Task 1 `mockupIconIsCrispAtDpr150`.
2. 1920 창, 그리고 그보다 좁은 1366·1093·1024 창의 리본. 기대: 어느 너비에서도 묶음을 접지 않는다. 1904는 글자+아이콘, 좁으면 아이콘만, 더 좁으면 작은 아이콘(사용자 결정 2026-10-01 19:40) → Task 2 `density_*`.
3. 「큰 글씨」 옵션(본문 15px)을 켠 경우. 기대: 레이어 줄과 표가 글자에 맞게 커지고 잘리지 않는다 → Task 6 `largeTextRowsDoNotClip`.
4. 조사를 열지 않은 홈에서 상태줄을 보는 경우. 기대: 「작업 —」처럼 빈 값으로 보이고 누를 수 없다(목업 6쪽) → Task 4 `homeShowsEmptyCrsChips`.
5. 예전 아이콘 모양이 필요한 경우(`KA_HGIS_ICON_STYLE=outline` 또는 `tile`). 기대: 옛 모양이 그대로 나온다 → Task 1 `environmentSelectsLegacyStyles`.

---

### Task 1: 목업 아이콘 모양(타일 + Lucide SVG)

**Files:**
- Create:
  - `data/icons/lucide/<이름>.svg` — 아래 목록만 넣는다. 원본은 `scratchpad\lucide\icons\icons\`, zip sha256 `66141a7b…bd5`.
  - `data/icons/lucide/LICENSE-lucide.txt` — ISC 원문.
  - `data/icons/ka/ka-cadastral.svg`, `ka-geology.svg`, `ka-ruler-horizontal.svg` — 원본은 `scratchpad\lucide\custom\`.
  - `data/icons/icons-mockup.qrc` — prefix `/ka-hgis/icons`, 별칭 `lucide/<이름>.svg`, `ka/<이름>.svg`.
  - `src/app/KaIconsMockup.h`, `src/app/KaIconsMockupEngine.cpp`(QIconEngine), `src/app/KaIconsMockupMap.cpp`(앱 id → SVG 이름 표).
- Modify:
  - `src/app/KaIcons.h` — `enum class GlyphStyle { Tile, Outline, Mockup };`.
  - `src/app/KaIcons.cpp`
    - 1023-1029: 기본값을 Mockup으로 바꾼다. 환경 변수 `KA_HGIS_ICON_STYLE`이 `outline`이면 Outline, `tile`이면 Tile이다.
    - 1042-1112: Mockup이면 `KaIconsMockup::mockupIcon(id, strong)`를 부른다. 매핑이 없는 id는 지금 Outline 그림으로 대신 그린다.
  - `CMakeLists.txt` — ka-hgis와 아이콘 시험 대상에 `data/icons/icons-mockup.qrc`와 새 cpp를 넣는다.
  - `THIRD_PARTY_NOTICES.md` — 「Lucide 1.49.0 | ISC | https://github.com/lucide-icons/lucide」 한 줄.
- Test:
  - `tests/test_icons_outline.cpp`(ctest `icon_outline`) — 기본 스타일을 기대하던 곳은 Outline을 `setGlyphStyle(Outline)`으로 명시한다.
  - 새 시험 함수는 `tests/test_icons.cpp`(ctest는 `ka_icon_glyph_tests`의 이름 — CMakeLists.txt 932 부근에서 확인).

**Interfaces:**
- Produces:
  ```cpp
  namespace KaIconsMockup {
  // Lucide/ka SVG resource path for an app icon id, or an empty string when unmapped.
  QString svgPathFor(const QString& id);
  // Tile + glyph icon. strong=true is the 저장-unsaved look (save_unsaved).
  QIcon mockupIcon(const QString& id, bool strong = false);
  struct Look { QColor tile, tileBorder, glyph; qreal glyphOpacity; bool dot; };
  Look lookFor(QIcon::Mode mode, QIcon::State state, bool strong);
  }
  ```
- 대응표(`KaIconsMockupMap.cpp`, 앱 id → svg). 앱 id는 `MainWindowRibbon.cpp` 리본 목록의 id다.

  | 앱 id | svg |
  |---|---|
  | new | lucide/file-plus |
  | open | lucide/folder |
  | save, save_unsaved | lucide/save |
  | save_as | lucide/save-pen |
  | select | lucide/mouse-pointer-2 |
  | measure | ka/ka-ruler-horizontal |
  | draw_poly | lucide/vector-polygon |
  | trench_grid | lucide/grid-3x3 |
  | buffer | lucide/circle-dot-dashed |
  | cadastral | ka/ka-cadastral |
  | topo_download | lucide/download |
  | heritage | lucide/house |
  | contour | lucide/mountain |
  | dem | lucide/mountain-snow |
  | survey_contour | lucide/target |
  | soil | lucide/layers |
  | paleo | lucide/rotate-ccw-clock |
  | geology | ka/ka-geology |
  | river | lucide/waves-horizontal |
  | old_map | lucide/map |
  | georef | lucide/locate-fixed |
  | pdf | lucide/file-text |
  | print | lucide/printer |
  | section | lucide/chart-line |
  | geotiff | lucide/image |
  | export_convert | lucide/box |
  | more | lucide/ellipsis |
  | home | lucide/house |
  | map | lucide/map |
  | region | lucide/map-pin |
  | search | lucide/search |
  | undo | lucide/undo-2 |
  | redo | lucide/redo-2 |
  | zoom_in | lucide/plus |
  | zoom_out | lucide/minus |
  | zoom_fit | lucide/maximize |
  | folder | lucide/folder |
  | note | lucide/clipboard-list |
  | layers | lucide/layers |
  | warn | lucide/triangle-alert |
  | snap | lucide/magnet |
  | chevron_left | lucide/chevron-left |
  | chevron_right | lucide/chevron-right |
  | satellite | lucide/satellite |

  리본·화면 코드가 실제로 쓰는 id가 이 표와 다르면 실제 id를 따르되 대응 svg는 바꾸지 않는다. 표에 없는 Lucide 파일은 넣지 않는다.

- [ ] **Step 1: 실패하는 시험 작성** (`tests/test_icons.cpp`)
  ```cpp
  void mockupIsDefault();             // QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Mockup) (환경 변수 없음)
  void environmentSelectsLegacyStyles(); // qputenv outline→Outline, tile→Tile
  void everyRibbonIdHasSvg();         // 리본 id 27개 모두 !KaIconsMockup::svgPathFor(id).isEmpty() && QFile::exists(path)
  void normalTileColours();           // 32px Normal/Off 픽스맵: 중앙에서 12px 떨어진 칸 = #E4EAED(±6), 글리프 선 위 = #2B4858(±12)
  void checkedTileHasBorder();        // Normal/On: 가장자리 1px 안쪽 = #1A68B0(±10), 안쪽 = #E0ECF8(±6)
  void unsavedSaveHasDot();           // mockupIcon("save_unsaved", true): 채움 #206CB0, 오른쪽 위 점 = #F2A33A(±10)
  void disabledIsFaded();             // Disabled: 타일 #EEF1F3(±6), 글리프 알파 ≤ 0.5
  void mockupIconIsCrispAtDpr150();   // pixmap(QSize(32,32), 1.5) 크기 48×48, 테두리 모서리 계단 없음(가로 선 한 줄 화소 2개 이하로 번짐)
  ```
- [ ] **Step 2: 실패 확인.** ctest를 돌린다. 예상: `KaIconsMockup` 정의가 없어 링크 실패.
- [ ] **Step 3: 구현**
  - 엔진 `paint()`:
    1. `lookFor`로 색을 고른다.
    2. 타일을 그린다. `QPainterPath::addRoundedRect`, 반경은 크기×0.25.
    3. SVG 원문에서 `currentColor`를 글리프 색 `#RRGGBB`로 바꿔 `QSvgRenderer`에 넣고, 가운데 18/32 크기로 그린다.
    4. strong이면 점을 그린다. 중심은 (크기-6, 6)×크기/32, 지름 7, 흰 테두리 1.5.
  - `pixmap()`은 dpr 크기로 그린다.
  - SVG 원문은 id마다 한 번 읽어 캐시한다.
- [ ] **Step 4: 통과 확인.** 위 ctest와 `icon_outline`이 통과해야 한다.
- [ ] **Step 5: 커밋.** `feat(icons): 목업 아이콘 모양(연회색 타일 + Lucide 선 아이콘)을 기본으로 한다`

### Task 2: 리본 칸·라벨·고른 상태

**Files:**
- Modify:
  - `src/app/KaTheme.h:71-92` `ButtonMetrics` — `ribbonIconSize` 32, `ribbonFontSize` 13, `ribbonMinFontSize` 12, `ribbonChipWidth` 40, `ribbonMinWidth` 40, `ribbonHeight` 82.
  - `src/app/KaBeginnerRibbon.cpp:149-184` `applyTwoLine` — 칸 너비는 max(40, 라벨 너비 + 8). 라벨은 타일 아래 6px.
  - QSS `data/theme/ka-hgis.qss`
    - 303-365: 고른 상태(:checked)의 단추 배경과 테두리를 투명으로 한다. 라벨만 `@ribbonActiveInk@`로 굵게 한다. 새 토큰 `ribbonActiveInk` = `#105088`, `ribbonGroupInk` = `#5E6670`.
    - 묶음 이름 11px `@ribbonGroupInk@`.
  - `src/app/KaThemeSheet.cpp:16-59` — 새 토큰 이름을 등록한다.
  - 그리기 보조 줄(`#subToolbar`, `MainWindowRibbon.cpp:722-729`, 단추는 `KaDrawSketchTools.cpp`·`MainWindowRibbon.cpp`): 단추 아이콘을 Mockup 글리프 20px 타일로, 라벨 13px, 고른 단추는 리본과 같은 타일 강조. 기존 `#subToolbar …:checked` 규칙의 `border-bottom`은 시험(test_workflow·test_save_open)이 찾으므로 남기되 색만 `@ribbonActiveInk@`로.
- Test: `tests/test_theme.cpp`(ctest `theme_qss`), `tests/test_ribbon_overflow.cpp`(`ribbon_overflow`), `tests/test_theme_render.cpp`(`theme_render`)

**Interfaces:**
- Consumes: Task 1의 `KaIcons::icon(id)`(Mockup)

- [ ] **Step 1: 시험 고치기·더하기**
  - `ribbonButtons_renderAtIntendedSize`:
    - 아이콘 32, 라벨 폰트 13px, 칸 너비 ≥ 40.
    - 같은 라벨 길이면 같은 너비.
    - 「다른 이름」 칸 너비 = 라벨 너비 + 8.
  - `toolbarCheckedHasDistinctTreatment`:
    - 고른 단추의 QSS 배경이 투명이어야 한다.
    - 아이콘 On 상태의 테두리 화소와 Off 상태 화소의 대비가 ≥ 4.5여야 한다.
  - `ribbon_overflow`:
    - 「제출 변환」 기대를 「검수·제출」로 바꾼다.
    - 새 시험 `mockupRibbonFitsAt1920`: 창 1904px에서 접히는 묶음 0개.
    - 접지 않고 줄이는 규칙과 그 시험은 Task 11이 맡는다(Task 2 다음에 한다).
- [ ] **Step 2: 실패 확인**(`theme_qss|ribbon_overflow|theme_render`)
- [ ] **Step 3: 구현.** 위 크기값과 QSS를 고친다. `MainWindowChrome.cpp:122-136`의 저장 안 됨 아이콘은 `KaIcons::strongIcon("save_unsaved")`를 그대로 쓴다(Task 1이 Mockup에서 strong을 처리).
- [ ] **Step 4: 통과 확인** + `--smoke-quit` 0
- [ ] **Step 5: 커밋.** `style(ribbon): 리본 칸·라벨·고른 도구 표시를 목업대로 맞춘다`

### Task 11: 리본 크기 단계 — 접지 않고 줄이기 (Task 2 바로 다음에 한다)

spec 「리본 크기 단계 (2026-10-01 19:40 결정)」을 그대로 따른다. 「더 많은 작업」과 묶음 접기를 없애고, 창 너비에 맞는 가장 큰 크기 단계를 고른다.

**Files:**
- Modify:
  - `src/app/KaBeginnerRibbon.h/.cpp`
    - 없앤다: `m_overflow`(「더 많은 작업」), `m_overflowMenu`, `m_groupMenus`, `m_groupScrolls`, `planGroups`, `setKeepPriority`, `setPinned`, `updateOverflow`, 「[ribbon] 접힘」 로그.
    - 단추 글자는 숨겨도 `text()`를 지우지 않는다. 툴팁은 늘 전체 이름이다.
    - `minimumSizeHint().width()`는 3단계(가장 작게) 너비다.
    - 단계가 바뀔 때만 세션 로그 한 줄: `[ribbon] 크기 단계 N · 창 W · 필요 [w0 w1 w2 w3]`(qWarning, 지금 접힘 로그와 같은 길).
  - `src/app/MainWindowRibbon.cpp` — `setKeepPriority`·`setPinned` 호출 두 문장을 지우고, 그리기 보조 줄을 만든 곳에 `KaToolbarFit::install(m_subToolbar);` 한 줄을 넣는다(파일은 줄어든다).
- Create: `src/app/KaToolbarFit.h/.cpp` — QToolBar에 붙는 이벤트 필터. 크기가 바뀌거나 액션이 더해지면 다시 계산한다.
  - 다 들어가면: 지금 글자 모양 그대로, 아이콘 20.
  - 모자라면: `Qt::ToolButtonIconOnly`, 아이콘 20.
  - 그래도 모자라면: `Qt::ToolButtonIconOnly`, 아이콘 16.
  - 자리가 생기면 위 순서를 거꾸로 되돌린다. 어느 단계에서도 QToolBar 펼침 단추(`qt_toolbar_ext_button`)가 보이지 않는다.
- CMakeLists.txt: `KaToolbarFit.cpp`를 ka-hgis와 ribbon_overflow 시험 대상에 넣는다.
- Test:
  - `tests/test_ribbon_overflow.cpp`(ctest `ribbon_overflow`) — `plan_*` 4개를 지우고 아래 시험으로 바꾼다.
  - `tests/test_theme.cpp`(ctest `theme_qss`) `ribbonOverflow_preservesControlsAndKeyboard`·`ribbonOverflow_keepsAlignAtFieldWidth` — 확인하던 것(좁은 창에서도 모든 단추를 Tab으로 갈 수 있음, 「정합」이 보임)을 새 규칙으로 다시 쓴다. 이름은 `ribbonNarrow_keepsControlsAndKeyboard`·`ribbonNarrow_keepsAlignVisible`.
  - `tests/test_region_locator.cpp:195-215`(ctest `region_locator_popup`) — 접힌 메뉴 대신, 리본을 `minimumSizeHint().width()`로 줄여도 지역 찾기 칩이 리본 위에 보이고 눌린다.
  - `tests/test_save_open.cpp`(ctest `save_open_window`) `narrowWindowKeepsSearchOnTheRibbonRow`·`ribbonButtonsAllHaveDifferentIcons` — 리본은 `ribbonOverflow`의 부모가 아니라 objectName `beginnerRibbon`으로 찾는다. 「더 많은 작업으로 접히지 않았다」 확인은 「모든 칩이 리본 안에 보인다」로 바꾼다.

**Interfaces:**
- Consumes: Task 1 `KaIcons::icon(id)`(Mockup 타일, 어느 크기에서도 선명), Task 2 `ButtonMetrics`(타일 32, 칸 너비 max(40, 라벨+8)).
- Produces:
  ```cpp
  // KaBeginnerRibbon.h
  enum class Density { Full = 0, IconOnly = 1, Small = 2, Tiny = 3 };
  struct DensityLook { int tile; int chipWidth; bool labels; };  // chipWidth 0 = max(40, 라벨 너비 + 8)
  static DensityLook lookFor(Density density);  // Full {32,0,true} · IconOnly {32,40,false} · Small {24,30,false} · Tiny {20,26,false}
  // widths[i] = 단계 i일 때 리본 전체 너비. 들어가는 가장 큰 단계, 하나도 안 들어가면 Tiny.
  static Density chooseDensity(const std::array<int, 4>& widths, int available);
  Density density() const;
  // KaToolbarFit.h
  namespace KaToolbarFit { void install(QToolBar* bar); }
  ```

- [ ] **Step 1: 실패하는 시험 작성**
  ```cpp
  void chooseDensity_picksLargestThatFits();
  // widths {1800,1200,900,780}: 1904→Full, 1366→IconOnly, 1000→Small, 800→Tiny, 500→Tiny
  void productionRibbon_data();   // 1904, 1536, 1366, 1280, 1093, 1024
  void productionRibbon();
  // findChild<QToolButton*>("ribbonOverflow") == nullptr
  // 모든 칩: isVisible(), 리본 안(0 ≤ x, 오른쪽 < ribbon->width()), 서로 겹치지 않음, toolTip()에 text()가 있음
  // 모든 묶음 이름(QLabel#ribbonGroupCaption)이 보인다
  // 1904: density()==Full, 모든 칩 ToolButtonTextUnderIcon
  // 1093·1024: density()!=Full, 칩 ToolButtonIconOnly; Small이면 iconSize 24, Tiny면 20
  void ribbonMinimumWidthIsTinyWidth();  // 0 < minimumSizeHint().width() < 1024, 그 너비에서 density()==Tiny이고 칩이 모두 보임
  void toolbarFit_hidesTextThenShrinksIcons();
  // QToolBar + 글자 있는 액션 14개, install 뒤 넓게 → 글자 보임·아이콘 20; 좁게 → IconOnly·20; 더 좁게 → IconOnly·16;
  // 다시 넓게 → 글자 보임. 모든 경우 qt_toolbar_ext_button 이 보이지 않는다.
  ```
  test_theme·test_region_locator·test_save_open의 위 시험도 새 규칙으로 고친다.
  새 `subToolbarKeepsTextAt1904WhileSketching`(test_save_open, `save_open_window` 목록에 더함): 조사를 열고 1904×1000 창에서 유구면 그리기를 시작하면 `#subToolbar`의 단추가 글자를 보이고(`ToolButtonIconOnly` 아님) `qt_toolbar_ext_button`이 보이지 않는다. 1024 창에서는 IconOnly이고 역시 펼침 단추가 없다.
- [ ] **Step 2: 실패 확인.** `ribbon_overflow|theme_qss|region_locator_popup|save_open_window`.
- [ ] **Step 3: 구현.** 단계별 너비는 묶음마다 max(묶음 이름 너비, 칩 너비 합 + 칩 간격) + 묶음 여백을 더해 계산한다. 단계를 바꾼 뒤 칩마다 `setToolButtonStyle`·`setIconSize`·고정 너비를 다시 준다.
- [ ] **Step 4: 통과 확인.** 위 네 묶음과 `theme_render`, `--smoke-quit` 0.
- [ ] **Step 5: 커밋.** `feat(ribbon): 리본을 접지 않고 좁으면 아이콘만·작은 아이콘으로 모두 보인다`

### Task 3: 앱 바·탭 줄·조사 열림 칩·창 제목·로고

**Files:**
- Modify:
  - `src/app/KaAppBar.cpp`
    - 「지역」에 `KaIcons::icon("region")` 16px를 단다.
    - 검색칸 앞에 `search` 아이콘 액션을 둔다(`QLineEdit::addAction(..., LeadingPosition)`).
    - placeholder를 「도로명·지번 (예: 하회종가길 40)  Ctrl+F」로 줄인다.
  - QSS 719-743 탭:
    - 탭 줄 바탕은 `@desk@`.
    - 고른 탭은 흰 바탕(`@surface@`)에 위·좌·우 1px `@border@`, 모서리 8 8 0 0. 밑줄은 없앤다.
    - 탭 아이콘은 `home`·`map` Mockup 글리프 16px.
  - `src/app/KaSurveyBadge.cpp` — 테두리 있는 알약 칩. `KaChip`에 Ok 톤과 1px `@ok@` 계열 테두리를 쓴다.
  - `src/app/MainWindowSession.cpp:1333-1341` `refreshWindowTitle` — 「<조사 이름> * - Strata」. 저장 안 됐을 때만 `*`, 조사가 없으면 「Strata」.
  - 앱·로고 아이콘: `data/theme/ka-hgis-app.png`(남색 타일 + 금색 흙손)는 목업 표지 로고와 같은 디자인이므로 그대로 둔다. 홈 로고 크기만 Task 5에서 맞춘다.
- Test: `tests/test_shell_chrome.cpp`(ctest `shell_widgets`), `tests/test_shell.cpp`(`shell_chrome`)

- [ ] **Step 1: 시험**
  - `appBar_regionHasPinIcon`: `#appBarRegion` icon 있음.
  - `appBar_searchHasLeadingIcon`: `#appBarSearch` actions 1개 이상, 위치 Leading.
  - `badge_hasOutline`: 칩 테두리 화소 존재.
  - `windowTitle_namesSurveyAndDirty`: 「광령리1 * - Strata」 / 「광령리1 - Strata」 / 「Strata」.
- [ ] **Step 2: 실패 확인.** **Step 3: 구현.** **Step 4: 통과 확인.**
- [ ] **Step 5: 커밋.** `style(shell): 앱 바·탭·조사 칩·창 제목을 목업대로 맞춘다`

### Task 4: 상태줄

**Files:**
- Modify:
  - `src/app/KaStatusBar.cpp`
    - 높이 36. 왼쪽 문장 앞 8px 상태 점(`QLabel#statusLeadDot`). 색은 `@accent@`, 경고 문장이면 `@warn@`.
    - 홈에서도 축척·작업·제출·지도갱신 칩을 보인다. 조사가 없으면 「작업 —」을 누를 수 없게 하고, X/Y는 「X —  Y —」.
    - 지도갱신 앞에 초록 점을 단다(`@ok@`).
    - X/Y 숫자는 `@monoFont@`.
  - `src/app/MainWindow.cpp:1068-1075, 1107-1110` — 홈에서 숨기던 줄을 위 규칙으로 바꾼다. MainWindow.cpp는 늘리지 않는다. 숨김 판단은 `KaStatusBar::setPageKind(PageKind)`로 옮긴다.
  - `src/app/KaDrawSketchTools.cpp:148-164` — 상태줄 `#toolChip`을 없앤다. 안내 띠가 같은 내용을 보인다.
- Test: `tests/test_shell_chrome.cpp`(ctest `shell_widgets`)

**Interfaces:**
- Produces: `enum class PageKind { Home, Map, Studio }; void KaStatusBar::setPageKind(PageKind kind);`

- [ ] **Step 1: 시험**
  - `statusBarIs36High`.
  - `homeShowsEmptyCrsChips`: Home + 조사 없음 → `crsButton` 보임·비활성·「작업 —」, `xyReadout` 「X —    Y —」.
  - `leadDotFollowsTone`.
  - `renderToggleHasGreenDot`.
  - `noToolChipInStatusBar`.
  - 기존 「홈에서 숨김」 기대는 새 규칙으로 바꾼다.
- [ ] **Step 2–4: 실패 확인 → 구현 → 통과 확인**
- [ ] **Step 5: 커밋.** `style(status): 상태줄을 목업처럼 칩 다섯 개와 상태 점으로 보인다`

### Task 5: 홈

**Files:**
- Modify:
  - `src/app/KaStartPage.cpp:30-35, 81-127, 180-210`
    - 히어로 높이 236은 유지한다.
    - 로고 56px. 「Strata」 44px. 단추 높이 48, 아이콘은 `new` → `plus`, `open` → `folder` 20px.
    - 오른쪽 열 너비 = max(340, 창 너비 × 26%).
  - `src/app/KaSplashArt.cpp:169-216` `contours` — 홈 히어로에서는 오른쪽 중앙 동심 타원 등고선을 그린다.
    - 새 함수 `contoursConcentric(QPainter&, QRectF, int rings = 7)`. 중심은 (70%, 50%), 흰색 알파 40, 굵기 1.2.
    - 시작 안내 창은 지금 함수를 그대로 쓴다. `test_startup_splash`를 지키기 위해서다.
  - `src/app/KaStartPage.cpp:129-176` 이어서 작업 카드: 안쪽 여백 24/20, 조사 이름 22px 굵게, 「이어서 열기 →」 높이 44.
  - `src/app/KaHomeRecentCard.cpp`, `KaHomeRecentCard.h:29`
    - 행 높이 60.
    - 머리글 줄 배경 `@stripe@`. 「조사」 머리글 왼쪽 정렬.
    - 개수는 `KaChip` Neutral 「N개」.
  - `src/app/KaHomeRowDelegate.cpp`
    - 썸네일 44×36: 격자(`@border@` 1px, 8px 간격) 위에 주황 다각형(새 토큰 `thumbPoly` = `#E8742A`, 채움 알파 60, 테두리 1.5px).
    - 경로는 마지막 두 폴더만 「바탕 화면 › 광령리1」로 보인다. 전체 경로는 툴팁에 둔다.
  - `src/app/KaHomeGuideCard.cpp`
    - 단계 원 사이에 세로 연결선(2px `@border@`)을 그린다. 새 `QWidget#startGuideRail`이 원 중심을 잇는다.
    - 완료 원은 연초록(`@okSurface@`) 바탕에 진초록 체크.
    - 3단계 문장은 「「도면」으로 종이에 옮기고, 「검수·제출」로 5179 SHP·PDF를 냅니다.」로 한다.
  - `src/app/KaHomeConnectionCard.cpp` — 「설정됨」을 「연결됨」으로. 카드 높이는 내용에 맞추고 안쪽 스크롤을 없앤다.
  - QSS 22-99, 1234-1251
    - 카드 모서리 12, 안쪽 여백 20, 옅은 그림자(`QGraphicsDropShadowEffect` blur 16, alpha 20, offset 0,2, 카드에만).
    - 선택 행은 연파랑에 왼쪽 3px `@accent@`.
- Test: `tests/test_home.cpp`(ctest `home_start`), `tests/test_home_cards.cpp`(`home_cards`), `tests/test_startup_splash.cpp`(`startup_splash`, 바뀌지 않아야 함)

- [ ] **Step 1: 시험 고치기·더하기**
  - 행 높이 48 → 60.
  - 오른쪽 열: 1920에서 499(=26%), 1366에서 355, 최소 340.
  - 「설정됨」 → 「연결됨」.
  - 새 시험:
    - `recentPathShowsLastTwoFolders`: 「C:/Users/a/Desktop/광령리1/광령리1.gpkg」 → 「Desktop › 광령리1」. 바탕 화면이면 「바탕 화면 › 광령리1」.
    - `guideStepsAreConnected`.
    - `recentCountIsChip`.
    - `connectionCardDoesNotScroll`.
- [ ] **Step 2–4.** 시작 안내 시험 `startup_splash`도 통과해야 한다.
- [ ] **Step 5: 커밋.** `style(home): 홈 카드·표·작업 순서·연결 상태를 목업대로 맞춘다`

### Task 6: 레이어 패널

**Files:**
- Modify:
  - `src/app/KaShellFocus.cpp:16-19`, `KaShellFocus.h:19-26` — 왼쪽 기본 폭은 20%. 하한 220, 상한은 1500 미만이면 300, 아니면 400.
  - `src/app/KaLayerInformationView.cpp:93-121` — 목록 글자를 `uiFontSize`(13, 큰 글씨 15)로 한다. 행 높이는 max(32, 글자 높이 + 14).
  - QSS 524-561
    - 행 min-height 30, padding 4px 6px.
    - 선택 행은 `@selected@` 배경, 왼쪽 3px `@accent@`, 굵게.
    - 「레이어」 머리글(`#cardCaption` in `#layersCard`)은 15px 굵게.
  - `src/app/KaThemeStyle.cpp:48-81` — 체크박스를 16px에서 18px 채움 사각형으로 한다. 부분 체크(–)를 그린다.
  - `src/app/MainWindow.cpp:821-833` — 「파일함」에 `folder` 아이콘 16px. MainWindow.cpp는 늘리지 않는다. 아이콘은 이미 있는 setText 줄에 setIcon으로 바꿔 넣는다.
  - 레이어 이름 표기: 「유구 (면)」을 「유구 면」으로, 「유구 (선)」을 「유구 선」으로. 정의는 `src/core/LayerOps.h` 또는 이름 표를 `git grep -n "유구 (면)"`으로 찾는다. 새로 만드는 레이어에만 적용하고, 이미 저장된 조사의 레이어 이름은 바꾸지 않는다(사용자가 바꾼 이름일 수 있음). 「조사 데이터」 안 순서는 조사구역 → 유구 면 → 유구 선 → 시굴격자 → 단면선 → 기준점 → 유물(목업 순서 뒤에 나머지).
- Test: `tests/test_layer_list_chrome.cpp`(ctest `layer_list_chrome`), `tests/test_theme_options.cpp`(`theme_options`), `tests/test_shell_focus.cpp`(`shell_focus`), `tests/test_layer_panel.cpp`(`layer_panel`)

- [ ] **Step 1: 시험 고치기·더하기**
  - 목록 글꼴 10 → 13, 기본 행 높이 ≥ 32. 띠 16은 유지.
  - 왼쪽 폭 22% → 20%, 상한 300/400.
  - 새 시험:
    - `largeTextRowsDoNotClip`: 큰 글씨면 행 ≥ 15+14.
    - `domainLayerNamesUseSpaces`: 「유구 면」, 「유구 선」.
    - `surveyGroupOrderFollowsMockup`.
    - `partialCheckIsDrawn`.
- [ ] **Step 2–4.** `layer_panel`도 통과해야 한다.
- [ ] **Step 5: 커밋.** `style(layers): 레이어 패널 줄·체크박스·선택 표시를 목업 크기로 키운다`

### Task 7: 지도 위 요소

**Files:**
- Create:
  - `src/app/KaMapNorthArrow.h/.cpp` — 36×36 흰 원(테두리 `@border@`), 위쪽 화살표 + 「북」 12px. 지도 오른쪽 아래, 좌표격자 칸 왼쪽 12px.
  - `src/app/KaEdgeHandle.h/.cpp` — 지도 왼쪽·오른쪽 가장자리 가운데에 붙는 16×48 흰 손잡이(‹ / ›). 누르면 `KaShellFocus`의 왼쪽·오른쪽 패널을 접고 편다.
- Modify:
  - 지도 액자:
    - `MainWindow.cpp:487-492, 934-942` 여백은 줄 수가 같은 수정만 한다.
    - QSS 570-603, 725-729: `#mapCard` 테두리·반경을 없애고 `#centralRoot` 여백 0.
  - `src/app/KaDrawGuideBand.cpp` — 띠 아이콘을 연파랑 24px 타일 위 글리프로. 되돌리기·다시 하기는 흰 바탕(`@surface@`)에 1px `@border@` 원.
  - `src/app/KaMapControls.cpp`
    - 확대 단추 40×40, 글리프는 Mockup `zoom_in/zoom_out/zoom_fit`.
    - `KaMapScaleBar`를 선과 눈금 모양으로: 가로 선 1.5px, 0·중간·끝 눈금 6px, 숫자 11px 「0 10 20 m」.
  - `src/app/KaLayerOpacityRail.cpp` — 모양만 흰 카드(반경 10, 옅은 그림자)로.
- Test: `tests/test_shell_chrome.cpp`(`shell_widgets`), `tests/test_shell_focus.cpp`(`shell_focus`)

- [ ] **Step 1: 시험**
  - `northArrowSitsLeftOfGridToggle`: 존재, 크기 36, 위치.
  - `edgeHandlesToggleSidePanels`: 왼쪽 손잡이 클릭 → 왼쪽 패널 폭 0 → 다시 클릭 → 이전 폭.
  - `mapHasNoFrame`: `#mapCard` QSS border 0.
  - `scaleBarDrawsTicks`: 렌더 이미지에서 눈금 3개.
  - `zoomButtonsAre40`.
  - 띠 위치 시험(12,12)은 그대로 둔다.
- [ ] **Step 2–4. Step 5: 커밋.** `feat(map): 북쪽 화살표·가장자리 접기 손잡이를 더하고 지도 액자를 없앤다`

### Task 8: 지도 기본 기호·라벨·선택·그리기 표시

**Files:**
- Modify:
  - `src/core/LayerStyleDefaults.cpp:40-76`
    - survey_area: 채움 (234,88,12,40), 테두리 (234,88,12), 1.6mm.
    - feature_poly: 채움 (180,120,70,150), 테두리 (110,60,25), 1.0mm.
    - 저장된 `ka_hgis/style_*` 값이 있으면 그대로 그 값을 쓴다.
  - `src/core/LayerOps.cpp:226-233`(줄 수 유지) — feature_poly·feature_line 기본 라벨은 detectNameField(이름) 굵게 9pt, 흰 테두리 1mm. survey_area는 지금처럼 면적.
  - `src/app/KaFeatureSelectTool.cpp:213-216`, `src/app/KaVertexEditTool.cpp:158-197` — 고른 도형 둘레에 점선 사각형(파랑 `#1A68B0`, 1.5px, DashLine)과 꼭짓점 흰 네모 9px(파란 테두리).
  - `src/app/KaCaptureMapTool.cpp:180-184` — 그리는 선은 갈색 점선(110,60,25) 2px, 꼭짓점은 흰 네모 8px.
    - 새 `KaSketchLengthLabel`(QgsMapCanvasItem, 새 파일 `src/app/KaSketchLengthLabel.h/.cpp`)이 마지막 변 가운데에 「57.5 m」를 그린다. 작업 CRS 평면 거리, 소수 1자리, 흰 테두리 12px 굵게.
- Test: `tests/test_layer_styles.cpp`(ctest `layer_styles`), `tests/test_editing_tools.cpp`(ctest `editing_tools`)

- [ ] **Step 1: 시험**
  - `layer_styles` feature_poly 기대 (22,163,74) → (180,120,70,150)/(110,60,25).
  - 새 시험:
    - `surveyAreaHasLightFill`.
    - `featureLabelsUseName`.
    - `sketchShowsLastSegmentLength`: 점 (0,0)→(57.5,0) 입력 시 라벨 「57.5 m」.
    - `selectionDrawsDashedBox`.
- [ ] **Step 2–4. Step 5: 커밋.** `style(map): 조사구역·유구 기본 색과 이름 라벨, 선택·그리기 표시를 목업대로`

### Task 9: 오른쪽 유구 패널(인스펙터)

**Files:**
- Modify:
  - `src/app/KaInspectorPanel.cpp:38-70`
    - 머리글의 개수는 글자 라벨 「1개 선택」(`@inkMuted@`, 12px)로 한다.
    - 접기 단추를 없앤다(Task 7 가장자리 손잡이가 맡음).
    - 탭은 아이콘 없이 글자만. 고른 탭은 연파랑(`@accentWash@`) 알약, 반경 14.
    - 기본 폭은 max(260, 창 × 21%), 최대 420. 지금 `kDefaultRightWidth` 272를 대신한다.
  - `src/app/KaFeatureCard.cpp:68-113`
    - 제목 아래에 종류·시대 칩 두 개(`KaChip` Neutral)를 둔다. 시대 칩 앞에는 시대 색 견본 10px.
    - 시대 콤보 항목 앞에 해칭·색 견본 아이콘 16px. `FeaturePresets` 색과 해칭을 쓴다.
  - `src/app/KaFeatureCard.cpp:206-210, 250` — 저장된 상태에서는 칩을 숨기고, 바뀌었을 때만 Warn 「● 변경됨 · 아직 저장 안 됨」을 보인다.
  - `src/app/KaBasemapQuickCard.cpp`
    - 문구는 「배경은 자동으로 올리지 않습니다」.
    - 각주(11px muted)는 「리본의 「배경 지도」와 같은 동작입니다.」.
    - 단추는 테두리 단추 + 아이콘. 고른 것만 연파랑.
  - `src/app/MainWindowFeatureCard.cpp:65-83` — 그리는 동안 카드가 배경 지도 카드에 가려 잘리지 않게, 패널 본문을 `QScrollArea`에 넣는다.
- Test: `tests/test_inspector_panel.cpp`(ctest `inspector_panel`), `tests/test_feature_card.cpp`(`feature_card`)

- [ ] **Step 1: 시험 고치기·더하기**
  - 탭 아이콘 기대를 「아이콘 없음」으로.
  - 폭 272/300 → 21% 규칙(1920에서 403, 1366에서 287, 최소 260).
  - 새 시험:
    - `kindPeriodChipsShow`.
    - `periodComboHasSwatches`.
    - `savedStateHasNoChip`.
    - `basemapNoteSaysNoAutoLoad`.
    - `drawingCardIsNotClipped`.
- [ ] **Step 2–4. Step 5: 커밋.** `style(inspector): 유구 패널 탭·칩·배경 지도 카드를 목업대로 맞춘다`

### Task 10: 공통 밀도와 끝 확인

**Files:**
- Modify:
  - `src/app/KaTheme.cpp:207-223` — 본문 13 → 14px(큰 글씨 16).
  - QSS — 카드·패널 안쪽 여백을 4px 격자(12/16/20)로 한다. 단추 높이 32/36.
- Test: `tests/test_theme_options.cpp`(`theme_options`) — 13→15 기대를 14→16으로.

- [ ] **Step 1–4.** 위 시험과 전체 화면 시험(`theme_qss|theme_options|theme_render|chip_theme|shell_chrome|shell_widgets|shell_focus|home_start|home_cards|layer_list_chrome|layer_panel|inspector_panel|feature_card|layer_styles|editing_tools|icon_outline|ribbon_overflow|startup_splash`)을 통과시킨다.
- [ ] **Step 5: 커밋.** `style(theme): 본문 글자와 여백을 목업 밀도로 맞춘다`
- [ ] **Step 6: 끝 확인**
  1. 전체 Release 빌드, 전체 ctest(-j3). 실패는 -j1로 다시 돌린다.
  2. `--smoke-quit`가 0이어야 한다.
  3. 바탕화면 「Strata (개발)」을 연결한다.
  4. 1920×1080에서 홈과 지도(그리는 중, 유구 선택)를 찍어 `scratchpad\eval\`에 둔다. 목업과 나란히 놓고 차이표를 다시 만든다. 남은 「다름」은 「그대로 둘 것」만이어야 한다.
