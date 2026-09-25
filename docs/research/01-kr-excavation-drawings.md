# 한국 발굴조사 위치·도면 작성 기준 연구 요약

> 제품: 고고학 전용 HGIS (`ka-hgis`)  
> 목적: 법령·행정규칙상 **도면 작성 유의사항**을 제품 체크리스트로 기계화하기 위한 근거 정리  
> 주의: 아래는 공개된 별표·2차 인용을 요약한 것이며, 현장 적용 시 **현행 국가유산청 고시 원문**을 최종 확인한다.

## 1. 근거 문서

| 구분 | 내용 | 출처 |
| --- | --- | --- |
| 위치 도면 유의사항 | 발굴된 매장문화재 위치 도면 작성시 유의사항 (제26조 관련) 별표 | [법제처/법령 별표 전문 인용](https://www.law.go.kr/), 조사기관 게시 전문 (예: 성림문화유산연구원 서식), PDF 규정집 |
| 발굴조사 방법·절차 | 「발굴조사의 방법 및 절차 등에 관한 규정」(국가유산청 고시, 개정 이력 있음) | [law.go.kr 행정규칙](https://www.law.go.kr/) |
| 보고서 수록 사항 | 발굴조사 보고서에 포함되어야 할 사항 (시행규칙 별표) | law.go.kr 별표 (목차·도면·사진 등) |
| 학술 문제 제기 | 보고서 필수 주제도 누락, 국가기본도 미사용, 공간정보 부정확 | KCI 「고고학적 공간정보의 취득관리와 GIS」 등 |

## 2. 필수 도면 세트 (일반사항)

완료·신고·보고서 맥락에서 **반드시 작성**해야 하는 도면 유형:

1. **조사구역도**
2. **유적위치도**
3. **유구배치도**
4. **개별유구실측도**
5. **층위도**

### 조사구역도·유적위치도 — 제출 형태

- **Shape(SHP) 파일**과, 정확한 구역을 알 수 있도록 **축척 1:5,000 이상**의 수치지적도 등에 작성된 도면 파일
- 완료 신고 시 **문화재 GIS 통합 인트라넷** 제출 대상(규정 문구)

→ 제품: `제출 패키지` 단계에서 SHP + PDF(또는 도면 이미지) 묶음, 체크리스트로 누락 차단.

## 3. 조사구역도 규칙 (제품 강제 후보)

| 규칙 | 요지 | severity 제안 |
| --- | --- | --- |
| 실제 다각형 | 국가기본도(수치지형·지적 등)에 **정확한 구역을 다각형**으로 표시 | error |
| 추상 심볼 금지 | 원·점·삼각형·사각형 등 **추상화 표기 금지** | error |
| 자체 지도 시 GPS | 부득이 자체 제작 지도 시 조사범위 경계 기준 **절대좌표 ≥2점** 도면 기재 | error |
| 측지 메타 | GPS 기재 시 **측지기준계, 회전타원체, 투영법, 투영원점** 정보 표시 | error |
| 측량 정밀도 | 2DRMS(95%) 기준 **1m 이내** 성능 기기; 고도(古都)는 DGPS/RTK 권장 | warn/error 정책화 |
| 1:5,000 표시 | 사업부지 내 발굴구역 경계를 **1:5,000 수치지형도**에 표시. 소규모는 연속지적도 대안 | warn |

## 4. 유구배치도 규칙

| 규칙 | 요지 | severity |
| --- | --- | --- |
| 축척·도엽 | 적절한 축척, **도엽명·축척·방위(자북·진북)** 표시 | error |
| GPS ≥2 | 배치구역 경계 기준 고정밀 GPS 절대좌표 **≥2점** | error |
| 구역 일치 | 유구배치구역 범위 = 발굴조사구역 범위와 **일치** | error |
| 형태 유지 | 유구 **심볼화 표기 금지**, 형태·배치 파악 가능해야 함 | error |
| 범례 | **유구 종류·시대**를 알 수 있는 범례 | error |

## 5. 보고서·계획 단계 도면 스케일 (보완)

발굴조사 계획·보고서 관행/규정에서 자주 요구되는 스케일 계열:

- 광역 위치: 약 **1/50,000 ~ 1/10,000** 지형도 + 주변 유적
- 현황: **1/5,000** 지형도 + 좌표
- 상세(그리드·트렌치 등): **1/2,000 ~ 1/1,000**

→ 제품 레이아웃 템플릿 프리셋으로 제공.

## 6. 현장에서 반복되는 문제 (학술·실무)

- 필수 주제도(위치도·구역도 등) **누락**
- **국가기본도** 미사용·임의 스케치 지도
- 좌표·축척·방위 표기 부실 → GIS 재사용 불가
- SHP 미제출 또는 좌표계·인코딩 불명

→ `ka-hgis`는 **내보내기 전 자동 검수**로 위 문제를 차단하는 것이 핵심 가치.

## 7. 제품 매핑

| 법령 개념 | ka-hgis |
| --- | --- |
| 조사구역 다각형 | `survey_area` 레이어 + 폴리곤 전용 도구 |
| GPS 기준점 ≥2 | `control_points` + 메타 필드 |
| 유구 형태 | `feature_poly` / `feature_line` (심볼-only 금지) |
| 종류·시대 범례 | 속성 필드 + 레이아웃 범례 |
| SHP 제출 | 제출 마법사 + 인코딩 |
| 필수 도면 5종 | 레이아웃 템플릿 5 + 체크리스트 |

## 8. 기계 체크리스트 원본 (→ `data/rules/drawing_checklist.v1.json`)

```json
{
  "version": 1,
  "title_ko": "발굴 위치도면 유의사항 기반 검수",
  "rules": [
    {
      "id": "REQ_DRAWING_SURVEY_AREA",
      "severity": "error",
      "message_ko": "조사구역도가 없습니다.",
      "check_type": "layer_nonempty:survey_area"
    },
    {
      "id": "REQ_DRAWING_SITE_LOCATION",
      "severity": "error",
      "message_ko": "유적위치도가 준비되지 않았습니다.",
      "check_type": "layout_exists:site_location"
    },
    {
      "id": "REQ_DRAWING_FEATURE_PLAN",
      "severity": "error",
      "message_ko": "유구배치도가 준비되지 않았습니다.",
      "check_type": "layout_exists:feature_plan"
    },
    {
      "id": "REQ_DRAWING_FEATURE_DETAIL",
      "severity": "warn",
      "message_ko": "개별유구실측도가 없습니다.",
      "check_type": "layout_exists:feature_detail"
    },
    {
      "id": "REQ_DRAWING_SECTION",
      "severity": "warn",
      "message_ko": "층위도가 준비되지 않았습니다.",
      "check_type": "layout_exists:section"
    },
    {
      "id": "GEOMETRY_VALID",
      "severity": "error",
      "message_ko": "자기교차 등 유효하지 않은 도형이 있습니다. 제출 전에 수정하세요.",
      "check_type": "geometry_valid:domain"
    },
    {
      "id": "GEOMETRY_NOT_EMPTY",
      "severity": "error",
      "message_ko": "빈 도형이 있습니다. 제출 전에 수정하세요.",
      "check_type": "geometry_nonempty:domain"
    },
    {
      "id": "GEOMETRY_NONZERO_AREA",
      "severity": "error",
      "message_ko": "면적이 0인 폴리곤이 있습니다. 제출 전에 수정하세요.",
      "check_type": "geometry_nonzero_area:domain"
    },
    {
      "id": "SURVEY_POLYGON_ONLY",
      "severity": "error",
      "message_ko": "조사구역은 실제 폴리곤이어야 합니다.",
      "check_type": "geometry_type:survey_area=Polygon"
    },
    {
      "id": "SURVEY_NO_ABSTRACT_MARKER",
      "severity": "error",
      "message_ko": "조사구역 추상 심볼 표기 금지.",
      "check_type": "forbid_abstract_marker:survey_area"
    },
    {
      "id": "GCP_MIN_TWO",
      "severity": "error",
      "message_ko": "GPS/기준점 2개 미만입니다.",
      "check_type": "count_min:control_points:2"
    },
    {
      "id": "GCP_DATUM_META",
      "severity": "error",
      "message_ko": "기준점 측지기준계 메타 없음.",
      "check_type": "field_nonempty:control_points:datum"
    },
    {
      "id": "GCP_ELLIPSOID_META",
      "severity": "error",
      "message_ko": "기준점 타원체 메타 없음.",
      "check_type": "field_nonempty:control_points:ellipsoid"
    },
    {
      "id": "GCP_PROJECTION_META",
      "severity": "error",
      "message_ko": "기준점 투영법 메타 없음.",
      "check_type": "field_nonempty:control_points:projection"
    },
    {
      "id": "GCP_ORIGIN_META",
      "severity": "warn",
      "message_ko": "투영원점 정보 비어 있음.",
      "check_type": "field_nonempty:control_points:origin"
    },
    {
      "id": "GCP_ACCURACY_NOTE",
      "severity": "warn",
      "message_ko": "측량 정확도 기록 없음.",
      "check_type": "field_nonempty:control_points:accuracy"
    },
    {
      "id": "FEATURE_NO_SYMBOL_ONLY",
      "severity": "error",
      "message_ko": "유구는 면/선 기하여야 합니다.",
      "check_type": "geometry_type:feature_poly|feature_line"
    },
    {
      "id": "FEATURE_LEGEND_FIELDS",
      "severity": "error",
      "message_ko": "유구 종류/시대 속성 필요.",
      "check_type": "field_any:feature_poly:kind,period"
    },
    {
      "id": "EXTENT_MATCH",
      "severity": "warn",
      "message_ko": "유구 범위가 조사구역과 어긋날 수 있음.",
      "check_type": "extent_within:features,survey_area"
    },
    {
      "id": "CRS_PROJECT_SET",
      "severity": "error",
      "message_ko": "프로젝트 좌표계 미설정 (EPSG:5179 권장).",
      "check_type": "project_crs_set"
    },
    {
      "id": "EXPORT_SHP_READY",
      "severity": "error",
      "message_ko": "SHP 내보내기 전 조사구역 무효.",
      "check_type": "export_ready:survey_area"
    }
  ]
}
```

제품 JSON에 기하 3항(GEOMETRY_VALID, GEOMETRY_NOT_EMPTY, GEOMETRY_NONZERO_AREA)이 추가되어 규칙 개수는 21이다. 원문 별표 항목 단위 대조표는 사용자가 원문을 주면 채운다.

규칙 개수: **21**

## 9. 한계

- 인트라넷 **업로드 API**는 공개 스펙이 없어 자동화하지 않음(수동 제출 + 패키지 생성).
- 고시 개정 시 `data/rules/drawing_checklist.v1.json` 버전 업 필요.
