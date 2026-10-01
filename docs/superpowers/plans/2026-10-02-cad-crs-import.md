# DXF·DWG 도면 읽어들이기 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 의뢰처 CAD 도면(DXF·DWG)을 좌표계를 알아내 작업 좌표계(5186·5187)로 바꿔 참조 지도에 제자리로 올리고, 좌표 없는 도면은 기존 정합으로 도면 전체를 맞춘다.

**Architecture:**
- core에 다섯 부품을 둔다. 각각 따로 시험한다.
  - `CadDwgConverter`: LibreDWG 실행.
  - `CadDrawingReader`: GDAL DXF 읽기.
  - `CadCrsGuess`: 좌표계 판단.
  - `CadDrawingStore`: 변환본 GPKG 쓰기, 정합 적용.
  - `CadDrawingLayers`: 참조 지도 묶음, 모양, 정합 보조.
- app에는 고르기 창 `KaCadCrsDialog`, 진행 창 `KaBlockingTask`, 흐름 `MainWindowCadImport.cpp`를 두고, 기존 진입점과 정합 도구에 연결 몇 줄만 넣는다.

**Tech Stack:** C++20, Qt 6.11 Widgets, QGIS 4.3 (qgis_core·qgis_gui), GDAL 3.14 C/C++ API(DXF 드라이버, OGRStyleMgr), LibreDWG 0.14 `dwg2dxf.exe`(별도 프로세스), QtTest/CTest.

**Spec:** `docs/superpowers/specs/2026-10-02-cad-crs-import-design.md`

## Global Constraints

- **사용자 결정 (2026-10-02):**
  - (1) DWG는 LibreDWG 0.14 공식 안정판을 앱·포터블에 넣어 읽는다.
  - (2) 조사 지역 가까이 놓이는 후보가 하나면 바로 올리고 「다른 좌표계로 바꾸기」를 보이며, 아니면 지역 이름이 붙은 목록에서 고른다.
  - (3) CAD 원래 색과 글자를 보이고, 흰색 계열은 `#3C3C3C`로 바꾼다.
  - (4) 변환본은 `<조사 폴더>/가져온자료/도면/<원본 이름>.gpkg`에 두고 원본은 손대지 않는다.
- **원본:** DXF·DWG는 읽기만 한다. 시험이 sha256으로 확인한다.
- **역할:** 도면은 참조 지도다(`LayerOps::markReferenceLayer` + `ka_hgis/imported_reference`). 조사 레이어(`ensureDomainLayer`)를 만들지 않는다.
- **좌표계:** 작업 좌표계는 프로젝트 좌표계(5186·5187)다. 5179를 가정하지 않는다.
- **글:** 사용자에게 보이는 글은 한국어다. GDAL·LibreDWG의 영어 문구는 `KaUserError::Spec::details`와 세션 기록(`KaSessionLog::line`, `[cad] ` 앞붙이)에만 둔다.
- **LibreDWG:**
  - 링크하지 않고 별도 프로세스로만 실행한다. 항상 `--as r2000`이다.
  - 작업 폴더에서 ASCII 이름(`source.dwg` → `source.dxf`)으로만 실행한다.
  - 바이너리는 `third_party/libredwg/0.14/`에서 온다. zip sha256은 `1ad7e15344d20b3426c3435b078d82fb84b35062815946b2cca9c5fc9810fea8`이다.
- **파일 크기:**
  - 새 파일은 300줄 이하다.
  - `MainWindow.cpp`, `MainWindowAlign.cpp`, `MainWindowRibbon.cpp`, `KaAlignMapTool.cpp`, `LayerOps.cpp`는 과제가 적은 연결 줄만 바꾼다. 늘어나는 줄은 과제마다 6줄 이하다. `MainWindow.cpp`는 지운 CAD 줄만큼 줄어든다.
- **지도 다시 그리기:** `LayerOps::refreshCanvasIfIdle`만 쓴다. 그리는 중에 `canvas->refresh()`를 하지 않는다.
- **QgsTask:** 작업 안에서는 `QgsProject`와 지도를 만지지 않는다.
- **Q_OBJECT 소스:** `R"(...)"`를 쓰지 않는다.
- **시험 규칙:**
  - `QTemporaryDir`만 쓰고, `QStandardPaths::setTestModeEnabled(true)`를 켜고, 앱 org/app 이름을 쓰지 않는다.
  - 새 ctest는 `CMakeLists.txt`의 PATH 환경 블록(약 1495줄, `georef_quality …` 줄과 같은 꼴)에 넣고 `TIMEOUT 120`을 준다.
  - `save_open_cad`는 `KA_SAVE_OPEN_TESTS`에 더한다.
- **빌드:**

  ```
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '$env:_MSPDBSRV_ENDPOINT_="ka-merge-1001"; $env:MSBUILDDISABLENODEREUSE="1"; . ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; cmake --build build --config Release --target ka-hgis <시험대상> -- /m:3 /p:CL_MPCount=4 /nodeReuse:false; exit $LASTEXITCODE'
  ```

- **시험:**

  ```
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; ctest --test-dir build -C Release -j3 --output-on-failure -R "^(이름)$"; exit $LASTEXITCODE'
  ```

  `save_open_*`는 `-j1`로 돌린다. 병렬일 때 `save_open_saveas`가 흔들린다.
- **커밋:**
  - 메시지는 한국어 `type(scope): 요약`이고, 본문에 시험 결과 한 줄을 적고, 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`를 단다.
  - Bash에서 경로를 적어 커밋한다(`git commit -- <경로들>`).

## Review Focus

1. **한글·괄호·빈칸이 든 파일 이름**(「[동국]소나무재선충 … (2010v).dwg」): 변환은 ASCII 작업 이름으로 하고, 묶음 제목과 GPKG 이름은 원래 이름을 따른다.
   - 시험: Task 1 `convert_readsTheBundledFixture`, Task 4 `outputPathFor_usesTheSurveyFolderAndNumbers`, Task 7 `cadDwgUsesTheBundledConverter`.
2. **망가진 DWG**: 한국어 오류가 나고, DXF·GPKG 찌꺼기가 남지 않는다.
   - 시험: Task 1 `convert_brokenDwgFailsCleanly`.
3. **아주 큰 도면을 취소**: 진행 창에서 취소하면 반쯤 쓴 파일이 남지 않는다.
   - 시험: Task 1 `convert_cancelKillsTheProcess`, Task 2 `read_cancelStops`, Task 4 `write_cancelLeavesNothing`.
4. **같은 도면을 두 번 불러오기**: 묶음은 하나이고 새 변환본을 가리킨다.
   - 시험: Task 5 `addToProject_replacesTheSameTitle`, Task 7 `cadSameDrawingTwiceKeepsOneGroup`.
5. **조사를 열지 않고 불러오기**: 앱 관리 폴더(`AppLocalData/cad-drawings`)에 쓰고, 그 경로는 `SurveyBundle::isAppManagedPath`가 참이다(저장 때 「가져온자료」로 모임).
   - 시험: Task 4 `outputPathFor_withoutSurveyUsesAppData`, Task 7 `cadNoSurveyOpenWritesIntoAppData`.

---

### Task 1: LibreDWG 넣기와 DWG → DXF 변환 (`CadDwgConverter`)

**Files:**
- Create:
  - `third_party/libredwg/0.14/dwg2dxf.exe`, `libredwg-0.dll`, `libiconv-2.dll`: `.superpowers/downloads/libredwg-0.14-win64.zip`에서 꺼낸다. 먼저 zip sha256을 확인한다.
  - `third_party/libredwg/0.14/COPYING`: `C:/Program Files/Git/usr/share/licenses/mintty/LICENSE.GPL`을 복사한다. sha256이 `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`인지 확인한다.
  - `third_party/libredwg/0.14/SOURCE.txt`에 다음을 적는다.
    - 버전 0.14, 받은 주소 `https://github.com/LibreDWG/libredwg/releases/download/0.14/libredwg-0.14-win64.zip`과 그 sha256.
    - 소스 주소 `https://github.com/LibreDWG/libredwg/releases/download/0.14/libredwg-0.14.tar.xz`와 sha256 `62ebb73b984f865960f20ed26619ea5f8789d5e3fd088fa40a2598384da81275`.
    - 「GPLv3 이상. Strata는 링크하지 않고 별도 프로그램으로 실행한다」.
