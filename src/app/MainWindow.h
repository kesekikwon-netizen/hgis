#pragma once
#include <QColor>
#include <QMainWindow>
#include <QJsonObject>
#include <QPair>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <vector>
#include <memory>
#include "core/LocationSearch.h"
#include "core/AdminBoundaryService.h"
#include "core/TrenchGridGenerator.h"
#include "core/HeritageImport.h"
class QListWidget;
class QListWidgetItem;
class QAction;
class QMenu;
class QLabel;
class QToolBar;
class QToolButton;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QEvent;
class KaAboveLabelsOverlay;
class KaLayerOpacityRail;
class KaReferenceDownloadJob;
class KaTopographicBrowser;
class KaTopographicImportDialog;
class QgsFeedback;
struct PreparedReferenceMap;
class QProgressDialog;
struct KaRemovedLayers;
class QgsRectangle;
class QShowEvent;
class QCloseEvent;
class QTabWidget;
class QFrame;
class ChecklistEngine;
class KaStatusBar;
class KaBeginnerRibbon;
#if KA_HGIS_HAS_QGIS
#include <qgsfeature.h>
class QgsMapCanvas;
class QgsLayerTreeView;
enum class HeritageDataset;  // core/HeritageStyle.h
class QgsMapLayer;
class QgsVectorLayer;
class QgsRasterLayer;
class QgsMapTool;
class QgsMapToolEmitPoint;
class QgsPointXY;
class KaCaptureMapTool;
class KaAttributeMapTool;
class KaAlignMapTool;
class KaAlignPickTool;
class KaImageView;
class KaAlignLinkOverlay;
class KaDrawingStudio;
class KaSectionDrawingStudio;
class KaTerrain3dStudio;
class KaTerrain3dLayoutStudio;
class KaStartPage;
class KaCoordPointMapTool;
class KaMeasureMapTool;
class KaFeatureSelectTool;
class KaFoundLocationMark;
class QSplitter;
class QListWidget;
class QTimer;
class QgsVertexMarker;
class QgsMapToolPan;
class QgsMapToolSelect;
class QgsGeometry;
class QgsFeature;
class QgsLayerTreeMapCanvasBridge;
class QgsMessageBar;
#endif

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  enum class OpenSurveyMode { PreferWorkspace, LayersOnly };
  bool openSurveyGpkg(const QString& gpkgPath);
  bool openSurveyGpkg(const QString& gpkgPath, OpenSurveyMode mode);
  int domainLayerCount() const;
  QString workCrsAuthId() const { return m_workCrs; }
  void runChecklistPublic() { runChecklist(); }
  int seedDemoFieldData();
  int lastChecklistErrorCount() const;
  // 부팅 때 미뤄 둔 배경지도 로딩을 지금 끝낸다(자동 QA·스모크가 결정적으로 돌게).
  void loadBootBasemaps();
  void setRestoreLastSurveyEnabled(bool enabled) { m_restoreLastSurveyEnabled = enabled; }
  bool addVectorFromPath(const QString& path);
  bool addRasterFromPath(const QString& path);
  bool tryAddDroppedUrls(const QList<QUrl>& urls);
  void showLayerTreeContextMenu(QgsLayerTreeView* treeView, const QPoint& pos);
  void updateLayerOpacityControl();
  // "2000" 과 "1:2000" 을 모두 분모 2000 으로 읽는다. 0 이면 숫자가 아니다.
  static double scaleDenominatorFromUi(const QString& raw);
  void editCurrentLayerStyle(QgsMapLayer* layer = nullptr);
  void editCurrentLayerAttributes(QgsMapLayer* layer = nullptr);
  void removeLayersFromTree(QgsLayerTreeView* tree);
  void undoMapAction();

private:
  enum class ReferenceMapKind { Soil, PaleoSoil, Geology, River };
  QProgressDialog* createDownloadProgress(const QString& title);
  void startFileDownload(const QString& title,
      std::function<PreparedReferenceMap(QgsFeedback*, const std::function<bool()>&)> prepare,
      std::function<void(const PreparedReferenceMap&)> apply);
  void startDemDownload();
  void watchUndoFeatureIds(QgsVectorLayer* layer);
  void populateMapContextMenu(QMenu* menu, const QPoint& pos);
  void showLayerAreaSummary(QgsVectorLayer* layer, bool showRatio);

