#include "KaSessionLog.h"
#include "KaLogExcept.h"
#include "LayoutService.h"
#include "HeritageLayoutNumbers.h"
#include "DemColorRampLegend.h"
#include "GeologyMapService.h"
#include "LayerFeatures.h"
#include "LayerOps.h"
#include "LayerLabelControls.h"
#include "PdfExportSettings.h"
#include "StandardScales.h"

#include <QColor>
#include <QGraphicsItem>
#include <QSet>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QStandardPaths>
#include <QStringList>
#include <QStyleOptionGraphicsItem>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <exception>
#include <initializer_list>

#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgslayout.h>
#include <qgslayoutexporter.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitemmapgrid.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemscalebar.h>
#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgstextformat.h>
#include <qgslayoutmanager.h>
#include <qgslayoutmeasurement.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutsize.h>
#include <qgslayoututils.h>
#include <qgslegendrenderer.h>
#include <qgslegendsettings.h>
#include <qgslegendstyle.h>
#include <qgsrendercontext.h>
#include <qgis.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgsmaplayer.h>
#include <qgsmaplayerlegend.h>
#include <qgsmasterlayoutinterface.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>

QStringList LayoutService::defaultLayoutNames() {
  return {
    QStringLiteral("survey_area_map"),
    QStringLiteral("site_location"),
    QStringLiteral("feature_plan"),
    QStringLiteral("feature_detail"),
    QStringLiteral("section")
  };
}

QString LayoutService::layoutId(DrawingKind kind) {
  switch (kind) {
    case DrawingKind::SurveyAreaMap: return QStringLiteral("survey_area_map");
    case DrawingKind::SiteLocation: return QStringLiteral("site_location");
    case DrawingKind::FeaturePlan: return QStringLiteral("feature_plan");
    case DrawingKind::FeatureDetail: return QStringLiteral("feature_detail");
    case DrawingKind::Section: return QStringLiteral("section");
  }
  return QStringLiteral("survey_area_map");
}

LayoutService::DrawingKind LayoutService::kindFromLayoutId(const QString& layoutId) {
  if (layoutId == QLatin1String("site_location")) return DrawingKind::SiteLocation;
  if (layoutId == QLatin1String("feature_plan")) return DrawingKind::FeaturePlan;
  if (layoutId == QLatin1String("feature_detail")) return DrawingKind::FeatureDetail;
  if (layoutId == QLatin1String("section")) return DrawingKind::Section;
  return DrawingKind::SurveyAreaMap;
}

QString LayoutService::koreanTitle(const QString& name) {
  if (name == QLatin1String("survey_area_map")) return QStringLiteral("조사구역도");
  if (name == QLatin1String("site_location")) return QStringLiteral("유적위치도");
  if (name == QLatin1String("feature_plan")) return QStringLiteral("유구배치도");
  if (name == QLatin1String("feature_detail")) return QStringLiteral("개별유구실측도");
  if (name == QLatin1String("section")) return QStringLiteral("층위도");
  return name;
}

LayoutService::DrawingRecipe LayoutService::recipe(DrawingKind kind) {
  DrawingRecipe r;
  r.kind = kind;
  r.layoutId = layoutId(kind);
  r.titleKo = koreanTitle(r.layoutId);
  switch (kind) {
    case DrawingKind::SurveyAreaMap:
      r.purposeKo = QStringLiteral("발굴·조사 범위를 1:5,000 기준으로 보여 줍니다.");
      r.defaultScale = 5000.0;
      r.gridIntervalM = 100.0;
      r.includeBasemap = true;
      r.layerKeys = {QStringLiteral("survey_area"), QStringLiteral("trial_trench"),
                     QStringLiteral("control_points")};
      r.emptyHintKo = QStringLiteral("먼저 「그리기 → 구역」으로 조사구역을 그리세요.");
      r.scaleChoices = {2500.0, 5000.0, 10000.0};
      break;
    case DrawingKind::SiteLocation:
      r.purposeKo = QStringLiteral("주변 지형에서 유적 위치를 넓게 보여 줍니다.");
      r.defaultScale = 25000.0;
      r.gridIntervalM = 1000.0;
      r.includeBasemap = true;
      r.layerKeys = {QStringLiteral("survey_area")};
      r.emptyHintKo = QStringLiteral("먼저 조사구역을 그리면 위치도에 표시됩니다.");
      r.scaleChoices = {10000.0, 25000.0, 50000.0};
      break;
    case DrawingKind::FeaturePlan:
      r.purposeKo = QStringLiteral("조사구역 안의 유구 배치와 종류·시대를 보여 줍니다.");
      r.defaultScale = 1000.0;
      r.gridIntervalM = 20.0;
      r.includeBasemap = false;
      r.layerKeys = {QStringLiteral("survey_area"), QStringLiteral("trial_trench"),
                     QStringLiteral("feature_poly"), QStringLiteral("feature_line"),
                     QStringLiteral("control_points")};
      r.emptyHintKo = QStringLiteral("유구 면·선을 그린 뒤 종류·시대를 넣으면 범례가 채워집니다.");
      r.scaleChoices = {500.0, 1000.0, 2000.0};
      break;
    case DrawingKind::FeatureDetail:
      r.purposeKo = QStringLiteral("선택한 유구 하나를 확대해 실측도로 만듭니다.");
      r.defaultScale = 100.0;
      r.gridIntervalM = 2.0;
      r.includeBasemap = false;
      r.layerKeys = {QStringLiteral("feature_poly"), QStringLiteral("feature_line")};
      r.emptyHintKo = QStringLiteral("유구를 그린 뒤, 왼쪽에서 실측할 유구를 고르세요.");
      r.scaleChoices = {50.0, 100.0, 200.0};
      break;
    case DrawingKind::Section:
      r.purposeKo = QStringLiteral("단면·층위 기준선을 도면으로 출력합니다.");
      r.defaultScale = 200.0;
      r.gridIntervalM = 0.0;
      r.includeBasemap = false;
      r.layerKeys = {QStringLiteral("section_line"), QStringLiteral("survey_area")};
      r.emptyHintKo = QStringLiteral("먼저 「그리기 → 단면」으로 단면선을 그리세요.");
      r.scaleChoices = {100.0, 200.0, 500.0};
      break;
  }
  return r;
}

QList<LayoutService::DrawingRecipe> LayoutService::allRecipes() {
  return {
    recipe(DrawingKind::SurveyAreaMap),
    recipe(DrawingKind::SiteLocation),
    recipe(DrawingKind::FeaturePlan),
    recipe(DrawingKind::FeatureDetail),
    recipe(DrawingKind::Section),
  };
}

static QgsRectangle featuresExtent(QgsVectorLayer* layer, QgsFeatureId onlyId = FID_NULL) {
  QgsRectangle ext;
  bool first = true;
  if (!layer || layer->featureCount() <= 0) return ext;
  QgsFeatureRequest req;
  if (onlyId != FID_NULL)
    req.setFilterFid(onlyId);
  QgsFeatureIterator it = layer->getFeatures(req);
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QgsGeometry g = f.geometry();
    if (g.isNull() || g.isEmpty()) continue;
    if (first) {
      ext = g.boundingBox();
      first = false;
    } else {
      ext.combineExtentWith(g.boundingBox());
    }
  }
  return ext;
}

static QString firstAttr(QgsVectorLayer* layer, const QString& field) {
  if (!layer) return {};
  QgsFeature f;
  QgsFeatureIterator it = layer->getFeatures();
  if (!it.nextFeature(f)) return {};
  return f.attribute(field).toString().trimmed();
}

static QList<QgsMapLayer*> layersForRecipe(QgsProject* project, const LayoutService::DrawingRecipe& rec) {
  QList<QgsMapLayer*> out;
  if (!project) return out;
  for (const QString& key : rec.layerKeys) {
    if (auto* vl = LayerOps::findByLayerKey(project, key))
      out.append(vl);
  }
  Q_UNUSED(rec.includeBasemap);
  return out;
}

// 유구 종류·시대 요약(범례 심볼 아래 보조 텍스트). 없으면 빈 문자열.
static QString kindPeriodSummary(const QList<QgsMapLayer*>& layers) {
  QStringList rows;
  for (QgsMapLayer* ml : layers) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || vl->fields().indexOf(QStringLiteral("kind")) < 0)
      continue;
    QgsFeature f;
    QgsFeatureIterator it = vl->getFeatures();
    while (it.nextFeature(f) && rows.size() < 10) {
      const QString k = f.attribute(QStringLiteral("kind")).toString().trimmed();
      const QString p = f.attribute(QStringLiteral("period")).toString().trimmed();
      QString row = k;
      if (!p.isEmpty())
        row += QStringLiteral(" / %1").arg(p);
      if (!row.isEmpty() && !rows.contains(row))
        rows << row;
    }
  }
  if (rows.isEmpty())
    return {};
  return QStringLiteral("유구 종류·시대\n· ") + rows.join(QStringLiteral("\n· "));
}

