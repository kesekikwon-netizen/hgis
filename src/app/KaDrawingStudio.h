#pragma once
#include "core/HeritageLayoutNumbers.h"
#include <QMainWindow>
#include <QPoint>
#include <QPointer>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>
#include <qgscoordinatereferencesystem.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>

class QAction;
class QCheckBox;
class QDoubleSpinBox;
class QDoubleSpinBox;
class QEvent;
class QFrame;
class QGraphicsRectItem;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QToolButton;
class QShowEvent;
class QSpinBox;
class QTimer;
class QSplitter;
class QWidget;
class KaLayerOpacityRail;
class QgsProject;
class QgsMapCanvas;
class QgsLayoutView;
class QgsLayoutViewToolSelect;
class QgsLayoutViewToolPan;
class QgsPrintLayout;
class QgsLayoutItem;
class QgsLayoutItemMap;
class QgsLayoutItemScaleBar;
class QgsLayerTreeView;
class QgsLayerTreeModel;
class QgsMapLayer;
class QgsVectorLayer;
class QgsCoordinateReferenceSystem;
class QgsRectangle;
class QgsPointXY;
class QgsGeometry;
class KaLayoutMapDrawTool;
class KaLayoutMapAdjustTool;
class KaLayoutCoordPointTool;

class KaDrawingStudio : public QMainWindow {
  Q_OBJECT
public:
  enum class PlaceKind { MapFrame, Legend, North, ScaleBar, ScaleLabel, CrsLabel };