private slots:
  void exportMapGeoTiff();
  void newSurvey();
  void openVectorLayer();
  void importSoilShapefile();
  void downloadSoilTerrain();
  void downloadGeologyMap();
  void downloadRiverMap();
  void startReferenceDownload(ReferenceMapKind kind, const QgsRectangle& extent,
                              const QString& targetPath, const QString& apiKey = {});
  void saveProject();
  void saveProjectAs();
  // 작업공간을 조사 파일에 쓴다. 20초 타이머로 자동 호출하던 것을 없앴으므로,
  // 이제 「저장」과 닫기 확인에서만 불린다. 테스트도 이 이름으로 직접 부른다.
  bool persistSurveyWork();
  void openProject();
  void startEditSurveyArea();
  void startEditFeaturePoly();
  void startEditFeatureLine();
  void startEditSectionLine();
  void startEditArtifact();
  void startEasyDraw();
  void mergeFeaturePolygons();
  void clipOverlappingLayers();
  void startSplitPolygonTool();
  void onWorkControlClicked(QListWidgetItem* item);
  void refreshWorkPanel();
  void saveEdits();
  void stopEdits();
  void startAttributeEditTool();
  void addUserLayer();
  void addControlPoint();
  void importControlCsv();
  void runChecklist();
  void exportPdf();
  void exportShpPackage();
  void crsDefineOnly();
  void crsReproject();
  void setWorkCrs5186();
  void setWorkCrs5187();
  void convertSelectedTo5179();
  void convertSelected5186To5179();
  void convertSelected5187To5179();
  void convertShpFileTo5179();
  void startSelectTool();
  void startMeasureTool();
  void toggleTerrainMap();
  void openTopographicDownload();
  void importTopographicFolder();
  void showTopographicFiles(const QString& folder);
  void toggleDemMap();
  void importDemElevationRaster();
  void runDemHillshade();
  void editDemElevationClasses();
  void startPaleoLandform();
  void startTrenchGrid();
  void placeTrenchGridAt(const QgsPointXY& origin);
  // Atomically replaces the previous grid and reports the excavation ratio
  // against the survey area when known. areaM2 <= 0 skips the ratio line.
  bool applyTrenchCells(const std::vector<TrenchGridGenerator::Cell>& cells, double areaM2,
                       double targetPct = 0.0, const QString& sourceCrs = {});
  void applyTrenchByRatio(double targetPct);
  // 속성 창의 「적용」: 자동 채움이면 구역 재배치, 아니면 기존 격자 중심을
  // 유지한 채 회전·간격만 바꿔 재배치. 격자가 없으면 원점 클릭으로 넘어간다.
  bool applyTrenchFromDialog();
  void beginTrenchOriginPick();
  void startTrenchGridMove();   // 전체 이동 모드
  void startTrenchGridEdit();   // 개별 편집 모드(그래픽식 선택·이동·삭제)
  void activateTrenchTool(bool single);
  void toggleMapGrid();
  // 격자 선 색을 바꾸고(빨강·파랑·검정·직접 고르기) 눌린 단추를 맞춘다.
  void setMapGridColor(const QColor& color);
  void syncMapGridColorButtons();
  void addBasemapVworld();
  void addBasemapVworldSat();
  void addBasemapVworldCadastral();
  void downloadCadastral();
  void configureCadastralAccount();
  void configureCadastralStyle();
  void addBasemapOsm();
  void addBasemapGoogle();
  void removeSelectedLayers();
  void clearDrawnFeaturesOfCurrentLayer();
  // 지금 화면 범위의 배경 타일을 MBTiles로 받아 두고 그 파일로 바꿔 쓴다.
  // 그 뒤로는 네트워크를 타지 않아 팬·줌이 디스크 속도로 돈다.
  void saveOfflineTilePack();
  void georefAssistant();
  void showSubToolsAlign();
  void showAbout();

  void configureVworldKey();
  void configureTopographicAccount();
  bool configureHeritageAccount();
  void updateTopographicDirectory(const QString& surveyPath);
  void rebuildLayouts();
  void onFileBrowserActivated(QListWidgetItem* item);
  void exportReportLayout();
  void browseDataFolder();
  void goFileBrowserRoot(const QString& path);
  static QString resolvedDesktopPath();
  void onMapContextMenu(const QPoint& pos);
  void onLayerTreeContextMenu(const QPoint& pos);
  void renameSelectedLayer(QgsMapLayer* layer = nullptr);
  void onLayerTreeDoubleClicked(const QModelIndex& index);
  void zoomMapToFullMax();
  void zoomSelectedLayerMax();
  void onCanvasScaleChanged(double scale);
  void applyMapScaleFromUi();
  void refreshMapCanvasNow();
  void showSubToolsDraw();
  void showSubToolsBuffer();
  // 조사구역이 속한 시/군의 국가유산 자료를 인트라넷에서 받아 참조 레이어로 올린다.
  // 묻는 것은 시/군 판정 확인 하나뿐이다.
  void fetchNearbyHeritage();
  void ensureHeritageBrowser();
  void openHeritageBrowserFor(const struct HeritageRegion& region, const QString& reason = {});
  HeritageImport::Result importHeritageDataset(HeritageDataset dataset, const QStringList& files);
  void saveHeritageAgreementReceipt(const QDateTime& when, const QString& terms);
  void showSubToolsBasemap();
  void showSubToolsSubmit();
  void hideSubTools();
  void startCoordPointTool();
  void runSiteBuffer500();
  void runSiteBuffer1000();
  void applySnapConfig();
  void openLayoutDesigner();
  void placeTerrain3dOnSheet();
  void openSectionDesigner();
  void openTerrain3dStudio();
  void openTerrain3dLayout();
  void applyTerrain3dSheetScale(int denominator);
  void refreshTerrain3dDrapeAndSheet();
  QString terrain3dSheetPngPath() const;
  void onViewTabCloseRequested(int index);
  void openRecentSurvey(const QString& path);
  void showHomePage();
  void showMapWorkspace();
  void rememberSurvey(const QString& path, const QString& name);
  void undoLastAction();
  void deleteSelectedFeatures();