// 조판용 방위표 PNG. 측량도면식: N 글자 위 + 반채움 니들(좌흑·우백).
static QString writeLayoutNorthArrowPng() {
  const int s = 256;
  QImage img(s, s, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, true);
  const QColor ink(17, 24, 39);

  // N 글자(위쪽 중앙).
  QFont f(QStringLiteral("Malgun Gothic"), 10, QFont::Bold);
  f.setPixelSize(int(s * 0.20));
  p.setFont(f);
  p.setPen(ink);
  p.drawText(QRectF(0, s * 0.02, s, s * 0.22), Qt::AlignHCenter | Qt::AlignVCenter,
             QStringLiteral("N"));

  // 니들: 위 꼭짓점에서 아래 좌우로 벌어지는 긴 이등변, 왼쪽 채움·오른쪽 백색.
  const QPointF tip(s * 0.5, s * 0.26);
  const QPointF tail(s * 0.5, s * 0.80);
  const double halfW = s * 0.085;
  QPainterPath left;
  left.moveTo(tip);
  left.lineTo(tail.x() - halfW, s * 0.86);
  left.lineTo(tail);
  left.closeSubpath();
  QPainterPath right;
  right.moveTo(tip);
  right.lineTo(tail.x() + halfW, s * 0.86);
  right.lineTo(tail);
  right.closeSubpath();
  p.setPen(QPen(ink, s * 0.012));
  p.setBrush(ink);
  p.drawPath(left);
  p.setBrush(Qt::white);
  p.drawPath(right);
  p.end();
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (dir.isEmpty())
    dir = QDir::tempPath();
  dir = QDir(dir).filePath(QStringLiteral("north"));
  if (!QDir().mkpath(dir))
    dir = QDir::tempPath();
  const QString path = QDir::fromNativeSeparators(
      QDir(dir).filePath(QStringLiteral("ka-hgis-layout-north.png")));
  if (!img.save(path, QByteArrayLiteral("PNG").constData()))
    return {};
  return QFile::exists(path) ? path : QString();
}

static bool hasDrawableContent(const QList<QgsMapLayer*>& layers, const LayoutService::DrawingRecipe& rec,
                               QgsFeatureId featureId) {
  if (rec.kind == LayoutService::DrawingKind::FeaturePlan) {
    for (QgsMapLayer* ml : layers) {
      auto* vl = qobject_cast<QgsVectorLayer*>(ml);
      if (!vl || LayerOps::isReferenceLayer(vl) || LayerOps::isCadastralLayer(vl) ||
          LayerOps::isBasemapLayer(vl))
        continue;
      const QString k = LayerOps::layerKeyOf(vl);
      if ((k == QLatin1String("feature_poly") || k == QLatin1String("feature_line")) && vl->featureCount() > 0)
        return true;
    }
    return false;
  }
  if (rec.kind == LayoutService::DrawingKind::Section) {
    for (QgsMapLayer* ml : layers) {
      auto* vl = qobject_cast<QgsVectorLayer*>(ml);
      if (!vl || LayerOps::isReferenceLayer(vl) || LayerOps::isCadastralLayer(vl) ||
          LayerOps::isBasemapLayer(vl))
        continue;
      const QString k = LayerOps::layerKeyOf(vl);
      if (k == QLatin1String("section_line") && vl->featureCount() > 0)
        return true;
    }
    return false;
  }
  for (QgsMapLayer* ml : layers) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || LayerOps::isReferenceLayer(vl) || LayerOps::isCadastralLayer(vl) ||
        LayerOps::isBasemapLayer(vl))
      continue;
    if (rec.kind == LayoutService::DrawingKind::FeatureDetail) {
      if (featureId != FID_NULL && vl->getFeature(featureId).isValid())
        return true;
      if (vl->featureCount() > 0) return true;
    } else if (vl->featureCount() > 0) {
      return true;
    }
  }
  return false;
}

static QgsRectangle resolveExtent(QgsProject* project, const LayoutService::DrawingRecipe& rec,
                                  const LayoutService::DrawingOptions& opt) {
  if (opt.extentMode == LayoutService::ExtentMode::Canvas && !opt.canvasExtent.isEmpty())
    return opt.canvasExtent;

  auto* sa = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
  auto* fp = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
  auto* fl = LayerOps::findByLayerKey(project, QStringLiteral("feature_line"));
  auto* sl = LayerOps::findByLayerKey(project, QStringLiteral("section_line"));

  if (opt.extentMode == LayoutService::ExtentMode::SelectedFeature ||
      rec.kind == LayoutService::DrawingKind::FeatureDetail) {
    QgsRectangle e = featuresExtent(fp, opt.featureId);
    if (e.isEmpty()) e = featuresExtent(fl, opt.featureId);
    if (!e.isEmpty()) {
      e.scale(1.35);
      return e;
    }
  }

  if (rec.kind == LayoutService::DrawingKind::Section) {
    QgsRectangle e = featuresExtent(sl);
    if (e.isEmpty()) e = featuresExtent(sa);
    if (!e.isEmpty()) {
      e.scale(1.4);
      return e;
    }
  }

  QgsRectangle survey = featuresExtent(sa);
  if (opt.extentMode == LayoutService::ExtentMode::WideContext ||
      rec.kind == LayoutService::DrawingKind::SiteLocation) {
    if (!survey.isEmpty()) {
      survey.scale(8.0);
      return survey;
    }
  }

  if (!survey.isEmpty()) {
    survey.scale(rec.kind == LayoutService::DrawingKind::FeaturePlan ? 1.08 : 1.15);
    return survey;
  }

  QgsRectangle feats = featuresExtent(fp);
  if (!feats.isEmpty()) {
    const QgsRectangle lineExt = featuresExtent(fl);
    if (!lineExt.isEmpty()) feats.combineExtentWith(lineExt);
    feats.scale(1.2);
    return feats;
  }
  return {};
}

static void applyPageSize(QgsPrintLayout* layout, LayoutService::Paper paper,
                          LayoutService::Orientation orientation) {
  double pageW = (paper == LayoutService::Paper::A3) ? 297.0 : 210.0;
  double pageH = (paper == LayoutService::Paper::A3) ? 420.0 : 297.0;
  if (orientation == LayoutService::Orientation::Landscape)
    std::swap(pageW, pageH);
  if (layout->pageCollection() && layout->pageCollection()->pageCount() > 0) {
    if (QgsLayoutItemPage* page = layout->pageCollection()->page(0))
      page->setPageSize(QgsLayoutSize(pageW, pageH, Qgis::LayoutUnit::Millimeters));
  }
}

