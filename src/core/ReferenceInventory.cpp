#include "ReferenceInventory.h"
#include "BasemapPolicy.h"
#include "LayerOps.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <QUrl>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>

namespace ReferenceInventory {
namespace {

bool included(const QgsMapLayer* layer) {
  return layer && (LayerOps::isCadastralLayer(layer) || LayerOps::isReferenceOrBasemapLayer(layer));
}

QString kindOf(const QgsMapLayer* layer) {
  if (LayerOps::isCadastralLayer(layer)) return QStringLiteral("지적도");
  if (!layer->customProperty(QStringLiteral("ka_hgis/topographic_group")).toString().isEmpty() ||
      !layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().isEmpty())
    return QStringLiteral("수치지형도");
  if (LayerOps::isBasemapLayer(layer)) return QStringLiteral("배경 지도");
  return QStringLiteral("참조 지도");
}

bool isWebProvider(const QString& provider) {
  static const QStringList web = {QStringLiteral("wms"), QStringLiteral("xyz"), QStringLiteral("vectortile"),
      QStringLiteral("wfs"), QStringLiteral("wcs"), QStringLiteral("arcgismapserver"),
      QStringLiteral("arcgisfeatureserver"), QStringLiteral("oapif")};
  return web.contains(provider);
}

qint64 datasetBytes(const QFileInfo& file) {
  if (!file.isFile()) return -1;
  const QString suffix = file.suffix().toLower();
  qint64 total = file.size();
  QStringList companions;
  if (suffix == QLatin1String("shp"))
    companions = {QStringLiteral("shx"), QStringLiteral("dbf"), QStringLiteral("prj"),
                  QStringLiteral("cpg"), QStringLiteral("qix")};
  for (const QString& extension : companions) {
    const QFileInfo part(file.dir().filePath(file.completeBaseName() + QLatin1Char('.') + extension));
    if (part.isFile()) total += part.size();
  }
  if (suffix == QLatin1String("gpkg")) {
    const QFileInfo wal(file.absoluteFilePath() + QStringLiteral("-wal"));
    if (wal.isFile()) total += wal.size();
  }
  return total;
}

QString extentText(const QgsMapLayer* layer, const QgsProject* project) {
  QgsRectangle extent = layer->extent();
  if (extent.isNull() || !extent.isFinite()) return QStringLiteral("—");
  try {
    QgsCoordinateTransform toMetres(layer->crs(), QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                    project->transformContext());
    toMetres.setBallparkTransformsAreAppropriate(true);
    extent = toMetres.transformBoundingBox(extent);
  } catch (const QgsCsException&) {
    return QStringLiteral("—");
  }
  if (!extent.isFinite()) return QStringLiteral("—");
  if (extent.width() > 600000. || extent.height() > 600000.) return QStringLiteral("전국 이상");
  if (extent.width() < 1. && extent.height() < 1.) return QStringLiteral("한 지점");
  const auto km = [](double metres) {
    return QString::number(metres / 1000., 'f', metres < 10000. ? 1 : 0);
  };
  return QStringLiteral("약 %1 × %2 km").arg(km(extent.width()), km(extent.height()));
}

Item describe(const QgsMapLayer* layer, const QgsProject* project) {
  Item item;
  item.layerId = layer->id();
  item.name = layer->name();
  item.kind = kindOf(layer);
  item.extentText = extentText(layer, project);
  const QString provider = layer->providerType().toLower();
  const QVariantMap parts = QgsProviderRegistry::instance()->decodeUri(layer->providerType(), layer->source());
  if (provider == QLatin1String("memory")) {
    item.availability = Availability::Unsaved;
    item.receivedText = QStringLiteral("저장 전");
    item.location = QStringLiteral("메모리");
    return item;
  }
  const bool mbtiles = parts.value(QStringLiteral("type")).toString() == QLatin1String("mbtiles");
  QString path = mbtiles ? QUrl(parts.value(QStringLiteral("url")).toString()).toLocalFile()
                         : parts.value(QStringLiteral("path")).toString();
  if (isWebProvider(provider) && !mbtiles) {
    item.availability = Availability::Online;
    item.receivedText = QStringLiteral("받지 않음");
    const QUrl url(parts.value(QStringLiteral("url")).toString());
    item.location = url.isLocalFile() || url.host().isEmpty() ? QStringLiteral("온라인 서비스") : url.host();
    // Only XYZ tiles can be packed by 「오프라인 저장」, and only where the provider allows it.
    if (parts.value(QStringLiteral("type")).toString() == QLatin1String("xyz")) {
      const auto decision = BasemapPolicy::offlineCaching(layer);
      item.offlineNote = !decision.allowed ? QStringLiteral("오프라인 저장 불가(제공처 조건)")
          : decision.askFirst ? QStringLiteral("오프라인 저장 전 제공처 조건 확인")
                              : QStringLiteral("오프라인 저장 가능");
    } else {
      item.offlineNote = QStringLiteral("오프라인 저장 불가");
    }
    return item;
  }
  if (path.isEmpty()) path = layer->source().section(QLatin1Char('|'), 0, 0);
  if (path.startsWith(QLatin1String("/vsicurl"), Qt::CaseInsensitive) ||
      path.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) {
    item.availability = Availability::Online;
    item.receivedText = QStringLiteral("받지 않음");
    QString remote = path;
    if (remote.startsWith(QLatin1String("/vsicurl/"), Qt::CaseInsensitive)) remote = remote.mid(9);
    item.location = QUrl(remote).host();
    if (item.location.isEmpty()) item.location = QStringLiteral("온라인 서비스");
    return item;
  }
  const QFileInfo file(path);
  item.location = QDir::toNativeSeparators(file.absoluteFilePath());
  if (!file.exists()) {
    item.availability = Availability::Missing;
    item.receivedText = QStringLiteral("파일 없음");
    return item;
  }
  item.availability = Availability::Offline;
  item.receivedText = file.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
  item.bytes = datasetBytes(file);
  return item;
}

}  // namespace

QList<Item> collect(const QgsProject* project) {
  QList<Item> items;
  if (!project) return items;
  QSet<QString> seen;
  // Legend order first, so the table reads like the layer list.
  if (const QgsLayerTree* root = project->layerTreeRoot()) {
    for (const QgsLayerTreeLayer* node : root->findLayers()) {
      const QgsMapLayer* layer = node ? node->layer() : nullptr;
      if (!included(layer) || seen.contains(layer->id())) continue;
      seen.insert(layer->id());
      items.append(describe(layer, project));
    }
  }
  for (const QgsMapLayer* layer : project->mapLayers()) {
    if (!included(layer) || seen.contains(layer->id())) continue;
    seen.insert(layer->id());
    items.append(describe(layer, project));
  }
  return items;
}

QString availabilityLabel(Availability availability) {
  switch (availability) {
    case Availability::Offline: return QStringLiteral("현장 사용 가능");
    case Availability::Online: return QStringLiteral("인터넷 필요");
    case Availability::Missing: return QStringLiteral("파일 없음");
    case Availability::Unsaved: return QStringLiteral("저장 전 임시");
  }
  return {};
}

QString sizeLabel(qint64 bytes) {
  if (bytes < 0) return QStringLiteral("—");
  return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString summary(const QList<Item>& items) {
  if (items.isEmpty()) return QStringLiteral("이 조사에 올린 참조 자료가 없습니다.");
  int counts[4] = {0, 0, 0, 0};
  for (const Item& item : items) ++counts[static_cast<int>(item.availability)];
  QStringList parts;
  for (const Availability kind : {Availability::Offline, Availability::Online, Availability::Missing,
                                  Availability::Unsaved}) {
    const int count = counts[static_cast<int>(kind)];
    if (count > 0) parts.append(QStringLiteral("%1 %2개").arg(availabilityLabel(kind)).arg(count));
  }
  return parts.join(QStringLiteral(" · "));
}

}  // namespace ReferenceInventory
