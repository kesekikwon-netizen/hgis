#include "SurveyHandles.h"

#include <QFileInfo>

#include <qgsmaplayerstyle.h>
#include <qgsproviderregistry.h>
#include <qgsvectorlayer.h>

namespace SurveyHandles {
namespace {

const QString kTable = QStringLiteral("ka_hgis/released_table");
const QString kEditable = QStringLiteral("ka_hgis/released_editable");
const QString kStyle = QStringLiteral("ka_hgis/released_style");

}  // namespace

QgsMapLayer* release(QgsMapLayer* layer, const QString& surveyGpkg) {
  auto* vl = qobject_cast<QgsVectorLayer*>(layer);
  if (!vl || vl->providerType() != QLatin1String("ogr") || surveyGpkg.isEmpty() || vl->isModified()) return layer;
  const QVariantMap parts = QgsProviderRegistry::instance()->decodeUri(QStringLiteral("ogr"), vl->source());
  const QString table = parts.value(QStringLiteral("layerName")).toString();
  if (table.isEmpty() || QFileInfo(parts.value(QStringLiteral("path")).toString())
                                 .absoluteFilePath()
                                 .compare(QFileInfo(surveyGpkg).absoluteFilePath(), Qt::CaseInsensitive) != 0)
    return layer;
  QgsMapLayerStyle style;
  style.readFromLayer(vl);  // 빈 경로로 바꾸면 기하 종류를 몰라 모양이 바뀔 수 있다
  vl->setCustomProperty(kTable, table);
  vl->setCustomProperty(kEditable, vl->isEditable());
  vl->setCustomProperty(kStyle, style.xmlData());
  if (vl->isEditable()) vl->rollBack(true);  // 바뀐 것이 없는 편집 상태만 닫는다
  vl->setDataSource(QString(), vl->name(), QStringLiteral("ogr"));
  return layer;
}

QgsMapLayer* reattach(QgsMapLayer* layer, const QString& surveyGpkg) {
  auto* vl = qobject_cast<QgsVectorLayer*>(layer);
  if (!vl || vl->customProperty(kTable).toString().isEmpty()) return layer;
  const QString table = vl->customProperty(kTable).toString();
  const bool editable = vl->customProperty(kEditable).toBool();
  const QgsMapLayerStyle style(vl->customProperty(kStyle).toString());
  vl->removeCustomProperty(kTable);
  vl->removeCustomProperty(kEditable);
  vl->removeCustomProperty(kStyle);
  vl->setDataSource(QStringLiteral("%1|layername=%2").arg(surveyGpkg, table), vl->name(), QStringLiteral("ogr"));
  if (vl->isValid()) {
    style.writeToLayer(vl);
    if (editable) vl->startEditing();
  }
  return layer;
}

}  // namespace SurveyHandles