static void fillLayout(QgsPrintLayout* layout, QgsProject* project,
                       const LayoutService::DrawingRecipe& rec,
                       const LayoutService::DrawingOptions& opt,
                       LayoutService::DrawingBuildResult* result) {
  const double pageW = (opt.paper == LayoutService::Paper::A3) ? 297.0 : 210.0;
  const double pageH = (opt.paper == LayoutService::Paper::A3) ? 420.0 : 297.0;
  const bool land = opt.orientation == LayoutService::Orientation::Landscape;
  const double W = land ? std::max(pageW, pageH) : std::min(pageW, pageH);
  const double H = land ? std::min(pageW, pageH) : std::max(pageW, pageH);

  applyPageSize(layout, opt.paper, opt.orientation);

  // 자동 생성 기본 템플릿 표식
  layout->setCustomProperty(QStringLiteral("ka_hgis/auto_template"), true);
  layout->setCustomProperty(QStringLiteral("ka_hgis/user_composed"), false);

  auto* sa = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
  const QString surveyName = !opt.surveyName.isEmpty() ? opt.surveyName : firstAttr(sa, QStringLiteral("survey_name"));
  const QString siteName = !opt.siteName.isEmpty() ? opt.siteName : firstAttr(sa, QStringLiteral("site_name"));
  const QString sheet = !opt.titleKo.isEmpty() ? opt.titleKo : rec.titleKo;
  const QString crsAuth = project && project->crs().isValid()
                              ? project->crs().authid()
                              : QStringLiteral("EPSG:5186");
  const QString crsName = LayoutService::koreanCrsName(crsAuth);
  const QString crsFull = crsName.isEmpty() ? crsAuth
                                            : QStringLiteral("%1 %2").arg(crsName, crsAuth);
  const QFont kFont(QStringLiteral("Malgun Gothic"));
  const QColor ink(17, 24, 39);
  const QColor sub(75, 85, 99);

  // ── 전문 도면 골격: 상단 제목띠 / 도곽 좌표 주기 여백 / 우측 정보열 / 하단 축척띠.
  const double margin = 10.0;
  const double annPad = 5.6;           // 도곽 밖 좌표 주기 공간(지브라+숫자)
  const double headerBottom = 15.0;    // 제목띠 아래 경계(룰 라인)
  const double sideW = 46.0;
  const double sideX = W - margin - sideW;
  const double bottomStripH = 14.0;
  const double mapX = margin + annPad;
  const double mapTop = headerBottom + 1.5 + annPad;
  const double mapW = sideX - 6.0 - annPad - mapX;
  const double mapH = H - mapTop - margin - bottomStripH - annPad;

  // 제목띠: 도면명(좌) + 조사명·유적명(우) + 가는 룰 라인.
  auto* title = new QgsLayoutItemLabel(layout);
  title->setId(QStringLiteral("sheet_title"));
  title->setText(sheet);
  title->attemptSetSceneRect(QRectF(margin, 5.0, W * 0.5, 9.0));
  QFont titleFont = kFont;
  titleFont.setPointSize(15);
  titleFont.setBold(true);
  titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  title->setFont(titleFont);
  title->setFontColor(ink);
  layout->addLayoutItem(title);

  QStringList metaParts;
  if (!surveyName.isEmpty()) metaParts << surveyName;
  if (!siteName.isEmpty() && siteName != surveyName) metaParts << siteName;
  if (!metaParts.isEmpty()) {
    auto* metaLbl = new QgsLayoutItemLabel(layout);
    metaLbl->setText(metaParts.join(QStringLiteral("  ·  ")));
    metaLbl->setHAlign(Qt::AlignRight);
    metaLbl->setVAlign(Qt::AlignBottom);
    metaLbl->attemptSetSceneRect(QRectF(W * 0.5 + 2.0, 6.5, W - margin - (W * 0.5 + 2.0), 7.0));
    QFont metaFont = kFont;
    metaFont.setPointSize(9);
    metaLbl->setFont(metaFont);
    metaLbl->setFontColor(sub);
    layout->addLayoutItem(metaLbl);
  }

  // 제목띠 아래 가는 룰(디바이더): 빈 라벨 + 잉크색 배경.
  auto* rule = new QgsLayoutItemLabel(layout);
  rule->setId(QStringLiteral("header_rule"));
  rule->setText(QString());
  rule->setBackgroundEnabled(true);
  rule->setBackgroundColor(ink);
  rule->attemptSetSceneRect(QRectF(margin, headerBottom, W - margin * 2.0, 0.4));
  layout->addLayoutItem(rule);

  const QList<QgsMapLayer*> mapLayers = layersForRecipe(project, rec);
  const bool content = hasDrawableContent(mapLayers, rec, opt.featureId);
  if (result) result->hasMapContent = content;

  auto* map = new QgsLayoutItemMap(layout);
  map->attemptSetSceneRect(QRectF(mapX, mapTop, mapW, mapH));
  map->setFrameEnabled(true);
  map->setFrameStrokeWidth(QgsLayoutMeasurement(0.3, Qgis::LayoutUnit::Millimeters));
  map->setFrameStrokeColor(ink);
  layout->addLayoutItem(map);
  if (project && project->crs().isValid())
    map->setCrs(project->crs());
  map->setLayers(mapLayers);
  map->setKeepLayerSet(true);
  map->setMapRotation(0.0);

  QgsRectangle ext = resolveExtent(project, rec, opt);
  // setExtent는 아이템 크기를 범위 종횡비로 바꿔 도곽·축척띠와 겹치게 하므로
  // 아이템 크기를 유지하는 zoomToExtent를 쓴다.
  if (!ext.isEmpty())
    map->zoomToExtent(ext);

  const double scale = opt.scaleOverride > 0.0 ? opt.scaleOverride : rec.defaultScale;
  if (scale > 0.0 && content && !ext.isEmpty())
    map->setScale(scale, true);

  // 도곽: 지브라 프레임 + 정수 TM 좌표 주기 + 십자 눈금(축척에 맞는 1-2-5 간격).
  if (rec.gridIntervalM > 0.0) {
    const double denom = map->scale() > 0.0 ? map->scale() : scale;
    const double interval = LayoutService::niceGridIntervalMeters(denom, mapW);
    const bool crosses = rec.kind != LayoutService::DrawingKind::SiteLocation;
    LayoutService::applySurveyFrameGrid(map, interval, crosses, true);
  }

  if (result) {
    result->appliedScale = map->scale();
    result->appliedExtent = map->extent();
  }

  // ── 우측 정보열: 방위표 → 범례 → (유구 요약) → 표제란.
  const QString northPng = writeLayoutNorthArrowPng();
  if (!northPng.isEmpty()) {
    auto* north = new QgsLayoutItemPicture(layout);
    north->setId(QStringLiteral("north_arrow"));
    north->setPicturePath(northPng, Qgis::PictureFormat::Raster);
    north->setMode(Qgis::PictureFormat::Raster);
    north->setResizeMode(QgsLayoutItemPicture::Zoom);
    north->setNorthMode(QgsLayoutItemPicture::GridNorth);
    north->setLinkedMap(map);
    north->attemptSetSceneRect(QRectF(sideX + (sideW - 16.0) * 0.5, mapTop, 16.0, 22.0));
    layout->addLayoutItem(north);
    north->refreshPicture();
  } else {
    auto* north = new QgsLayoutItemLabel(layout);
    north->setText(QStringLiteral("N\n↑"));
    north->setHAlign(Qt::AlignHCenter);
    north->attemptSetSceneRect(QRectF(sideX + 8, mapTop, 28, 22));
    north->setFont(QFont(QStringLiteral("Malgun Gothic"), 12, QFont::Bold));
    layout->addLayoutItem(north);
  }

  // 표제란(도면 정보 상자): 하단 고정. 전문 도면의 필수 요소.
  const double blockH = 30.0;
  const double blockHeadH = 6.0;
  const double blockY = H - margin - blockH - blockHeadH;
  auto* blockHead = new QgsLayoutItemLabel(layout);
  blockHead->setId(QStringLiteral("title_block_head"));
  blockHead->setText(QStringLiteral("도 면 정 보"));
  blockHead->setHAlign(Qt::AlignHCenter);
  blockHead->setVAlign(Qt::AlignVCenter);
  QFont headFont = kFont;
  headFont.setPointSize(8);
  headFont.setBold(true);
  headFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
  blockHead->setFont(headFont);
  blockHead->setFontColor(ink);
  blockHead->setFrameEnabled(true);
  blockHead->setFrameStrokeColor(ink);
  blockHead->setFrameStrokeWidth(QgsLayoutMeasurement(0.3, Qgis::LayoutUnit::Millimeters));
  blockHead->setBackgroundEnabled(true);
  blockHead->setBackgroundColor(QColor(238, 239, 241));
  blockHead->attemptSetSceneRect(QRectF(sideX, blockY, sideW, blockHeadH));
  layout->addLayoutItem(blockHead);

  const int scNow = map->scale() > 0 ? int(std::lround(map->scale())) : 0;
  const QString scaleText = scNow > 0 ? QStringLiteral("1 : %1").arg(QLocale().toString(scNow))
                                      : QStringLiteral("축척자 참조");
  auto* block = new QgsLayoutItemLabel(layout);
  block->setId(QStringLiteral("title_block"));
  block->setText(QStringLiteral("도면명   %1\n조사명   %2\n축  척   %3\n좌표계   %4\n작성일   %5   ·   ka-hgis")
                     .arg(sheet,
                          surveyName.isEmpty() ? QStringLiteral("―") : surveyName,
                          scaleText, crsFull,
                          QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
  block->setVAlign(Qt::AlignVCenter);
  block->setMarginX(2.0);
  block->setMarginY(1.5);
  QFont blockFont = kFont;
  blockFont.setPointSize(7);
  block->setFont(blockFont);
  block->setFontColor(ink);
  block->setFrameEnabled(true);
  block->setFrameStrokeColor(ink);
  block->setFrameStrokeWidth(QgsLayoutMeasurement(0.3, Qgis::LayoutUnit::Millimeters));
  block->setBackgroundEnabled(true);
  block->setBackgroundColor(Qt::white);
  block->attemptSetSceneRect(QRectF(sideX, blockY + blockHeadH, sideW, blockH));
  layout->addLayoutItem(block);

  // 범례: 방위표 아래 ~ 표제란 위.
  const QString kinds = kindPeriodSummary(mapLayers);
  const double legendTop = mapTop + 25.0;
  const double kindsH = kinds.isEmpty() ? 0.0 : 24.0;
  const double legendH = std::max(30.0, blockY - 3.0 - kindsH - (kindsH > 0.0 ? 2.0 : 0.0) - legendTop);
  auto* legend = new QgsLayoutItemLegend(layout);
  legend->setId(QStringLiteral("legend"));
  legend->setTitle(QStringLiteral("범  례"));
  legend->setLinkedMap(map);
  legend->setLegendFilterByMapEnabled(true);
  legend->setResizeToContents(false);
  legend->setStyleFont(Qgis::LegendComponent::Title,
                       QFont(QStringLiteral("Malgun Gothic"), 9, QFont::Bold));
  legend->setStyleFont(Qgis::LegendComponent::Group, QFont(QStringLiteral("Malgun Gothic"), 8));
  legend->setStyleFont(Qgis::LegendComponent::Subgroup, QFont(QStringLiteral("Malgun Gothic"), 8));
  legend->setStyleFont(Qgis::LegendComponent::SymbolLabel,
                       QFont(QStringLiteral("Malgun Gothic"), 8));
  legend->setFrameEnabled(true);
  legend->setFrameStrokeColor(ink);
  legend->setFrameStrokeWidth(QgsLayoutMeasurement(0.3, Qgis::LayoutUnit::Millimeters));
  legend->setBackgroundEnabled(true);
  legend->attemptSetSceneRect(QRectF(sideX, legendTop, sideW, legendH));
  layout->addLayoutItem(legend);
  legend->updateLegend();
  LayoutService::tuneSheetLegend(legend);

  if (!kinds.isEmpty()) {
    auto* kindsLbl = new QgsLayoutItemLabel(layout);
    kindsLbl->setText(kinds);
    kindsLbl->attemptSetSceneRect(QRectF(sideX, legendTop + legendH + 2.0, sideW, kindsH));
    kindsLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 7));
    kindsLbl->setFontColor(sub);
    kindsLbl->setFrameEnabled(true);
    kindsLbl->setFrameStrokeColor(QColor(156, 163, 175));
    kindsLbl->setFrameStrokeWidth(QgsLayoutMeasurement(0.2, Qgis::LayoutUnit::Millimeters));
    kindsLbl->setBackgroundEnabled(true);
    layout->addLayoutItem(kindsLbl);
  }

  // ── 하단 축척띠: 교호식 축척자 + 1:N + 좌표계.
  const double stripY = H - margin - bottomStripH + 1.0;
  auto* sb = new QgsLayoutItemScaleBar(layout);
  sb->setId(QStringLiteral("scale_bar"));
  layout->addLayoutItem(sb);
  sb->setLinkedMap(map);
  sb->setStyle(QStringLiteral("Double Box"));
  sb->setUnits(Qgis::DistanceUnit::Meters);
  sb->setUnitLabel(QStringLiteral("m"));
  sb->setNumberOfSegments(4);
  sb->setNumberOfSegmentsLeft(0);
  sb->setLabelVerticalPlacement(Qgis::ScaleBarDistanceLabelVerticalPlacement::AboveSegment);
  sb->setHeight(2.4);
  sb->setLabelBarSpace(1.0);
  sb->setBoxContentSpace(0.6);
  if (content && !ext.isEmpty())
    sb->applyDefaultSize(Qgis::DistanceUnit::Meters);
  QgsTextFormat sbFmt;
  sbFmt.setFont(QFont(QStringLiteral("Malgun Gothic")));
  sbFmt.setSize(7.0);
  sbFmt.setSizeUnit(Qgis::RenderUnit::Points);
  sbFmt.setColor(ink);
  sb->setTextFormat(sbFmt);
  if (content && !ext.isEmpty() && map->scale() > 0.0) {
    const double seg = LayoutService::niceScaleBarSegmentMeters(mapW, map->scale(), 4);
    if (seg > 0.0) {
      sb->setUnitsPerSegment(seg);
      sb->setNumberOfSegments(4);
    }
  }
  LayoutService::applySheetScaleBarInk(sb);
  sb->attemptSetSceneRect(QRectF(mapX, stripY, 88, 11));

  auto* scaleLbl = new QgsLayoutItemLabel(layout);
  scaleLbl->setText(scNow > 0 ? QStringLiteral("S = 1 : %1").arg(QLocale().toString(scNow))
                              : QStringLiteral("축척: 축척자 참조"));
  scaleLbl->setId(QStringLiteral("scale_label"));
  scaleLbl->setVAlign(Qt::AlignBottom);
  scaleLbl->attemptSetSceneRect(QRectF(mapX + 92, stripY + 1.0, 52, 9));
  QFont scaleFont = kFont;
  scaleFont.setPointSize(9);
  scaleFont.setBold(true);
  scaleLbl->setFont(scaleFont);
  scaleLbl->setFontColor(ink);
  layout->addLayoutItem(scaleLbl);

  auto* crsLbl = new QgsLayoutItemLabel(layout);
  crsLbl->setId(QStringLiteral("crs_label"));
  crsLbl->setText(QStringLiteral("좌표계  %1").arg(crsFull));
  crsLbl->setHAlign(Qt::AlignRight);
  crsLbl->setVAlign(Qt::AlignBottom);
  crsLbl->attemptSetSceneRect(QRectF(mapX + 148, stripY + 1.0, mapX + mapW - (mapX + 148), 9));
  QFont crsFont = kFont;
  crsFont.setPointSize(7);
  crsLbl->setFont(crsFont);
  crsLbl->setFontColor(sub);
  layout->addLayoutItem(crsLbl);

  if (!content) {
    auto* hint = new QgsLayoutItemLabel(layout);
    hint->setText(rec.emptyHintKo);
    hint->setHAlign(Qt::AlignHCenter);
    hint->setVAlign(Qt::AlignVCenter);
    hint->attemptSetSceneRect(QRectF(mapX + 8, mapTop + mapH * 0.38, mapW - 16, 18));
    hint->setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
    hint->setBackgroundEnabled(true);
    hint->setFrameEnabled(true);
    hint->setId(QStringLiteral("empty_hint"));
    layout->addLayoutItem(hint);
    if (result) result->warningKo = rec.emptyHintKo;
  }
}

