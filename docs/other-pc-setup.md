# 다른 PC에서 바로 개발하기 (Windows)

원격: **https://github.com/kwonyoungin11/hgis** · 브랜치 **`main`**  
DLL 미포함 → 대상 PC에 **OSGeo4W `qgis-dev`** 필요.

---

## 30초 요약 (권장)

```powershell
git clone https://github.com/kwonyoungin11/hgis.git
cd hgis
# 최초 1회만 — 관리자 PowerShell 권장 (CMake/VS/OSGeo4W 설치 시도)
# .\scripts\install-deps.ps1

.\scripts\bootstrap-dev-pc.ps1
.\scripts\run-ka-hgis.ps1
```

성공 시: `build\Release\ka-hgis.exe` + ctest + smoke 통과.

---

## A) 개발 환경 요구

| 항목 | 권장 |
|------|------|
| OS | Windows 10/11 x64 |
| Git | 설치 |
| CMake | 4.x (`C:\Program Files\CMake\bin`, `C:\CMake\bin` 또는 PATH) |
| 컴파일러 | **VS 2022** (MSVC C++ 워크로드 / Build Tools) |
| GIS SDK | **OSGeo4W** `A:\OSGeo4W`, `C:\OSGeo4W`, `D:\OSGeo4W` 또는 `$env:OSGEO4W_ROOT` |
| C++ 분석 | 설치된 clangd + `.clangd` + 실제 CMake 컴파일 DB |

OSGeo4W 패키지:

- `qgis-dev`
- `qt6-devel`
- `gdal-dev-devel`
- `sqlite3-devel`
- `pdal-dev`

핀: 저장소 `VERSION_QGIS_PIN.txt` (가능하면 같은 qgis-dev 계열).

### 의존성 자동 설치 (관리자 PowerShell)

```powershell
cd <클론>\hgis
.\scripts\install-deps.ps1
```

수동 OSGeo4W: https://download.osgeo.org/osgeo4w/v2/osgeo4w-setup.exe → 루트 `C:\OSGeo4W`.

---

## B) 클론 → 빌드 → 실행

### 원클릭

```powershell
git clone https://github.com/kwonyoungin11/hgis.git
cd hgis
git checkout main
git pull
.\scripts\bootstrap-dev-pc.ps1
.\scripts\run-ka-hgis.ps1
```

OSGeo가 다른 경로면:

```powershell
.\scripts\bootstrap-dev-pc.ps1 -OsgeoRoot "D:\OSGeo4W"
```

### 수동 (bootstrap 없이)

```powershell
. .\scripts\dev-env.ps1
cmake --preset vs
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run-ka-hgis.ps1 --smoke-quit
.\scripts\run-ka-hgis.ps1
```

또는 `.\scripts\build-all.ps1` (build + test + smoke + e2e). 포터블 생성은 별도 요청 시에만 실행한다.

### 현재 PC 경로와 clangd

현재 체크아웃은 `A:\qgis`, GIS SDK는 `A:\OSGeo4W`다. 스크립트는 저장소 위치를 기준으로 실행하고 `dev-env.ps1`이 환경 변수 또는 설치 경로에서 SDK와 CMake를 찾는다.

```powershell
cd A:\qgis
.\scripts\build-now.ps1
.\scripts\gen-compile-commands.ps1
```

일반 앱 빌드는 VS 2022 x64의 `build\Release`를 사용한다. 별도 `build-clangd` Ninja 구성은 컴파일 DB 생성용이다. 생성 스크립트는 VS 개발 환경을 자동으로 읽고 실제 MSVC/Windows SDK include 경로를 `build\compile_commands.json`에 기록하므로 일반 편집기에서도 표준 헤더를 찾을 수 있다. SDK나 소스 구성이 바뀌면 다시 실행한다. 절대 경로가 들어가는 생성 DB는 Git에서 제외한다. `compile_flags.txt`는 최소 C++ fallback이며 전체 GIS 분석에는 생성 DB가 필요하다.

---

## C) 일상 동기화 (두 PC)