- Create: `tests/data/cad/r2000-small.dxf`, `tests/data/cad/r2000-small.dwg`.
  - DXF 내용: 층 `WALL`에 LINE (100,200)–(110,200), TEXT `ABC`(높이 2.5, (105,205)).
  - DWG 만들기: zip의 `dxf2dwg.exe`로 `dxf2dwg --as r2000 -y -o r2000-small.dwg r2000-small.dxf`를 한 번 돌린다.
  - 최소 DXF를 받지 않으면 `ogr2ogr -f DXF`로 만든 DXF를 쓴다.
  - 만든 DWG를 `dwg2dxf --as r2000`으로 되돌려 GDAL이 LINE과 TEXT를 보는지 확인한 뒤 넣는다.
- Create: `src/core/CadDwgConverter.h`, `src/core/CadDwgConverter.cpp`, `tests/test_cad_dwg.cpp`.
- Modify: `CMakeLists.txt`.
  - `add_library(ka_core …)`에 새 파일을 더한다.
  - ka-hgis POST_BUILD(약 561줄 목록)에 `third_party/libredwg/0.14/`의 다섯 파일을 `$<TARGET_FILE_DIR:ka-hgis>/tools/libredwg/`로 `copy_if_different`한다.
  - 시험 대상 `ka_cad_dwg_tests`를 `ka_georef_tests`처럼 만들고, 정의 `KA_TEST_LIBREDWG="${CMAKE_SOURCE_DIR}/third_party/libredwg/0.14/dwg2dxf.exe"`, `KA_TEST_CAD_DATA="${CMAKE_SOURCE_DIR}/tests/data/cad"`를 준다. ctest 이름은 `cad_dwg`다.
- Modify: `scripts/make-portable.ps1`: `data` 복사 바로 뒤에서 `build\Release\tools\libredwg`를 `$out\tools\libredwg`로 복사한다.
- Modify: `scripts/verify-portable-pack.ps1`: 필수 목록에 `tools/libredwg/dwg2dxf.exe`, `tools/libredwg/libredwg-0.dll`, `tools/libredwg/COPYING`를 더한다.
- Modify: `THIRD_PARTY_NOTICES.md`: 「앱이 링크하는 핵심」 표 아래에 「함께 두는 별도 프로그램」 표를 만든다. 줄 하나: LibreDWG 0.14 | GPLv3 이상, 별도 프로세스 | 소스 주소·sha256.
- Modify: `src/app/MainWindow.cpp`의 `showAbout`: 「본 소프트웨어는 GNU GPL v2 이상으로 배포됩니다.」 줄 앞에 한 줄을 넣는다: `"DWG 변환: GNU LibreDWG 0.14 (GPLv3 이상, 별도 프로그램 tools/libredwg)\n"`.
- Modify: `docs/ERROR_REGRESSION.md`에 한 줄을 더한다.
  - 증상: 2004 이후 DWG를 넣으면 열리지 않고 영어 GDAL 오류만 기록된다.
  - 원인: libopencad는 R2000만 읽는다.
  - 고침: LibreDWG `--as r2000`.
  - 시험: `cad_dwg`.

**Interfaces:**
- Produces:

```cpp
namespace CadDwgConverter {
// <appDir>/tools/libredwg/dwg2dxf.exe 가 있으면 그 경로, 없으면 빈 문자열.
QString bundledToolIn(const QString& appDir);
QString bundledTool();  // bundledToolIn(QCoreApplication::applicationDirPath())
QString converterLabel();  // "LibreDWG 0.14"
struct Result {
  bool ok = false;
  QString dxfPath;  // <outDir>/source.dxf
  QString error;    // 한국어 한 줄
  QString details;  // dwg2dxf stderr 마지막 비어 있지 않은 줄, 없으면 "종료 코드 N"
};
// dwgPath 를 <outDir>/source.dwg 로 복사하고, 작업 폴더 outDir 에서
// `<tool> --as r2000 -y -o source.dxf source.dwg` 를 창 없이 돌린다. canceled 는 100 ms마다 본다.
// 취소·시간 초과·실패이면 프로세스를 끝내고 source.dwg·source.dxf 를 지운다. 성공이면 source.dwg 만 지운다.
Result convert(const QString& dwgPath, const QString& outDir, const QString& tool,
               int timeoutMs = 120000, const std::function<bool()>& canceled = {});
}
```

  오류 문구는 정확히 다음과 같다.
  - 도구 없음: `DWG 변환 도구(LibreDWG)를 찾지 못했습니다.` (details에 찾은 경로)
  - 실패나 출력 없음: `이 DWG를 읽지 못했습니다.`
  - 시간 초과: `DWG 변환이 너무 오래 걸려 멈췄습니다.`
  - 취소: `DWG 변환을 취소했습니다.`

- [ ] **Step 1: 시험을 쓴다** (`tests/test_cad_dwg.cpp`, main은 `test_survey_durability.cpp`와 같은 꼴)

```cpp
void convert_readsTheBundledFixture();
// 고정 DWG를 임시 폴더에 「[동국] 시험 도면(2010v).dwg」로 복사해 convert() → ok,
// dxfPath == out/"source.dxf", GDALOpenEx(DXF)로 열면 entities 에 Layer=="WALL" 인 도형이 2개 이상
void convert_leavesTheOriginalUntouched();   // 입력 sha256 이 convert 전후 같다
void convert_missingToolSaysSoInKorean();    // tool = tmp/"없음.exe" → !ok, error == "DWG 변환 도구(LibreDWG)를 찾지 못했습니다."
void convert_brokenDwgFailsCleanly();        // 4096바이트 의사난수 "broken.dwg" → !ok, error == "이 DWG를 읽지 못했습니다.", outDir 에 *.dxf·source.dwg 없음
void convert_cancelKillsTheProcess();        // canceled 가 처음부터 true → !ok, error == "DWG 변환을 취소했습니다.", outDir 에 *.dxf 없음
void bundledToolIn_findsOnlyAnExistingTool(); // 빈 임시 폴더 → ""; tools/libredwg/dwg2dxf.exe 파일을 만들면 그 경로
```

- [ ] **Step 2: 실패를 확인한다.** 빌드 대상은 `ka_cad_dwg_tests`이고, 시험은 `cad_dwg`다. `CadDwgConverter`가 없으니 빌드가 실패해야 한다.
- [ ] **Step 3: `CadDwgConverter`를 만든다.**
  - `QProcess`를 쓴다(`setWorkingDirectory(outDir)`, 인자 목록, 창 없음).
  - 기다릴 때는 `waitForFinished(100)`을 되풀이하며 `canceled`와 시간을 본다.
  - 실패한 경우에는 출력 DXF를 지운다.