int LayoutService::niceScaleDenominator(double rawScale) {
  if (!(rawScale > 0.0) || !std::isfinite(rawScale))
    return 1000;
  // Smallest 10-ending cartographic scale that still contains the extent
  // (do not snap down — that clips the dragged survey). One table for chips,
  // snap, section and print lists lives in StandardScales.
  return StandardScales::snapUp(rawScale, StandardScales::Snap);
}

double LayoutService::niceScaleBarSegmentMeters(double mapWidthMm, double scaleDenominator,
                                                int segments) {
  if (segments < 1)
    segments = 4;
  if (!(mapWidthMm > 0.0) || !(scaleDenominator > 0.0))
    return 1.0;
  const double targetBarMm = std::clamp(mapWidthMm * 0.35, 40.0, 160.0);
  const double rawSeg =
      (targetBarMm / 1000.0 * scaleDenominator) / static_cast<double>(segments);
  // 0.1–0.4 m segments let 1:10–1:30 excavation drawings carry 10–20 cm ticks.
  static const double kLen[] = {0.1, 0.2, 0.4, 0.5, 1.0, 2.0, 4.0, 5.0, 10.0, 20.0, 40.0, 50.0,
                                100.0, 200.0, 400.0, 500.0, 1000.0, 2000.0, 5000.0};
  double best = kLen[0];
  double bestRel = 1.0e99;
  for (double n : kLen) {
    const double rel = std::fabs(rawSeg - n) / n;
    if (rel < bestRel) {
      bestRel = rel;
      best = n;
    }
  }
  return best;
}

double LayoutService::scaleBarWidthMm(double segmentMeters, int segments, double scaleDenominator) {
  if (!(segmentMeters > 0.0) || segments < 1 || !(scaleDenominator > 0.0))
    return 80.0;
  return segmentMeters * static_cast<double>(segments) / scaleDenominator * 1000.0;
}

void LayoutService::applySheetScaleBarInk(QgsLayoutItemScaleBar* sb) {
  if (!sb)
    return;
  const QString ink = QStringLiteral("#111827");
  const QString paper = QStringLiteral("#FFFFFF");
  QgsTextFormat fmt = sb->textFormat();
  fmt.setColor(QColor(ink));
  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.6);
  buf.setColor(QColor(paper));
  fmt.setBuffer(buf);
  sb->setTextFormat(fmt);
  if (auto fill = QgsFillSymbol::createSimple({
          {QStringLiteral("color"), ink},
          {QStringLiteral("outline_color"), ink},
          {QStringLiteral("outline_width"), QStringLiteral("0.30")},
          {QStringLiteral("outline_width_unit"), QStringLiteral("MM")}}))
    sb->setFillSymbol(fill.release());
  if (auto alt = QgsFillSymbol::createSimple({
          {QStringLiteral("color"), paper},
          {QStringLiteral("outline_color"), ink},
          {QStringLiteral("outline_width"), QStringLiteral("0.30")},
          {QStringLiteral("outline_width_unit"), QStringLiteral("MM")}}))
    sb->setAlternateFillSymbol(alt.release());
  if (auto ln = QgsLineSymbol::createSimple({
          {QStringLiteral("line_color"), ink},
          {QStringLiteral("line_width"), QStringLiteral("0.30")},
          {QStringLiteral("line_width_unit"), QStringLiteral("MM")}}))
    sb->setLineSymbol(ln.release());
  if (auto div = QgsLineSymbol::createSimple({
          {QStringLiteral("line_color"), ink},
          {QStringLiteral("line_width"), QStringLiteral("0.30")},
          {QStringLiteral("line_width_unit"), QStringLiteral("MM")}}))
    sb->setDivisionLineSymbol(div.release());
  if (auto sub = QgsLineSymbol::createSimple({
          {QStringLiteral("line_color"), ink},
          {QStringLiteral("line_width"), QStringLiteral("0.30")},
          {QStringLiteral("line_width_unit"), QStringLiteral("MM")}}))
    sb->setSubdivisionLineSymbol(sub.release());
  sb->setBackgroundEnabled(true);
  sb->setBackgroundColor(QColor(paper));
}

double LayoutService::niceGridIntervalMeters(double scaleDenominator, double mapWidthMm) {
  if (!(scaleDenominator > 0.0) || !std::isfinite(scaleDenominator))
    return 100.0;
  const double paperMm = mapWidthMm > 0.0 ? mapWidthMm : 180.0;
  // 한 칸이 종이에서 25~70mm 사이가 되게 목표 간격을 잡고 1-2-5 계열로 스냅.
  const double targetMm = std::clamp(paperMm * 0.24, 25.0, 70.0);
  const double rawM = targetMm / 1000.0 * scaleDenominator;
  static const double kSteps[] = {0.1,   0.2,   0.5,   1.0,   2.0,    5.0,    10.0,   20.0,
                                  25.0,  50.0,  100.0,  200.0,  250.0,  500.0,
                                  1000.0, 2000.0, 5000.0, 10000.0};
  double best = kSteps[0];
  double bestRel = 1.0e99;
  for (double s : kSteps) {
    const double rel = std::fabs(rawM - s) / s;
    if (rel < bestRel) {
      bestRel = rel;
      best = s;
    }
  }
  return best;
}

QString LayoutService::koreanCrsName(const QString& authId) {
  if (authId == QLatin1String("EPSG:5186")) return QStringLiteral("중부원점(GRS80)");
  if (authId == QLatin1String("EPSG:5187")) return QStringLiteral("동부원점(GRS80)");
  if (authId == QLatin1String("EPSG:5185")) return QStringLiteral("서부원점(GRS80)");
  if (authId == QLatin1String("EPSG:5188")) return QStringLiteral("동해원점(GRS80)");
  if (authId == QLatin1String("EPSG:5179")) return QStringLiteral("UTM-K 통일원점(GRS80)");
  if (authId == QLatin1String("EPSG:4326")) return QStringLiteral("경위도(WGS84)");
  if (authId == QLatin1String("EPSG:3857")) return QStringLiteral("웹 메르카토르");
  return {};
}