private:
  void buildUi();
  void buildMenus();
  void setupWorkPanel();
  void updateNextActionStatus();
  void setupFileBrowser();
  void clearSubToolbar();
  // 지금 켜져 있는 도구에 파란 밑줄이 오게 체크 상태를 맞춘다.
  void updateSubToolbarChecks();
  // 큰 항공사진처럼 Qt 로 못 여는 원판은 GDAL 축소본으로 왼쪽 칸을 채운다.
  bool loadAlignPreviewFromRaster();
  // 레이어 체크를 한 번에 모두 끄고 켜는 단추. 라벨은 다음에 할 일을 보여 준다.
  // 레이어 순서를 글자(라벨)에도 그대로 먹인다.
  // 창 단축키(Delete·Ctrl+Z)를 열려 있는 조판 화면으로 넘긴다.
  bool routeEditKeyToActiveStudio(bool isDelete);
  void applyLabelStackOrder();
  void refreshAboveLabelsOverlay();
  void toggleAllLayersChecked();
  void refreshLayerCheckAllButton();
  bool addSectionGeoTiffFromPath(const QString& path, const QString& crsAuthId);
  bool tryAddDroppedPaths(const QStringList& paths);
  QStringList selectedBrowserFiles() const;
  void loadSurveyLayers(const QString& gpkgOrStub);
  // 내장(.gpkg) / 동반(.qgz) 두 복원 경로가 공유하는 마무리.
  void finishOpenedProject(const QString& gpkgPath, const QString& sourceLabel,
                           qint64 elapsedMs);
  void ensureDefaultBasemaps();
  // 주제도 아이콘의 눌림 상태를 범례에서 되읽는다. 레이어를 지우거나 체크를
  // 끄면 아이콘도 꺼져야 한다(범례가 진실).
  void syncThematicButtons();
  // 조사구역 안 DEM 표고로 오르막 방위를 낸다(트렌치 장축 = 등고선 직교).
  TrenchGridGenerator::SlopeAspect terrainAspectForArea(const QByteArray& areaWkb,
                                                       const QString& areaCrs);
  void applyStartupMap();
  void ensureStartupViewReady();
  void scheduleMapDisplayRefresh();
  void bindMapDisplayScreen();
  void setWorkCrs(const QString& authId);
  void searchLocation(const QString& query, bool parcel = false);
  void applySurfaceSurveyFieldMap(const QString& sido, const QString& city, const QString& dong);
  void onAdminBoundaryFetched(const AdminBoundaryParse& parsed);
  void onAdminBoundaryFailed(const QString& message);
  void onLocationResults(const QVector<LocationHit>& hits);
  void onLocationFailed(const QString& message);
  void zoomToLocation(const LocationHit& hit);
  // 검색으로 찾은 자리에 표식을 남긴다. 화면만 옮기면 어디를 찾았는지 모른다.
  void markFoundLocation(const QgsPointXY& mapPt, const QString& title);
  void clearFoundLocationMark();
  QJsonObject buildProjectState() const;
  QString rulesPath() const;
  QString vworldApiKeyOrPrompt();
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layerByKey(const QString& layerKey) const;
  QgsVectorLayer* ensureDomainLayerForEdit(const QString& layerKey, const QString& titleKo);
  void onLayerTreeRowsMoved();
  void moveSelectedLayer(int dir);
  void startAlignSession(QgsMapLayer* layer);
  void stopAlignSession();
  void ensureAlignSplit();
  void showAlignSplit();
  void hideAlignSplit();
  void refreshAlignUi();
  void updateAlignOverlay();
  void trackAlignPointer(const QPoint& globalPos);
  void deleteSelectedAlignPoint();
  void applyAlignMove();
  void beginEdit(QgsVectorLayer* layer);
  void onGeometryCaptured(const QgsGeometry& geom);
  void stopCaptureTool();
  void ensureAttributeTool();
  void editFeatureAttributes(QgsVectorLayer* layer, const QgsFeature& feature);
  void editAttributesAtCanvasPos(const QPoint& canvasPos);
  static QString attributeFieldLabelKo(const QString& fieldName);