- [ ] **Step 4: 통과를 확인한다.**
  - `cad_dwg` 6개가 통과해야 한다.
  - ka-hgis를 빌드한 뒤 `build/Release/tools/libredwg/dwg2dxf.exe`가 있어야 한다.
  - 스모크(`--smoke-quit`)가 0으로 끝나야 한다.
  - 두 ps1이 파싱돼야 한다: `[System.Management.Automation.Language.Parser]::ParseFile(<경로>,[ref]$null,[ref]$e); $e.Count -eq 0`.
- [ ] **Step 5: 커밋한다.** 새·바뀐 파일을 모두 경로로 적는다. 메시지: `feat(cad): DWG를 LibreDWG 0.14(별도 프로그램)로 DXF로 바꾼다`.

### Task 2: DXF 읽기 (`CadDrawingReader`)

**Files:**
- Create: `src/core/CadDrawingReader.h`, `src/core/CadDrawingReader.cpp`.
- Create: `tests/cad_fixture.h`: DXF 바이트를 만드는 도우미. Task 5·7·8도 쓴다.
- Create: `tests/test_cad_reader.cpp`.
- Modify: `CMakeLists.txt`(ka_core 목록, `ka_cad_reader_tests` / ctest `cad_reader`, 환경 블록).

**Interfaces:**
- Produces:

```cpp
enum class CadKind { Line, Fill, Point, Text };
struct CadEntity {
  CadKind kind = CadKind::Line;
  QgsGeometry geometry;   // 2D, 한 조각
  QString cadLayer;
  QColor color;           // 불투명. 밝은 색은 이미 #3C3C3C
  QString text;           // Text 만
  double textHeight = 0;  // 도면 단위(m)
  double textAngle = 0;   // 도, 반시계
  int textAnchor = 1;     // OGR LABEL p: (1~12)
};
struct CadDrawing {
  QVector<CadEntity> entities;  // 모형 공간만
  QgsRectangle robustExtent;    // 도형 중심 X·Y 각각 5~95 %
  int paperSpaceSkipped = 0;
  int gdalWarnings = 0;
  bool readAsUtf8 = false;
};
namespace CadDrawingReader {
bool looksUtf8(const QByteArray& bytes);         // 올바른 UTF-8 이고 0x7F 를 넘는 바이트가 하나 이상
QColor colorFromStyle(const QString& ogrStyle);   // 아래 규칙
QgsRectangle robustExtent(const QVector<QgsPointXY>& centres);
bool read(const QString& dxfPath, CadDrawing* out, QString* error, QString* details,
          const std::function<bool()>& canceled = {});
}
// tests/cad_fixture.h
namespace CadFixture {
QByteArray document(const QByteArray& codepage, const QByteArray& entities);  // HEADER($ACADVER AC1015, $DWGCODEPAGE) + ENTITIES, CRLF
QByteArray line(const char* layer, int aci, double x1, double y1, double x2, double y2, bool paper = false, double z = 0);
QByteArray closedPolyline(const char* layer, int aci, const QVector<QPointF>& pts);
QByteArray text(const char* layer, int aci, double x, double y, double height, double angle, const QByteArray& value);
QByteArray point(const char* layer, int aci, double x, double y);
QByteArray solid(const char* layer, int aci, const QVector<QPointF>& corners);  // 3~4 꼭짓점
bool write(const QString& path, const QByteArray& bytes);
}
```

- 규칙:
  - **열기:** `GDALOpenEx`로 DXF 드라이버만, 읽기 전용으로 연다.
    - `looksUtf8(파일 바이트)`가 참이면 열기 옵션 `ENCODING=UTF-8`을 준다.
    - 오류 처리기는 경고 수만 세는 조용한 처리기다(`CPLPushErrorHandlerEx`).
  - **종이 공간:** `PaperSpace`가 1인 도형은 세기만 하고 건너뛴다.
  - **종류:**
    - Point·MultiPoint: `Text` 필드가 비어 있지 않으면 Text, 아니면 Point.
    - 선 종류(곡선 포함): Line. 직선 조각으로 바꾼다.
    - 면 종류: Fill.
    - GeometryCollection: 조각마다 자기 종류를 따른다.
    - 여러 조각 도형은 조각마다 한 개로 나눈다. Z·M은 버린다.
  - **글자:** 스타일 LABEL의 `s:`가 `textHeight`, `a:`가 `textAngle`(없으면 0), `p:`가 `textAnchor`(없으면 1)다. `dx`·`dy`는 무시한다. 스타일은 `OGRStyleMgr`/`OGRStyleTool`로 읽는다.
  - **`colorFromStyle`:**
    - PEN, BRUSH, LABEL, SYMBOL 가운데 처음 나오는 색을 쓴다. `#RRGGBB[AA]`의 AA는 무시한다.
    - 휘도 (0.2126 R + 0.7152 G + 0.0722 B) / 255가 0.9 이상이면 `#3C3C3C`다.
    - 색이 없으면 `#000000`이다.
  - **`robustExtent`:** 값을 정렬해 nearest-rank로 자른다. 아래 끝은 `floor(0.05·(n−1))`, 위 끝은 `ceil(0.95·(n−1))` 번째 값이다. 입력이 없으면 빈 사각형이다.
  - **오류 문구**(정확히):
    - `DXF를 열지 못했습니다.` (details는 `CPLGetLastErrorMsg`)
    - `도면에 모형 공간 도형이 없습니다.` (종이 공간 도형이 있으면 details `종이 공간 도형 %1개만 있습니다.`)
    - `도면 읽기를 취소했습니다.`

- [ ] **Step 1: 시험을 쓴다**

```cpp
void looksUtf8_tellsUtf8FromCp949();
// looksUtf8("가수리".toUtf8()) 참, CP949 바이트 거짓(QStringEncoder("CP949") 또는 고정 바이트 B0 A1 BC F6 B8 AE), "ABC" 거짓
void colorFromStyle_ignoresAlphaAndDarkensWhite();
// "PEN(c:#00000e00)" → QColor(0,0,14) alpha 255; "PEN(c:#ffffff)" → #3c3c3c;
// "LABEL(f:\"Arial\",t:\"x\",c:#00ff00)" → #00ff00; "" → #000000
void read_skipsPaperSpaceAndSortsKinds();
// LINE, 종이 공간 LINE, 닫힌 LWPOLYLINE, TEXT, POINT, SOLID →
// Line 2(선+닫힌 폴리선), Text 1, Point 1, Fill 1, paperSpaceSkipped == 1
void read_utf8TextUnderAnAnsi949Header();   // "가수리 374-1전" UTF-8 바이트 + ANSI_949 → text 같음, readAsUtf8 참
void read_cp949TextUnderAnAnsi949Header();  // 같은 글자 CP949 바이트 → text 같음, readAsUtf8 거짓
void read_textHeightAngleAnchorIgnoringOffsets();
// TEXT 높이 2.5 각도 30 (387050,280025) → textHeight 2.5, textAngle 30, textAnchor 1, 점 (387050,280025)
void read_dropsZ();                          // z=12 LINE → QgsWkbTypes::hasZ(geometry.wkbType()) 거짓
void read_robustExtentIgnoresFarAway();
// (387000..387100, 280000..280100) 안 LINE 40개 + (445796,195312) TEXT 1개 →
// robustExtent 가 (386990,279990)-(387110,280110) 안
void read_onlyPaperSpaceSaysSo();
// 종이 공간 LINE 2개뿐 → false, error "도면에 모형 공간 도형이 없습니다.", details "종이 공간 도형 2개만 있습니다."
void read_leavesTheFileUntouched();          // 파일 바이트가 read 전후 같다
void read_cancelStops();                     // canceled 가 늘 true → false, error "도면 읽기를 취소했습니다."
```