void LayoutService::applySurveyFrameGrid(QgsLayoutItemMap* map, double intervalM, bool crosses,
                                         bool showCoords) {
  if (!map || !map->grids() || !(intervalM > 0.0))
    return;
  // 기존 격자를 전부 걷어내고 표준 도곽 하나만 남긴다.
  QStringList oldIds;
  const QList<QgsLayoutItemMapGrid*> olds = map->grids()->asList();
  for (QgsLayoutItemMapGrid* old : olds) {
    if (!old) continue;
    old->setEnabled(false);
    if (!old->name().isEmpty()) oldIds.append(old->name());
    if (!old->id().isEmpty()) oldIds.append(old->id());
  }
  oldIds.removeDuplicates();
  for (const QString& id : oldIds)
    map->grids()->removeGrid(id);

  auto* g = new QgsLayoutItemMapGrid(QStringLiteral("survey_frame_grid"), map);
  map->grids()->addGrid(g);
  if (map->crs().isValid())
    g->setCrs(map->crs());
  g->setEnabled(true);
  g->setUnits(Qgis::MapGridUnit::MapUnits);
  g->setIntervalX(intervalM);
  g->setIntervalY(intervalM);
  g->setOffsetX(0.0);
  g->setOffsetY(0.0);

  const QColor ink(17, 24, 39);
  // 내부 눈금: 십자(도면 판독을 가리지 않는 측량 관례). crosses=false면 도곽만.
  g->setStyle(crosses ? Qgis::MapGridStyle::LineCrosses
                      : Qgis::MapGridStyle::FrameAndAnnotationsOnly);
  g->setCrossLength(2.2);
  g->setGridLineWidth(0.18);
  g->setGridLineColor(QColor(17, 24, 39, 160));

  // 도곽: 지형도·측량원도식 지브라(흑백 교차) 프레임.
  g->setFrameStyle(Qgis::MapGridFrameStyle::Zebra);
  g->setFrameWidth(1.4);
  g->setFramePenSize(0.22);
  g->setFramePenColor(ink);
  g->setFrameFillColor1(ink);
  g->setFrameFillColor2(Qt::white);

  // 좌표 주기: 정수 m(TM), 상하 수평·좌우 세로쓰기, 도곽 밖.
  g->setAnnotationEnabled(showCoords);
  if (showCoords) {
    g->setAnnotationFormat(Qgis::MapGridAnnotationFormat::Decimal);
    g->setAnnotationPrecision(0);
    g->setAnnotationFrameDistance(0.8);
    g->setAnnotationPosition(Qgis::MapGridAnnotationPosition::OutsideMapFrame,
                             Qgis::MapGridBorderSide::Left);
    g->setAnnotationPosition(Qgis::MapGridAnnotationPosition::OutsideMapFrame,
                             Qgis::MapGridBorderSide::Right);
    g->setAnnotationPosition(Qgis::MapGridAnnotationPosition::OutsideMapFrame,
                             Qgis::MapGridBorderSide::Top);
    g->setAnnotationPosition(Qgis::MapGridAnnotationPosition::OutsideMapFrame,
                             Qgis::MapGridBorderSide::Bottom);
    g->setAnnotationDirection(Qgis::MapGridAnnotationDirection::Horizontal,
                              Qgis::MapGridBorderSide::Top);
    g->setAnnotationDirection(Qgis::MapGridAnnotationDirection::Horizontal,
                              Qgis::MapGridBorderSide::Bottom);
    g->setAnnotationDirection(Qgis::MapGridAnnotationDirection::Vertical,
                              Qgis::MapGridBorderSide::Left);
    g->setAnnotationDirection(Qgis::MapGridAnnotationDirection::Vertical,
                              Qgis::MapGridBorderSide::Right);
    QgsTextFormat tf;
    QFont annFont(QStringLiteral("Malgun Gothic"));
    tf.setFont(annFont);
    tf.setSize(6.5);
    tf.setSizeUnit(Qgis::RenderUnit::Points);
    tf.setColor(QColor(31, 41, 55));
    g->setAnnotationTextFormat(tf);
  }
  map->updateBoundingRect();
}

QRectF LayoutService::equalFullSheetMapRect(const QRectF& page, const QRectF& map) {
  if (!page.isValid() || !map.isValid() || map.width() < page.width() * 0.65)
    return map;
  const double left = map.left() - page.left();
  const double right = page.right() - map.right();
  const double top = map.top() - page.top();
  if (qAbs(left - right) > 3.0)
    return map;
  const double side = (left + right) * 0.5;
  if (side < 8.0 || qAbs(top - side) < 0.75)
    return map;
  const double bottomKeep = page.bottom() - map.bottom();
  if (bottomKeep < 8.0)
    return map;
  return QRectF(page.left() + side, page.top() + side,
                std::max(40.0, page.width() - 2.0 * side),
                std::max(40.0, page.height() - side - bottomKeep));
}

LayoutService::SheetChromeRects LayoutService::standardSheetChrome(const QRectF& page,
                                                                   const QRectF& requestedMap) {
  // Field sheet strip locked under the map (not on imagery):
  // [0 50 100 … m]                    좌표계 … EPSG:518x   [N]
  //      축척 1 : N
  // kGap은 도곽(지브라 프레임+좌표 주기)이 지도 아래로 뻗는 공간까지 포함한다.
  constexpr double kChromeH = 33.0;
  constexpr double kGap = 7.0;
  constexpr double kMargin = 8.0;
  constexpr double kMinMapH = 40.0;
  constexpr double kBarH = 12.0;
  constexpr double kLabelH = 7.0;
  constexpr double kLabelGap = 2.5;
  constexpr double kNorth = 20.0;
  constexpr double kCrsH = 7.0;
  constexpr double kCrsW = 58.0;
  constexpr double kCrsGap = 6.0;

  SheetChromeRects out;
  QRectF map = requestedMap;
  if (!map.isValid() || map.width() < 8.0 || map.height() < 8.0) {
    map = QRectF(kMargin, kMargin, std::max(40.0, page.width() - 2.0 * kMargin),
                 std::max(kMinMapH, page.height() - 2.0 * kMargin - kChromeH));
  }

  if (map.bottom() + kChromeH > page.bottom() - kMargin) {
    const double maxBottom = page.bottom() - kMargin - kChromeH;
    double newH = maxBottom - map.top();
    if (newH < kMinMapH)
      newH = kMinMapH;
    map.setHeight(newH);
  }
  out.map = map;

  const double rowTop = map.bottom() + kGap;
  out.north = QRectF(map.right() - kNorth, rowTop, kNorth, kNorth);
  out.crs = QRectF(out.north.left() - kCrsGap - kCrsW,
                   rowTop + std::max(0.0, (kBarH - kCrsH) * 0.5), kCrsW, kCrsH);

  const double barMaxRight = out.crs.left() - 8.0;
  const double barW = std::min({88.0, std::max(48.0, map.width() * 0.38),
                                std::max(32.0, barMaxRight - map.left())});
  out.scaleBar = QRectF(map.left(), rowTop, barW, kBarH);
  out.scaleLabel = QRectF(map.left(), out.scaleBar.bottom() + kLabelGap, barW, kLabelH);

  auto clampOnPage = [&](QRectF& r) {
    if (r.top() < map.bottom())
      r.moveTop(map.bottom());
    if (r.bottom() > page.bottom() - kMargin)
      r.setHeight(std::max(4.0, page.bottom() - kMargin - r.top()));
    if (r.left() < page.left())
      r.moveLeft(page.left());
    if (r.right() > page.right())
      r.setWidth(std::max(4.0, page.right() - r.left()));
  };
  clampOnPage(out.scaleBar);
  clampOnPage(out.scaleLabel);
  clampOnPage(out.north);
  clampOnPage(out.crs);

  if (out.crs.right() > out.north.left() - 2.0)
    out.crs.setRight(std::max(out.crs.left() + 4.0, out.north.left() - 2.0));
  if (out.scaleBar.right() > out.crs.left() - 4.0)
    out.scaleBar.setWidth(std::max(32.0, out.crs.left() - 4.0 - out.scaleBar.left()));
  out.scaleLabel.setLeft(out.scaleBar.left());
  out.scaleLabel.setWidth(out.scaleBar.width());
  if (out.scaleLabel.top() < out.scaleBar.bottom())
    out.scaleLabel.moveTop(out.scaleBar.bottom() + kLabelGap);
  clampOnPage(out.scaleLabel);

  return out;
}

bool LayoutService::applyCanvasViewToLayoutMap(QgsLayoutItemMap* map,
                                               const QgsRectangle& canvasExtent,
                                               double canvasScale) {
  if (!map || canvasExtent.isNull() || !canvasExtent.isFinite() || canvasExtent.width() <= 0.0 ||
      canvasExtent.height() <= 0.0)
    return false;
  map->zoomToExtent(canvasExtent);
  if (canvasScale > 1.0 && std::isfinite(canvasScale))
    map->setScale(canvasScale, true);
  return true;
}

QgsRectangle LayoutService::extentForPaperScale(const QgsRectangle& currentExtent,
                                                double mapWidthMm, double scaleDenominator) {
  if (mapWidthMm <= 0.0 || scaleDenominator <= 0.0)
    return currentExtent;
  const double groundWidth = scaleDenominator * (mapWidthMm / 1000.0);
  QgsPointXY center(0.0, 0.0);
  double ratio = 1.0;
  if (currentExtent.isFinite() && currentExtent.width() > 0.0) {
    center = currentExtent.center();
    if (currentExtent.height() > 0.0)
      ratio = currentExtent.height() / currentExtent.width();
  }
  const double groundHeight = groundWidth * ratio;
  return QgsRectangle(center.x() - groundWidth * 0.5, center.y() - groundHeight * 0.5,
                      center.x() + groundWidth * 0.5, center.y() + groundHeight * 0.5);
}

QPointF LayoutService::layoutMapItemFromXy(const QgsRectangle& extent, const QRectF& mapRect,
                                           double mapX, double mapY) {
  if (!extent.isFinite() || !(extent.width() > 0.0) || !(extent.height() > 0.0)
      || mapRect.width() < 1e-9 || mapRect.height() < 1e-9)
    return QPointF();
  return QPointF(mapRect.x() + (mapX - extent.xMinimum()) / extent.width() * mapRect.width(),
                 mapRect.y() + (extent.yMaximum() - mapY) / extent.height() * mapRect.height());
}

