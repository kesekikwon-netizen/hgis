#include "KaTitleBlock.h"

#include "core/LayerOps.h"
#include "core/LayoutService.h"

#include <QColor>
#include <QFont>
#include <algorithm>

#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgslayout.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutmeasurement.h>
#include <qgsproject.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>

namespace {
const QString kTitleProperty = QStringLiteral("ka_hgis/title_block_name");

QString orDash(const QString& value) {
  const QString v = value.trimmed();
  return v.isEmpty() ? QStringLiteral("―") : v;
}
}  // namespace

namespace KaTitleBlock {

QString itemId() { return QStringLiteral("ka_title_block"); }

Info collect(QgsProject* project) {
  Info info;
  if (!project) return info;
  const auto layers = LayerOps::domainLayersForKey(project, QStringLiteral("survey_area"));
  for (QgsVectorLayer* layer : layers) {
    if (!layer) continue;
    const int surveyIdx = layer->fields().indexOf(QStringLiteral("survey_name"));
    const int siteIdx = layer->fields().indexOf(QStringLiteral("site_name"));
    if (surveyIdx < 0 && siteIdx < 0) continue;
    QgsFeatureRequest request;
    request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
    QgsFeatureIterator it = layer->getFeatures(request);
    QgsFeature feature;
    while (it.nextFeature(feature)) {
      if (info.surveyName.isEmpty() && surveyIdx >= 0)
        info.surveyName = feature.attribute(surveyIdx).toString().trimmed();
      if (info.siteName.isEmpty() && siteIdx >= 0)
        info.siteName = feature.attribute(siteIdx).toString().trimmed();
      if (!info.surveyName.isEmpty() && !info.siteName.isEmpty()) return info;
    }
  }
  return info;
}

QString text(const QString& drawingTitle, const Info& info, const QDate& date,
             const QString& mapItemId) {
  // QGIS evaluates [% ... %] when the label is drawn, so the line follows scale changes.
  const QString scale =
      QStringLiteral("1 : [% round(map_get(item_variables('%1'), 'map_scale')) %]").arg(mapItemId);
  return QStringLiteral("도면명   %1\n조사명   %2\n유적명   %3\n축  척   %4\n작성일   %5")
      .arg(orDash(drawingTitle), orDash(info.surveyName), orDash(info.siteName), scale,
           date.isValid() ? date.toString(QStringLiteral("yyyy-MM-dd")) : QStringLiteral("―"));
}

QRectF defaultRect(const QRectF& page, const QRectF& mapRect) {
  const auto chrome = LayoutService::standardSheetChrome(page, mapRect);
  const double top = chrome.crs.bottom() + 1.5;
  const double height = std::min(18.0, page.bottom() - 3.0 - top);
  const double width = std::max(0.0, chrome.north.left() - 2.0 - chrome.crs.left());
  if (height >= 13.0 && width >= 50.0)
    return QRectF(chrome.crs.left(), top, width, height);
  const QRectF map = chrome.map;
  const double w = std::min(64.0, std::max(40.0, map.width() * 0.4));
  return QRectF(map.right() - w - 2.0, map.bottom() - 19.0, w, 17.0);
}

QgsLayoutItemLabel* place(QgsLayout* layout, const QRectF& rect, const QString& drawingTitle,
                          const QString& text) {
  if (!layout) return nullptr;
  auto* label = dynamic_cast<QgsLayoutItemLabel*>(layout->itemById(itemId()));
  const bool created = !label;
  if (created) {
    label = new QgsLayoutItemLabel(layout);
    label->setId(itemId());
    QgsTextFormat format;
    format.setFont(QFont(QStringLiteral("Malgun Gothic")));
    format.setSize(6.5);
    format.setSizeUnit(Qgis::RenderUnit::Points);
    format.setColor(QColor(17, 24, 39));
    label->setTextFormat(format);
    label->setHAlign(Qt::AlignLeft);
    label->setVAlign(Qt::AlignTop);
    label->setMarginX(1.2);
    label->setMarginY(0.8);
    label->setFrameEnabled(true);
    label->setFrameStrokeColor(QColor(17, 24, 39));
    label->setFrameStrokeWidth(QgsLayoutMeasurement(0.25, Qgis::LayoutUnit::Millimeters));
    label->setBackgroundEnabled(true);
    label->setBackgroundColor(Qt::white);
  }
  label->setText(text);
  label->setCustomProperty(kTitleProperty, drawingTitle);
  if (created) {
    label->attemptSetSceneRect(rect);
    layout->addLayoutItem(label);
  }
  label->refresh();
  return label;
}

QString drawingTitleOf(QgsLayout* layout) {
  auto* label = layout ? dynamic_cast<QgsLayoutItemLabel*>(layout->itemById(itemId())) : nullptr;
  return label ? label->customProperty(kTitleProperty).toString() : QString();
}

}  // namespace KaTitleBlock
