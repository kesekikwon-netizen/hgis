#pragma once

// Names what the one-click elevation layer really is (evaluation F062).
// The DEM button downloads Copernicus GLO-30: a 30 m surface model (DSM) that includes
// tree canopy and buildings. Calling it only "DEM" invites reading ridges, terraces and
// old channels from canopy height, so the legend row and its tooltip say so.
// Colour ramp and relief (user decisions) are not touched here.
// The layer keeps ka_hgis/reference_kind = dem, so lookups never depend on the title.

#include "ReferenceKind.h"

#include <QString>

#include <qgsmaplayer.h>
#include <qgsmaplayerserverproperties.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>

namespace BasemapDsm {

inline QString copernicusTitle() { return QStringLiteral("지표모델(DSM) 30m · Copernicus"); }

inline QString copernicusTooltip() {
  return QStringLiteral(
      "Copernicus GLO-30 지표모델(DSM), 격자 약 30 m. 나무·건물 높이가 섞여 있어 능선·단구·"
      "옛 물길 같은 미세지형 판독에는 맞지 않습니다. 정밀 판독에는 국토지리정보원 DEM을 "
      "불러오세요.");
}

// Title, tooltip and identity for a Copernicus DSM raster the app added.
inline void labelCopernicus(QgsMapLayer* layer) {
  if (!layer) return;
  ReferenceKind::tag(layer, QString::fromLatin1(ReferenceKind::kDem));
  QString tooltip = copernicusTooltip();
  if (layer->crs().isValid())
    tooltip += QStringLiteral("\n좌표계 %1").arg(layer->crs().authid());
  if (QgsMapLayerServerProperties* server = layer->serverProperties())
    server->setAbstract(tooltip);
  if (layer->name() != copernicusTitle()) layer->setName(copernicusTitle());
}

// The elevation layer behind the DEM button, whatever its title: tagged dem, or an
// untagged layer titled "DEM" from an older project. Use this instead of name == "DEM".
inline bool isDemLayer(const QgsMapLayer* layer) {
  return ReferenceKind::is(layer, QString::fromLatin1(ReferenceKind::kDem));
}

inline QgsRasterLayer* findDem(const QgsProject* project) {
  QgsRasterLayer* fallback = nullptr;
  for (QgsMapLayer* l : ReferenceKind::find(project, QString::fromLatin1(ReferenceKind::kDem))) {
    auto* raster = qobject_cast<QgsRasterLayer*>(l);
    if (!raster) continue;
    if (raster->isValid()) return raster;
    if (!fallback) fallback = raster;
  }
  return fallback;
}

}  // namespace BasemapDsm