QgsPointXY LayoutService::layoutMapXyFromItem(const QgsRectangle& extent, const QRectF& mapRect,
                                              const QPointF& itemPt) {
  if (!extent.isFinite() || !(extent.width() > 0.0) || !(extent.height() > 0.0)
      || mapRect.width() < 1e-9 || mapRect.height() < 1e-9)
    return QgsPointXY();
  const double fx = (itemPt.x() - mapRect.x()) / mapRect.width();
  const double fy = (itemPt.y() - mapRect.y()) / mapRect.height();
  return QgsPointXY(extent.xMinimum() + fx * extent.width(),
                    extent.yMaximum() - fy * extent.height());
}

bool LayoutService::layoutMapItemContains(const QPointF& itemPt, const QRectF& mapRect,
                                          double marginMm) {
  return mapRect.adjusted(-marginMm, -marginMm, marginMm, marginMm).contains(itemPt);
}

QgsRectangle LayoutService::zoomExtentAtAnchor(const QgsRectangle& extent, double fx, double fy,
                                               double zoomFactor) {
  if (!extent.isFinite() || !(extent.width() > 0.0) || !(extent.height() > 0.0) ||
      !(zoomFactor > 0.0) || !std::isfinite(zoomFactor))
    return extent;
  fx = std::clamp(fx, 0.0, 1.0);
  fy = std::clamp(fy, 0.0, 1.0);
  // 커서가 가리키는 지상 좌표(고정점).
  const double gx = extent.xMinimum() + fx * extent.width();
  const double gy = extent.yMaximum() - fy * extent.height();
  const double w = extent.width() / zoomFactor;
  const double h = extent.height() / zoomFactor;
  // 새 범위에서도 같은 상대 위치에 고정점이 오게 맞춘다.
  const double xMin = gx - fx * w;
  const double yMax = gy + fy * h;
  return QgsRectangle(xMin, yMax - h, xMin + w, yMax);
}

void LayoutService::applySingleRasterPassRendering(QgsLayout* layout) {
  KaPdfExport::prepareLayout(layout);
}

QString LayoutService::createBlankSheet(QgsProject* project, double widthMm, double heightMm,
                                        const QString& name, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  if (widthMm < 20.0 || heightMm < 20.0) {
    if (errorOut) *errorOut = QStringLiteral("용지가 너무 작습니다.");
    return {};
  }
  if (QgsMasterLayoutInterface* old = project->layoutManager()->layoutByName(name))
    project->layoutManager()->removeLayout(old);
  auto* layout = new QgsPrintLayout(project);
  layout->initializeDefaults();
  layout->setName(name);
  layout->setUnits(Qgis::LayoutUnit::Millimeters);
  applySingleRasterPassRendering(layout);
  ensureLayoutPage(layout, widthMm, heightMm);
  layout->setCustomProperty(QStringLiteral("ka_hgis/auto_template"), false);
  layout->setCustomProperty(QStringLiteral("ka_hgis/user_composed"), false);
  project->layoutManager()->addLayout(layout);
  return name;
}

bool LayoutService::ensureLayoutPage(QgsLayout* layout, double widthMm, double heightMm) {
  if (!layout || !layout->pageCollection() || widthMm < 20.0 || heightMm < 20.0)
    return false;
  auto* pages = layout->pageCollection();
  const QgsLayoutSize paper(widthMm, heightMm, Qgis::LayoutUnit::Millimeters);
  if (pages->pageCount() <= 0) {
    auto* page = new QgsLayoutItemPage(layout);
    page->setPageSize(paper);
    pages->addPage(page);
  }
  QgsLayoutItemPage* page = pages->page(0);
  if (!page)
    return false;
  // initializeDefaults()가 A4 가로를 먼저 만든다. 요청한 세로 크기로 다시 맞춘다.
  page->setPageSize(paper);
  layout->refresh();
  const QRectF ready = page->mapRectToScene(page->rect());
  return ready.width() >= 8.0 && ready.height() >= 8.0;
}

void LayoutService::markStudioSheetComposed(QgsLayout* layout) {
  if (!layout)
    return;
  layout->setCustomProperty(QStringLiteral("ka_hgis/auto_template"), false);
  layout->setCustomProperty(QStringLiteral("ka_hgis/user_composed"), true);
}

static QgsPrintLayout* replaceLayout(QgsProject* project, const QString& name) {
  if (QgsMasterLayoutInterface* old = project->layoutManager()->layoutByName(name))
    project->layoutManager()->removeLayout(old);
  auto* layout = new QgsPrintLayout(project);
  layout->initializeDefaults();
  layout->setName(name);
  layout->setUnits(Qgis::LayoutUnit::Millimeters);
  LayoutService::applySingleRasterPassRendering(layout);
  project->layoutManager()->addLayout(layout);
  return layout;
}

LayoutService::DrawingBuildResult LayoutService::buildDrawing(QgsProject* project, DrawingKind kind,
                                                              const DrawingOptions& options,
                                                              QString* errorOut) {
  DrawingBuildResult result;
  result.layoutId = layoutId(kind);
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    result.warningKo = QStringLiteral("프로젝트가 없습니다.");
    return result;
  }
  const DrawingRecipe rec = recipe(kind);
  if (kind == DrawingKind::FeatureDetail && options.featureId == FID_NULL) {
    auto* fp = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    if (!fp || fp->featureCount() <= 0) {
      result.warningKo = rec.emptyHintKo;
      result.hasMapContent = false;
    }
  }
  try {
    QgsPrintLayout* layout = replaceLayout(project, result.layoutId);
    fillLayout(layout, project, rec, options, &result);
  } catch (const std::exception& ex) {
    result.warningKo = QString::fromUtf8(ex.what());
    if (errorOut) *errorOut = result.warningKo;
  } catch (...) {
    KA_LOG_EXCEPT();
    result.warningKo = QStringLiteral("도면을 만드는 중 오류가 났습니다.");
    if (errorOut) *errorOut = result.warningKo;
  }
  return result;
}

int LayoutService::ensureDefaultLayouts(QgsProject* project) {
  if (!project) return 0;
  int created = 0;
  DrawingOptions opt;
  opt.orientation = Orientation::Landscape;
  for (const DrawingRecipe& rec : allRecipes()) {
    if (project->layoutManager()->layoutByName(rec.layoutId)) continue;
    buildDrawing(project, rec.kind, opt, nullptr);
    ++created;
  }
  return created;
}

int LayoutService::rebuildDefaultLayouts(QgsProject* project) {
  if (!project) return 0;
  for (const QString& name : defaultLayoutNames()) {
    if (QgsMasterLayoutInterface* old = project->layoutManager()->layoutByName(name))
      project->layoutManager()->removeLayout(old);
  }
  DrawingOptions opt;
  opt.orientation = Orientation::Landscape;
  int n = 0;
  for (const DrawingRecipe& rec : allRecipes()) {
    buildDrawing(project, rec.kind, opt, nullptr);
    ++n;
  }
  return n;
}

bool LayoutService::isComposedStudioSheet(QgsProject* project, const QString& layoutName) {
  if (!project || !project->layoutManager())
    return false;
  const QString target = layoutName.isEmpty() ? QStringLiteral("user_sheet") : layoutName;
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      project->layoutManager()->layoutByName(target));
  if (!ly)
    return false;
  if (ly->customProperty(QStringLiteral("ka_hgis/auto_template")).toBool() &&
      !ly->customProperty(QStringLiteral("ka_hgis/user_composed")).toBool())
    return false;

  auto* map = dynamic_cast<QgsLayoutItemMap*>(ly->itemById(QStringLiteral("ka_map")));
  if (!map)
    map = dynamic_cast<QgsLayoutItemMap*>(ly->itemById(QStringLiteral("ka_section_map")));
  if (!map) {
    QList<QgsLayoutItemMap*> maps;
    ly->layoutItems(maps);
    for (auto* m : maps) {
      if (m) {
        map = m;
        break;
      }
    }
  }
  if (!map)
    return false;

  // 1. 유효한 축척 (0 초과 및 유한값)
  if (!(map->scale() > 0.0) || !std::isfinite(map->scale()))
    return false;

  // 2. 유한하고 비어있지 않은 지도 범위
  const QgsRectangle ext = map->extent();
  if (!ext.isFinite() || ext.isEmpty() || !(ext.width() > 0.0) || !(ext.height() > 0.0))
    return false;

  // 3. 비-참조 유효 레이어 확인 (더미 layout_blank 및 참조 지도 배제)
  bool hasValidLayer = false;
  for (QgsMapLayer* l : map->layers()) {
    if (!l || !l->isValid())
      continue;
    const QString n = l->name();
    if (n == QLatin1String("layout_blank"))
      continue;
    if (target != QLatin1String("section_sheet") && n == QLatin1String("ka_section_blank"))
      continue;
    if (LayerOps::isReferenceLayer(l) || LayerOps::isCadastralLayer(l) ||
        LayerOps::isBasemapLayer(l))
      continue;

    if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
      if (LayerFeatures::any(vl) || (target == QLatin1String("section_sheet") && n == QLatin1String("ka_section_blank"))) {
        hasValidLayer = true;
        break;
      }
    } else if (auto* rl = qobject_cast<QgsRasterLayer*>(l)) {
      if (rl->width() > 0 && rl->height() > 0 && !rl->extent().isEmpty()) {
        hasValidLayer = true;
        break;
      }
    }
  }
  return hasValidLayer;
}

