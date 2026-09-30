#pragma once
// Project-state probes behind ProjectStateBuilder. Each probe writes plain
// state keys that ChecklistEngine reads, and records the offending features
// so the review dialog can move the map to them.
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <qgscoordinatereferencesystem.h>
#include <qgsfeatureid.h>
#include <qgsrectangle.h>

class QgsFeature;
class QgsGeometry;
class QgsProject;
class QgsVectorLayer;

namespace ChecklistStateChecks {

// state["offenders"][key] = [{layer_id, layer_name, fid, label}], state["notes"][key] = text.
class Offenders {
public:
  void add(const QString& key, const QgsVectorLayer* layer, QgsFeatureId fid, const QString& label);
  void note(const QString& key, const QString& text);
  int count(const QString& key) const { return m_counts.value(key); }
  void writeTo(QJsonObject& state) const;

private:
  QHash<QString, QJsonArray> m_items;
  QHash<QString, int> m_counts;
  QHash<QString, QString> m_notes;
};

// point_id, feature_no, section_id … or "#fid" when the row has no name.
QString featureLabel(const QgsFeature& feature);
// Points and triangle- or circle-shaped polygons are symbols, not surveyed boundaries.
bool isAbstractMarker(const QgsGeometry& geometry);

// Union of bounding boxes in one CRS.
struct Extent {
  QgsRectangle rect;
  QgsCoordinateReferenceSystem crs;
  bool isValid() const { return crs.isValid() && !rect.isNull() && rect.isFinite(); }
  void add(const QgsRectangle& box, const QgsCoordinateReferenceSystem& boxCrs, QgsProject* project);
};

struct DomainExtents {
  Extent survey;    // survey_area geometries
  Extent features;  // feature_poly + feature_line geometries
};

// Geometry quality, survey shape, feature kinds, extent match and export CRS.
DomainExtents scanGeometry(QgsProject* project, QJsonObject& state, Offenders& offenders);
// Control-point meta (every point), kind/period, required fields, unique point_id.
void scanFields(QgsProject* project, QJsonObject& state, Offenders& offenders);

// How a composed layout shows a target extent.
struct SheetView {
  bool shows = false;     // a map with real (non-reference) data overlaps the target
  bool covers = false;    // that map contains the whole target
  double minScale = 0.0;  // smallest denominator among the overlapping maps
};
SheetView sheetView(QgsProject* project, const QString& layoutName, const Extent& target);
// Missing sheet items in Korean ("방위", "축척", "범례"). A title stays optional.
QStringList missingSheetElements(QgsProject* project, const QString& layoutName);
// Named layout (not the studio sheet) the user composed, with real data on a map.
bool isNamedLayoutComposed(QgsProject* project, const QString& name);

}  // namespace ChecklistStateChecks
