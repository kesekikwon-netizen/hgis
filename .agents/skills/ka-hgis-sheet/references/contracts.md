# 완료 계약

함수 이름은 현재 소스에서 Graft로 확인한다. 줄 번호는 힌트일 뿐 증거가 아니다.

공식 문서:

- 자석: https://docs.qgis.org/3.44/en/docs/user_manual/working_with_vector/editing_geometry_attributes.html
- 라벨·콜아웃: https://docs.qgis.org/3.44/en/docs/user_manual/style_library/label_settings.html
- 조판 여백: https://docs.qgis.org/3.44/en/docs/user_manual/print_layout/create_output.html

## 정합·지적

- 지적도는 `ka_hgis/cadastral` + 역할 `cadastral` + 범례 그룹 **지적도**. 참조 지도가 아니다.
- `LayerOps::markReferenceLayer`는 지적이면 바로 반환한다. `isCadastralLayer`가 `isReferenceLayer`보다 앞선다.
- `LayerOps::isSnapSourceLayer` = 지적 또는 비참조. `applySnapSettings`는 이 레이어에만 자석을 건다. 범례에서 끈 지적도는 다시 켜지 않는다.
- `placeCadastralLayer`는 이미 지적도 그룹에 있는 레이어의 켜짐/꺼짐을 바꾸지 않는다. 수치지형도·토양도를 받을 때 `layersAdded` → 자석 갱신이 지적을 다시 켜던 길을 막는다.
- 정합 중 `MainWindowEditing`은 `SurveyLayers`를 강제한다. 현재 레이어가 사진이어도 조사 도형·지적 선에 붙는다.
- 위성·VWorld 지적 WMS는 그림이다. 자석 대상이 아니다. 수치지형도 합친 레이어는 켜지 않는다.
- 지적도 레이어·지적도 묶음은 Delete와 우클릭 삭제로 목록에서 뺀다. 원본 파일은 그대로다. 지운 뒤에는 `ka_hgis/skip_auto_cadastral`로 자동 VWorld 지적을 다시 올리지 않는다. 이미 받은 공식 지적(`isCadastralLayer`)이 있으면 같은 그림도 추가하지 않는다.

파일: `LayerOps.cpp` (`markCadastralLayer`, `placeCadastralLayer`, `isCadastralLayer`, `isSnapSourceLayer`, `applySnapSettings`, `removableCadastralLayersFromNode`, `rememberUserRemovedCadastral`), `CadastralImport.cpp`, `MainWindowUndo.cpp`, `MainWindowContextMenus.cpp`, `MainWindowEditing.cpp`, `KaAlignMapTool.cpp`.

테스트: `workflow_engine` (`snapSettings_surviveProjectWriteAndReopen`, `applySnapSettingsKeepsCadastralUnchecked`), `dem_trench_engine`, `layer_state_regressions`, `save_open_edit` (`layerDeleteKeyRemovesCadastralAndKeepsItGone`).

되돌리지 말 것: 지적을 참조 예외로 다시 묶기, 정합에서 CurrentLayer만 쓰기, 자석 갱신으로 꺼 둔 지적을 다시 켜기, 지운 지적도를 시작 배경이 다시 올리기.

## 줌 한 칸

- 맵·조판 휠 한 칸은 `LayerOps::kWheelZoomFactor` 1.2(20%). QGIS 기본 2.0과 조판 옛 1.35는 쓰지 않는다.
- Ctrl+휠은 조판에서 그 차이의 35%만 더한다.
- `applyWheelZoomFactor`가 캔버스와 `qgis/zoom_factor`를 같이 맞춘다. https://qgis.org/pyqgis/master/gui/QgsMapCanvas.html

파일: `LayerOps.cpp` (`applyWheelZoomFactor`), `MainWindow.cpp`, `KaDrawingStudio.cpp` (`KaLayoutMapAdjustTool::wheelEvent`).

테스트: `layer_state_regressions` (`wheelZoomFactorIsFinerThanQgisDefault`), `workflow_engine` (`layoutWheelZoom_keepsPointUnderCursor`).

되돌리지 말 것: 한 칸 2배(QGIS 기본).

## 조판 여백

- 전면 지도 칸의 위·좌·우 여백은 같다(기본 10mm). 아래만 축척·CRS·방위 띠.
- 용지 기본은 A4 세로(210×297). 현장 A4 가로는 317×220 mm. 저장된 297×210 가로는 조판을 열 때 키운다.
- 덧지도(`ka_map_above`)는 범위·레이어가 같으면 다시 그리지 않는다. `applyLayersToMap`이 본지도를 맞춘 뒤 같은 호출에서 `syncAboveLabelsMap`을 돌린다. 번호 범례는 다음 이벤트 틱에서 한 번에 올린다.
- `KaDrawingStudio::defaultMapRect`와 `LayoutService::equalFullSheetMapRect`. 저장 전면 도면은 `applyStandardChromePositions`에서 10mm로 맞춘다. 사용자가 좁게 그린 칸은 건드리지 않는다.

