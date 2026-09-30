# bad_survey — 검수 오류 연습 (EPSG:5186)

`bad.gpkg` 는 **제출이 막히는** 합성 조사다. 복사본을 **조사 열기**로 열고 **검수·제출**(Ctrl+E)을 누르면 검수 error 가 나와야 한다.

| 일부러 빠뜨린 것 | 검수 규칙 |
| --- | --- |
| 기준점 0개 | `GCP_MIN_TWO` (2점 이상) |
| 유구 면의 종류·시대 빈칸 | `FEATURE_LEGEND_FIELDS` |

기대값 메타데이터는 `bad.ka-survey.json`. 앱은 JSON 을 열지 않는다. 좌표계·도형 위치는 demo_survey 와 같다.
