# 고고학 전용 HGIS (ka-hgis) v2

C++/Qt6 독립 실행형 필드고고학 HGIS. **OSGeo4W qgis-dev (QGIS 4.x) 라이브러리 링크**. 소스 포크 아님.

기본 CRS: **EPSG:5179**. 측량 폴리곤 중심 작성(제26조급 규칙 검수).

**저장소:** https://github.com/kwonyoungin11/hgis · 브랜치 `main`

## 다른 PC에서 바로 개발

상세: [`docs/other-pc-setup.md`](docs/other-pc-setup.md)

```powershell
git clone https://github.com/kwonyoungin11/hgis.git
cd hgis
# 기준 PC와 같은 OSGeo4W 판을 복사해 온다 (install-deps 는 설치한 날의 qgis-dev 를 받는다)
.\scripts\osgeo4w-bundle.ps1 -Import E:\ka-hgis-sdk
.\scripts\setup-dev-paths.ps1    # Graft 경로를 이 체크아웃으로

.\scripts\bootstrap-dev-pc.ps1   # dev-env.lock.json 비교 + build + ctest + smoke
.\scripts\run-ka-hgis.ps1
```

- **같은 환경:** 기준 PC 판은 `dev-env.lock.json`에 잠근다. `.\scripts\dev-env-lock.ps1`로 이 PC가 같은지 본다.
- **개발:** 클론 + OSGeo4W(`qgis-dev`) + VS2022 + CMake → `bootstrap-dev-pc.ps1` 또는 `build-all.ps1`
- **실행만:** 개발 PC에서 `.\scripts\make-portable.ps1` 후 `dist\ka-hgis-portable\` 폴더 전체를 복사 → `start.bat` (OSGeo4W 설치 불필요)
- 조사 GPKG/SHP는 git에 없음 → 별도 복사
- 규칙: `AGENTS.md` (Codex + QGIS 매뉴얼 연동) · `.codex/NOW.md` · `HANDOFF.md`

## 환경 (검증된 구성)
- CMake는 `dev-env.lock.json`의 앞 두 자리 (`C:\Program Files\CMake\bin`, `C:\CMake\bin` 또는 PATH)
- VS 2022 BuildTools MSVC. 도구 모음 앞 두 자리는 `dev-env.lock.json`
- 체크아웃은 클론한 폴더. SDK는 `scripts/dev-env.ps1`이 `OSGEO4W_ROOT` → `C:\OSGeo4W` → `D:\OSGeo4W` → `A:\OSGeo4W` 순으로 찾는다
  - 패키지 판: `dev-env.lock.json` (`qgis-dev`, `qt6-devel`, `gdal-dev-devel`, `sqlite3-devel`, `pdal-dev`)
- 산출물: `build\Release\ka-hgis.exe`, `ka_hgis_tests.exe`, `ka_workflow_tests.exe`

## 원클릭 빌드·검증
```powershell
cd <클론한 폴더>
.\scripts\setup-dev-paths.ps1
.\scripts\build-all.ps1
```
포함: cmake build → ctest → smoke-quit → e2e. 포터블 생성은 별도 요청 시에만 실행한다.
clangd용 실제 컴파일 DB는 `.\scripts\gen-compile-commands.ps1`로 생성한다.
Codex의 clangd 탐색·Graft 검색·Archify 구조도 설정과 사용 범위는 [`docs/developer-tools.md`](docs/developer-tools.md)를 따른다.

## 수동 빌드
```powershell
. .\scripts\dev-env.ps1
cmake --preset vs
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run-ka-hgis.ps1 --smoke-quit
.\scripts\e2e-smoke.ps1
```

## 실행
```powershell
.\scripts\run-ka-hgis.ps1
# 또는 포터블 (형제 ka-hgis.exe 우선)
cd dist\ka-hgis-portable
.\run.bat
# 또는
.\run-ka-hgis.ps1
```

### 제품 축 (현재)

1. **새 조사** → GPKG 스키마 준비, **범례는 비움** (그릴 때/가져올 때 레이어 등장)
2. **배경지도** → OSM/VWorld 등 **참조 지도** (조사 데이터와 분리)
3. **그리기** → 면/선/구역/GPS · 우클릭 완료 · 속성 팝업 없음 · **편집저장**
4. **조판 편집 창** → 별도 창 QgsLayoutView (도구 메뉴)
5. **도면검수** → 체크리스트 (error 시 제출 차단)
6. **제출패키지** → SHP(+PDF) **EPSG:5179** + MANIFEST

작업 CRS 5186/5187 OK. 업로드만 5179.

### 도구 메뉴
| 기능 | 설명 |
|---|---|
| CRS 이름만 지정 | 좌표 변환 없이 라벨만 변경 (위험) |
| 좌표 변환(재투영) | 벡터 → 새 GPKG/SHP |
| 지오레퍼런스 | GCP≥2 월드파일; pixel_x/y 3점+ 시 아핀 LS |
| OSM 배경 | XYZ 타일 |
| 유구 스타일 | kind/period 범주 |

## 문서
- **다른 PC 셋업:** `docs/other-pc-setup.md`
- 시나리오: `docs/user/gui-scenario-checklist.md`
- ADR: `docs/adr/0001-standalone-cpp-qgis-libs.md`
- UX: `docs/ux/ia-beginner.md`
- 데이터 흐름: `docs/architecture/data-flow.md`
- 잡카드: `docs/user/job-cards/`
- 에이전트: `AGENTS.md`, `HANDOFF.md`, `.grok/skills/ka-hgis/`

## 런타임 주의
`PATH`에 `qgis-dev\bin`, `Qt6\bin`, `gdal-dev\bin`, **`pdal-dev\bin`** 필요 (`pdal-devcpp210.dll`).  
`scripts\dev-env.ps1` / `run-ka-hgis.ps1`이 설정합니다.

VWorld 배경지도는 **도움말 → VWorld API 키 설정**에 키가 있을 때만 추가됩니다 (바이너리 기본 키 없음).

## 라이선스
GNU GPLv2 or later (QGIS 링크 파생물)

## 커밋 진행 상태 (항상 확인)

현재 브랜치/HEAD/원격 동기화/최근 커밋 목록:

- 파일: `docs/COMMIT_STATUS.md`
- 수동 갱신: `.\scripts\update-commit-status.ps1`
- 훅 설치(한 번): `.\scripts\install-git-hooks.ps1`  → 이후 **매 커밋마다 자동 갱신**
- 헬퍼 커밋: `.\scripts\commit.ps1 -Message "..." -Path path1,path2 -Push`