QString LayoutService::createReportLayout(QgsProject* project, const QString& titleKo,
                                          Paper paper, Orientation orientation,
                                          QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  const QString name = QStringLiteral("report_%1_%2")
                           .arg(paper == Paper::A3 ? QStringLiteral("a3") : QStringLiteral("a4"),
                                orientation == Orientation::Landscape ? QStringLiteral("land")
                                                                      : QStringLiteral("port"));
  DrawingOptions opt;
  opt.titleKo = titleKo.isEmpty() ? QStringLiteral("조사도면") : titleKo;
  opt.paper = paper;
  opt.orientation = orientation;
  const DrawingRecipe rec = recipe(DrawingKind::FeaturePlan);
  QgsPrintLayout* layout = replaceLayout(project, name);
  DrawingBuildResult dummy;
  fillLayout(layout, project, rec, opt, &dummy);
  return name;
}

QString LayoutService::exportLayoutPdf(QgsProject* project, const QString& layoutName,
                                       const QString& pdfPath, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  // user_sheet는 도면만들기 용지. 없으면 5종 템플릿을 심지 않는다.
  QgsMasterLayoutInterface* master = project->layoutManager()->layoutByName(layoutName);
  if (!master && layoutName != QLatin1String("user_sheet")) {
    ensureDefaultLayouts(project);
    master = project->layoutManager()->layoutByName(layoutName);
  }
  if (!master) {
    if (errorOut) *errorOut = QStringLiteral("도면을 찾을 수 없습니다: %1").arg(koreanTitle(layoutName));
    return {};
  }
  auto* layout = dynamic_cast<QgsPrintLayout*>(master);
  if (!layout) {
    if (errorOut) *errorOut = QStringLiteral("인쇄 도면이 아닙니다.");
    return {};
  }

  // 저장된 조판(예전 프로젝트에서 열린 것)에도 래스터 단일 렌더를 보장한다.
  KaPdfExport::prepareLayout(layout);
  // Saved studio sheets and submission PDFs need the same actual print-label
  // filtering as the Studio PDF button, including sheets reopened from disk.
  if (auto* map = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map")))) {
    HeritageLayoutNumbers localNumbers;
    auto* numbers = HeritageLayoutNumbers::forMap(map);
    if (!numbers) numbers = &localNumbers;
    if (!numbers->update(map, true)) {
      if (errorOut) *errorOut = numbers->error();
      return {};
    }
    auto* legend = dynamic_cast<QgsLayoutItemLegend*>(layout->itemById(QStringLiteral("ka_legend")));
    // Same call (and KaPdfExport settings) as the drawing studio 「PDF 저장」.
    if (!numbers->exportPdf(map, legend, pdfPath, KaPdfExport::kSheetDpi, errorOut)) return {};
    return pdfPath;
  }
  QgsLayoutExporter exporter(layout);
  const QgsLayoutExporter::PdfExportSettings settings = KaPdfExport::sheetSettings();
  // 화면 미리보기용으로 낮춰 둔 해상도가 남아 있어도 인쇄는 300 DPI로 나가게 한다.
  const double keepDpi = layout->renderContext().dpi();
  layout->renderContext().setDpi(settings.dpi);
  settleSheetLegendsForExport(layout);
  const auto r = exporter.exportToPdf(pdfPath, settings);
  layout->renderContext().setDpi(keepDpi);
  if (r != QgsLayoutExporter::Success) {
    if (errorOut) *errorOut = QStringLiteral("PDF 내보내기 실패 (코드 %1)").arg(int(r));
    return {};
  }
  if (!QFile::exists(pdfPath) || QFileInfo(pdfPath).size() < 500) {
    if (errorOut) *errorOut = QStringLiteral("PDF 파일이 비었거나 너무 작습니다.");
    return {};
  }
  return pdfPath;
}

QImage LayoutService::renderPreview(QgsProject* project, const QString& layoutName,
                                    const QSize& imageSize, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  QgsMasterLayoutInterface* master = project->layoutManager()->layoutByName(layoutName);
  auto* layout = dynamic_cast<QgsPrintLayout*>(master);
  if (!layout) {
    if (errorOut) *errorOut = QStringLiteral("미리볼 도면이 없습니다.");
    return {};
  }
  applySingleRasterPassRendering(layout);
  try {
    QgsLayoutExporter exporter(layout);
    const QSize sz = imageSize.isEmpty() ? QSize(720, 510) : imageSize;
    QImage img = exporter.renderPageToImage(0, sz, 96.0);
    if (img.isNull()) {
      if (errorOut) *errorOut = QStringLiteral("미리보기를 만들지 못했습니다.");
      return {};
    }
    return img;
  } catch (const std::exception& ex) {
    if (errorOut) *errorOut = QString::fromUtf8(ex.what());
    return {};
  } catch (...) {
    KA_LOG_EXCEPT();
    if (errorOut) *errorOut = QStringLiteral("미리보기를 그리는 중 오류가 났습니다.");
    return {};
  }
}

int LayoutService::exportDrawingPdfs(QgsProject* project, const QString& outDir, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return 0;
  }
  QDir dir(outDir);
  if (!dir.exists() && !QDir().mkpath(outDir)) {
    if (errorOut) *errorOut = QStringLiteral("폴더를 만들 수 없습니다.");
    return 0;
  }
  if (!isComposedStudioSheet(project, QStringLiteral("user_sheet"))) {
    if (errorOut)
      *errorOut = QStringLiteral("도면만들기에서 용지를 만든 뒤 다시 보내기 하세요.");
    return 0;
  }
  const QString path = dir.filePath(QStringLiteral("조사도면.pdf"));
  QString err;
  if (!exportLayoutPdf(project, QStringLiteral("user_sheet"), path, &err).isEmpty())
    return 1;
  if (errorOut)
    *errorOut = err.isEmpty() ? QStringLiteral("도면만들기 PDF를 만들지 못했습니다.") : err;
  return 0;
}

bool LayoutService::sheetLegendOmitsLayerName(const QString& name) {
  const QString n = name.trimmed();
  if (n.contains(QStringLiteral("흙토람 그림"))) return true;
  if (n == QStringLiteral("지질 음영")) return true;
  if (n == QStringLiteral("위성") || n.contains(QStringLiteral("VWorld 위성")) ||
      n.startsWith(QStringLiteral("Google 위성")))
    return true;
  if (n.startsWith(QStringLiteral("지적")) || n.contains(QStringLiteral("지적 본번")) ||
      n.contains(QStringLiteral("지적 부번")))
    return true;
  return false;
}

namespace {

bool omitSheetLegendLayer(QgsMapLayer* ml, const QString& fallbackName) {
  const QString name = ml ? ml->name() : fallbackName;
  if (LayoutService::sheetLegendOmitsLayerName(name)) return true;
  if (GeologyMapService::omitFromSheetLegend(name)) return true;
  if (ml && ml->customProperty(QStringLiteral("ka_hgis/omit_sheet_legend")).toBool())
    return true;
  if (ml) {
    const QString p = ml->providerType().toLower();
    if (p == QLatin1String("wms") || p == QLatin1String("xyz")) return true;
  }
  return false;
}

bool projectLayerChecked(QgsMapLayer* ml) {
  if (!ml) return false;
  QgsProject* proj = ml->project();
  if (!proj || !proj->layerTreeRoot()) return true;
  QgsLayerTreeLayer* node = proj->layerTreeRoot()->findLayer(ml->id());
  if (!node) return false;
  return node->isVisible();
}

bool onLinkedMap(QgsLayoutItemLegend* legend, QgsMapLayer* ml) {
  if (!legend || !ml) return false;
  QgsLayoutItemMap* map = legend->linkedMap();
  if (!map) return true;
  if (map->layers().contains(ml)) return true;
  if (auto* layout = legend->layout()) {
    for (QGraphicsItem* item : layout->items()) {
      auto* other = dynamic_cast<QgsLayoutItemMap*>(item);
      if (!other) continue;
      if ((other->id() == QLatin1String("ka_map_above") ||
           other->id() == QLatin1String("ka_map_numbers")) &&
          other->layers().contains(ml))
        return true;
    }
  }
  return LayerLabelControls::isHeritage(ml);
}

void addMissingSheetLegendLayers(QgsLayoutItemLegend* legend, QgsLayerTree* root) {
  if (!legend || !root) return;
  QSet<QString> seen;
  for (auto* existing : root->findLayers()) {
    if (existing && existing->layer()) seen.insert(existing->layer()->id());
  }
  QList<QgsMapLayer*> extra;
  if (auto* map = legend->linkedMap()) extra.append(map->layers());
  if (auto* layout = legend->layout()) {
    for (QGraphicsItem* item : layout->items()) {
      auto* other = dynamic_cast<QgsLayoutItemMap*>(item);
      if (!other) continue;
      if (other->id() == QLatin1String("ka_map_above") ||
          other->id() == QLatin1String("ka_map_numbers"))
        extra.append(other->layers());
    }
    if (auto* project = layout->project()) {
      for (auto* node : project->layerTreeRoot()->findLayers()) {
        if (!node || !node->isVisible()) continue;
        auto* vl = qobject_cast<QgsVectorLayer*>(node->layer());
        if (vl && LayerLabelControls::isHeritage(vl)) extra.append(vl);
      }
    }
  }
  for (auto* layer : extra) {
    if (!layer || seen.contains(layer->id())) continue;
    if (omitSheetLegendLayer(layer, layer->name()) || !projectLayerChecked(layer)) continue;
    root->addLayer(layer);
    seen.insert(layer->id());
  }
}

}  // namespace

