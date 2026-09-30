#include "LayerRole.h"

#include "LayerOps.h"

#include <qgslayertree.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>

namespace {
QString roleText(const QgsMapLayer* layer) {
  return layer->customProperty(QString::fromUtf8(LayerOps::kPropLayerRole)).toString();
}
}  // namespace

LayerRole::Kind LayerRole::stored(const QgsMapLayer* layer) {
  if (!layer) return Kind::Unknown;
  // The explicit cadastral flag predates ka_hgis/layer_role and still wins.
  if (layer->customProperty(QStringLiteral("ka_hgis/cadastral")).toBool()) return Kind::Cadastral;
  const QString role = roleText(layer);
  if (role == QLatin1String(LayerOps::kRoleCadastral)) return Kind::Cadastral;
  if (role == QLatin1String(LayerOps::kRoleReference)) return Kind::Reference;
  if (role == QLatin1String(LayerOps::kRoleSurvey)) return Kind::Survey;
  return Kind::Unknown;
}

LayerRole::Kind LayerRole::resolve(const QgsMapLayer* layer) {
  if (!layer) return Kind::Unknown;
  const Kind kind = stored(layer);
  if (kind != Kind::Unknown) return kind;
  // Domain keys and imported "user:" keys are survey data whatever the title says.
  if (!LayerOps::layerKeyOf(layer).isEmpty()) return Kind::Survey;
  return legacyGuess(layer);
}

LayerRole::Kind LayerRole::legacyGuess(const QgsMapLayer* layer) {
  if (!layer) return Kind::Unknown;
  const QString n = layer->name();
  // Downloaded cadastral SHP/GPKG saved before the flag existed. Live tiles
  // (VWorld 지적 WMS) are pictures, never snap sources.
  if (!LayerOps::isBasemapLayer(layer) && !n.contains(QStringLiteral("VWorld")) &&
      n.contains(QStringLiteral("지적")) &&
      layer->providerType().compare(QLatin1String("ogr"), Qt::CaseInsensitive) == 0)
    return Kind::Cadastral;
  // The former `== "위성"` / `startsWith("지적")` tests compared Latin-1 bytes and never
  // matched a Korean title; they are not revived (titles are labels, F020).
  if (n.contains(QStringLiteral("OSM")) || n.contains(QStringLiteral("VWorld")) ||
      n.contains(QStringLiteral("Carto")) || n.contains(QStringLiteral("Google")) ||
      n.contains(QStringLiteral("고도맵")) || n.contains(QStringLiteral("지형맵")) ||
      n == QLatin1String("DEM") || n.contains(QStringLiteral("OpenTopoMap")) ||
      n.contains(QStringLiteral("고지형")) || n.contains(QStringLiteral("대동여지도")) ||
      n.contains(QStringLiteral("1919 조선지형도")))
    return Kind::Reference;
  return Kind::Unknown;
}

bool LayerRole::legacyLooksLikeBackground(const QgsMapLayer* layer) {
  if (!layer) return false;
  QgsProject* project = layer->project() ? layer->project() : QgsProject::instance();
  if (QgsLayerTree* root = project ? project->layerTreeRoot() : nullptr) {
    if (QgsLayerTreeLayer* node = root->findLayer(layer->id())) {
      for (QgsLayerTreeNode* parent = node->parent(); parent; parent = parent->parent()) {
        if (parent->name() == QString::fromUtf8(LayerOps::kGroupReference) ||
            parent->name().contains(QStringLiteral("참조")))
          return true;
      }
    }
  }
  const QString n = layer->name();
  static const char* const kThematic[] = {"지질", "토양", "수계", "음영", "단면", "배경",
                                          "정사", "위성", "지적", "DEM", "지형", "등고"};
  for (const char* word : kThematic) {
    if (n.contains(QString::fromUtf8(word))) return true;
  }
  // Before roles were stored, every key-less raster was a background picture.
  return layer->type() == Qgis::LayerType::Raster;
}

int LayerRole::persistLegacyRoles(QgsProject* project) {
  if (!project) return 0;
  const bool wasDirty = project->isDirty();
  int written = 0;
  const auto layers = project->mapLayers();
  for (QgsMapLayer* layer : layers) {
    if (!layer || stored(layer) != Kind::Unknown) continue;
    switch (resolve(layer)) {
      case Kind::Survey:
        layer->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerRole),
                                 QString::fromUtf8(LayerOps::kRoleSurvey));
        ++written;
        break;
      case Kind::Cadastral:
        LayerOps::markCadastralLayer(layer);
        ++written;
        break;
      case Kind::Reference:
        LayerOps::markReferenceLayer(layer);
        ++written;
        break;
      case Kind::Unknown:
        break;
    }
  }
  if (!wasDirty && project->isDirty()) project->setDirty(false);
  return written;
}
