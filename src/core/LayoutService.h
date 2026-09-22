#pragma once
#include <QImage>
#include <QList>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <qgsfeatureid.h>
#include <qgsrectangle.h>
#include <qgspointxy.h>
class QgsProject;
class QgsLayout;
class QgsLayoutItemMap;
class QgsLayoutItemLegend;

class LayoutService {
public:
  // Reflow the existing legend into readable columns inside its chosen width.
  static void flowSheetLegend(QgsLayoutItemLegend* legend);
  // Finish pending legend filtering and sizing before the first PDF paint.
  static void settleSheetLegendsForExport(QgsLayout* layout);
  enum class Paper { A4, A3 };
  enum class Orientation { Portrait, Landscape };

  enum class DrawingKind {
    SurveyAreaMap,
    SiteLocation,
    FeaturePlan,
    FeatureDetail,
    Section
  };

  enum class ExtentMode {
    Auto,
    SurveyArea,
    Canvas,
    SelectedFeature,
    WideContext
  };

  struct DrawingRecipe {
    DrawingKind kind = DrawingKind::SurveyAreaMap;
    QString layoutId;
    QString titleKo;
    QString purposeKo;
    double defaultScale = 5000.0;
    double gridIntervalM = 100.0;
    bool includeBasemap = true;
    QStringList layerKeys;
    QString emptyHintKo;
    QList<double> scaleChoices;
  };

  struct DrawingOptions {
    QString titleKo;
    QString surveyName;
    QString siteName;
    Paper paper = Paper::A4;
    Orientation orientation = Orientation::Landscape;
    double scaleOverride = 0.0;
    ExtentMode extentMode = ExtentMode::Auto;
    QgsRectangle canvasExtent;
    QgsFeatureId featureId = FID_NULL;
  };

  struct DrawingBuildResult {
    QString layoutId;
    double appliedScale = 0.0;
    QgsRectangle appliedExtent;
    bool hasMapContent = false;
    QString warningKo;
  };

  static int ensureDefaultLayouts(QgsProject* project);
  static int rebuildDefaultLayouts(QgsProject* project);
  /// 사용자가 직접 도면만들기(KaDrawingStudio) 또는 단면도에서 배치한 조판이 실질적인 내용을 갖는지.
  /// 유효한 축척(scale > 0), 유한한 지도 범위, 비-참조 유효 레이어(피처 수 > 0인 벡터 또는 유효한 래스터)를 점검한다.
  /// layout_blank 등 더미 메모리 레이어 및 참조 지도는 명시적으로 배제한다.
  /// ka_hgis/auto_template 이고 ka_hgis/user_composed 가 아니면 조판으로 보지 않는다.
  static bool isComposedStudioSheet(QgsProject* project,
                                   const QString& layoutName = QStringLiteral("user_sheet"));
  /// 도면만들기에서 지도를 올린 용지. 자동 5종 템플릿과 구분한다.
  static void markStudioSheetComposed(QgsLayout* layout);
  static QString exportLayoutPdf(QgsProject* project, const QString& layoutName,
                                 const QString& pdfPath, QString* errorOut = nullptr);
  static QStringList defaultLayoutNames();
  static QString koreanTitle(const QString& layoutName);

  static QString createReportLayout(QgsProject* project, const QString& titleKo,
                                    Paper paper, Orientation orientation,
                                    QString* errorOut = nullptr);

  static QString createBlankSheet(QgsProject* project, double widthMm, double heightMm,
                                  const QString& name = QStringLiteral("user_sheet"),
                                  QString* errorOut = nullptr);
  // 저장된 조판에 용지가 없거나 크기가 0이면 페이지를 다시 만든다.
  static bool ensureLayoutPage(QgsLayout* layout, double widthMm, double heightMm);

  // 도면의 래스터(위성·지적·지질 배경)를 조각내지 않고 한 번에 그리게 한다.
  // QGIS 기본값은 래스터를 여러 조각으로 나눠 그리는데, 타일 배경에서는 조각 하나가
  // 빈 채로 돌아오면 그 사각형만 통째로 비어 위성지도가 반만 나온 것처럼 보인다.
  // 조판을 새로 만들 때와 이미 있는 조판을 열 때 모두 걸어 준다.
  static void applySingleRasterPassRendering(QgsLayout* layout);

  // 용지 mm × 축척 → 지도 범위(미터 CRS). 입력한 1:N이 그대로 유지되게 한다.
  static QgsRectangle extentForPaperScale(const QgsRectangle& currentExtent,
                                          double mapWidthMm, double scaleDenominator);

