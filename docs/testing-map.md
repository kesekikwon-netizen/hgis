# 시험 지도 (축 A–J)

계획서 `plans/2026-09-19-world-class-gis-plan.md` §1.1 축마다 **자동 시험 1개 이상**.  
실행은 저장소 루트, Release CTest. 원본 조사 GPKG는 쓰지 않는다.

```powershell
$env:PATH = "C:\Program Files\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
ctest --test-dir build -C Release -R '<아래 이름>' --output-on-failure
```

플레이크 5회: `.\scripts\ctest-flake.ps1 -Repeat 5`

| 축 | 자동 시험 | 무엇을 보나 |
| --- | --- | --- |
| A 저장·무결성 | `storage_safety` | 세대 저장, `SurveySession::persistWork`, 동반 QGZ |
| A | `save_open_roundtrip` | 저장→재열기→제출 (슬롯 실패 이력 있음) |
| B CRS·변환 | `export_survey_areas` | 5179 SHP 좌표 ≤1mm |
| B | `topographic_source_crs` | 수치지형 원본 CRS |
| C 편집 | `workflow_engine` | undoStack, 스냅, 위상 편집 |
| C | `save_open_edit` | 정점 Undo·메뉴 (ctrlZ 슬롯 실패 이력) |
| D 성능·안정 | `perf_engine` | 15만 필지·열기·조판 상한 |
| D | `parallel_render` | 병렬 렌더 재현, 제품은 꺼 둠 |
| D | `cadastral` | 지적 준비 cheap key · P3-4 절대 HTTP 마감 |
| D | [network-download-timeout-checklist.md](network-download-timeout-checklist.md) | 내려받기 취소·시간 제한·재시도·오프라인 문구 |
| E UX | `layer_information` | 작은 창 목록 ≥5행 |
| E | `theme_qss` | Tab/Enter 새 조사→저장 |
| E | `above_labels_200` | 200% 라벨 |
| E | `ribbon_overflow` · `ribbon_pixels` | 리본은 접지 않고 창 너비에 맞는 가장 큰 크기(타일 56~20)로 모든 칩을 보인다, 글자·타일이 잘리지 않는다 |
| E | `toolbar_fit` | 그리기 보조 줄은 » 없이 글자를 숨긴 뒤 아이콘을 20에서 16으로 줄인다 |
| F 제출 | `checklist_engine` | 자기교차·빈 도형·0면적 차단 |
| F | `export_survey_areas` | SHP+PDF 패키지 |
| G 배포 | `theme_qss` 슬롯 `versionAndLaunchScripts_exist` | `VERSION`·바로가기·QGIS 핀 파일 |
| H 시험·CI | `scripts/ctest-flake.ps1` | CTest 5회, 5/5 아닌 목록 |
| H | `catch_log` | 시험 하네스가 로그 경로를 씀 |
| I 구조 | `catch_log` 슬롯 `srcCatchHandlers_allLog` | `catch (...)` 로그 누락 검출 |
| J 비밀·문서 | `e2e_opaque_suite` | VWorld 키를 URL에만 주입, 하드코딩 없음 |
| J | `theme_qss` 슬롯 `noUrl` | QSS `url(` 없음 |
| J | [docs/README.md](README.md) | 현행 문서 한 장 (`docs/archive/` 분리) |

## save_open 분할 (7-3)

한 exe, 최장 ≤60초. `RESOURCE_LOCK ka_save_open`.

`save_open_window` · `save_open_topo` · `save_open_drawing` · `save_open_edit` · `save_open_roundtrip` · `save_open_open` · `save_open_open_invalid` · `save_open_commit` · `save_open_saveas`

## 이 표가 아닌 것

- 사용자 화면 확인(0-4, 5-4 기록).
- 다른 PC·서명·기관 접수(§2 결정).
- `run-ka-hgis.ps1 --smoke-quit` — 자동이나 CTest 등록은 아님.