```powershell
# 시작
git pull origin main
.\scripts\build-all.ps1   # 또는 cmake --build build --config Release

# 끝 (커밋 후)
git push origin main
```

- 조사 파일 `*.gpkg` / 필드 SHP는 **git에 없음** → OneDrive/NAS 별도.
- VWorld 키: **도움말 → VWorld API 키 설정** (PC 로컬, 커밋 금지).

---

## D) 에이전트 / 제품 규칙 (개발 시 필수)

| 파일 | 내용 |
|------|------|
| `AGENTS.md` | 에이전트 라우팅 + **QGIS 매뉴얼 연동 규칙** + 불변식 |
| `HANDOFF.md` | 제품 SSOT 요약 (`.codex/NOW.md`와 현재 코드 우선) |
| `docs/vendor/qgis-manual-3.44/` | PyQGIS Cookbook PDF (git) + User Guide 다운로드 스크립트 |
| `docs/domain/data-model.md` | 도메인 레이어/필드 |
| `docs/COMMIT_STATUS.md` | 최근 커밋 장부 |

대용량 Desktop User Guide PDF:

```powershell
.\scripts\download-qgis-manuals.ps1
```

### 현재 제품 동작 (헷갈리지 말 것)

1. **새 조사** → 저장소(GPKG 스키마)만 준비, **범례는 비어 있음** (QGIS: 레이어는 add 할 때만).
2. **그리기 → 면/선/구역/GPS** → 그때 레이어가 생기고 디지타이즈.
3. 좌클릭=점, **우클릭/더블클릭/Enter=완료**(도구 유지), **편집저장**=커밋.
4. 그리기 중 **속성 팝업 없음** (나중에 편집).
5. **도구 → 조판 편집 창** → 별도 창에서 QgsLayoutView 편집 + PDF.
6. 제출: 검수 error 있으면 SHP 패키지 차단 · 업로드 CRS **EPSG:5179**.

---

## E) 실행만 (포터블)

실행만: 개발 PC에서 `.\scripts\make-portable.ps1 -OutDir '<새 출력 폴더>'` 후 해당 폴더를 **통째** 복사 → `ka-hgis.exe` 또는 `start.bat`. 대상 PC에 OSGeo4W 불필요. 기존 폴더는 덮어쓰거나 삭제하지 않으므로 매번 새 출력 경로를 사용한다.

사용자가 개인 계정까지 포함하도록 요청한 경우에만 `-IncludeLocalCredentials`를 추가한다. 현재 앱의 VWorld 키와 수치지형도·국가유산 인트라넷 계정을 포함하며 값은 로그에 출력하지 않는다. `scripts/verify-portable-pack.ps1 -OutDir '<출력 폴더>'`로 주요 런타임 파일을 확인한다. 실제 실행·좌표계·웹 엔진 검증은 이 파일 검사와 별도로 수행한다.

---

## F) 자주 막히는 것

| 증상 | 조치 |
|------|------|
| `OSGEO4W_ROOT not found` | OSGeo4W 설치 또는 `$env:OSGEO4W_ROOT` |
| `qgis-dev missing` | OSGeo4W에서 `qgis-dev` |
| DLL 없음 / 즉시 종료 | `dev-env.ps1` 후 실행; `pdal-dev\bin` PATH |
| CMake 없음 | `winget install Kitware.CMake` |
| VS 제너레이터 실패 | VS 2022 Build Tools + C++ 워크로드 |
| VWorld 배경 안 됨 | 도움말 → API 키 (로컬) |
| 조판 창 안 뜸 | Release 빌드 후 `도구 → 조판 편집 창` |

---

## G) 검증 체크리스트 (다른 PC 첫날)

- [ ] `git log -1 --oneline` 가 GitHub `main` tip과 같음
- [ ] `.\scripts\bootstrap-dev-pc.ps1` 성공
- [ ] `ctest` 100%
- [ ] 앱 기동 → 새 조사 → 레이어 목록 비어 있음
- [ ] 그리기 → 면 → 우클릭 완료 → 도형 표시
- [ ] 도구 → 조판 편집 창 열림