  // 지도 화면 범위·축척을 조판 맵 칸에 그대로 옮긴다. zoomToExtent + setScale.
  // setExtent는 쓰지 않는다(칸 mm가 바뀜). niceScaleDenominator로 분모를 올리지 않는다.
  static bool applyCanvasViewToLayoutMap(QgsLayoutItemMap* map, const QgsRectangle& canvasExtent,
                                         double canvasScale);

  // 커서 아래 지점을 고정한 채 확대·축소한 지도 범위.
  // fx, fy는 지도 칸 안에서 커서의 상대 위치(0~1, fy는 위에서 아래로).
  // zoomFactor > 1 이면 확대(범위가 좁아짐).
  static QgsRectangle zoomExtentAtAnchor(const QgsRectangle& extent, double fx, double fy,
                                         double zoomFactor);

  // 조판 맵 칸(item mm) ↔ 지도 XY. 축척이 바뀌면 같은 땅점은 다른 용지 자리로 간다.
  static QPointF layoutMapItemFromXy(const QgsRectangle& extent, const QRectF& mapRect,
                                     double mapX, double mapY);
  static QgsPointXY layoutMapXyFromItem(const QgsRectangle& extent, const QRectF& mapRect,
                                        const QPointF& itemPt);
  static bool layoutMapItemContains(const QPointF& itemPt, const QRectF& mapRect,
                                    double marginMm = 0.5);

  // 지도 칸 아래 고정 띠: 왼쪽 축척자, 그 아래 축척 1:N, 오른쪽 CRS, 맨 오른쪽 방위.
  // 위성/지적 픽셀 위에 올리지 않는다. 도면만들기마다 이 기하를 다시 앉힌다.
  struct SheetChromeRects {
    QRectF map;
    QRectF scaleBar;
    QRectF scaleLabel;
    QRectF crs;
    QRectF north;
  };
  static SheetChromeRects standardSheetChrome(const QRectF& page, const QRectF& requestedMap);
  // Full-sheet maps keep equal top/left/right paper margins. Bottom stays
  // taller for the scale/CRS/north strip. Custom narrow frames are unchanged.
  static QRectF equalFullSheetMapRect(const QRectF& page, const QRectF& map);

  // 1:20 / 1:40 / 1:50 / 1:100 … 분모가 10으로 끝나는 도면 축척.
  static int niceScaleDenominator(double rawScale);
  // 축척자 한 칸(m). 1-2-4-5 계열.
  static double niceScaleBarSegmentMeters(double mapWidthMm, double scaleDenominator,
                                          int segments = 4);
  static double scaleBarWidthMm(double segmentMeters, int segments, double scaleDenominator);

  /// Ink #111827 / paper #FFFFFF / 0.30 mm outline so Double Box reads on satellite and white paper.
  /// Call after setStyle. Does not change segment mode or linked map.
  static void applySheetScaleBarInk(class QgsLayoutItemScaleBar* sb);

  // 전문 측량도면 도곽: 지브라(흑백 교차) 프레임 + 정수 TM 좌표 주기(상하 수평,
  // 좌우 세로쓰기) + 내부 십자 눈금. 기존 격자는 지우고 하나로 다시 만든다.
  // crosses=false면 내부 눈금 없이 도곽·주기만 그린다(위치도처럼 배경이 촘촘할 때).
  static void applySurveyFrameGrid(QgsLayoutItemMap* map, double intervalM, bool crosses,
                                   bool showCoords);
  // 도곽 격자 간격(m): 종이에서 한 칸이 25~70mm가 되는 1-2-5 계열 값.
  static double niceGridIntervalMeters(double scaleDenominator, double mapWidthMm);
  // 좌표계 한글 원점명(중부원점(GRS80) 등). 표제란·도면 라벨 공용.
  static QString koreanCrsName(const QString& authId);

  static DrawingKind kindFromLayoutId(const QString& layoutId);
  static QString layoutId(DrawingKind kind);
  static DrawingRecipe recipe(DrawingKind kind);
  static QList<DrawingRecipe> allRecipes();

  static DrawingBuildResult buildDrawing(QgsProject* project, DrawingKind kind,
                                         const DrawingOptions& options,
                                         QString* errorOut = nullptr);

  static QImage renderPreview(QgsProject* project, const QString& layoutName,
                              const QSize& imageSize, QString* errorOut = nullptr);

  static int exportDrawingPdfs(QgsProject* project, const QString& outDir,
                               QString* errorOut = nullptr);

  // 도면 범례는 분포지형 색칸. 흙토람 그림·위성·지적 레이어 이름은 범례가 아니다.
  static bool sheetLegendOmitsLayerName(const QString& name);
  static void tuneSheetLegend(class QgsLayoutItemLegend* legend);
  static QString sheetLegendLabelDump(class QgsLayoutItemLegend* legend);
};
