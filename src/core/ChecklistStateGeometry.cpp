#include "ChecklistStateChecks.h"
#include "LayerFeatures.h"
#include "LayerOps.h"

#include <cmath>
#include <memory>

#include <qgsabstractgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsgeometryengine.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgswkbtypes.h>

namespace ChecklistStateChecks {
namespace {
constexpr int kMaxOffendersPerKey = 200;

bool transformTo(QgsGeometry& g, const QgsCoordinateReferenceSystem& from,
                 const QgsCoordinateReferenceSystem& to, QgsProject* project) {
  if (!from.isValid() || !to.isValid() || from == to) return true;
  try {
    QgsCoordinateTransform ct(from, to, project->transformContext());
    return g.transform(ct) == Qgis::GeometryOperationResult::Success;
  } catch (const QgsCsException&) {
    return false;
  }
}

bool isCircleLike(const QgsPolylineXY& ring) {
  // A buffered point: many vertices, all at the same distance from the centre.
  const int n = ring.size() - (ring.size() > 1 && ring.first() == ring.last() ? 1 : 0);
  if (n < 12) return false;
  double cx = 0, cy = 0;
  for (int i = 0; i < n; ++i) { cx += ring[i].x(); cy += ring[i].y(); }
  cx /= n; cy /= n;
  double mean = 0;
  for (int i = 0; i < n; ++i) mean += std::hypot(ring[i].x() - cx, ring[i].y() - cy);
  mean /= n;
  if (!(mean > 0)) return false;
  for (int i = 0; i < n; ++i) {
    if (std::abs(std::hypot(ring[i].x() - cx, ring[i].y() - cy) - mean) / mean > 0.02) return false;
  }
  return true;
}

int distinctVertices(const QgsPolylineXY& ring) {
  QgsPolylineXY pts;
  for (const QgsPointXY& p : ring) {
    if (pts.isEmpty() || !(pts.last() == p)) pts.append(p);
  }
  if (pts.size() > 1 && pts.first() == pts.last()) pts.removeLast();
  return pts.size();
}
}  // namespace

void Offenders::add(const QString& key, const QgsVectorLayer* layer, QgsFeatureId fid,
                    const QString& label) {
  int& n = m_counts[key];
  ++n;
  if (n > kMaxOffendersPerKey) return;
  QJsonObject o;
  o.insert(QStringLiteral("layer_id"), layer ? layer->id() : QString());
  o.insert(QStringLiteral("layer_name"), layer ? layer->name() : QString());
  o.insert(QStringLiteral("fid"), static_cast<double>(fid));
  o.insert(QStringLiteral("label"), label);
  m_items[key].append(o);
}

void Offenders::note(const QString& key, const QString& text) { m_notes[key] = text; }

void Offenders::writeTo(QJsonObject& state) const {
  QJsonObject items;
  for (auto it = m_items.cbegin(); it != m_items.cend(); ++it) items.insert(it.key(), it.value());
  QJsonObject notes;
  for (auto it = m_notes.cbegin(); it != m_notes.cend(); ++it) notes.insert(it.key(), it.value());
  // Counts beyond the stored list are still reported in the note.
  for (auto it = m_counts.cbegin(); it != m_counts.cend(); ++it) {
    if (it.value() > kMaxOffendersPerKey && !notes.contains(it.key()))
      notes.insert(it.key(), QStringLiteral("%1건 중 앞 %2건만 목록에 보입니다.")
                                 .arg(it.value()).arg(kMaxOffendersPerKey));
  }
  state.insert(QStringLiteral("offenders"), items);
  state.insert(QStringLiteral("notes"), notes);
}

QString featureLabel(const QgsFeature& feature) {
  for (const char* field : {"point_id", "feature_no", "section_id", "artifact_no", "survey_name", "name"}) {
    const int index = feature.fields().lookupField(QString::fromLatin1(field));
    if (index < 0) continue;
    const QString text = feature.attribute(index).toString().trimmed();
    if (!text.isEmpty()) return text;
  }
  return QStringLiteral("#%1").arg(feature.id());
}

bool isAbstractMarker(const QgsGeometry& geometry) {
  if (geometry.isNull() || geometry.isEmpty()) return false;
  const Qgis::GeometryType type = QgsWkbTypes::geometryType(geometry.wkbType());
  if (type == Qgis::GeometryType::Point) return true;
  if (type != Qgis::GeometryType::Polygon) return false;
  const QgsMultiPolygonXY parts = geometry.isMultipart()
                                      ? geometry.asMultiPolygon()
                                      : QgsMultiPolygonXY{geometry.asPolygon()};
  for (const QgsPolygonXY& polygon : parts) {
    if (polygon.isEmpty()) continue;
    const QgsPolylineXY& ring = polygon.first();
    if (distinctVertices(ring) <= 3 || isCircleLike(ring)) return true;
  }
  return false;
}

void Extent::add(const QgsRectangle& box, const QgsCoordinateReferenceSystem& boxCrs, QgsProject* project) {
  if (box.isNull() || !box.isFinite()) return;
  if (!crs.isValid()) crs = boxCrs;
  QgsRectangle mapped = box;
  if (boxCrs.isValid() && crs.isValid() && boxCrs != crs) {
    try {
      QgsCoordinateTransform ct(boxCrs, crs, project->transformContext());
      mapped = ct.transformBoundingBox(box);
    } catch (const QgsCsException&) {
      return;
    }
  }
  if (rect.isNull()) rect = mapped;
  else rect.combineExtentWith(mapped);
}

DomainExtents scanGeometry(QgsProject* project, QJsonObject& st, Offenders& off) {
  DomainExtents out;
  bool geosValid = true, noEmpty = true, noZeroArea = true, surveyAllPoly = true;
  bool marker = false, realGeometry = true, exportCrsOk = true, within = true;
  QgsCoordinateReferenceSystem common = project->crs();
  QVector<QgsGeometry> surveyGeoms;
  struct Placed { const QgsVectorLayer* layer; QgsFeatureId fid; QString label; QgsGeometry g; };
  QVector<Placed> placed;

  for (const QString& key : LayerOps::domainLayerKeys()) {
    for (QgsVectorLayer* layer : LayerOps::domainLayersForKey(project, key)) {
      if (!LayerFeatures::any(layer)) continue;
      if (!layer->crs().isValid()) {
        exportCrsOk = false;
        off.add(QStringLiteral("export_crs_valid"), layer, -1, layer->name());
      }
      if (!common.isValid()) common = layer->crs();
      QgsFeatureIterator it = layer->getFeatures();
      QgsFeature f;
      while (it.nextFeature(f)) {
        const QgsGeometry g = f.geometry();
        const QString label = featureLabel(f);
        if (g.isNull() || g.isEmpty()) {
          noEmpty = false;
          off.add(QStringLiteral("geometries_nonempty"), layer, f.id(), label);
          continue;
        }
        const Qgis::GeometryType type = QgsWkbTypes::geometryType(g.wkbType());
        if (!g.isGeosValid()) {
          geosValid = false;
          off.add(QStringLiteral("geometries_valid"), layer, f.id(), label);
        }
        if (type == Qgis::GeometryType::Polygon && !(g.area() > 0.0)) {
          noZeroArea = false;
          off.add(QStringLiteral("geometries_nonzero_area"), layer, f.id(), label);
        }
        QgsGeometry inCommon = g;
        const bool mapped = transformTo(inCommon, layer->crs(), common, project);
        if (key == QLatin1String("survey_area")) {
          if (type != Qgis::GeometryType::Polygon) {
            surveyAllPoly = false;
            off.add(QStringLiteral("survey_is_polygon"), layer, f.id(), label);
          }
          if (isAbstractMarker(g)) {
            marker = true;
            off.add(QStringLiteral("has_abstract_marker"), layer, f.id(), label);
          }
          out.survey.add(g.boundingBox(), layer->crs(), project);
          if (type == Qgis::GeometryType::Polygon && mapped) surveyGeoms.append(inCommon);
          continue;
        }
        const bool poly = key == QLatin1String("feature_poly");
        const bool line = key == QLatin1String("feature_line");
        if ((poly && type != Qgis::GeometryType::Polygon) || (line && type != Qgis::GeometryType::Line)) {
          realGeometry = false;
          off.add(QStringLiteral("features_real_geometry"), layer, f.id(), label);
        }
        if (poly || line) out.features.add(g.boundingBox(), layer->crs(), project);
        if ((poly || line || key == QLatin1String("artifact_point")) && mapped)
          placed.append({layer, f.id(), label, inCommon});
      }
    }
  }

  // Features must lie inside the survey polygons, not merely touch their bounding box.
  if (!surveyGeoms.isEmpty() && !placed.isEmpty()) {
    const double tolerance = common.isGeographic() ? 0.5 / 111320.0 : 0.5;
    const QgsGeometry area = QgsGeometry::unaryUnion(surveyGeoms).buffer(tolerance, 4);
    std::unique_ptr<QgsGeometryEngine> engine(
        area.isNull() ? nullptr : QgsGeometry::createGeometryEngine(area.constGet()));
    if (engine) {
      engine->prepareGeometry();
      for (const Placed& p : placed) {
        if (!engine->contains(p.g.constGet())) {
          within = false;
          off.add(QStringLiteral("features_within_survey"), p.layer, p.fid, p.label);
        }
      }
    }
  }

  st.insert(QStringLiteral("geometries_valid"), geosValid);
  st.insert(QStringLiteral("geometries_nonempty"), noEmpty);
  st.insert(QStringLiteral("geometries_nonzero_area"), noZeroArea);
  st.insert(QStringLiteral("survey_is_polygon"), surveyAllPoly);
  st.insert(QStringLiteral("has_abstract_marker"), marker);
  st.insert(QStringLiteral("features_real_geometry"), realGeometry);
  st.insert(QStringLiteral("features_within_survey"), within);
  st.insert(QStringLiteral("export_crs_valid"), exportCrsOk);
  return out;
}

}  // namespace ChecklistStateChecks
