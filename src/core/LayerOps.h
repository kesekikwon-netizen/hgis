#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QColor>
#include <functional>
#include <qgsfeatureid.h>
class QgsProject;
class QgsVectorLayer;
class QgsRasterLayer;
class QgsMapLayer;
class QgsMapCanvas;
class QgsRectangle;
class QgsPointXY;
class QgsFeature;
class QgsLayerTreeGroup;
class QgsLayerTreeLayer;
class QgsGeometry;
class QgsCoordinateReferenceSystem;

class LayerOps {
public:
  static constexpr const char* kGroupSurveyData = "조사 데이터";
  static constexpr const char* kGroupReference = "참조 지도";
  static constexpr const char* kPropLayerKey = "ka_hgis/layer_key";
  static constexpr const char* kPropLayerRole = "ka_hgis/layer_role";
  static constexpr const char* kAdminEmdKey = "admin_emd";
  static constexpr const char* kRoleSurvey = "survey";
  static constexpr const char* kRoleReference = "reference";
  static constexpr const char* kPropAlignPending = "ka_hgis/align_pending";
  /// 신규 주제도 내려받기 범위를 정하는 축척. 내려받은 레이어의 표시 제한이 아니다.
  static constexpr double kThematicMinScaleDenom = 100000.0;
  static void applyThematicOverlayScaleRange(QgsMapLayer* layer);
  static void restoreThematicOverlayVisibility(QgsProject* project);
  /// Move a legend row without removing the datasource from the project.
  static bool moveLegendLayer(QgsLayerTreeLayer* node, int destinationIndex);
  static bool clampCanvasToThematicScale(QgsMapCanvas* canvas);
  /// Grow a 5186 envelope around its center until the longer side equals
  /// maxSpanMeters. Too-large envelopes are returned unchanged (caller rejects).
  static QgsRectangle expandExtentToMaxSpan(const QgsRectangle& extent, double maxSpanMeters);

  static void setAlignPending(QgsMapLayer* layer, bool pending);
  static bool isAlignPending(const QgsMapLayer* layer);

  enum class KoreaBasemap {
    VWorldBase,
    VWorldSatellite,
    VWorldHybrid,
    GoogleRoad,
    GoogleSatellite,
    Osm
  };

  // addToMap: 재투영 결과를 범례에 올릴지. 제출용 5179는 false — 작업 CRS 지도에
  // 업로드 레이어를 섞지 않는다. project 는 변환 맥락용으로 그대로 넘긴다.
  static QString reprojectVectorLayer(QgsVectorLayer* layer, const QString& targetCrsAuthId,
                                      const QString& outPath, QgsProject* project,
                                      QString* errorOut = nullptr, bool addToMap = true);

  static int ensureControlPointQualityFields(QgsVectorLayer* controlPoints);