  explicit KaDrawingStudio(QgsProject* project, QgsMapCanvas* mapCanvas,
                           double paperWidthMm, double paperHeightMm,
                           QWidget* parent = nullptr);
  static bool promptPaper(QWidget* parent, double* widthMm, double* heightMm);
  static constexpr double kA4PortraitWidthMm = 210.0;
  static constexpr double kA4PortraitHeightMm = 297.0;
  // A4 가로(297×210)에서 좌·우·위 각 10mm를 더 준 현장 조판.
  static constexpr double kA4LandscapeWidthMm = 297.0;
  static constexpr double kA4LandscapeHeightMm = 210.0;
  static constexpr double kFieldPaperExtraMm = 10.0;
  static constexpr double kFieldPaperWidthMm =
      kA4LandscapeWidthMm + kFieldPaperExtraMm * 2.0;
  static constexpr double kFieldPaperHeightMm =
      kA4LandscapeHeightMm + kFieldPaperExtraMm;
  static constexpr double kFieldEdgeMm = 10.0;
  static constexpr double kFieldBottomChromeMm = 38.0;
  static bool isLegacyA4LandscapeMm(double widthMm, double heightMm);
  void applyFieldPageGrow();
  void applyFieldEdge();
  void resetPaper(double widthMm, double heightMm, bool preserveExisting = true);
  void refreshMapFromProject();
  void updateLayerOpacityControl();
  void repaintMapLayers();
  double drawingScale() const;
  // 입체지형 3D 그림을 맵 칸에 올리고, 축척·방위는 DEM 범위에 맞춘다.
  void placeTerrain3dPicture(const QString& pngPath, const QgsRectangle& groundExtent,
                             const QgsCoordinateReferenceSystem& crs);
  void importMapCoordCallouts(const QVector<QgsPointXY>& pts, const QVector<QString>& letters,
                              const QVector<QString>& texts, const QgsGeometry& frame);
  void applyImportedCoordCallouts();
  bool isMapAdjusting() const { return m_adjustingMap; }
  bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
  void setDrawingScale(double denominator);
  void beginActivateMap();
  void endActivateMap();
  void centerSurveyInMap();
  void centerOnMapCanvas();
  void openPaperSettingsDialog();
  void beginPlaceCoordPoint();
  void undoLastCoordCallout();
  void endPlaceCoordPoint();
  bool isPlacingCoordPoint() const;
  // 창 단축키(Delete·Ctrl+Z)가 이 화면 대신 가로채므로, 바깥에서 넘겨받는다.
  void handleDeleteKey();
  void handleUndoKey();
  void handleRedoKey();
  void deleteSelectedItems();
  void removeSelectedLayers();
  void undoLastChange();
  void savePdf();

signals:
  void drawingScaleChanged(double denominator);

private slots:
  void beginDrawMapFrame();
  void useSelectTool();
  void usePanTool();
  void zoomFull();
  void zoomPaperVisible();
  void onRectDrawn(const QRectF& layoutRect);
  void syncMapFromLayers();
  void flushLayerSync();
  void toggleAllLayersChecked();
  void refreshLayerCheckAllButton();
  void beginPlaceLegend();
  void beginPlaceNorth(const QString& svgRel);
  void beginPlaceScaleBar(const QString& style);
  void beginPlaceScaleLabel();
  void beginPlaceCrsLabel();
  void applyOnScreenScale();
  void applyLegendSettings();
  void onLayoutSelectionChanged(QgsLayoutItem* item);
  void syncScaleDecorations();
  void flushHeavyScaleSync();
  void focusGridSettings();
  void placeCoordCallout(const QPointF& layoutPt);
  void relayoutCoordCallouts();
  void panLayoutMapTo(const QgsPointXY& center);
  void updateCoordFrame();

private:
  void buildUi();
  void ensureBlankLayout();
  void attachLayoutToView();
  QgsPrintLayout* layout() const;
  QgsLayoutItemMap* mapItem() const;
  void applyLayersToMap(QgsLayoutItemMap* map, bool includeLiveBasemap, bool refitExtent);
  // 라벨 위에 위 레이어를 한 번 더 그리는 덧지도를 본 지도에 맞춘다.
  void syncAboveLabelsMap(QgsLayoutItemMap* base);
  bool syncHeritageNumbers(bool force = false);
  void applyHeritageNumberChrome();
  QgsVectorLayer* blankMapLayer();
  static void ensureLayoutGuiRegistered(QgsMapCanvas* mapCanvas);
  void startPlace(PlaceKind kind);
  void createOrResizeMap(const QRectF& layoutRect);
  void placeLegend(const QRectF& layoutRect);
  void placeNorth(const QRectF& layoutRect, bool selectAfter = true);
  void placeScaleBar(const QRectF& layoutRect, bool selectAfter = true);
  void placeScaleLabel(const QRectF& layoutRect, bool selectAfter = true);
  void placeCrsLabel(const QRectF& layoutRect);
  void refreshScaleWidgets(bool readFromMap = false);
  void applyScaleBarNow();
  void applyNorthNow();
  void applyCrsLabelNow();
  void relinkDecorations();
  void ensureStandardDecorations();
  void applyStandardChromePositions();
  void snapMapScaleToNice();
  void applyNiceScaleBar(QgsLayoutItemScaleBar* sb);
  QRectF defaultItemRect(const char* id) const;
  QgsRectangle surveyExtentOnMap(QgsLayoutItemMap* map) const;
  void finishPlace();
  void selectPlacedItem();
  void updateInspector(QgsLayoutItem* item);
  void resizeSelectedDecoration(double percent);
  void setDrawerCardActive(QFrame* card);
  void syncScaleChips();
  void applyCrsGrid(QgsLayoutItemMap* map);
  void clearGridCoordinateLabels();
  void autoPlaceDefaultSheet();
  QRectF defaultMapRect() const;
  void updatePageOutline();
  void panLayoutView(const QPoint& from, const QPoint& to);
  void recenterPaper();
  void setFastLayoutPreview(bool on);
  void showEvent(QShowEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  static int displayScale(double raw);

  QPointer<QgsProject> m_project;
  QPointer<QgsMapCanvas> m_mapCanvas;
  double m_paperW = kA4PortraitWidthMm;
  double m_paperH = kA4PortraitHeightMm;
  QgsLayoutView* m_view = nullptr;
  QgsLayoutViewToolSelect* m_toolSelect = nullptr;
  QgsLayoutViewToolPan* m_toolPan = nullptr;
  KaLayoutMapAdjustTool* m_toolMoveContent = nullptr;
  KaLayoutMapDrawTool* m_toolDrawMap = nullptr;
  KaLayoutCoordPointTool* m_toolCoordPoint = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
  QgsLayerTreeModel* m_layerModel = nullptr;
  QToolButton* m_layerCheckAllBtn = nullptr;
  class KaFileBrowserPanel* m_filesPanel = nullptr;
  // 일부러 지운 범례는 글자 설정을 만져도 되살아나지 않는다.
  bool m_legendRemoved = false;
  KaLayerOpacityRail* m_opacityRail = nullptr;
  // 투명도·밝기 막대가 지금 어느 레이어를 보고 있는지. 막대를 만질 때
  // 트리의 현재 항목이 바뀌어 있을 수 있어, 켤 때 잡아 둔 레이어를 쓴다.
  QPointer<QgsMapLayer> m_railLayer;
  QSplitter* m_studioSplit = nullptr;
  // 도곽 +/테두리 자는 격자 설정에서 켠다. 간격 0 = 축척에 맞춰 자동(1-2-5).
  bool m_gridEnabled = false;
  bool m_gridShowNums = false;
  double m_gridIntervalM = 0.0;
  QLabel* m_status = nullptr;
  QFrame* m_adjustBar = nullptr;
  QFrame* m_scaleBar = nullptr;
  QAction* m_actEndAdjust = nullptr;
  QLineEdit* m_legendTitle = nullptr;
  QSpinBox* m_legendFont = nullptr;
  QCheckBox* m_legendBold = nullptr;
  QCheckBox* m_legendItalic = nullptr;
  QDoubleSpinBox* m_northSize = nullptr;
  QDoubleSpinBox* m_decorationSize = nullptr;
  QSpinBox* m_scaleSpin = nullptr;
  QWidget* m_inspector = nullptr;
  QLabel* m_inspectorCap = nullptr;
  QWidget* m_legendProps = nullptr;
  QWidget* m_scaleProps = nullptr;
  QFrame* m_cardLegend = nullptr;
  QFrame* m_cardNorth = nullptr;
  QFrame* m_cardScaleBar = nullptr;
  QFrame* m_cardScale = nullptr;
  PlaceKind m_placeKind = PlaceKind::MapFrame;
  QString m_pendingNorthSvg;
  QString m_pendingScaleBarStyle;
  QStringList m_placeUndo;
  bool m_adjustingMap = false;
  bool m_holdTerrainExtent = false;
  QgsRectangle m_terrainGroundExtent;
  QgsCoordinateReferenceSystem m_terrainCrs;
  bool m_paperFitPending = true;
  bool m_keepPaperCentered = true;
  bool m_mmbPanning = false;
  QPoint m_mmbLast;
  QSize m_lastFitViewport;
  bool m_syncingMapFromLayers = false;
  bool m_syncingHeritageNumbers = false;
  bool m_numberChromePending = false;
  QByteArray m_aboveMapDigest;
  HeritageLayoutNumbers m_heritageNumbers;
  QTimer* m_layerSyncTimer = nullptr;
  bool m_layerSyncPending = false;
  QTimer* m_scaleSyncTimer = nullptr;
  double m_savedLayoutDpi = 0.0;
  QVector<QPointF> m_coordMapPts;
  QVector<QPointF> m_coordFrameMap;
  QVector<QgsPointXY> m_importCoordPts;
  QVector<QString> m_importCoordLetters;
  QVector<QString> m_importCoordTexts;
  QGraphicsRectItem* m_pageOutline = nullptr;
  QPointer<QgsVectorLayer> m_blankMapLayer;
};
