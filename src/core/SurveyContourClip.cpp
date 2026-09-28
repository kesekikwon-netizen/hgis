#include "SurveyContourClip.h"

#include <QPair>
#include <QString>
#include <QVector>

#include <ogr_geometry.h>
#include <ogrsf_frmts.h>

namespace {

void collectParts(OGRGeometry* geometry, QVector<OGRGeometry*>* parts) {
  if (!geometry || geometry->IsEmpty() || !parts) return;
  const OGRwkbGeometryType kind = wkbFlatten(geometry->getGeometryType());
  if (kind == wkbMultiLineString || kind == wkbMultiPolygon || kind == wkbGeometryCollection) {
    OGRGeometryCollection* collection = geometry->toGeometryCollection();
    if (!collection) return;
    for (int i = 0; i < collection->getNumGeometries(); ++i)
      collectParts(collection->getGeometryRef(i), parts);
    return;
  }
  if (kind != wkbLineString && kind != wkbPolygon) return;
  if (OGRGeometry* clone = geometry->clone()) parts->push_back(clone);
}

}  // namespace

int clipSurveyLayerToWkt(OGRLayer* layer, const QString& wkt) {
  if (!layer || wkt.isEmpty()) return layer ? static_cast<int>(layer->GetFeatureCount(TRUE)) : 0;
  const QByteArray bytes = wkt.toUtf8();
  const char* text = bytes.constData();
  OGRGeometry* mask = nullptr;
  if (OGRGeometryFactory::createFromWkt(&text, nullptr, &mask) != OGRERR_NONE || !mask)
    return static_cast<int>(layer->GetFeatureCount(TRUE));

  QVector<GIntBig> drop;
  QVector<QPair<GIntBig, QVector<OGRGeometry*>>> keep;
  layer->ResetReading();
  while (OGRFeature* feature = layer->GetNextFeature()) {
    OGRGeometry* geometry = feature->GetGeometryRef();
    OGRGeometry* cut = geometry ? geometry->Intersection(mask) : nullptr;
    QVector<OGRGeometry*> parts;
    collectParts(cut, &parts);
    if (cut) OGRGeometryFactory::destroyGeometry(cut);
    if (parts.isEmpty()) drop.push_back(feature->GetFID());
    else keep.push_back({feature->GetFID(), parts});
    OGRFeature::DestroyFeature(feature);
  }
  for (GIntBig fid : drop) layer->DeleteFeature(fid);
  for (auto& item : keep) {
    OGRFeature* feature = layer->GetFeature(item.first);
    if (!feature) {
      for (OGRGeometry* part : item.second) OGRGeometryFactory::destroyGeometry(part);
      continue;
    }
    feature->SetGeometry(item.second.first());
    layer->SetFeature(feature);
    for (int i = 1; i < item.second.size(); ++i) {
      OGRFeature* extra = feature->Clone();
      extra->SetFID(OGRNullFID);
      extra->SetGeometry(item.second.at(i));
      layer->CreateFeature(extra);
      OGRFeature::DestroyFeature(extra);
    }
    OGRFeature::DestroyFeature(feature);
    for (OGRGeometry* part : item.second) OGRGeometryFactory::destroyGeometry(part);
  }
  OGRGeometryFactory::destroyGeometry(mask);
  layer->SyncToDisk();
  return static_cast<int>(layer->GetFeatureCount(TRUE));
}