  static bool applyFeaturePolyStyle(QgsVectorLayer* featurePoly);
  static bool applyDomainDrawStyle(QgsVectorLayer* layer, const QString& layerKey = {});
  static bool applyAreaM2Labels(QgsVectorLayer* layer);
  static QString detectNameField(const QgsVectorLayer* layer);
  struct FeatureFormFields {
    int nameIndex = -1;
    int numberIndex = -1;
    QString nameField;
    QString numberField;
  };
  // Name = survey_name/site_name/name/kind. Number = feature_no/artifact_no/section_id/point_id.
  static FeatureFormFields featureFormFields(const QgsVectorLayer* layer);
  // Writes name/number into the edit buffer. Does not commit. Empty text clears the field.
  static bool applyFeatureFormValues(QgsVectorLayer* layer, qint64 featureId, const QString& name,
                                     const QString& number, QString* errorOut = nullptr);
  static bool applyNameAttributeLabels(QgsVectorLayer* layer, const QString& fieldName = QString(),
                                       double fontSizePt = 5.0, bool showArea = false);
  static bool setLabelFontSize(QgsVectorLayer* layer, double fontSizePt);
  static double labelFontSize(const QgsVectorLayer* layer, double defaultSize = 5.0);
  static bool labelShowArea(const QgsVectorLayer* layer, bool defaultShow = false);
  static QString currentLabelField(const QgsVectorLayer* layer);
  // 외부에서 받은 SHP 파일(.cpg 없는 경우)의 한글 깨짐을 방지하기 위한 인코딩 준비 및 변경
  static QString prepareShapefileEncoding(const QString& shpPath);
  static bool setShapefileEncoding(QgsVectorLayer* layer, const QString& encoding);
  // Vector labeling only. Cadastral WMS/XYZ text is baked into tiles.
  static bool hasToggleableLabels(const QgsMapLayer* layer);
  // 레이어가 밑에 있으면 글자도 밑으로. 레이어 순서가 바뀔 때마다 부른다.
  static void applyLayerOrderToLabels(QgsProject* project, QgsMapCanvas* canvas = nullptr);
  // 라벨 뒤(위)에 한 번 더 그려야 하는 레이어들. 맨 위가 앞이다.
  // QGIS 는 도형을 모두 그린 뒤 라벨을 한 번에 얹으므로, 위 레이어가 아래
  // 레이어의 글자에 가린다. 그 레이어들을 2차 패스로 다시 그리기 위한 목록이다.
  static QList<QgsMapLayer*> layersDrawnAboveLabels(QgsProject* project);
  static bool labelsVisible(const QgsMapLayer* layer);
  static bool setLabelsVisible(QgsMapLayer* layer, bool on);
  static bool applySimpleVectorStyle(QgsVectorLayer* layer, const QColor& fill, const QColor& stroke,
                                     double strokeWidthMm, double markerSizeMm = 3.5,
                                     bool noFill = false, bool noStroke = false, bool dashed = false);
  static bool readSimpleVectorStyle(const QgsVectorLayer* layer, QColor* fill, QColor* stroke,
                                    double* strokeWidthMm, double* markerSizeMm,
                                    bool* noFill = nullptr, bool* noStroke = nullptr,
                                    bool* dashed = nullptr);
  static bool mergePolygonFeatures(QgsVectorLayer* layer, QString* errorOut = nullptr);
  static bool mergePolygonFeatures(QgsVectorLayer* layer, const QgsFeatureIds& featureIds, QString* errorOut = nullptr);
  static bool explodeMultipartFeatures(QgsVectorLayer* layer, const QgsFeatureIds& featureIds = QgsFeatureIds(), QString* errorOut = nullptr);
  static QgsVectorLayer* clipLayerByBoundary(QgsVectorLayer* sourceLayer,
                                             QgsVectorLayer* boundaryLayer,
                                             QgsProject* project,
                                             QString* errorOut = nullptr);
  static bool splitPolygonWithLine(QgsVectorLayer* layer,
                                   const QVector<QgsPointXY>& splitLine,
                                   QString* errorOut = nullptr);
  static bool splitTwoOverlappingFeatures(QgsVectorLayer* layer1, qint64 fid1,
                                          QgsVectorLayer* layer2, qint64 fid2,
                                          qint64* outCreatedFid = nullptr,
                                          QgsVectorLayer** outTargetLayer = nullptr,
                                          QString* errorOut = nullptr);
  static bool restoreDeletedFeature(QgsVectorLayer* layer, const QgsFeature& feat, QString* errorOut = nullptr);

  struct FieldBasemapPackResult {
    bool satelliteOk = false;
    bool cadastralOk = false;
  };
  static double suggestCadastralScale(double currentScale, double target = 4000.0,
                                      double maxOk = 5000.0);
  static FieldBasemapPackResult prepareFieldBasemapPack(QgsProject* project, QgsMapCanvas* canvas,
                                                        const QString& apiKey,
                                                        const QString& workCrsAuthId,
                                                        QString* errorOut = nullptr);