- [ ] **Step 2: 실패를 확인한다** (`cad_reader`: 빌드 실패).
- [ ] **Step 3: 읽기를 만든다.** 모형은 `TopographicCatalog::readInput`(`src/core/TopographicCatalog.cpp:151-200`)의 열기와 오류 처리기 꼴이다.
- [ ] **Step 4: 통과를 확인한다** (`cad_reader` 11개 통과).
- [ ] **Step 5: 커밋한다.** 메시지: `feat(cad): DXF를 종이 공간 없이 선·면·점·글자로 읽는다`.

### Task 3: 좌표계 알아내기 (`CadCrsGuess`)

**Files:**
- Create: `src/core/CadCrsGuess.h`, `src/core/CadCrsGuess.cpp`, `tests/test_cad_crs.cpp`.
- Modify: `CMakeLists.txt`(ka_core, `ka_cad_crs_tests` / ctest `cad_crs`, 환경 블록).

**Interfaces:**
- Consumes: `KoreaRegionCatalog::sidoNames()`, `KoreaRegionCatalog::overviewBounds()`, `LayerRole::resolve()`.
- Produces:

```cpp
struct CadCrsCandidate {
  QString authId;              // "EPSG:5174"
  QString label;               // label(authId)
  QString region;              // "경상북도"
  QgsPointXY centreWork;       // robustExtent 중심을 작업 좌표계로
  double distanceToSiteM = -1; // 조사 위치가 없으면 -1
};
enum class CadCrsVerdict { Certain, Choose, NoCrs };
struct CadCrsResult {
  CadCrsVerdict verdict = CadCrsVerdict::NoCrs;
  QVector<CadCrsCandidate> candidates;  // [0] 이 제안. NoCrs 이면 비어 있다
};
namespace CadCrsGuess {
QStringList candidateAuthIds();
QString label(const QString& authId);
CadCrsResult guess(const QgsRectangle& robustExtent, const QgsCoordinateReferenceSystem& workCrs,
                   const std::optional<QgsPointXY>& siteWork, const QgsCoordinateTransformContext& context);
std::optional<QgsPointXY> siteLocation(const QgsProject* project, const QgsRectangle& canvasExtentWork);
QString describe(const CadCrsCandidate& candidate);
}
```

- 값과 규칙:
  - **`candidateAuthIds()`** (이 순서가 우선순위):
    - EPSG:5186, 5187, 5185, 5188
    - EPSG:5174, 5176, 5173, 5177, 5175
    - EPSG:5181, 5183, 5180, 5184, 5182
    - EPSG:2097, 2096, 2098
    - EPSG:5179, 32652, 32651, 4326
  - **`label()`:**
    - 5186 `GRS80 중부원점(2010)`, 5187 `GRS80 동부원점(2010)`, 5185 `GRS80 서부원점(2010)`, 5188 `GRS80 동해원점(2010)`
    - 5174 `베셀 중부원점 보정`, 5176 `베셀 동부원점 보정`, 5173 `베셀 서부원점 보정`, 5177 `베셀 동해원점 보정`, 5175 `베셀 제주원점 보정`
    - 5181 `GRS80 중부원점(2002)`, 5183 `GRS80 동부원점(2002)`, 5180 `GRS80 서부원점(2002)`, 5184 `GRS80 동해원점(2002)`, 5182 `GRS80 제주원점(2002)`
    - 2097 `베셀 중부원점`, 2096 `베셀 동부원점`, 2098 `베셀 서부원점`
    - 5179 `UTM-K`, 32652 `UTM 52N`, 32651 `UTM 51N`, 4326 `경위도(WGS84)`
  - **그럴듯함:**
    - 범위 중심을 EPSG:4326으로 바꿔 `sidoNames()` 순서로 처음 들어가는 `overviewBounds`의 시·도가 `region`이다. 아무 데도 안 들면 버린다.
    - 범위의 긴 변이 투영 후보는 100000 이하, 4326은 1.0 이하여야 한다.
    - 바꾸기가 예외를 내면 버린다.
  - **거리:** 작업 좌표계에서 잰 직선거리(m)다.
  - **무리:** 우선순위 순서로, 첫 구성원의 `centreWork`에서 2000 m 안이면 그 무리에 넣고, 아니면 새 무리를 만든다.
  - **판정:**
    - Certain: 조사 위치가 있고, 구성원 최소 거리가 10000 m 이하인 무리가 정확히 하나다. 그 무리의 첫(우선순위) 구성원이 제안이다.
    - NoCrs: 그럴듯한 후보가 없다.
    - Choose: 나머지 경우다.
  - **`candidates` 순서:**
    - Certain: 제안, 같은 무리 나머지(우선순위), 다른 무리(무리 거리순, 안에서는 우선순위).
    - 조사 위치가 있는 Choose: 무리 거리순.
    - 조사 위치가 없는 Choose: 우선순위순.
  - **`describe()`:** `"%1 부근 · %2 (%3)"`. 거리가 0 이상이면 `" · 조사 지역에서 %4 km"`를 붙이고, km는 소수 한 자리다.
  - **`siteLocation()`:**
    1. `LayerRole::resolve == Survey`이고 도형이 있는 레이어들의 범위를 프로젝트 좌표계로 합친 중심.
    2. 없으면 `canvasExtentWork`가 비어 있지 않고 폭이 50000 이하일 때 그 중심.
    3. 둘 다 아니면 `nullopt`.

- [ ] **Step 1: 시험을 쓴다**

```cpp
void candidateAuthIds_followTheSpecOrder();     // 위 21개 그대로
void guess_gasuriNumbersNearYeongcheonAreCertain5174();
// 범위 (387199.70,279739.90)-(388199.70,280739.90), 작업 5187, 조사 위치 (207440.79,378546.00)
// → Certain, [0] "EPSG:5174", region "경상북도", distance < 1000, [1]·[2] 이 "EPSG:5181"·"EPSG:2097"
void guess_withoutSiteListsPlacesToChoose();
// 같은 범위, 조사 위치 없음 → Choose, [0] "EPSG:5186" region "부산광역시",
// "EPSG:5174" 가 region "경상북도" 로 들어 있다, 모든 distance == -1
void guess_localNumbersHaveNoCrs();             // (0,0)-(1000,800) → NoCrs, candidates 비어 있음
void guess_jejuNumbersWithJejuSiteAreCertain5186();
// (148078.47,98110.53)-(148107.88,98129.23), 작업 5186, 조사 위치 = 범위 중심 → Certain "EPSG:5186" region "제주특별자치도"
void guess_degreesAreWgs84();
// (129.07,35.99)-(129.09,36.01), 작업 5187, 조사 위치 = (129.08,36.0) 을 5187 로 → Certain "EPSG:4326"
void guess_utmkIsFound();
// (129.08,36.0) 을 5179 로 바꾼 점 ±300, 조사 위치 같은 점의 5187 → Certain "EPSG:5179"
void describe_readsLikeTheSpec();
// {"EPSG:5174", label, "경상북도", {}, 1234.0} → "경상북도 부근 · 베셀 중부원점 보정 (EPSG:5174) · 조사 지역에서 1.2 km"
void siteLocation_prefersSurveyLayersThenANarrowView();
// survey 역할 메모리 레이어(사각형 1개) → 그 중심; 없으면 폭 10 km 화면 → 화면 중심; 폭 80 km 화면 → nullopt
```

- [ ] **Step 2: 실패를 확인한다** (`cad_crs`).
- [ ] **Step 3: 만든다.**
- [ ] **Step 4: 통과를 확인한다** (`cad_crs` 9개 통과).
- [ ] **Step 5: 커밋한다.** 메시지: `feat(cad): 도면 숫자로 좌표계 후보를 찾고 조사 지역 가까이면 바로 고른다`.