파일: `LayoutService.cpp`, `KaDrawingStudio.cpp` (`applyFieldPageGrow`, `applyFieldEdge`, `promptPaper`, `defaultMapRect`), `MainWindowExport.cpp`.

테스트: `workflow_engine` (`layoutEqualFullSheetMapRect_matchesTopToSides`, `layoutStandardSheetChrome_sitsBelowMap`), `save_open_drawing` (`drawingStudio_fieldPageGrowsA4LandscapeByOneCentimetre`).

되돌리지 말 것: 위·좌우를 다시 18mm, 현장 가로를 다시 297×210.

## 레이어 접힘

- 주변유적을 올리면 종류 그룹(`지정유산` 등)과 그 아래 SHP는 접힌 채 열린다.
- `HeritageImport::loadDataset`: `node->setExpanded(false)`, 종류 그룹도 `setExpanded(false)`.

파일: `HeritageImport.cpp`.

테스트: `heritage_import`.

되돌리지 말 것: 받은 뒤 종류 그룹을 펼쳐 두기.

## 번호·범례

- 조판 번호는 `HeritageLayoutNumbers` override만. 원본 레이어·명칭 라벨은 그대로.
- 같은 이름은 번호 하나. 페이지에 가장 많이 걸린 도형 `$id`에만 찍는다.
- 페이지 안 번호 원·글자는 2.99mm / 5.2pt(절반에서 30%만 키움). 얇은 검정 외곽선 0.15mm. 유적이 용지에서 1.0mm 넘게 떨어져 있으면 각자 유적 위. 같은 점(1.0mm 안)만 2.4mm 한두 칸 비키고 가는 선. 칸이 없으면 유적 위. 마을 전체를 배지 지름으로 밀지 않는다.
- 조판 번호는 `ka_map_numbers`에서 도형 위에 다시 그린다. 본 지도·덧지도(`ka_map_above`)는 `labelsEnabled=0`. PAL은 번호 지도에서만 한 번 돈다. zIndex 10000.
- PDF·미리보기 번호 결과는 `ka_map_numbers` UUID. https://qgis.org/pyqgis/master/core/QgsLayoutExporter.html
- 멤버십은 용지∩지도 발자국(`mapFootprintOnPaper`). `intersects`면 포함한다. 점의 빈 `intersection`은 버려지지 않는다.
- 용지에서 지도를 밀면 `sizePositionChanged`가 `update()`를 다시 돌린다. PAL compact(`number==0`)에 의존하지 않는다.
- 발자국·범위만 바뀌면 레이어 `clone()`과 전체 심볼 순회를 하지 않는다. 원본 분류표에서 페이지에 보이는 범주만 그린다. 원본 렌더러에 `startRender`를 걸지 않는다. https://qgis.org/pyqgis/master/core/QgsFeatureRequest.html https://qgis.org/pyqgis/master/core/QgsCategorizedSymbolRenderer.html
- 번호 위치는 C++에서 구한 페이지 위 `PositionX`/`PositionY`. `geom_from_wkt(intersection)` 생성기에 의존하지 않는다.
- 범례 숫자는 `m_entries`와 같다. 페이지에 없는 유적을 넣지 않는다.
- 원본은 단색(노드 1개)이다. 범례 뱃지는 override 분류 노드를 만든 뒤 다시 찍는다. 번호 갱신마다 `tuneSheetLegend`로 트리를 갈아엎지 않는다. https://qgis.org/pyqgis/master/core/QgsMapLayerLegendUtils.html

파일: `HeritageLayoutNumbers.cpp` (`update`, `stackedPins`, `offsetHeritageNumber`, `applyHeritageNumberCallout`, `applyBaseStyleOverrides`, `raiseAboveGeometries`, `applyLegend`, `mapFootprintOnPaper`).

테스트: `heritage_style` (`layoutNumberCirclesAndLegendMatchWithoutChangingSourceStyle`, `layoutLegendNumbersMatchEntriesAfterTuneAndFilter`, `layoutNumbersFollowPaperClipNotOffPageCentroid`, `layoutNumbersKeepOnPagePoints`, `layoutNumbersOffsetStackedAnchorsWithCallout`, `layoutNumbersKeepDenseClusterOnShortRings`, `layoutNumbersSitOnNearbyDistinctSites`), `save_open_drawing` (`drawingStudio_heritageRefreshRequestsCoalesceAndReuseOnRevisit`).

되돌리지 말 것: `$id IN (...)`로 모든 형제에 같은 숫자, 범위 전체를 범례에 넣기, 원본 SHP에 번호 쓰기, 배지 지름으로 1:25000 마을을 밀어 긴 선을 다시 그리기.

