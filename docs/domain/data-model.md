# ka-hgis 도메인 데이터 모델

새 조사 작업 좌표계 기본값: **EPSG:5187**. 사용자는 **EPSG:5186 / EPSG:5187**을 선택할 수 있으며, 저장·다시 열기에서는 실제 조사/레이어 CRS를 보존한다. 근거: `SurveyProjectFactory::defaultWorkCrsAuthId()`와 현재 `HANDOFF.md`.

제출 SHP 좌표계: **EPSG:5179** (Korea 2000 / Unified CS). `ExportService`의 좌표 변환 경로를 사용한다. 프로젝트 CRS 지정, 화면 재투영, 실제 피처 좌표 변환은 서로 다른 작업이다.

작업 저장: **GeoPackage** (`<조사명>.gpkg`, 작업공간도 그 안의 `qgis_projects` 에 들어 있다)  
제출: SHP/PDF (내보내기 전용)

**스키마 원본:** [`data/schemas/ka_hgis_layers.yaml`](../../data/schemas/ka_hgis_layers.yaml). `SurveyProjectFactory::createNewSurvey` 가 만드는 7개 레이어·도형 종류·필드·형이 그 파일과 같아야 하며, `workflow_engine` 의 `surveySchemaYaml_matchesFactory` 가 새 조사를 만들어 대조한다(다르면 실패). 아래 표는 그 파일을 사람이 읽기 쉽게 옮긴 것이다. 앱은 실행 중에 YAML 을 읽지 않는다. 「필수」는 제출 검수(`data/rules/drawing_checklist.v1.json`)가 빈 값을 알리는 필드다.

## 레이어

### survey_area (조사구역) — Polygon
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| survey_name | string | Y | 조사명 |
| site_name | string | N | 유적명 |
| note | string | N | 비고 |
| uid | string | N | 고유번호(자동) |
| created_at | string | N | 만든 시각(자동) |
| updated_at | string | N | 고친 시각(자동) |

**제약:** 실제 폴리곤만. 점/원 등 추상 마커 금지.

### feature_poly (유구 면) — Polygon
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| kind | string | Y | 유구종류 |
| period | string | Y | 시대 |
| feature_no | string | N | 유구번호 |
| note | string | N | 비고 |
| layer_no | string | N | 층위·문맥 번호 |
| depth_m | double | N | 깊이(m) |
| top_el | double | N | 상면 표고(m) |
| bottom_el | double | N | 바닥 표고(m) |
| relation | string | N | 선후관계 |
| status | string | N | 조사 상태 |
| photo | string | N | 사진 |
| surveyor | string | N | 조사자 |
| surv_date | string | N | 조사일 |
| uid | string | N | 고유번호(자동) |
| created_at | string | N | 만든 시각(자동) |
| updated_at | string | N | 고친 시각(자동) |

### feature_line (유구/경계 선) — LineString
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| kind | string | Y | 종류(단면선, 경계 등) |
| period | string | N | 시대 |
| note | string | N | 비고 |
| feature_no | string | N | 유구번호 |
| layer_no | string | N | 층위·문맥 번호 |
| relation | string | N | 선후관계 |
| status | string | N | 조사 상태 |
| photo | string | N | 사진 |
| surveyor | string | N | 조사자 |
| surv_date | string | N | 조사일 |
| uid | string | N | 고유번호(자동) |
| created_at | string | N | 만든 시각(자동) |
| updated_at | string | N | 고친 시각(자동) |

### section_line (층위·단면 기준선) — LineString
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| section_id | string | Y | 단면 번호 |
| note | string | N | 비고 |
| surveyor | string | N | 조사자 |
| surv_date | string | N | 조사일 |
| uid | string | N | 고유번호(자동) |
| created_at | string | N | 만든 시각(자동) |
| updated_at | string | N | 고친 시각(자동) |