void LayoutService::flowSheetLegend(QgsLayoutItemLegend* legend) {
  if (!legend || !legend->layout() || !legend->model()) return;
  const QString widthKey = QStringLiteral("ka_hgis/legend_flow_width");
  if (!legend->findChild<QTimer*>(QStringLiteral("ka_legend_flow_timer"))) {
    auto* timer = new QTimer(legend);
    timer->setObjectName(QStringLiteral("ka_legend_flow_timer"));
    timer->setSingleShot(true);
    timer->setInterval(120);
    QObject::connect(legend, &QgsLayoutItem::sizePositionChanged, timer, [legend, timer, widthKey] {
      // Moving or adjusting height must not rebuild the legend or map styles.
      if (qAbs(legend->rect().width() - legend->customProperty(widthKey).toDouble()) > .01)
        timer->start();
    });
    QObject::connect(timer, &QTimer::timeout, legend, [legend] { flowSheetLegend(legend); });
    // Symbol hit testing finishes after the initial size measurement. Reflow
    // once it removes off-map geology/soil rows so no tall empty frame remains.
    QObject::connect(legend->model(), &QgsLayerTreeModel::hitTestCompleted, timer,
                     [timer] { timer->start(); });
    // Export uses blocking hit tests, which update rows without emitting the
    // asynchronous hitTestCompleted signal.
    QObject::connect(legend->model(), &QAbstractItemModel::rowsRemoved, timer,
                     [timer] { timer->start(); });
    QObject::connect(legend->model(), &QAbstractItemModel::rowsInserted, timer,
                     [timer] { timer->start(); });
    QObject::connect(legend->model(), &QAbstractItemModel::modelReset, timer,
                     [timer] { timer->start(); });
  }
  auto* flowTimer = legend->findChild<QTimer*>(QStringLiteral("ka_legend_flow_timer"));
  if (legend->property("ka_interacting").toBool()) {
    flowTimer->start();
    return;
  }
  flowTimer->stop();
  const double width = legend->layout()->convertFromLayoutUnits(
      legend->rect().width(), Qgis::LayoutUnit::Millimeters).length();
  if (width <= 0.) return;
  legend->setCustomProperty(widthKey, legend->rect().width());
  legend->setResizeToContents(false);
  legend->setSplitLayer(true);
  legend->setEqualColumnWidth(true);
  // QGIS 기본 안쪽 여백은 2mm×2. 번호 원과 짧은 이름이 왼쪽 정렬이면 오른쪽이
  // 비어 보인다. 칸을 늘리거나 줄일 때도 이 여백이 그대로 남는다.
  legend->setBoxSpace(1.0);
  legend->setColumnSpace(1.5);
  legend->setStyleMargin(Qgis::LegendComponent::SymbolLabel, QgsLegendStyle::Left, 1.0);
  legend->setStyleMargin(Qgis::LegendComponent::SymbolLabel, QgsLegendStyle::Right, 0.4);
  auto settings = legend->legendSettings();
  const double points = qMax(5., legend->styleFont(Qgis::LegendComponent::SymbolLabel).pointSizeF());
  // 예전 55mm@9pt(7pt 바닥)는 70–100mm 칸에서도 한 열만 써서 오른쪽이 비었다.
  // 번호 원(~5mm)과 짧은 이름에 맞춘 열 폭. 긴 이름은 줄바꿈한다.
  const double minColumn = 32. * points / 9.;
  const double available = qMax(1., width - 2. * settings.boxSpace());
  int rows = 0;
  for (auto* node : legend->model()->rootGroup()->findLayers())
    rows += legend->model()->layerLegendNodes(node).size();
  int columns = qBound(1, static_cast<int>((available + settings.columnSpace()) /
      (minColumn + settings.columnSpace())), qMax(1, rows));
  auto context = QgsLayoutUtils::createRenderContextForLayout(legend->layout(), nullptr);
  QSizeF measured;
  double wrap = 0.;
  do {
    settings.setColumnCount(columns);
    const double columnWidth = (available - (columns - 1) * settings.columnSpace()) / columns;
    wrap = qMax(1., columnWidth - settings.symbolSize().width() - 1.5);
    settings.setAutoWrapLinesAfter(wrap);
    QgsLegendRenderer renderer(legend->model(), settings);
    measured = renderer.minimumSize(&context);
    if (measured.width() <= width + .1 || columns == 1) break;
    --columns;
  } while (true);
  legend->setColumnCount(columns);
  legend->setAutoWrapLinesAfter(wrap);
  // QGIS measures the same numbered symbols and wrapped text used for export.
  // Keep the chosen horizontal size and anchor while fitting the height.
  const QPointF position = legend->pos();
  legend->attemptResize(QgsLayoutSize(width, measured.height() + .5, Qgis::LayoutUnit::Millimeters));
  legend->setPos(position);
  legend->update();
}

void LayoutService::settleSheetLegendsForExport(QgsLayout* layout) {
  if (!layout) return;
  QList<QgsLayoutItemLegend*> legends;
  layout->layoutItems(legends);
  const double dpi = qMax(1., layout->renderContext().dpi());
  const double pixelsPerUnit = dpi / 25.4 *
      layout->convertFromLayoutUnits(1., Qgis::LayoutUnit::Millimeters).length();
  for (auto* legend : legends) {
    if (!legend->isVisible() || legend->excludeFromExports()) continue;
    // Public legend paint initializes map scale and runs its pending filter.
    // Paint only the legend into scratch storage; do not render the map twice.
    QImage scratch(8, 8, QImage::Format_ARGB32_Premultiplied);
    scratch.fill(Qt::transparent);
    scratch.setDotsPerMeterX(qRound(dpi / .0254));
    scratch.setDotsPerMeterY(qRound(dpi / .0254));
    QPainter painter(&scratch);
    painter.scale(pixelsPerUnit, pixelsPerUnit);
    QStyleOptionGraphicsItem option;
    option.exposedRect = legend->rect();
    legend->invalidateCache();
    legend->paint(&painter, &option, nullptr);
    painter.end();
    legend->model()->waitForHitTestBlocking();
    flowSheetLegend(legend);
  }
}

void LayoutService::tuneSheetLegend(QgsLayoutItemLegend* legend) {
  if (!legend) return;
  // Map filtering is decided in HeritageLayoutNumbers::applyLegend.
  // Turning it on here starts an async hit test that replaces legend rows
  // after the sheet has already been numbered.

  QgsProject* proj = legend->layout() ? legend->layout()->project() : nullptr;
  if (proj) {
    for (QgsMapLayer* ml : proj->mapLayers()) {
      if (!ml) continue;
      if (auto* vl = qobject_cast<QgsVectorLayer*>(ml))
        GeologyMapService::pruneStructureLegend(vl);
      if (!omitSheetLegendLayer(ml, ml->name())) continue;
      ml->setCustomProperty(QStringLiteral("ka_hgis/omit_sheet_legend"), true);
      if (QgsMapLayerLegend* lg = ml->legend())
        lg->setFlag(Qgis::MapLayerLegendFlag::ExcludeByDefault, true);
    }
  }

  // Manual clone of the linked map layer set (체크된 레이어). Do not strip the project tree.
  legend->setSyncMode(Qgis::LegendSyncMode::Manual);
  legend->resetManualLayers(legend->linkedMap() ? Qgis::LegendSyncMode::VisibleLayers
                                                : Qgis::LegendSyncMode::AllProjectLayers);
  legend->updateLegend();

  QgsLegendModel* model = legend->model();
  if (!model || !model->rootGroup()) return;
  QgsLayerTree* root = model->rootGroup();
  addMissingSheetLegendLayers(legend, root);

  QList<QgsLayerTreeLayer*> toRemove;
  for (QgsLayerTreeLayer* ll : root->findLayers()) {
    if (!ll) continue;
    QgsMapLayer* ml = ll->layer();
    const QString name = ml ? ml->name() : ll->name();
    if (omitSheetLegendLayer(ml, name) || !projectLayerChecked(ml) ||
        !onLinkedMap(legend, ml))
      toRemove.append(ll);
  }
  for (QgsLayerTreeLayer* ll : toRemove) {
    QgsLayerTreeNode* parent = ll->parent();
    if (parent && QgsLayerTree::isGroup(parent))
      QgsLayerTree::toGroup(parent)->removeChildNode(ll);
  }
  for (QgsLayerTreeLayer* ll : root->findLayers()) {
    if (ll->layer() && dynamic_cast<DemColorRampLegend*>(ll->layer()->legend())) {
      // Keep the chosen width: adjustBoxSize() cannot measure until the first
      // paint, so the old 1 mm reset became a clipped, vertically wrapped strip.
      legend->setResizeToContents(false);
      const double width = legend->layout()->convertFromLayoutUnits(
          legend->rect().width(), Qgis::LayoutUnit::Millimeters).length();
      if (width <= 1.1) {
        // Repair only the old collapsed size; ordinary user widths survive.
        const QPointF position = legend->pos();
        legend->attemptResize(QgsLayoutSize(55., 40., Qgis::LayoutUnit::Millimeters));
        legend->setPos(position);
      }
      flowSheetLegend(legend);
      break;
    }
  }
}

QString LayoutService::sheetLegendLabelDump(QgsLayoutItemLegend* legend) {
  if (!legend || !legend->model() || !legend->model()->rootGroup()) return {};
  QgsLegendModel* model = legend->model();
  QStringList labels;
  for (QgsLayerTreeLayer* ll : model->rootGroup()->findLayers()) {
    if (!ll) continue;
    if (!ll->name().isEmpty()) labels << ll->name();
    if (ll->layer() && ll->layer()->name() != ll->name()) labels << ll->layer()->name();
    const QList<QgsLayerTreeModelLegendNode*> nodes = model->layerLegendNodes(ll);
    for (QgsLayerTreeModelLegendNode* node : nodes) {
      if (!node) continue;
      const QString t = node->data(Qt::DisplayRole).toString().trimmed();
      if (!t.isEmpty()) labels << t;
    }
  }
  return labels.join(QLatin1Char('\n'));
}
