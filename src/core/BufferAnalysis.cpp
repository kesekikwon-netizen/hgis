#include "KaSessionLog.h"
#include "KaLogExcept.h"
#include "BufferAnalysis.h"
#include "LayerOps.h"

#include <QColor>
#include <QFont>
#include <QVector>
#include <cmath>
#include <memory>
#include <vector>

#include <qgis.h>
#include <qgsproject.h>
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgscoordinatetransform.h>
#include <qgspolygon.h>
#include <qgslinestring.h>
#include <qgscurve.h>
#include <qgspoint.h>
#include <qgspointxy.h>
#include <qgsmaplayer.h>
#include <qgsrectangle.h>
#include <qgssymbol.h>
#include <qgslinesymbol.h>
#include <qgsmarkersymbol.h>
#include <qgssinglesymbolrenderer.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgstextbuffersettings.h>
#include <qgslabelobstaclesettings.h>

namespace BufferAnalysis {
namespace {

QString ringKey(double meters) {
  return QStringLiteral("user:buffer_ring_%1").arg(qRound(meters));
}
QString labelKey(double meters) {
  return QStringLiteral("user:buffer_label_%1").arg(qRound(meters));
}

void removeOld(QgsProject* project, const QString& key) {
  if (QgsMapLayer* old = LayerOps::findByLayerKey(project, key))
    project->removeMapLayer(old->id());
}

QgsGeometry unionInCrs(QgsVectorLayer* source, const QgsCoordinateReferenceSystem& dest,
                       QString* errorOut) {
  QgsGeometry acc;
  QgsCoordinateTransform xf;
  const bool needXf = source->crs().isValid() && dest.isValid() && source->crs() != dest;
  if (needXf) {
    xf = QgsCoordinateTransform(source->crs(), dest, QgsProject::instance()
                                                         ? QgsProject::instance()->transformContext()
                                                         : QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
  }
  QgsFeatureIterator it = source->getFeatures();
  QgsFeature f;
  QVector<QgsGeometry> parts;
  while (it.nextFeature(f)) {
    QgsGeometry g = f.geometry();
    if (g.isEmpty()) continue;
    if (needXf) {
      try {
        if (g.transform(xf) != Qgis::GeometryOperationResult::Success) continue;
      } catch (...) {
        KA_LOG_EXCEPT();
        continue;
      }
    }
    parts << g;
  }
  // One GEOS union of every feature instead of pairwise combine().
  if (!parts.isEmpty()) acc = QgsGeometry::unaryUnion(parts);
  if (parts.isEmpty() || acc.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("선택한 레이어에 도형이 없습니다");
    return {};
  }
  return acc;
}

// All polygon parts of g (a (multi)polygon or a mixed collection) as one geometry.
QgsGeometry polygonsOnly(const QgsGeometry& g) {
  if (g.isEmpty() || g.type() == Qgis::GeometryType::Polygon) return g;
  QVector<QgsGeometry> polys;
  for (const QgsGeometry& part : g.asGeometryCollection()) {
    if (part.type() == Qgis::GeometryType::Polygon) polys << part;
  }
  return polys.isEmpty() ? QgsGeometry() : QgsGeometry::collectGeometry(polys);
}

std::unique_ptr<QgsLineString> exteriorLine(const QgsGeometry& polygonPart) {
  const QgsPolygon* pg = qgsgeometry_cast<const QgsPolygon*>(polygonPart.constGet());
  if (!pg || !pg->exteriorRing()) return {};
  std::unique_ptr<QgsCurve> cloned(pg->exteriorRing()->clone());
  if (QgsLineString* asLs = qgsgeometry_cast<QgsLineString*>(cloned.get())) {
    cloned.release();
    return std::unique_ptr<QgsLineString>(asLs);
  }
  return std::unique_ptr<QgsLineString>(cloned ? cloned->curveToLine() : nullptr);
}

void applyRingStyle(QgsVectorLayer* layer) {
  // Solid by default. Dash is a 도형 색 checkbox, not a geometry gap.
  LayerOps::applySimpleVectorStyle(layer, QColor(0, 0, 0, 0), QColor(0xC2, 0x41, 0x0C), 1.15, 3.5,
                                   true, false, false);
}

QVector<double> labelDistances(const QgsLineString* ls) {
  QVector<double> cuts;
  if (!ls || ls->numPoints() < 2) return cuts;
  QgsGeometry g(ls->clone());
  const QgsRectangle b = g.boundingBox();
  const QgsPoint targets[] = {
      QgsPoint((b.xMinimum() + b.xMaximum()) * 0.5, b.yMaximum()),
      QgsPoint(b.xMaximum(), (b.yMinimum() + b.yMaximum()) * 0.5),
      QgsPoint((b.xMinimum() + b.xMaximum()) * 0.5, b.yMinimum()),
      QgsPoint(b.xMinimum(), (b.yMinimum() + b.yMaximum()) * 0.5),
  };
  for (const QgsPoint& t : targets) {
    const double d = g.lineLocatePoint(QgsGeometry::fromPointXY(QgsPointXY(t.x(), t.y())));
    if (d >= 0.0 && std::isfinite(d)) cuts.append(d);
  }
  if (cuts.isEmpty()) {
    const double len = ls->length();
    cuts << len * 0.125 << len * 0.375 << len * 0.625 << len * 0.875;
  }
  return cuts;
}

void applyGapLabel(QgsVectorLayer* layer) {
  auto ms = QgsMarkerSymbol::createSimple({
      {QStringLiteral("name"), QStringLiteral("circle")},
      {QStringLiteral("size"), QStringLiteral("0")},
      {QStringLiteral("color"), QStringLiteral("#00000000")},
      {QStringLiteral("outline_style"), QStringLiteral("no")},
  });
  layer->setRenderer(new QgsSingleSymbolRenderer(ms.release()));

  QgsPalLayerSettings s;
  s.drawLabels = true;
  s.fieldName = QStringLiteral("label");
  s.isExpression = false;
  s.placement = Qgis::LabelPlacement::OverPoint;
  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);
  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSize(10);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(10);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(194, 65, 12));
  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(1.2);
  buf.setColor(QColor(255, 255, 255, 240));
  fmt.setBuffer(buf);
  s.setFormat(fmt);
  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
}

}  // namespace