#endif
  bool commitSurveyEdits(int* committedCount = nullptr);
  bool confirmSaveBeforeOpeningSurvey();
  bool validateStandaloneProjectForOpen(const QString& path);
  // 저장하지 않은 작업이 있는가. 프로젝트 dirty 플래그 + 커밋 안 된 편집 버퍼.
  bool surveyHasUnsavedChanges() const;
  // 저장 직후·열기 직후 호출해 "깨끗한 상태"로 되돌린다.
  void markSurveySaved();
  // 새 조사·다른 이름으로 저장이 처음 여는 폴더. 바탕화면이 OneDrive 로 리디렉션된
  // PC에서 기본값이 그리로 향하지 않도록, 마지막에 쓴 조사 폴더를 먼저 쓴다.
  QString preferredSurveyDir() const;
  void rememberSurveyDir(const QString& path);
  void refreshWindowTitle();
  // 레이어 점호. 직전과 달라졌으면 무엇이 사라졌는지 세션 로그에 적고 되살린다.
  void auditLayerHealth();
  void logLayerCensus(const QString& tag);
  // 화면에 실제로 무엇이 그려졌는지. 확대했을 때 위성이 비는 현상을 잡기 위한 계측 —
  // 캔버스가 그리기로 잡고 있는 레이어 목록과 축척을, 목록이 바뀔 때만 기록한다.
  void logCanvasPaintState();
  void restoreLastSurvey();

  // Non-blocking feedback on the canvas. Reserve QMessageBox for questions and
  // for failures the user must acknowledge before anything else happens.
  enum class Notice { Info, Success, Warning, Critical };
  void notify(Notice level, const QString& title, const QString& text,
              const QString& details = QString());

  void refreshLayerEmptyState();
  QLabel* m_layerEmpty = nullptr;
  QToolButton* m_layerCheckAllBtn = nullptr;
  QLabel* m_help = nullptr;
  QLabel* m_checkView = nullptr;
  KaStatusBar* m_status = nullptr;
  QLabel* m_workHint = nullptr;
  QListWidget* m_workList = nullptr;
  QSplitter* m_leftSplit = nullptr;
  QFrame* m_layersCard = nullptr;
  QFrame* m_filesCard = nullptr;
  class KaFileBrowserPanel* m_filesPanel = nullptr;
  QListWidget* m_fileBrowser = nullptr;
  QString m_browserPath;
  ChecklistEngine* m_checklist = nullptr;
  LocationSearch* m_locator = nullptr;
  AdminBoundaryService* m_adminBoundary = nullptr;
  QString m_surveyPath;
  class HeritageRegionResolver* m_heritageResolver = nullptr;
  QPointer<class KaHeritageBrowser> m_heritageBrowser;
  QString m_workCrs = QStringLiteral("EPSG:5187");