### control_points (GPS 기준점) — Point
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| point_id | string | Y | 점 이름(조사 안에서 겹치지 않게) |
| x | double | N | X (또는 경도) |
| y | double | N | Y (또는 위도) |
| z | double | N | 표고 |
| datum | string | Y | 측지기준계 |
| ellipsoid | string | Y | 타원체 |
| projection | string | Y | 투영 |
| origin | string | Y(경고) | 투영원점 |
| accuracy | string | N | 정확도 메모 (2DRMS 등). accuracy_m 과 둘 중 하나는 있어야 경고가 없다 |
| accuracy_m | double | N | 정확도(m) |
| pdop | double | N | PDOP |
| fix_type | string | N | 측위 방식(RTK Fix 등) |
| pixel_x | double | N | 스캔 도면 정합용 화소 X |
| pixel_y | double | N | 스캔 도면 정합용 화소 Y |
| uid | string | N | 고유번호(자동) |
| created_at | string | N | 만든 시각(자동) |
| updated_at | string | N | 고친 시각(자동) |

**제약:** 제출 검수 시 **≥ 2점**. 측지기준계·타원체·투영이 빈 점은 제출을 막는다.

### artifact_point (유물) — Point
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| kind | string | N | 유물 종류 |
| period | string | N | 시대 |
| artifact_no | string | N | 유물 번호 |
| note | string | N | 비고 |
| in_feature | string | N | 소속 유구 |
| z | double | N | 표고 Z |
| layer_no | string | N | 층위·문맥 번호 |
| photo | string | N | 사진 |
| surveyor | string | N | 조사자 |
| surv_date | string | N | 조사일 |
| uid | string | N | 고유번호(자동) |
| created_at | string | N | 만든 시각(자동) |
| updated_at | string | N | 고친 시각(자동) |

### trial_trench (시굴격자) — Polygon
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| name | string | N | 격자 이름 |
| width | double | N | 폭(m) |
| length | double | N | 길이(m) |

## 스키마 판 3 (선택 기록·자동 항목)

표의 `layer_no`·`depth_m`·`top_el`·`bottom_el`·`relation`·`status`·`photo`·`surveyor`·`surv_date`·`in_feature`, `feature_line` 의 `feature_no`, `artifact_point` 의 `z` 는 판 3에서 더한 **선택 기록 항목**이다. 그리는 중에 묻지 않고 제출 검수도 막지 않는다. `uid`·`created_at`·`updated_at` 은 **자동 항목**으로 `FeatureRecord` 가 채운다(`stampNew` 새 도형, `touch` 고친 도형). 손으로 적지 않는다. `trial_trench` 는 만들어지는 격자라 두 가지 모두 없다. 필드 이름은 SHP(DBF) 제출을 위해 10자 이내다. 코드 원본은 `src/core/SurveySchema.cpp` 이다.

새 조사 GPKG 에는 GDAL 데이터셋 메타데이터(`gpkg_metadata`) 항목 `KA_HGIS_SCHEMA_VERSION=3` 이 적힌다. 적힌 값이 없는 예전 조사는 0 으로 읽는다. 예전 조사는 **열 때 바뀌지 않는다.** 빠진 항목은 저장(Ctrl+S) 때 만드는 다음 세대 사본에서만 `SurveySchema::migrateGenerationCopy` 가 더하고 판 번호를 적는다. 기존 필드와 값은 바꾸거나 지우지 않고, 없는 표(`artifact_point`·`trial_trench`)는 새로 만들지 않는다.

## 레이어 등장 규칙

GPKG 에는 7개 테이블이 모두 만들어지지만 범례에는 사용자가 그리거나 가져오거나 연 레이어만 나온다(`LayerOps::ensureDomainLayer` 만 도메인 레이어를 더한다). 레이어 제목(한국어)은 표시용이고, 논리 식별자는 `ka_hgis/layer_key` 다.

## 예전 기본값

2026-09 이전 문서와 스키마의 `default_crs: EPSG:5179` 는 폐기했다. 현재 YAML 은 `default_work_crs: EPSG:5187`, `work_crs_choices: [EPSG:5186, EPSG:5187]`, `submit_crs: EPSG:5179` 로 코드(`SurveyProjectFactory::defaultWorkCrsAuthId()`, `uploadCrsAuthId()`)와 같다. 기존 조사를 5179 로 재지정하지 않는다.