bool addDistanceRing(QgsProject* project, QgsMapCanvas* canvas, QgsVectorLayer* source,
                     double meters, QString* errorOut) {
  if (!project || !source || !source->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("면 레이어를 먼저 선택하세요");
    return false;
  }
  if (meters < 1) {
    if (errorOut) *errorOut = QStringLiteral("거리가 올바르지 않습니다");
    return false;
  }
  QgsCoordinateReferenceSystem dest = project->crs();
  if (!dest.isValid() && canvas) dest = canvas->mapSettings().destinationCrs();
  if (!dest.isValid()) dest = source->crs();
  if (dest.isGeographic()) {
    if (errorOut) *errorOut = QStringLiteral("미터 좌표계(5186/5187)에서만 거리를 그릴 수 있습니다");
    return false;
  }

  const QgsGeometry src = unionInCrs(source, dest, errorOut);
  if (src.isEmpty()) return false;

  // Every part of the survey area counts. Parts farther apart than twice the distance
  // keep their own outline; closer parts merge into one.
  QgsGeometry poly = polygonsOnly(src);
  if (poly.isEmpty()) {
    if (errorOut) {
      *errorOut = (source->geometryType() != Qgis::GeometryType::Polygon)
                      ? QStringLiteral("면(폴리곤) 레이어를 선택한 뒤 다시 누르세요")
                      : QStringLiteral("면 도형이 없습니다");
    }
    return false;
  }
  poly.removeDuplicateNodes(0.05);
  {
    const QgsGeometry valid = polygonsOnly(poly.makeValid());
    if (!valid.isEmpty()) poly = valid;
  }

  // Parallel offset of the site outline. Do not simplify — that collapsed small
  // sites into a diamond. Dash/gaps are style, not geometry.
  const QgsGeometry buf =
      poly.buffer(meters, 16, Qgis::EndCapStyle::Flat, Qgis::JoinStyle::Miter, 5.0);
  if (buf.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("주변 범위를 만들지 못했습니다");
    return false;
  }
  std::vector<std::unique_ptr<QgsLineString>> rings;
  for (const QgsGeometry& part : buf.asGeometryCollection()) {
    std::unique_ptr<QgsLineString> ls = exteriorLine(part);
    if (ls && ls->length() >= 20) rings.push_back(std::move(ls));
  }
  if (rings.empty()) {
    if (errorOut) *errorOut = QStringLiteral("범위가 너무 작거나 외곽선을 얻지 못했습니다");
    return false;
  }

  removeOld(project, ringKey(meters));
  removeOld(project, labelKey(meters));

  const QString title = QStringLiteral("주변 %1m").arg(qRound(meters));
  auto* ringLayer = new QgsVectorLayer(QStringLiteral("LineString?crs=%1").arg(dest.authid()),
                                       title, QStringLiteral("memory"));
  if (!ringLayer->isValid()) {
    delete ringLayer;
    if (errorOut) *errorOut = QStringLiteral("선 레이어를 만들지 못했습니다");
    return false;
  }
  ringLayer->dataProvider()->addAttributes({QgsField(QStringLiteral("note"), QMetaType::Type::QString)});
  ringLayer->updateFields();
  QgsFeatureList ringFeats;
  for (const auto& ls : rings) {
    QgsFeature rf(ringLayer->fields());
    rf.setGeometry(QgsGeometry(ls->clone()));
    rf.setAttribute(0, title);
    ringFeats << rf;
  }
  ringLayer->dataProvider()->addFeatures(ringFeats);
  applyRingStyle(ringLayer);
  LayerOps::markSurveyLayer(ringLayer, ringKey(meters));
  LayerOps::applyLegendCrsLabel(ringLayer);
  project->addMapLayer(ringLayer, true);
  LayerOps::placeInLegendGroup(project, ringLayer, QString::fromUtf8(LayerOps::kGroupSurveyData));

  auto* labLayer = new QgsVectorLayer(QStringLiteral("Point?crs=%1").arg(dest.authid()),
                                      QStringLiteral("%1 거리").arg(title), QStringLiteral("memory"));
  if (!labLayer->isValid()) {
    delete labLayer;
    if (errorOut) *errorOut = QStringLiteral("거리 글자 레이어를 만들지 못했습니다");
    return false;
  }
  labLayer->dataProvider()->addAttributes({QgsField(QStringLiteral("label"), QMetaType::Type::QString)});
  labLayer->updateFields();
  const QString labTxt = QStringLiteral("%1m").arg(qRound(meters));
  QgsFeatureList labFeats;
  for (const auto& ls : rings) {
    QVector<double> cuts = labelDistances(ls.get());
    if (cuts.isEmpty()) cuts.append(ls->length() * 0.25);
    for (double d : cuts) {
      std::unique_ptr<QgsPoint> pt(ls->interpolatePoint(d));
      if (!pt) continue;
      QgsFeature lf(labLayer->fields());
      lf.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(pt->x(), pt->y())));
      lf.setAttribute(0, labTxt);
      labFeats << lf;
    }
  }
  if (labFeats.isEmpty()) {
    delete labLayer;
    if (errorOut) *errorOut = QStringLiteral("동서남북 거리 글자를 넣지 못했습니다");
    return false;
  }
  labLayer->dataProvider()->addFeatures(labFeats);
  applyGapLabel(labLayer);
  LayerOps::markSurveyLayer(labLayer, labelKey(meters));
  LayerOps::applyLegendCrsLabel(labLayer);
  project->addMapLayer(labLayer, true);
  LayerOps::placeInLegendGroup(project, labLayer, QString::fromUtf8(LayerOps::kGroupSurveyData));

  if (canvas)
    LayerOps::syncMapCanvas(project, canvas, false);
  return true;
}

}  // namespace BufferAnalysis