#if KA_HGIS_HAS_QGIS
  void healTileLayer(QgsRasterLayer* layer);
  void notifyBasemapFailure(bool timedOut);
  bool m_basemapNoticePending = false;
#endif
  // 타일 자동 복구 상태: 레이어별 재시도 횟수(화면 이동 시 초기화)와 예약 중 표시.
  QHash<QString, int> m_tileHealCount;
  QSet<QString> m_tileHealPending;
  int m_stubSurveyArea = 0;
  int m_stubFeatures = 0;
  int m_stubGcp = 0;
  bool m_stubHasMeta = false;
  bool m_packageCreated = false;
  mutable int m_lastChecklistErrors = -1;
#if KA_HGIS_HAS_QGIS
  QgsMapCanvas* m_canvas = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
  KaCaptureMapTool* m_captureTool = nullptr;
  KaCoordPointMapTool* m_coordPointTool = nullptr;
  KaMeasureMapTool* m_measureTool = nullptr;
  QAction* m_actMeasure = nullptr;
  QAction* m_actSelect = nullptr;
  QAction* m_actGeology = nullptr;
  QAction* m_actRiver = nullptr;
  QToolButton* m_btnDraw = nullptr;
  QToolButton* m_btnSoil = nullptr;
  QToolButton* m_btnTerrain = nullptr;
  QToolButton* m_btnDem = nullptr;
  QToolButton* m_btnPaleo = nullptr;
  QgsMapToolEmitPoint* m_trenchOriginTool = nullptr;
  class KaTrenchMoveTool* m_trenchMoveTool = nullptr;
  class KaTrenchDialog* m_trenchDlg = nullptr;
  class KaCanvasGridOverlay* m_mapGrid = nullptr;
  // 라벨 위에 위 레이어를 한 번 더 그리는 덧그림(2차 패스).
  KaAboveLabelsOverlay* m_aboveLabels = nullptr;
  int m_aboveLabelsCount = -1;
  QCheckBox* m_mapGridCheck = nullptr;
  QDoubleSpinBox* m_mapGridStep = nullptr;
  QDoubleSpinBox* m_mapGridRot = nullptr;
  QDoubleSpinBox* m_mapGridWidth = nullptr;
  QComboBox* m_mapGridDash = nullptr;
  QColor m_mapGridColor = QColor(51, 65, 85, 210);
  QVector<QToolButton*> m_mapGridColorBtns;
  KaAttributeMapTool* m_attributeTool = nullptr;
  KaLayerOpacityRail* m_layerOpacityRail = nullptr;
  KaAlignMapTool* m_alignTool = nullptr;
  KaAlignPickTool* m_alignPickTool = nullptr;
  KaImageView* m_alignImage = nullptr;
  QgsMapCanvas* m_alignLeftCanvas = nullptr;
  QWidget* m_alignLeftPane = nullptr;
  QLabel* m_alignLeftLabel = nullptr;
  QListWidget* m_alignPointList = nullptr;
  KaAlignLinkOverlay* m_alignOverlay = nullptr;
  QVector<QgsVertexMarker*> m_alignLeftMarks;
  QSplitter* m_mapSplitter = nullptr;
  bool m_alignApplied = false;
  bool m_alignCursorValid = false;
  bool m_alignLiveScreenValid = false;
  double m_alignCursorX = 0;
  double m_alignCursorY = 0;
  QPoint m_alignLiveScreen;
  QTimer* m_alignCursorTimer = nullptr;
  QgsMapToolPan* m_panTool = nullptr;
  QgsMapToolSelect* m_selectTool = nullptr;
  KaFeatureSelectTool* m_featureSelectTool = nullptr;
  KaFoundLocationMark* m_locationMark = nullptr;
  QString m_locationMarkTitle;
  QgsVectorLayer* m_editLayer = nullptr;
  bool m_isSplittingPolygon = false;
  QgsLayerTreeMapCanvasBridge* m_bridge = nullptr;
  QgsMessageBar* m_messageBar = nullptr;
  QLineEdit* m_scaleEdit = nullptr;
  QComboBox* m_scaleCombo = nullptr;
  bool m_scaleUiGuard = false;
  bool m_extentClampGuard = false;
  QSplitter* m_mainSplit = nullptr;
  bool m_locationSearchBusy = false;
  bool m_startupViewApplied = false;
  bool m_workspaceRestoreFailed = false;  // .qgz를 못 읽어 외부 레이어가 빠진 채 열렸다
  // 복원되지 않은 작업공간은 덮어쓰지 않는다. 수동 저장도 새 사본으로 안내한다.
  bool m_workspaceRestoreSuppressesAutosave = false;
  bool m_basemapBootPending = false;
  // 조사 열기가 끝나기를 기다리며 다시 시도한 횟수. 0.3초 × 40 = 12초까지.
  int m_basemapBootRetries = 0;
  static constexpr int kBasemapBootRetryMax = 40;
  bool m_restoreLastSurveyEnabled = false;
  bool m_isLoadingBasemaps = false;
  bool m_isOpeningSurvey = false;
  bool m_closingWindow = false;
  bool m_canZoomPrevious = false;
  bool m_canZoomNext = false;
  QPointer<KaReferenceDownloadJob> m_referenceDownload;
  QPointer<KaTopographicBrowser> m_topographicBrowser;
  QPointer<KaTopographicImportDialog> m_topographicImport;
  quint64 m_surveyGeneration = 0;
  bool m_surveySessionReady = false;
  bool m_mapScreenBound = false;
  QTimer* m_displayRefresh = nullptr;
  // 20초 자동 저장은 없앴다. 저장은 사용자가 「저장」을 누를 때만 일어나고,
  // 저장 안 된 작업은 창 제목 뒤 * 와 닫기 확인창으로 알린다.
  // 레이어 사라짐 추적. 직전 점호 결과와 다를 때만 로그를 남긴다.
  QTimer* m_layerWatchTimer = nullptr;
  QString m_lastLayerCensus;
  QString m_lastCanvasPaintState;
  QStringList m_lastLayerKeys;
  QToolBar* m_subToolbar = nullptr;
  QString m_subToolsMode;
  bool m_snapEnabled = true;
  KaDrawingStudio* m_drawingStudio = nullptr;
  KaSectionDrawingStudio* m_sectionStudio = nullptr;
  KaTerrain3dStudio* m_terrain3dStudio = nullptr;
  KaTerrain3dLayoutStudio* m_terrain3dLayoutStudio = nullptr;
  QAction* m_actMapGeoTiff = nullptr;
  QTabWidget* m_viewTabs = nullptr;
  KaStartPage* m_startPage = nullptr;
  QWidget* m_mapPage = nullptr;
  KaBeginnerRibbon* m_ribbon = nullptr;
  struct KaUndoAction {
    enum Type { FeatureAdded, FeatureDeleted, FeatureChanged, AttributesChanged, LayerAdded, LayersRemoved };
    Type type = FeatureAdded;
    QString layerId;
    qint64 featureId = -1;
    QgsFeature featureData;
    QVector<QPair<QString, QgsFeature>> deletedFeatures;
    std::shared_ptr<KaRemovedLayers> removedLayers;
    QString description;
  };
  QVector<KaUndoAction> m_undoActions;
  QSet<QString> m_undoObservedLayers;
  QPointer<QProgressDialog> m_boundaryProgress;
  QPointer<QProgressDialog> m_searchProgress;
#endif
};
