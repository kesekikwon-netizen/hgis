---
name: ka-hgis-sheet
description: 조판 여백, 정합 지적 자석, 주변유적 번호·범례·표시, 추가 레이어 접힘, 수치지형도 가벼운 선이 깨졌을 때 복구한다. 완료된 현장 도면 계약을 보존한다. Use when 조판, 범례 번호, 주변유적 표시, 정합 자석, 지적도 루트 배치(옛 지적도 그룹 해체), 레이어 접힘, or 수치지형도 렉 regresses or a completed sheet behavior must be recorded.
---

# ka-hgis 조판·정합 완료 계약

프로젝트 전용. 구현은 [ka-hgis-gis](../ka-hgis-gis/SKILL.md), 인트라넷 받기는 [문화재인트라넷](../heritage-intranet/SKILL.md). 이 스킬은 **이미 맞춘 동작이 다시 틀어졌을 때**와 **새 완료를 기록할 때**만 연다.

현재 소스와 `.codex/NOW.md` 맨 위, 양쪽 `HANDOFF.md`가 이긴다. 2026-09-13 인트라넷 기록의 「번호 제거」는 폐기된 과거다.

설계도: [docs/architecture/ka-hgis-sheet-contracts.workflow.json](../../../docs/architecture/ka-hgis-sheet-contracts.workflow.json). 상세 계약은 [references/contracts.md](references/contracts.md).

## 시작

1. 증상과 맞는 계약을 [contracts.md](references/contracts.md)에서 고른다.
2. Graft로 그 함수를 찾고, 소스에서 서명을 확인한다. 줄 번호만 믿지 않는다.
3. 계약을 되돌리는 「번들 정리」를 하지 않는다. 한 증상만 고친다.
4. 계약에 적힌 CTest를 돌린다. 조판·지도면 `.\scripts\run-ka-hgis.ps1 --smoke-quit`. 실행 중 앱은 끄지 않는다.

## 완료를 남길 때

새 현장 동작이 CTest·smoke까지 끝났으면 NOW/HANDOFF만 쓰지 말고:

1. [contracts.md](references/contracts.md)에 계약 한 덩어리를 더한다. 증상, 유지 조건, 파일·함수, 테스트, 공식 URL, 금지 되돌림.
2. 설계도 JSON의 노드/증거가 현재 소스와 다르면 Archify 후보를 고치고 `validate` → `deliver`한다.
3. 양쪽 `HANDOFF.md`에 같은 한 줄을 남긴다.

## 빠른 경로

| 증상 | 계약 | 테스트 |
|---|---|---|
| 정합이 지적에 안 붙음 / 지적이 참조 지도 | 정합·지적 | `workflow_engine`, `dem_trench_engine` |
| 지적도를 Delete·우클릭으로 못 지움 | 정합·지적 | `layer_state_regressions`, `save_open_edit` |
| 참조 지도 묶음 우클릭에 삭제 없음 | 참조 지도 묶음 삭제 | `layer_state_regressions` |
| 다른 지도를 받으면 끈 지적도가 켜짐 | 정합·지적 | `workflow_engine` (`applySnapSettingsKeepsCadastralUnchecked`) |
| 조판 위·좌·우 여백이 다름 | 조판 여백 | `workflow_engine` (`layoutEqualFullSheetMapRect_matchesTopToSides`), `save_open_drawing` |
| 페이지가 작아 번호가 범례와 다름 | 조판 여백 | `save_open_drawing` (`drawingStudio_fieldPageGrowsA4LandscapeByOneCentimetre`) |
| 조판 용지가 가끔 안 보임 | 조판 페이지 | `workflow_engine` (`layoutRegainsPageWhenTheSheetHasNone`) |
| 받은 레이어가 펼쳐짐 | 레이어 접힘 | `heritage_import` |
| 페이지 번호와 범례가 다름 | 번호·범례 | `heritage_style` |
| 1:25000~1:50000에서 유적이 사라짐 | 페이지 표시 | `heritage_style` |
| 번호가 유적 위에 겹침 | 번호·범례 | `heritage_style` (`layoutNumbersOffsetStackedAnchorsWithCallout`) |
| 번호 선이 멀리 떨어짐 | 번호·범례 | `heritage_style` (`layoutNumbersSitOnNearbyDistinctSites`, `layoutNumbersKeepDenseClusterOnShortRings`) |
| 수치지형도 팬마다 멈춤 | 지형도 선 | `topographic_import` |
| 주변유적·지형이 시·군 전체로 올라옴 | 조사 주변 5km 적재 | `survey_scope_clip`, `heritage_import` |
| 주변유적이 보였다가 사라짐 | 덧그림 대상 | `layer_state_regressions` (`heritageDatasetStaysInAboveLabelsPass`, `heritageAloneStaysInAboveLabelsPass`) |
| 조판에 유적 도형이 하나만 | 덧그림 대상 | `heritage_style` (`layoutNumbersIncludeHeritageMissingFromBaseMap`), `save_open_drawing` |
| 줌 한 칸이 너무 큼 | 줌 한 칸 | `layer_state_regressions` (`wheelZoomFactorIsFinerThanQgisDefault`) |

## 금지

- 지적도를 참조 지도로 되돌리지 않는다. VWorld 지적 WMS에 자석을 걸지 않는다.
- 조판 번호를 원본 SHP에 쓰지 않는다. 스타일 override만.
- 같은 점에 쌓인 번호만 짧게 비키고 잇는다. 근처 다른 유적은 각자 위에 둔다.
- `PreventOverlap` / `AllowOverlapIfRequired`만으로 작은 축척 번호를 지우지 않는다.
- 점 유적의 빈 `intersection`을 페이지 밖이라고 버리지 않는다.
- 수치지형도에 점·면·속성을 다시 올리지 않는다. 팬마다 SHP를 다시 열지 않는다.
- 커밋·푸시·포터블·실행 중 앱 조작은 사용자가 말한 뒤에만.