### Task 4: 변환본 쓰기와 정합 적용 (`CadDrawingStore`)

**Files:**
- Create: `src/core/CadDrawingStore.h`, `src/core/CadDrawingStore.cpp`, `tests/test_cad_store.cpp`.
- Modify: `CMakeLists.txt`(ka_core, `ka_cad_store_tests` / ctest `cad_store`, 환경 블록).

**Interfaces:**
- Consumes: Task 2의 `CadDrawing`/`CadEntity`, `GeorefService::Affine`·`transformGeometry`, `SurveyBundle::collectedFolderName()`.
- Produces:

```cpp
struct CadStoreInfo {
  QString sourcePath;
  QString sourceSha256;
  QString sourceCrs;   // "EPSG:5174" 또는 "" (좌표 없음)
  QString converter;   // "LibreDWG 0.14" 또는 ""
};
namespace CadDrawingStore {
QString outputPathFor(const QString& sourcePath, const QString& surveyDir);
bool write(const CadDrawing& drawing, const CadStoreInfo& info, const QgsCoordinateReferenceSystem& workCrs,
           const QgsCoordinateTransformContext& context, const QString& outPath, QString* error,
           const std::function<bool()>& canceled = {});
CadStoreInfo readInfo(const QString& gpkgPath);
QStringList tableNames(const QString& gpkgPath);   // "lines","fills","points","texts" 가운데 있는 것, 이 순서
bool applyAffine(const QList<QgsVectorLayer*>& layers, const GeorefService::Affine& a, QString* error);
}
```

- 값과 규칙:
  - **`outputPathFor`:**
    - `<surveyDir>/가져온자료/도면/<completeBaseName>.gpkg`. 있으면 ` (2)`, ` (3)`을 붙인다.
    - `surveyDir`가 비어 있으면 `QStandardPaths::writableLocation(AppLocalDataLocation) + "/cad-drawings/"` 아래다.
  - **표:**
    - `lines`(LineString), `fills`(Polygon), `points`(Point), `texts`(Point) 가운데 도형이 있는 것만 만든다.
    - 필드는 네 표 모두 `cad_layer`(문자), `color`(`#rrggbb`), `text`(문자), `text_height`(실수), `text_angle`(실수), `text_anchor`(정수)다.
    - 정보 표 `ka_cad_drawing`(도형 없음): `source_path`, `source_sha256`, `source_crs`, `converter`, `created_at`(ISO 8601) 한 줄.
  - **좌표:**
    - `sourceCrs`가 있으면 `QgsCoordinateTransform(source, workCrs, context)`으로 바꾼다. 실패하면 `도면 좌표를 작업 좌표계로 바꾸지 못했습니다.`
    - 없으면 숫자 그대로 `workCrs`로 적는다.
  - **쓰기:**
    - `<outPath>.part`에 다 쓴 뒤 이름을 바꾼다.
    - `outPath`가 이미 있으면 쓰지 않고 `같은 이름의 변환본이 이미 있습니다.`
    - 실패하면 `변환본을 쓰지 못했습니다.`, 취소하면 `도면 변환을 취소했습니다.`. 두 경우 모두 `.part`를 지운다.
  - **`applyAffine`:**
    - 레이어마다 `transformGeometry`로 바꿔 `dataProvider()->changeGeometryValues`로 쓴다.
    - `text_angle` 필드가 있으면 angle += θ, height ×= s로 `changeAttributeValues`한다. θ = atan2(d, a)를 도로, s = hypot(a, d)다.
    - 어느 레이어든 실패하면 앞서 바꾼 레이어에 기억해 둔 바꾸기 전 도형·속성을 다시 쓰고 false, `맞춘 결과를 도면 전체에 적용하지 못했습니다.`

- [ ] **Step 1: 시험을 쓴다**

```cpp
void outputPathFor_usesTheSurveyFolderAndNumbers();
// source "C:/x/[동국] 시험 도면(2010v).dwg" → <tmp>/가져온자료/도면/[동국] 시험 도면(2010v).gpkg; 그 파일을 만들면 " (2).gpkg"
void outputPathFor_withoutSurveyUsesAppData();   // "" → AppLocalData/cad-drawings 아래, SurveyBundle::isAppManagedPath 참
void write_moves5174NumbersInto5187();
// 선 (387699.70,280239.90)-(387709.70,280239.90), sourceCrs 5174, 작업 5187 → 첫 꼭짓점 (207440.79,378546.00) ±0.05
void write_keepsNumbersWithoutCrs();             // sourceCrs "" → 첫 꼭짓점이 입력과 같다
void write_onlyNonEmptyTablesWithFieldsAndInfo();
// 선 + 글자 → tableNames == {"lines","texts"}; 필드 6개 이름; readInfo 의 sourceCrs "EPSG:5174", converter "LibreDWG 0.14", sha256 같음
void write_neverOverwrites();                    // outPath 가 이미 있으면 false, error "같은 이름의 변환본이 이미 있습니다.", 파일 바이트 그대로
void write_cancelLeavesNothing();                // canceled 늘 true → false, outPath·.part 모두 없음
void applyAffine_movesEveryTableAndTurnsTexts();
// lines·texts 레이어, 아핀 a=0 b=-2 c=10 d=2 e=0 f=20 (90°·2배·(10,20)) →
// 선 꼭짓점 (1,0)→(10,22); 글자 angle 30→120, height 2.5→5.0
void applyAffine_restoresEarlierTablesWhenOneFails();
// 둘째 레이어를 읽기 전용 속성 파일 사본에서 열어 실패시킨다 → false, 첫 레이어 도형이 원래 그대로
```

- [ ] **Step 2: 실패를 확인한다** (`cad_store`).
- [ ] **Step 3: 만든다.** 쓰기는 `QgsVectorFileWriter::create`(GPKG, 표마다 `CreateOrOverwriteLayer`)로 한다.
- [ ] **Step 4: 통과를 확인한다** (`cad_store` 9개 통과).
- [ ] **Step 5: 커밋한다.** 메시지: `feat(cad): 도면을 조사 폴더 가져온자료에 작업 좌표계 GPKG로 쓴다`.

### Task 5: 지도에 올리기 (`CadDrawingLayers`)

**Files:**
- Create: `src/core/CadDrawingLayers.h`, `src/core/CadDrawingLayers.cpp`, `tests/test_cad_layers.cpp`.
- Modify: `CMakeLists.txt`(ka_core, `ka_cad_layers_tests` / ctest `cad_layers`, 환경 블록).

**Interfaces:**
- Consumes: Task 4의 GPKG 표와 필드, `HeritageImport::referenceGroupName()`, `LayerOps::markReferenceLayer`.
- Produces:

```cpp
namespace CadDrawingLayers {
constexpr const char* kPropDrawing = "ka_hgis/cad_drawing";  // 값 = 도면 id (QUuid, 괄호 없음)
QString groupTitle(const QString& sourcePath);               // "<completeBaseName> (도면)"
QList<QgsVectorLayer*> addToProject(QgsProject* project, const QString& gpkgPath, const QString& title,
                                    const QString& drawingId, QString* error);
QList<QgsVectorLayer*> layersOf(const QgsProject* project, const QString& drawingId);
QgsVectorLayer* alignLayerOf(const QgsProject* project, const QString& drawingId);  // 선 → 면 → 점 → 글자
void removeFromProject(QgsProject* project, const QString& drawingId);
QString drawingIdOfGroup(const QgsProject* project, const QString& title);  // 그 제목 묶음의 도면 id, 없으면 ""
}
```

