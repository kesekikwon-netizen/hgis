#include "ChecklistStateChecks.h"
#include "LayerFeatures.h"
#include "LayerOps.h"

#include <cmath>

#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutmanager.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>

namespace ChecklistStateChecks {
namespace {
QgsPrintLayout* layoutNamed(QgsProject* project, const QString& name) {
  if (!project || !project->layoutManager()) return nullptr;
  return dynamic_cast<QgsPrintLayout*>(project->layoutManager()->layoutByName(name));
}

bool usableExtent(const QgsRectangle& ext) {
  return ext.isFinite() && !ext.isEmpty() && ext.width() > 0.0 && ext.height() > 0.0;
}

// A map counts when it has a valid scale, a finite extent and at least one
// survey (non-reference) layer with data. Placeholder layers never count.
bool mapHasRealData(QgsLayoutItemMap* map) {
  if (!map || !(map->scale() > 0.0) || !std::isfinite(map->scale())) return false;
  if (!usableExtent(map->extent())) return false;
  for (QgsMapLayer* l : map->layers()) {
    if (!l || !l->isValid()) continue;
    if (l->name() == QLatin1String("layout_blank") || l->name() == QLatin1String("ka_section_blank"))
      continue;
    if (LayerOps::isReferenceLayer(l) || LayerOps::isCadastralLayer(l) || LayerOps::isBasemapLayer(l))
      continue;
    if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
      if (LayerFeatures::any(vl)) return true;
    } else if (auto* rl = qobject_cast<QgsRasterLayer*>(l)) {
      if (rl->width() > 0 && rl->height() > 0 && !rl->extent().isEmpty()) return true;
    }
  }
  return false;
}
}  // namespace

SheetView sheetView(QgsProject* project, const QString& layoutName, const Extent& target) {
  SheetView view;
  QgsPrintLayout* ly = layoutNamed(project, layoutName);
  if (!ly || !target.isValid()) return view;
  QList<QgsLayoutItemMap*> maps;
  ly->layoutItems(maps);
  // Only maps that hold survey data are views of the sheet: the studio's ka_map and
  // its overlay copies (ka_map_above, ka_map_numbers) share one frame; an inset that
  // shows a basemap alone must not stand in for the drawing.
  for (QgsLayoutItemMap* map : maps) {
    if (!mapHasRealData(map)) continue;
    const QgsCoordinateReferenceSystem mapCrs = map->crs().isValid() ? map->crs() : project->crs();
    QgsRectangle inMap = target.rect;
    if (mapCrs.isValid() && mapCrs != target.crs) {
      try {
        QgsCoordinateTransform ct(target.crs, mapCrs, project->transformContext());
        inMap = ct.transformBoundingBox(target.rect);
      } catch (const QgsCsException&) {
        continue;
      }
    }
    const QgsRectangle ext = map->extent();
    // A point-like target has an empty box; test it as a point.
    const bool overlaps = inMap.isEmpty() ? ext.contains(inMap.center()) : ext.intersects(inMap);
    if (!overlaps) continue;
    view.shows = true;
    // Half a percent of the frame absorbs rounding from zoomToExtent.
    QgsRectangle loose = ext;
    loose.grow(0.005 * std::max(ext.width(), ext.height()));
    if (loose.contains(inMap)) view.covers = true;
    if (view.minScale <= 0.0 || map->scale() < view.minScale) view.minScale = map->scale();
  }
  return view;
}

QStringList missingSheetElements(QgsProject* project, const QString& layoutName) {
  QgsPrintLayout* ly = layoutNamed(project, layoutName);
  if (!ly) return {};
  bool north = false, scale = false, legend = false;
  QList<QgsLayoutItem*> items;
  ly->layoutItems(items);
  for (QgsLayoutItem* item : items) {
    if (!item) continue;
    const QString id = item->id();
    if (id == QLatin1String("ka_north")) north = true;
    if (auto* picture = dynamic_cast<QgsLayoutItemPicture*>(item)) {
      if (picture->linkedMap() || picture->picturePath().contains(QLatin1String("north"), Qt::CaseInsensitive))
        north = true;
    }
    if (dynamic_cast<QgsLayoutItemScaleBar*>(item) || id == QLatin1String("ka_scale")) scale = true;
    if (dynamic_cast<QgsLayoutItemLegend*>(item)) legend = true;
  }
  QStringList missing;
  if (!north) missing << QStringLiteral("방위");
  if (!scale) missing << QStringLiteral("축척");
  if (!legend) missing << QStringLiteral("범례");
  return missing;
}

bool isNamedLayoutComposed(QgsProject* project, const QString& name) {
  QgsPrintLayout* ly = layoutNamed(project, name);
  if (!ly) return false;
  // Auto templates the user never composed do not pass.
  if (ly->customProperty(QStringLiteral("ka_hgis/auto_template")).toBool() &&
      !ly->customProperty(QStringLiteral("ka_hgis/user_composed")).toBool())
    return false;
  if (ly->itemById(QStringLiteral("empty_hint"))) return false;
  QList<QgsLayoutItemMap*> maps;
  ly->layoutItems(maps);
  for (QgsLayoutItemMap* map : maps) {
    if (mapHasRealData(map)) return true;
  }
  return false;
}

}  // namespace ChecklistStateChecks
