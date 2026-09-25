# 한 번 난 오류 — 재발 금지

같은 증상이 다시 나오면 이 표의 **고정 방법**을 먼저 확인한다.
새 오류를 고치면 이 파일과 `tests/test_workflow.cpp`에 회귀 테스트를 추가한다.

| 증상 | 원인 | 재발 방지 |
| --- | --- | --- |
| 위성/지적 버튼을 눌러도 지도가 안 보임 | 공식 WMTS URI가 `isValid()`만 통과하고 타일은 실패. 폴백 xdworld를 안 탐. 지적 WMS를 5186/5179로 먼저 열어 `Cannot calculate extent`. | 위성은 xdworld를 먼저. 지적은 `cadastralWmsCrsCandidates()`가 **4326→3857→900913만**. 5186/5187/5179 금지. 테스트: `cadastralWmsCrs_neverStartsWithWorkCrs5179`, `addVworldSatellite_allowsEmptyKeyViaPublicTiles` |
| 지적 레이어는 범례에 있는데 필지가 안 보임 | 구버전(eac6c9c)은 `crs=EPSG:3857` + `tilePixelRatio=2` + `KEY/DOMAIN` tiled WMS. 이후 GDAL/4326/5179 경로가 그걸 깨뜨림. | 구버전 URI로 복구. GDAL은 최후 폴백만. 테스트: `cadastralWmsCrs_neverStartsWithWorkCrs5179` |
| 「이 레이어만 보기」가 그 레이어로 안 감 | `QgsRectangle::isEmpty()`가 점/축평행 선을 “도형 없음”으로 처리. 우클릭 선택을 `QgsLayerTreeModel`로 캐스팅해 실패. | `isNull()`/`isFinite()`만 실패. `index2node` + `currentLayer()`. 줌 실패 시 isolate 안 함. 테스트: `zoomToLayerMax_movesCanvasToPointFeature`, `isolateAndZoom_hidesOtherSurveyKeepsReference` |
| “실행했다”는데 창이 없음 | 에이전트 Job Object가 자식 프로세스를 같이 종료. `exe` 직접 실행은 OSGeo DLL 경로 없음. | `고고학 전용 HGIS.lnk` / `scripts/start-ka-hgis.vbs` / `launch.ps1`. explorer로 분리 실행. |
| 조판 축척자가 0 m, 지적 안 보임, 범례 크기 무시 | 지도 칸이 **선택된** 레이어만 씀. `setLayers` 순서가 해시라 위성이 지적 위. 축척자 `applyDefaultSize` 없음. 범례 `resizeToContents`가 드래그 크기를 덮음. | `visibleLayersPaintOrder`(조사→지적→위성). 체크된 레이어만. 축척자 `setLinkedMap`+`applyDefaultSize`. 범례 `setResizeToContents(false)`. 설정은 즉시 반영. 테스트: `layoutStudio_checkedLayersScaleBarLegendSize` |
| 조판에서 용지 안 범위를 그리면 다운 | (1) QGIS 고무줄 `layout()` null 역참조. (2) `setLayers({})` = 위성·지적 WMS 전체. (3) `setExtent`가 칸 높이를 다시 씀. (4) `QgsLayoutViewToolSelect::setLayout` 미호출 → `mMouseHandles` null. (5) `setCurrentLayout(nullptr)`는 `pageCollection()`을 무조건 탐. | QGIS 고무줄 금지. `zoomToExtent`. 빈 레이어 금지. WMS는 큐 이후. `m_toolSelect->setLayout(ly)`. 용지 교체는 `setScene(nullptr)` 후 새 시트. `refresh()` 금지. 테스트: `layoutBlankSheetMapItemKeepsFrameAndNonEmptyLayers` |
| 다른 좌표계가 안 겹침 | QGIS 3는 항상 OTF. `ensureOtfEnabled`가 작업 CRS(5186/5187)를 캔버스 목적 CRS로 두고, 위성 3857은 재투영한다. | `ensureOtf_projectAndCanvasDestinationCrs5186`, `syncMapCanvas_surveyLayersAboveReferenceBasemap`. 업로드만 5179. |
| 배경/위성이 안 보임 (키는 있는데) | 위성이 xdworld를 먼저 열어 `isValid()`만 통과하고, 저장된 VWorld 키 WMTS를 안 탐. | 키가 있으면 `api.vworld.kr` WMTS를 먼저. xdworld는 키 없을 때만. 테스트: `addVworldSatellite_usesOfficialWmtsWhenKeyPresent` |
| 조사 열고 몇 초 뒤 앱이 꺼짐 | 위성 XYZ(`provider_wms`) 타일이 내려오는 중 `refreshXyzBasemapTiles`가 `stopRendering`+`clearCache`+600/1800ms 재갱신. `QgsTileDownloadManagerReply::finished` → `deleteLater` → `lockThreadPostEventList` `0xc0000005`. ParallelJob+OTF도 같은 AV. | 갱신은 `isDrawing()`이면 skip, stop/clear 금지. 시작 뷰 재갱신 타이머 제거. `qgis/parallel_rendering=false`, `setMaxThreads(1)`. 테스트: `refreshXyzBasemapTiles_doesNotAbortInFlightWmsJob`, `startupView_doesNotRestackXyzRefreshWhileWmsDownloads` |
| 제주에서 지질도가 안 나옴 / 범례가 본토와 다름 | 본토 litho WFS 남단 33.97°N. 예전에는 제주를 공식 WMS 래스터로 올려 범례가 색칩만 됨. 제주 암상 WFS `l_jeju_50k_geology_litho_view`는 본토와 같은 기호·지층 필드. | 제주는 Jeju litho WFS + `applyGeologyStyle`. 래스터는 최후 수단. 테스트: `geologyLithoWfs_excludesJejuUsesOfficialRasterUri`, `geologyJejuLitho_sameCategorizedLegendAsMainland` |
| 지질도 켠 뒤 앱이 꺼짐 (0xc0000005) | `sampleOfficialColors` blocking WMS GetMap + `canvas->refresh` / `layersAdded→refreshMapCanvasNow`가 위성 타일 그리기 중 nested event loop → `provider_wms` `deleteLater` AV. 2026-09-01 08:17 병산동 덤프. | 그리기 중 공식색 샘플·refresh 금지. `refreshCanvasIfIdle`. 테스트: `geologyDownload_skipsBlockingWmsAndRefreshWhileDrawing` |
| 토양도 첫 로딩이 20~90초 | `expandExtentToMaxSpan`이 조사 화면(~4 km)을 80 km로 키운 뒤 SOIL_1/2/3 전체 속성 GeoJSON을 순차 수신. 제주 4 km SOIL_1만 2.8 MB/3.8초, 80 km는 1.5만 피처. | 화면 bbox만. `propertyName=soil_type_geo,geom`. 빈 테이블은 `resultType=hits`로 건너뜀. 테스트: `soilDownload_usesViewExtentAndTerrainFieldOnly` |
| 토양도는 받아졌는데 화면에 폴리곤이 없음 | WFS `propertyName=soil_type_geo`만 요청하면 GeoServer가 `geom`을 빼 `geometry:null` → GPKG `MULTIPOLYGON EMPTY`. 제주 광령리 2026-09-01 09:03. | `propertyName=soil_type_geo,geom`. 빈 도형은 저장하지 않음. 테스트: `soilDownload_usesViewExtentAndTerrainFieldOnly` |
| 토양도 산능선이 비어 있음 (고지형 꺼도 동일) | SOIL_1/2/3 WFS·WMS는 정밀 벡터라 능선에 폴리곤이 없다. 흙토람 웹은 같은 서버의 `TOP_A_SOIL_T_GEO` 3857 타일(L08–L15)로 산악지를 채운다. | 벡터 아래에 XYZ 그림. URI `crs=EPSG:3857`, zmax=15. 십진 타일 경로를 `L00/Rhex/Chex`로 바꿈. 테스트: `soilTerrainPicture_is3857ArcGisCacheNot5186Wms` |
| 조판 범례가 흙토람 그림·지적·위성 이름만 | 그림 XYZ/WMS는 색칩이 없고, 범례 기본이 `AllProjectLayers`라 레이어 제목만 나열됨. | `tuneSheetLegend`: Manual 전용 트리에서 그림·지적·위성 제거, 벡터 분포지형 분류 유지. 테스트: `sheetLegend_soilShowsTerrainClassesNotPictureName` |
| 레이어에서 토양도를 꺼도 조판 범례에 남음 | Manual 범례가 레이어 체크를 안 따라가고, 숨긴 `토양도(흙토람)`을 다시 넣었음. | 체크·지도에 있는 레이어만 범례. 끄면 빠지고 켜면 돌아옴. 테스트: `sheetLegend_followsLayerCheckOnAndOff` |
| 조판으로 넘어가면 맵이 축소된다 | 진입 시 가운데만 옮기고 조판 축척을 유지. 첫 칸은 `niceScaleDenominator`가 1:1847→1:2000처럼 분모를 올림. | `applyCanvasViewToLayoutMap`: 화면 extent + canvas scale. 테스트: `layoutEnter_matchesCanvasViewWithoutNiceSnap` |
| 한글 사용자 경로 PC에서 CTest·조판 범례/번호·참조 지도 Ctrl+Z가 깨짐 | VWorld 지적 본번·부번이 `참조 지도` 안에 들어가 묶음 삭제·복원 개수가 틀림. `tuneSheetLegend` 지도 필터·스타일 재적용이 유적 번호 노드를 지움. 낮은 줌에서 방위 드래그가 회전 핸들에 걸림. | VWorld 지적은 트리 루트. 범례는 불필요 재빌드·지도필터 금지·번호 배지 restamp. 방위 드래그 전 paper-fit 대기+줌. 2026-09-25 이 PC(`C:\Users\권을\…`) Release CTest **65/65** (`ctest_exit=0`, 540.64s). |

## 지적 GetMap (확인된 사실)

- 키 + `domain=localhost`로 GetCapabilities에 `lp_pa_cbnd_bonbun` 있음. bbox는 **EPSG:4326만**.
- 약 600m 창 EPSG:3857 GetMap은 주황 필지선 PNG.
- 수십 km 창은 투명 빈 PNG. 그래서 한국 전체 축척에서는 지적 선이 안 나온다.