- 값과 규칙:
  - **묶음:** 루트의 `참조 지도`(없으면 만든다) 아래 제목 `title`의 묶음이다. 같은 제목 묶음이 있으면 그 레이어와 묶음을 먼저 지운다. 만드는 방식은 `SurveyContourStyle::apply`와 같다.
  - **레이어:** 위에서부터 `texts`→「글자」, `points`→「점」, `lines`→「선」, `fills`→「면」(있는 표만).
  - **태그:** `markReferenceLayer`, `ka_hgis/imported_reference` = true, `kPropDrawing` = drawingId.
  - **모양:**
    - 선: 너비 0.26 mm, 선 색 데이터 정의 `"color"`.
    - 면: 채움 색 `set_color_part("color", 'alpha', 77)`, 테두리 `"color"` 0.26 mm.
    - 점: 원 1.5 mm, 색 `"color"`.
    - 글자:
      - 기호 없음(`QgsNullSymbolRenderer`). 라벨 필드 `text`, 글꼴 `Malgun Gothic`, 크기 단위는 지도 단위다.
      - 데이터 정의: 크기 `"text_height"`, 회전 `-"text_angle"`(QGIS 라벨 회전은 시계 방향), 색 `"color"`.
      - 놓기는 OverPoint이고, 사분면은 다음 식이다: `CASE "text_anchor" WHEN 1 THEN 2 WHEN 10 THEN 2 WHEN 2 THEN 1 WHEN 11 THEN 1 WHEN 3 THEN 0 WHEN 12 THEN 0 WHEN 4 THEN 5 WHEN 5 THEN 4 WHEN 6 THEN 3 WHEN 7 THEN 8 WHEN 8 THEN 7 WHEN 9 THEN 6 ELSE 2 END`.

- [ ] **Step 1: 시험을 쓴다** (GPKG는 Task 4 `write`로 만든다)

```cpp
void groupTitle_isTheFileNamePlusDrawing();    // "C:/a/[동국] 도면(2010v).dwg" → "[동국] 도면(2010v) (도면)"
void addToProject_buildsTheGroupUnderReference();
// 선+글자 → 루트/"참조 지도"/제목 의 자식 이름 {"글자","선"} 이 순서; 레이어마다 LayerRole::stored == Reference,
// imported_reference 참, kPropDrawing == id; 프로젝트에 이름 "entities" 레이어 없음
void addToProject_stylesFollowTheSpec();
// 선 기호 너비 0.26 mm·선 색 식 "\"color\""; 글자 렌더러 type() == "nullSymbol";
// 라벨 fieldName "text", 크기 단위 MapUnits, 식 크기 "\"text_height\"" 회전 "-\"text_angle\"" 색 "\"color\""
void addToProject_replacesTheSameTitle();      // 같은 제목 두 번 → 묶음 1개, 레이어 2개
void layersOf_andAlignLayerOf();               // alignLayerOf == 「선」; 글자만 있으면 「글자」
void removeFromProject_dropsLayersAndGroup();  // 레이어 0개, 제목 묶음 없음
```

- [ ] **Step 2: 실패를 확인한다** (`cad_layers`).
- [ ] **Step 3: 만든다.**
- [ ] **Step 4: 통과를 확인한다** (`cad_layers` 6개 통과).
- [ ] **Step 5: 커밋한다.** 메시지: `feat(cad): 도면을 참조 지도 아래 원래 색·글자 묶음으로 올린다`.

### Task 6: 좌표계 고르기 창 (`KaCadCrsDialog`)

**Files:**
- Create: `src/app/KaCadCrsDialog.h`, `src/app/KaCadCrsDialog.cpp`, `tests/test_cad_dialog.cpp`.
- Modify: `CMakeLists.txt`.
  - ka-hgis 목록에 새 파일을 더한다.
  - `ka_cad_dialog_tests`는 `ka_gdal_log_tests`처럼 `src/app/KaCadCrsDialog.cpp`를 직접 묶는다. ctest `cad_dialog`, 환경 블록.

**Interfaces:**
- Consumes: Task 3의 `CadCrsResult`, `CadCrsGuess::describe`.
- Produces:

```cpp
class KaCadCrsDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaCadCrsDialog(const CadCrsResult& guess, QWidget* parent = nullptr);
  // 단추를 누른 뒤의 결과: 0 이상 = 후보 줄, -1 = 좌표 없는 도면(직접 맞추기), nullopt = 취소·아직
  std::optional<int> outcome() const;
  static std::optional<int> choose(QWidget* parent, const CadCrsResult& guess);  // 시험용 함수가 있으면 그것, 아니면 exec()
  static void setChooserForTests(std::function<std::optional<int>(const CadCrsResult&)> chooser);  // 빈 함수면 되돌림
};
```

- 화면:
  - 창 제목 `도면 좌표계 고르기`, 안내 `도면의 좌표계를 고르세요. 지역 이름이 조사 지역과 맞는 줄을 고르면 됩니다.`
  - 목록 `cadCrsList`: 줄마다 `describe()`이고, 첫 줄이 골라져 있다.
  - 단추: `cadCrsAccept` 「이 좌표계로 불러오기」, `cadCrsNoCrs` 「좌표 없는 도면으로 — 직접 맞추기」, `cadCrsCancel` 「취소」.

- [ ] **Step 1: 시험을 쓴다**

```cpp
void dialog_listsDescribedCandidatesWithTheFirstSelected();  // 줄 글자 == describe(후보), currentRow 0, 창 제목
void dialog_buttonsGiveIndexNoCrsOrCancel();
// 2번 줄 고르고 cadCrsAccept 클릭 → outcome 2; cadCrsNoCrs → -1; cadCrsCancel → nullopt (창마다 새로)
void choose_usesTheTestChooser();                              // 시험용 함수가 1 → choose == 1, 끝나면 되돌림
```

- [ ] **Step 2: 실패를 확인한다** (`cad_dialog`).
- [ ] **Step 3: 만든다.**
- [ ] **Step 4: 통과를 확인한다** (`cad_dialog` 3개 통과, `--smoke-quit` 0).
- [ ] **Step 5: 커밋한다.** 메시지: `feat(cad): 좌표계 후보를 지역 이름과 함께 고르는 창`.

### Task 7: 불러오기 흐름과 진입점 연결

**Files:**
- Create:
  - `src/app/KaBlockingTask.h`, `src/app/KaBlockingTask.cpp`.
  - `src/app/MainWindowCadImport.cpp`: `MainWindow` 멤버 함수.
- Modify: `src/app/MainWindow.h`.
  - 공개(`addVectorFromPath` 옆): `importCadDrawing`.
  - 비공개: `openCadDrawingFile`, `showCadDrawingNotice`, `startCadDrawingAlign`.
- Modify: `src/app/MainWindow.cpp`.
  - `addVectorFromPath` 첫 문장에 `if (GeorefService::isCadPath(path)) return importCadDrawing(path);`를 넣는다.
  - `takeLayer` 안의 `isCadPath` 갈래와 `added.isEmpty()`일 때의 dwg/dxf 안내 덩어리는 이제 쓰이지 않으므로 지운다.
  - 파일함 `fileActivated` 람다 맨 앞에 `if (GeorefService::isCadPath(path)) { importCadDrawing(path); return; }`를 넣는다.