  static bool addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);

  static bool addVworldBaseMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldSatelliteMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);
  static void applyCanvasScreenDpi(QgsMapCanvas* canvas);
  static void refreshXyzBasemapTiles(QgsMapCanvas* canvas);
  // XYZ/WMS 타일: 브라우저형 User-Agent. 입체지형 Google drape도 호출한다.
  static void ensureTileNetworkIdentity();
  // Show / DevicePixelRatioChange: 4K·혼합 DPI에서 XYZ 타일을 다시 받는다.
  static bool canvasDisplayEventNeedsTileRefresh(int eventType);

  static bool addVworldCadastralMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);
  // VWorld GetCapabilities only publish 4326 (and 900913/3857). 5186/5187/5179
  // make QGIS WMS fail with "Cannot calculate extent".
  static QStringList cadastralWmsCrsCandidates(const QString& workCrsAuthId = {});

  static bool addVworldHybridMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldContourMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  // 지형맵: OpenTopoMap XYZ only (EPSG:3857). Never VWorld WMS GetMap —
  // WMS + OTF(QgsRasterProjector) + pan aborts TileDownloadManager (Windows AV).
  // 작업 CRS(5186/5187)는 OTF destinationCrs — URI에 5186/5179를 넣지 않는다.
  static bool addElevationHillshadeMap(QgsProject* project, QgsMapCanvas* canvas,
                                       const QString& apiKey, QString* errorOut = nullptr);

  // DEM click: Copernicus GLO-30 COG (single-band, meter legend) when the
  // canvas has an extent; NASA GIBS color tiles as fallback. Never VWorld WMS.
  static bool addDemColorReliefMap(QgsProject* project, QgsMapCanvas* canvas,
                                   QString* errorOut = nullptr);
  // Copernicus GLO-30 COG (/vsicurl). lat/lon in WGS84 degrees.
  static QString copernicusCogUriForWgs84(double latDeg, double lonDeg);

  // 국토지리원 공개DEM(.img) / GeoTIFF. Single-band elevation + meter ramp.
  static bool addDemElevationRaster(QgsProject* project, QgsMapCanvas* canvas,
                                    const QString& path, QString* errorOut = nullptr);

  // Color-ramp renderer so 조판 범례 lists height in meters.
  struct DemElevationClass {
    double lo = 0.0;
    double hi = 0.0;  // last class is +inf
    QColor color;
    QString label;
  };
  struct DemElevationStyle {
    int classCount = 0;      // 0 = auto (2–8)
    double stepMeters = 0.0; // 0 = auto nice step
    QList<DemElevationClass> classes;  // if set, used as-is (last forced +inf)
  };
  static bool applyDemElevationStyle(QgsRasterLayer* layer);
  static bool applyDemElevationStyle(QgsRasterLayer* layer, const QgsRectangle& statsExtent);
  static bool applyDemElevationStyle(QgsRasterLayer* layer, const QgsRectangle& statsExtent,
                                     const DemElevationStyle& style);
  static QList<DemElevationClass> buildDemElevationClasses(double zMin, double zMax, int classCount,
                                                           double stepMeters);
  static QList<DemElevationClass> readDemElevationClasses(const QgsRasterLayer* layer);
  // Equal-interval step (m) so alluvial sites get 1–5 m classes, not one -2…1155 band.
  static double demElevationClassStep(double zMin, double zMax);
  // Ensure a high-resolution shaded relief layer (Hillshade) is blended over the DEM (Multiply blend).
  static QgsRasterLayer* ensureDemRelief(QgsProject* project, QgsRasterLayer* demLayer);
  // 지금 올라와 있는 DEM이 화면 전체를 덮고 있는가. 덮지 못하면 다시 만들어야 한다.
  static bool demCoversCanvas(QgsProject* project, QgsMapCanvas* canvas);

  // 흙토람(농진청) 토양도 신청으로 내려받은 SHP를 참조 지도로 불러온다.
  // crsOverrideAuthId가 비어 있지 않으면 레이어 좌표계를 그 값으로 지정한다
  // (.prj 없는 배포본 대응 — 흙토람 고시 좌표계는 EPSG:2097 중부원점/Bessel).
  // categoryField가 있으면 그 필드 값별 반투명 색으로 구분한다(분포지형 등).
  // 성공 시 프로젝트에 추가된 레이어를 반환한다.
  static QgsVectorLayer* addSoilShapefile(QgsProject* project, QgsMapCanvas* canvas,
                                          const QString& path, const QString& crsOverrideAuthId,
                                          const QString& categoryField, QString* errorOut = nullptr);

  static bool setLayerOpacity(QgsProject* project, QgsMapCanvas* canvas, const QString& name, double opacity);
  static bool setMapLayerOpacity(QgsMapLayer* layer, double opacity, QgsMapCanvas* canvas = nullptr);
  static double mapLayerOpacity(const QgsMapLayer* layer);
  // 그림(래스터) 밝기. -255~255, 0이 원본. 벡터에는 밝기가 없다.
  static bool canAdjustBrightness(const QgsMapLayer* layer);
  static bool setMapLayerBrightness(QgsMapLayer* layer, int brightness, QgsMapCanvas* canvas = nullptr);
  static int mapLayerBrightness(const QgsMapLayer* layer);
  static bool isReferenceOrBasemapLayer(const QgsMapLayer* layer);

  static bool toggleLayerVisibility(QgsProject* project, QgsMapCanvas* canvas, const QString& name, bool visible);
  static bool isLayerVisible(QgsProject* project, const QString& name);
  // refresh() only when no WMS/XYZ job is in flight (provider_wms deleteLater AV).
  static void refreshCanvasIfIdle(QgsMapCanvas* canvas);

  static bool addKoreaBasemap(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                              QString* errorOut = nullptr);

  static bool setWorkCrs(QgsProject* project, QgsMapCanvas* canvas, const QString& epsgAuthId,
                         QString* errorOut = nullptr, bool zoomKorea = true);

  static bool ensureOtfEnabled(QgsProject* project, QgsMapCanvas* canvas, const QString& workCrsAuthId);

  static QgsRectangle koreaExtentForCrs(const QString& epsgAuthId);
  // 3857 위성 커버를 작업 CRS로 옮긴 뒤, 타일이 화면을 메우는 안쪽 상자.
  static QgsRectangle satelliteFillExtentForCrs(const QString& epsgAuthId);

  static void zoomToKorea(QgsMapCanvas* canvas, const QString& epsgAuthId, bool refresh = true);
  static void syncMapCanvas(QgsProject* project, QgsMapCanvas* canvas, bool zoomKorea = true);
  static QList<QgsMapLayer*> visibleLayersPaintOrder(QgsProject* project);
  // 진단: 레이어 하나하나의 상태를 한 줄로 적는다. 세션 로그에서 "언제 사라졌나"를
  // 되짚을 수 있는 유일한 근거다. id|이름|유효|범례노드|체크|피처수 순.
  static QString layerCensus(QgsProject* project);
  // 데이터 원본이 잠깐 끊겨 무효가 된 레이어를 같은 원본으로 다시 연다. GPKG에 쓰는
  // 동안(자동 저장·스타일 기록)이나 OneDrive가 파일을 바꿔치기할 때 생긴다.
  // 되살린 레이어 이름은 revived에, 끝내 못 살린 것은 stillBroken에 담는다.
  static int reviveInvalidLayers(QgsProject* project, QStringList* revived = nullptr,
                                 QStringList* stillBroken = nullptr);
  // 저장된 작업공간의 VWorld 주소에는 그때 쓰던 인증키가 통째로 박혀 있다. 키를 새로
  // 발급받아도 예전 조사를 열면 만료된 키로 타일을 받아 배경지도가 백지가 된다.
  // 여는 순간 현재 키로 갈아 끼운다. 바꾼 레이어 수를 돌려준다.
  static int refreshVworldApiKeyInLayers(QgsProject* project, const QString& currentKey,
                                         QStringList* changed = nullptr);
  // 위 함수가 쓰는 순수 문자열 치환. 테스트에서 직접 부른다.
  static QString withVworldApiKey(const QString& source, const QString& currentKey);
  static void ensureSatelliteAtBottom(QgsProject* project);
  static void pruneDuplicateSatelliteLayers(QgsProject* project);
  static bool zoomToLayerMax(QgsMapCanvas* canvas, QgsMapLayer* layer);
  static bool zoomToProjectDataLayers(QgsMapCanvas* canvas, QgsProject* project);
  static bool isolateAndZoomToLayer(QgsProject* project, QgsMapCanvas* canvas, QgsMapLayer* layer,
                                    bool keepReference = true);
  static bool isAdminEmdLayer(const QgsMapLayer* layer);
  static bool isImportedSiteLayer(const QgsMapLayer* layer);
  static QgsVectorLayer* findImportedSiteLayer(QgsProject* project);
  static bool applyInvertedPaperMask(QgsVectorLayer* layer);
  static QgsVectorLayer* upsertAdminEmdMask(QgsProject* project, const QgsGeometry& geom,
                                            const QgsCoordinateReferenceSystem& srcCrs,
                                            const QString& workCrsAuthId, const QString& titleKo);
  static bool isolateSurfaceSurveyView(QgsProject* project, QgsMapCanvas* canvas,
                                       QgsMapLayer* siteLayer = nullptr);
  static void zoomToFullMax(QgsMapCanvas* canvas);
  static void applyKoreaMapLimits(QgsProject* project, QgsMapCanvas* canvas);
  static bool clampCanvasToKorea(QgsMapCanvas* canvas);

  static QString convertToShp5179(QgsVectorLayer* layer, const QString& outShpPath,
                                  QgsProject* project, QString* errorOut = nullptr,
                                  bool addToMap = false);

  static QString convertFileToShp5179(const QString& inPath, const QString& outShpPath,
                                      QgsProject* project, QString* errorOut = nullptr,
                                      bool addToMap = false);

  static QString georeferenceImageSimple(const QString& imagePath, QgsVectorLayer* controlPoints,
                                         QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);

  static bool hasVisibleReferenceLayer(QgsProject* project);

  static bool removeConfirmedLayers(QgsProject* project, QgsMapCanvas* canvas, const QStringList& layerIds);

  struct ControlCsvPreview {
    bool ok = false;
    int count = 0;
    bool geographic = false;
    bool swapSuggested = false;
    QString encoding;
    QString summary;
  };
  // 조사구역이 있으면 한국 측량 관례(X=북쪽, Y=동쪽)로 읽은 점이 더 가까운지 본다.
  // 경위도는 EPSG:4326으로 보고, 가져오기는 호출자가 swapAxes를 고른 뒤에 한다.
  static ControlCsvPreview previewControlPointsCsv(QgsVectorLayer* controlPoints, const QString& csvPath,
                                                   QgsProject* project, QString* errorOut = nullptr);
  static QString controlPointAxisHint();
  static QgsPointXY controlPointMapXy(double x, double y, bool swapAxes);
  static ControlCsvPreview suggestControlPointAxisSwap(QgsProject* project, double x, double y);
  static int importControlPointsCsv(QgsVectorLayer* controlPoints, const QString& csvPath,
                                    QString* errorOut = nullptr, bool swapAxes = false);

  static QgsLayerTreeGroup* ensureLegendGroup(QgsProject* project, const QString& groupName);
  static void placeInLegendGroup(QgsProject* project, QgsMapLayer* layer, const QString& groupName,
                                 bool insertAtBottom = false);
  static void markSurveyLayer(QgsMapLayer* layer, const QString& layerKey);
  static void markReferenceLayer(QgsMapLayer* layer);
  // 미리 받아 둔 MBTiles 타일팩을 참조 지도로 올린다. 원격 XYZ와 같은 자리에
  // 쓰이지만 네트워크를 타지 않는다(TilePackService가 만든 파일).
  static bool addTilePackBasemap(QgsProject* project, QgsMapCanvas* canvas, const QString& path,
                                 const QString& name, QString* errorOut);
  // RGB GeoTIFF 용지 흰색을 투명으로. 이미 적용돼 있으면 그대로 둔다.
  static void knockOutRasterPaper(QgsRasterLayer* layer);
  static void knockOutProjectRasterPaper(QgsProject* project);
  static QString layerKeyOf(const QgsMapLayer* layer);
  static bool isReferenceLayer(const QgsMapLayer* layer);
  static bool isBasemapLayer(const QgsMapLayer* layer);
  static QgsVectorLayer* findByLayerKey(QgsProject* project, const QString& layerKey);
  // 같은 layer_key 를 가진 레이어를 모두 돌려준다. 조사구역처럼 사용자가 레이어를
  // 여러 개 만들 수 있으므로, 제출 패키지가 하나만 내보내지 않게 하려고 쓴다.
  // 이름 폴백은 하지 않는다. 표시 이름만 도메인 이름으로 바꾼 참조 자료가 섞이면 안 된다.
  // 순서는 범례(레이어 트리) 순서를 따르므로 실행할 때마다 같다.
  static QList<QgsVectorLayer*> findAllByLayerKey(QgsProject* project, const QString& layerKey);
  // 제출과 검수가 함께 쓰는 도메인 레이어 목록. 키로 찾고, 키를 가진 레이어가 하나도
  // 없을 때만 이름으로 찾는다(layer_key 가 없는 예전 조사 대응). 다른 키를 가진
  // 레이어와 참조 자료는 제외한다.
  static QList<QgsVectorLayer*> domainLayersForKey(QgsProject* project, const QString& layerKey);
  // Current layer is used only when its ka_hgis/layer_key equals requiredKey.
  // 유구면 must not fall back to survey_area just because it is the current polygon.
  static QgsVectorLayer* digitizeTargetLayer(QgsProject* project, QgsVectorLayer* current,
                                             const QString& requiredKey);
  static void removeSurveyDomainLayers(QgsProject* project);
  static QStringList domainLayerKeys();
  static void pruneEmptyLegendGroups(QgsProject* project);
  static QList<QgsVectorLayer*> surveyAreaLayers(QgsProject* project);
  static QgsVectorLayer* createSurveyAreaLayer(QgsProject* project, const QString& gpkgPath,
                                               const QString& titleKo, const QColor& stroke,
                                               const QColor& fill, double widthMm,
                                               QString* errorOut = nullptr);
  static QgsVectorLayer* ensureDomainLayer(QgsProject* project, const QString& gpkgPath,
                                           const QString& layerKey, const QString& titleKo,
                                           QString* errorOut = nullptr);
  // GPKG 테이블에 도형이 있는데 작업공간에 없으면 범례에 올린다. 빈 테이블은 올리지 않는다.
  static int addNonEmptyDomainLayers(QgsProject* project, const QString& gpkgPath);
  // 저장된 프로젝트에 등록되어 있지만 범례에서 빠진 유효 레이어의 노드만 복원한다.
  // 외부 벡터·사진도 포함하며 기존 노드의 표시 여부·순서는 바꾸지 않는다.
  static int restoreMissingLayerTreeNodes(QgsProject* project);
  // 도메인 + user_poly_* + 저장 때 들여온 벡터 테이블. 깨진(invalid) 작업공간
  // 레이어는 제거하고 같은 GPKG 테이블을 다시 연다. 빈/메타 테이블은 올리지 않는다.
  // 유효한 레이어의 빠진 범례 노드도 복원한다. 기존 노드의 표시 여부는 유지한다.
  static int addNonEmptySavedGpkgLayers(QgsProject* project, const QString& gpkgPath);
  // GPKG layer_styles 기본 심볼. leftover/LayersOnly 가 공장색으로 덮지 않게 한다.
  static bool loadGpkgDefaultStyle(QgsVectorLayer* layer);
  static int saveGpkgDefaultStyles(QgsProject* project, const QString& gpkgPath);
  // 조사 파일에 작업공간을 쓴 뒤, 아직 유효해 보이는 OGR 레이어도
  // GetNextRawFeature 가 실패하면 도형이 지도에서 사라진다. 커밋되지 않은 편집은 유지한다.
  static int reloadSurveyGpkgReaders(QgsProject* project, const QString& gpkgPath);
  static QgsVectorLayer* createUserPolygonLayer(QgsProject* project, const QString& gpkgPath,
                                                const QString& titleKo, const QString& crsAuthId,
                                                QString* errorOut = nullptr);
  static void applyLegendCrsLabel(QgsMapLayer* layer);
  static bool undoCommittedFeature(QgsVectorLayer* layer, qint64 featureId, QString* errorOut = nullptr);
  // startEditing + begin/endEditCommand. Does not commit; QGIS undoStack keeps the command.
  static bool runEditCommand(QgsVectorLayer* layer, const QString& title,
                             const std::function<bool()>& change, QString* errorOut = nullptr);
  static QgsVectorLayer* preferredUndoLayer(QgsProject* project, QgsVectorLayer* preferred = nullptr);
  static QgsVectorLayer* preferredRedoLayer(QgsProject* project, QgsVectorLayer* preferred = nullptr);
  static bool undoLayerEdits(QgsVectorLayer* layer);
  static bool redoLayerEdits(QgsVectorLayer* layer);
  // Deletes every saved feature and writes GPKG. After this, ensureDomainLayer
  // must reopen the same table empty — legend-only remove leaves the polygons.
  static bool purgeCommittedFeatures(QgsVectorLayer* layer, QString* errorOut = nullptr);
  // Moves one vertex of a saved feature. Does not commit; caller writes GPKG.
  static bool moveFeatureVertex(QgsVectorLayer* layer, qint64 featureId, int vertex,
                                double x, double y, QString* errorOut = nullptr);

  enum class SnapTarget { CurrentLayer, SurveyLayers };
  struct SnapSettings {
    bool enabled = true;
    double tolerancePx = 16.0;
    SnapTarget target = SnapTarget::SurveyLayers;
    bool topological = true;
  };
  // Writes QgsSnappingConfig plus ka_hgis/snap_target. SurveyLayers uses AdvancedConfiguration
  // on non-reference vectors only. CurrentLayer uses ActiveLayer.
  // topological writes QgsProject::setTopologicalEditing and ka_hgis/topological.
  static void applySnapSettings(QgsProject* project, const SnapSettings& settings);
  static SnapSettings readSnapSettings(const QgsProject* project);
  // Moves one vertex. If topological, also moves other features' vertices at the
  // same place (1 mm). Does not commit. Layer must already be editable.
  static bool applyVertexMove(QgsVectorLayer* layer, qint64 featureId, int vertex,
                              double x, double y, bool topological, QString* errorOut = nullptr);
};