## 페이지 표시

- 1:5000~1:50000에서 페이지 안 번호는 남는다. PAL `PreventOverlap`만으로 지우지 않는다. 선은 짧은 비킴에만 그린다.
- 면은 채움 없음(원본). 조판 클론에 `CentroidFill` 점을 붙여 작은 축척에서도 보이게 한다.
- 캔버스 주변유적은 단색 도형+명칭. 번호 원은 조판만.

파일: `HeritageLayoutNumbers.cpp`, `HeritageStyle.cpp`.

테스트: `heritage_style`.

되돌리지 말 것: 작은 축척에서 PAL이 숨긴 번호를 정상으로 보기.

## 덧그림 대상

- 본 지도·조판 2차 패스는 조사 도형·지적, 그리고 주변유적 도형. 지질·토양·지형도·위성은 한 번만 그린다.
- 주변유적은 아래 글자(`labeledBelow`)가 없어도 덧그림에 모은다. 본지도에서도 빼지 않는다.
- `HeritageLayoutNumbers::update`와 `raiseAboveGeometries`는 본지도에 없는 유적도 프로젝트에서 찾는다. 번호 지도는 여섯 자료를 모두 담는다.
- 같은 이름 번호는 하나. 형제 도형은 조판에 그대로 그린다.
- 주변유적을 빼면 지적 덧그림이 유적을 가리고, 범례를 전부 껐다 켜야 다시 보인다.
- 아래 글자를 덮는 조사 선은 그대로 다시 그린다.

파일: `LabelOps.cpp` (`refreshLabelOrderCache`, `layersDrawnAboveLabels`), `HeritageLayoutNumbers.cpp` (`numberedSourceLayers`, `raiseAboveGeometries`), `KaDrawingStudio.cpp` (`applyLayersToMap`, `syncAboveLabelsMap`).

테스트: `layer_state_regressions` (`referenceMapsStayOutOfAboveLabelsPass`, `heritageDatasetStaysInAboveLabelsPass`, `heritageAloneStaysInAboveLabelsPass`), `heritage_style` (`layoutNumbersIncludeHeritageMissingFromBaseMap`, `layoutNumbersKeepEverySiblingDrawing`), `save_open_drawing` (`drawingStudioAboveLabelsMapDrawsGeometryWithoutDuplicateNumberLabels`), `workflow_engine` (빨간 선이 지번 위).

되돌리지 말 것: 지질·토양·지형도·위성을 2차 패스에 다시 넣기, 조사 선·주변유적을 2차 패스에서 빼기, 번호 지도를 본지도 레이어만으로 채우기.

## 지형도 선

- 수치지형도는 회색 0.2mm 선만. 점·면·속성은 한 번 보고 버린다.
- 팬해도 올린 선을 다시 열지 않는다. 중심이 500m 안이면 조회 없음. 선 레이어 `createSpatialIndex`.
- 조사구역이 있으면 받은 뒤 조사구역 주변 5km와 겹치는 선만 올린다. 원본 도엽은 그대로. 화면 중심 도엽 검색 반경도 5km.

파일: `KaTopographicImportDialog.cpp` (`updateCoverage`, `loadNext`), `SurveyScopeClip.cpp`, `KaTopographicScopePanel.cpp` (`kRadiusKm`).

테스트: `topographic_import` (`panDoesNotReopenPublishedOrDiscardedSources`, `fiveKilometerCoverageLoadsNeighborSheetNotFarSheet`), `survey_scope_clip`.

되돌리지 말 것: 팬마다 점·면 SHP 재오픈, 지형도에 이름 라벨, 원본 도엽을 자르기.

## 조사 주변 5km 적재

- 지적도는 이미 조사구역 `bufferMeters = 5000`으로 자른다.
- 주변유적은 인트라넷 시·군 ZIP을 받은 뒤 `HeritageImport`가 조사구역 5km만 올린다. ZIP은 그대로.
- 맵 유적명 PAL은 1:10000보다 작으면 끈다. 조판 번호는 그대로.
- 조판 `ka_map`은 조사·지적을 빼고 `ka_map_above`에 그린다. 주변유적은 본지도에 남겨 범례·도형이 빠지지 않게 하고, 덧지도에도 다시 그린다.

파일: `SurveyScopeClip.cpp`, `HeritageImport.cpp`, `HeritageStyle.cpp`, `KaDrawingStudio.cpp` (`sheetBasePaintLayers`).

테스트: `survey_scope_clip`, `heritage_import` (`loadKeepsOnlySurveyBuffer`), `heritage_style` (`canvasNamesHideWhenZoomedOutPastTenThousand`), `layer_state_regressions` (`sheetBasePaintLayersOmitAboveGeometries`).

되돌리지 말 것: 인트라넷/NGII 수제 반경 API, 원본 ZIP/도엽 수정.