- Modify: `src/app/MainWindowAlign.cpp`의 `georefAssistant`: 파일을 고른 직후 CAD면 `importCadDrawing(path); return;`.
- Modify: `src/app/MainWindowRibbon.cpp`의 더보기: 「맞추기」 다음 줄에 `moreMenu->addAction(KaIcons::icon(QStringLiteral("import")), QStringLiteral("도면 불러오기 (DXF·DWG)"), this, &MainWindow::openCadDrawingFile);`.
- Modify: `CMakeLists.txt`.
  - ka-hgis 목록에 새 app 파일을 더한다. `ka_save_open_tests`는 ka-hgis의 SOURCES를 그대로 가져온다(`CMakeLists.txt:1360-1362`).
  - `ka_add_qtest_filter(save_open_cad ka_save_open_tests …)`를 만들고 `KA_SAVE_OPEN_TESTS`에 `save_open_cad`를 더한다.
- Modify: `tests/test_save_open.cpp`(`#include "cad_fixture.h"`), `docs/ERROR_REGRESSION.md`(두 줄).
- 지우기 전에 `tests/`에서 「이 CAD 파일을 열지 못했습니다」·「DXF는 보통 열리고」를 찾는다. 걸리는 시험은 새 흐름을 확인하도록 바꾸고, 확인하던 내용은 남긴다.

**Interfaces:**
- Consumes: Task 1~6 전부.
- Produces:

```cpp
// MainWindow (public)
inline constexpr const char* kCadNoCrs = "none";  // MainWindowCadImport 쪽 헤더나 MainWindow.h
bool importCadDrawing(const QString& path, const QString& forcedAuthId = QString());  // "" = 판단, "EPSG:x" = 그 좌표계, kCadNoCrs = 좌표 없음
// MainWindow (private)
void openCadDrawingFile();
void showCadDrawingNotice(const QString& drawingId, const QString& sourcePath, const CadCrsResult& guess,
                          const QString& usedAuthId);
void startCadDrawingAlign(const QString& drawingId);   // Task 7: alignLayerOf 로 startAlignSession. Task 8 이 넓힌다
// KaBlockingTask
namespace KaBlockingTask {
// work 를 QgsTask 로 돌리는 동안 창 모달 진행 창(label, 「취소」)을 보이고 끝날 때까지 기다린다. 취소면 false.
bool run(QWidget* parent, const QString& label, const std::function<bool(QgsFeedback*)>& work);
}
```

- `importCadDrawing` 차례:
  1. 원본 sha256을 구한다. 작업 좌표계는 프로젝트 좌표계(무효면 `m_workCrs`)다. 작업 폴더는 `AppLocalData/cad-convert/` 아래 `QTemporaryDir`다.
  2. `KaBlockingTask::run(this, "도면을 읽는 중…", …)` 안에서 처리한다. DWG면 `CadDwgConverter::convert(…, bundledTool(), 120000, feedback 취소)`를 돌리고, 이어서 `CadDrawingReader::read`를 한다.
  3. 실패하면 `KaUserError::warn`(title `도면 불러오기`)을 띄운다. 취소면 창 없이 상태줄 `도면 불러오기를 취소했습니다.`만 보이고 false를 돌려준다.

     | 경우 | what | why | how |
     |---|---|---|---|
     | 도구 없음 | 변환 오류 | `앱 폴더의 tools/libredwg 가 비어 있습니다.` | `앱을 다시 설치하거나 CAD에서 DXF로 저장해 넣어 주세요.` |
     | DWG 실패 | 변환 오류 | `DWG 형식이 새롭거나 일부가 손상되었을 수 있습니다.` | `CAD 프로그램에서 DXF(2000 형식)로 저장해 다시 넣어 주세요.` |
     | DXF 열기 실패 | 읽기 오류 | `DXF 형식이 아니거나 손상되었습니다.` | `CAD 프로그램에서 다시 DXF로 저장해 보세요.` |
     | 모형 공간 없음 | 읽기 오류 | `도면 내용이 모두 종이 공간(배치)에 있습니다.` | `CAD에서 모형 공간에 그린 도면을 넣어 주세요.` |

     details에는 각각의 details를 넣는다.
  4. 좌표계를 정한다.
     - `forcedAuthId`가 비어 있으면 `guess(robustExtent, 작업, siteLocation(project, 지도 범위), context)`를 쓴다.
     - Choose이면 `KaCadCrsDialog::choose`: 결과가 nullopt면 false, -1이면 좌표 없음.
  5. 변환본을 쓴다.
     - 경로: `outputPathFor(path, m_surveyPath 가 비면 "" 아니면 QFileInfo(m_surveyPath).absolutePath())`.
     - 정보: `{path, sha, authId 또는 "", DWG면 converterLabel()}`.
     - `KaBlockingTask::run(this, "도면을 저장하는 중…", write…)`로 쓴다. 실패하면 `KaUserError`(what = error)다.
  6. 같은 제목 묶음이 있으면 `drawingIdOfGroup` → `layersOf`로 옛 변환본 파일을 기억한다. 새 id(`QUuid::createUuid().toString(QUuid::WithoutBraces)`)로 `addToProject`하고, 옛 파일은 새 파일과 다르고 `가져온자료/도면` 또는 `cad-drawings` 아래일 때만 `QFile::remove`한다(실패해도 넘어간다).
  7. 마무리:
     - `LayerOps::ensureOtfEnabled`를 부른다.
     - 화면은 `robustExtent`를 작업 좌표계로 바꾼 범위(좌표 없음은 그대로)에 10% 여백을 두고 `m_canvas->setExtent` + `LayerOps::refreshCanvasIfIdle`.
     - 상태줄 `도면 「%1」 올림 · 도형 %2개 · %3`, 세션 기록 `[cad] <원본> · <판정> · <authId 또는 없음> · 선 n 면 n 점 n 글자 n · 종이 공간 n · GDAL 경고 n`.
  8. 좌표 없음이면 `startCadDrawingAlign(id)`, 아니면 `showCadDrawingNotice`. 그리고 true를 돌려준다.
- `showCadDrawingNotice`:
  - `QgsMessageBarItem`: 제목 `도면`, 글 `도면을 %1(%2)로 읽어 %3로 바꿔 올렸습니다.`(라벨, authId, 작업 authId). 사용자가 닫을 때까지 둔다.
  - `QToolButton` `cadOtherCrs` 「다른 좌표계로 바꾸기」, 메뉴:
    - 나머지 후보마다 `describe()` → `importCadDrawing(sourcePath, 그 authId)`.
    - 구분선 다음 「좌표 없는 도면으로 보기」 → `importCadDrawing(sourcePath, kCadNoCrs)`.
    - 원본이 없으면 `KaUserError` what `원본 도면 파일을 찾지 못했습니다.`
  - `QPushButton` `cadAlignNow` 「직접 맞추기」 → `startCadDrawingAlign(id)`.
- `openCadDrawingFile`: `QFileDialog::getOpenFileName(this, "불러올 도면", QString(), "도면 (*.dxf *.dwg)")` → `importCadDrawing`.

- [ ] **Step 1: 시험을 쓴다** (`tests/test_save_open.cpp`)
  - 도우미 `writeGasuriDxf(dir, name)`: UTF-8 + ANSI_949. 층 JIJUK에 (387699.70±400, 280239.90±400) 안의 LINE 30개, TEXT `374-1전` 하나.
  - 도우미 `makeYeongcheonSurvey(name)`: 5187, survey_area (207390.79,378496.00)-(207490.79,378596.00).

```cpp
void cadDxfNearTheSurveyLandsInPlace();
// addVectorFromPath(dxf) 참; "참조 지도"/"<name> (도면)" 자식 {"글자","선"};
// 「선」 원본 파일 == <조사 폴더>/가져온자료/도면/<name>.gpkg; 「선」 범위 중심이 픽스처 중심의 5174→5187 값과 30 m 안;
// 이름 "entities" 레이어 없음; DXF 바이트 그대로; 메시지 막대 현재 항목에 cadOtherCrs·cadAlignNow
void cadDxfWithoutSurveyAreaAsksWhichCrs();
// survey_area 도형 없는 조사 + 지도를 한국 전체로 → 시험용 고르기 함수가 한 번 불리고(후보 2개 이상),
// "EPSG:5174" 줄을 돌려주면 「선」 중심이 같은 기대값과 30 m 안
void cadDwgUsesTheBundledConverter();
// QVERIFY2(<시험 exe 폴더>/tools/libredwg/dwg2dxf.exe 있음, "ka-hgis를 먼저 빌드해 tools/libredwg를 복사하세요");
// 고정 DWG 를 「[동국] 작은 도면(2010v).dwg」로 복사해 addVectorFromPath → 묶음 "[동국] 작은 도면(2010v) (도면)" 있음
void cadSameDrawingTwiceKeepsOneGroup();      // 같은 DXF 두 번 → 그 제목 묶음 1개
void cadNoSurveyOpenWritesIntoAppData();
// 조사 없이(시험용 고르기 함수가 0 을 돌려줌) → 「선」 원본 파일이 cad-drawings 아래, SurveyBundle::isAppManagedPath 참
```

- [ ] **Step 2: 실패를 확인한다.** 빌드 대상은 `ka-hgis ka_save_open_tests`이고, 시험 `save_open_cad`는 `-j1`로 돌린다. 시험이 실패해야 한다.
- [ ] **Step 3: 만든다.** `KaBlockingTask`의 모형은 `src/app/MainWindowSurveyContour.cpp:60`(`SurveyContourTask`)이다.
- [ ] **Step 4: 통과를 확인한다.**
  - `save_open_cad` 5개, `save_open_window`, `align_tool`, `workflow_engine`이 통과해야 한다. `workflow_engine`은 기준선 실패 1개가 그대로여야 한다.
  - `--smoke-quit`가 0으로 끝나야 한다.
- [ ] **Step 5: `docs/ERROR_REGRESSION.md`에 두 줄을 더한다.**
  - DXF 좌표계를 모른 채 숫자를 그대로 써서 5174 도면이 동해에 놓임 → 좌표계 판단(`cad_crs`, `save_open_cad`).
  - 하위 레이어 이름이 `entities` → 도면 묶음 이름(`cad_layers`).
- [ ] **Step 6: 커밋한다.** 메시지: `feat(cad): DXF·DWG를 좌표계를 알아내 참조 지도 제자리에 올린다`.

### Task 8: 도면 정합 — 도면 전체가 함께 움직이기

**Files:**
- Modify: `src/core/CadDrawingLayers.h/.cpp`: 아래 세 함수를 더한다. 파일이 300줄을 넘으면 `CadDrawingAlign.h/.cpp`로 나눈다.
- Modify: `src/app/KaAlignMapTool.h/.cpp`. 이 과제에 한해 `KaAlignMapTool.cpp`는 20줄까지 늘 수 있다. 덜어 낼 수 있는 일은 `CadDrawingLayers` 쪽으로 옮긴다.
  - `saveAligned` 벡터 갈래: 숨긴 원본(`m_hiddenSource`)에 `kPropDrawing`이 있으면 `saveAlignment(QgsProject::instance(), id, m_affine, errorOut)`를 부른다.
  - 성공하면 메모리 복제본을 프로젝트에서 빼고 `m_layer`를 원본 「선」 레이어로 바꿔 보이게 한다. `*savedPath`는 그 GPKG 파일이고, `saveVectorCopyGpkg`(원본 옆 `_aligned.gpkg`)는 부르지 않는다.
  - `endSession`: 도면 세션을 저장하지 않고 끝내면 복제본을 빼고 원본을 다시 보인다.
- Modify: `src/app/MainWindow.h`: 비공개 `QStringList m_cadAlignHidden;`와 `void restoreCadAlignCompanions();`.
- Modify: `src/app/MainWindowCadImport.cpp`.
  - `startCadDrawingAlign`: `m_cadAlignHidden = hideCompanions(project, id, alignLayer)`를 부른 뒤 `startAlignSession(alignLayer)`.
  - 같은 파일에 `restoreCadAlignCompanions()`(비공개, `showLayers` 후 비움)를 둔다.
- Modify: `src/app/MainWindowAlign.cpp`: `stopAlignSession` 끝과 「맞추기 저장」 성공 갈래에서 `restoreCadAlignCompanions()`를 한 줄씩 부른다.
- Modify: `tests/test_cad_layers.cpp`, `tests/test_save_open.cpp`, `CMakeLists.txt`(`save_open_cad` 목록에 두 시험), `docs/ERROR_REGRESSION.md`(한 줄).

**Interfaces:**
- Consumes: Task 4 `applyAffine`, Task 5 `layersOf`·`alignLayerOf`, Task 7 `startCadDrawingAlign`·`m_cadAlignHidden`.
- Produces:

```cpp
namespace CadDrawingLayers {
QStringList hideCompanions(QgsProject* project, const QString& drawingId, const QgsMapLayer* keep);  // 숨긴 id
void showLayers(QgsProject* project, const QStringList& layerIds);
// layersOf → CadDrawingStore::applyAffine, 성공하면 레이어마다 triggerRepaint
bool saveAlignment(QgsProject* project, const QString& drawingId, const GeorefService::Affine& a, QString* error);
}
```

- [ ] **Step 1: 시험을 쓴다**

```cpp
// test_cad_layers.cpp
void hideCompanions_keepsTheAlignLayer();         // 「선」 빼고 숨김, 돌려준 id 수 == 레이어 수 - 1; showLayers 로 다시 보임
void saveAlignment_movesEveryLayerOfTheDrawing(); // 선·글자 레이어 모두 Task 4 와 같은 아핀만큼 움직이고 글자 각도가 돈다
// test_save_open.cpp (save_open_cad)
void cadLocalDrawingStartsAlignWithOthersHidden();
// (0,0)-(100,80) 로컬 DXF(선 10개 + 글자 1개) → 보조 줄 caption("subToolbarCaption") 에 "맞추기",
// 그 도면 「글자」가 목록에서 꺼짐, "선 맞춤" 복제 레이어 있음
void cadAlignmentSaveMovesEveryTableAndWritesNoAlignedCopy();
// 위 상태에서 findChild<KaAlignMapTool*>() 로 setSourcePoint(0,0) → 지도 (207400,378500) 픽셀 클릭,
// setSourcePoint(100,0) → (207500,378500) 클릭(지도 폭 200 m / 800 px), 보조 줄 「맞추기 저장」 trigger →
// 선 꼭짓점 (10,10) → (207410,378510) ±1 m, 글자도 같은 만큼; DXF 폴더에 *_aligned.gpkg 없음;
// 그 도면 레이어 모두 보임; "선 맞춤" 레이어 없음
```

- [ ] **Step 2: 실패를 확인한다** (`cad_layers`, `save_open_cad` `-j1`).
- [ ] **Step 3: 만든다.**
- [ ] **Step 4: 통과를 확인한다.**
  - `cad_layers` 8개, `save_open_cad` 7개, `align_tool`, `georef_engine`, `save_open_window`가 통과해야 한다.
  - `--smoke-quit`가 0으로 끝나야 한다.
- [ ] **Step 5: `docs/ERROR_REGRESSION.md`에 한 줄을 더한다.** 정합이 도면의 첫 하위 레이어만 움직임 → 도면 전체에 같은 변환(`save_open_cad`).
- [ ] **Step 6: 커밋한다.** 메시지: `feat(cad): 도면 정합 저장이 선·글자·점·면 모두에 같은 변환을 적용한다`.
